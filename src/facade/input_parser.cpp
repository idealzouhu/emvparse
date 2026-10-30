// Public DLL facade: wires TLV + APDU registries, classifies input, and routes to formatters.
#include "emvparse/input_parser.h"
#include "apdu_parser.h"
#include "emv_parse_context.h"
#include "logger.h"
#include "parse_output_pipeline.h"
#include "tlv_tree_formatter.h"
#include "transaction_script.h"
#include "tlv_parser.h"
#include "hex_util.h"
#include "input_parser_internal.h"
#include <fstream>
#include <sstream>

namespace {

// Distinguishes invalid hex from empty input (empty hex is valid → empty bytes).
bool hexToBytesOrError(
    const std::string& hex,
    std::vector<uint8_t>& out,
    std::string& err
) {
    out = parseHexString(hex);
    if (out.empty() && !hex.empty()) {
        err = "invalid hex input";
        return false;
    }
    return true;
}

} // namespace

struct EmvInputParser::Impl {
    Logger logger_;           // shared by both registries via setLogger
    TLVParser tlvParser_;
    ApduRegistry apduRegistry_;
    std::string lastManifestPath_; // retained for script scheme reload
    std::string lastScheme_;
    std::string lastError_;        // facade-level summary; detail still in LogCallback

    Impl() {
        tlvParser_.setLogger(&logger_);
        apduRegistry_.setLogger(&logger_);
    }
};

EmvParseContext EmvInputParserImplAccess::context(EmvInputParser& parser) {
    return EmvParseContext{parser.impl_->tlvParser_, parser.impl_->apduRegistry_};
}

EmvParseContext EmvInputParserImplAccess::context(const EmvInputParser& parser) {
    return EmvParseContext{
        const_cast<TLVParser&>(parser.impl_->tlvParser_),
        const_cast<ApduRegistry&>(parser.impl_->apduRegistry_)};
}

TLVParser& EmvInputParserImplAccess::tlvParser(EmvInputParser& parser) {
    return parser.impl_->tlvParser_;
}

const TLVParser& EmvInputParserImplAccess::tlvParser(const EmvInputParser& parser) {
    return parser.impl_->tlvParser_;
}

TagRegistry* EmvInputParserImplAccess::tagRegistry(EmvInputParser& parser) {
    return parser.impl_->tlvParser_.tagRegistry();
}

ApduRegistry* EmvInputParserImplAccess::apduRegistry(EmvInputParser& parser) {
    return &parser.impl_->apduRegistry_;
}

EmvInputParser::EmvInputParser() : impl_(std::make_unique<Impl>()) {}

EmvInputParser::EmvInputParser(const std::string& manifestPath, const std::string& scheme)
    : impl_(std::make_unique<Impl>()) {
    loadRegistry(manifestPath, scheme);
}

EmvInputParser::EmvInputParser(EmvInputParser&&) noexcept = default;

EmvInputParser& EmvInputParser::operator=(EmvInputParser&&) noexcept = default;

EmvInputParser::~EmvInputParser() = default;

bool EmvInputParser::loadRegistry(const std::string& manifestPath, const std::string& scheme) {
    impl_->lastManifestPath_ = manifestPath;
    impl_->lastScheme_ = scheme;
    impl_->lastError_.clear();
    const bool apduOk = impl_->apduRegistry_.loadFromManifest(manifestPath);
    const bool tagOk = impl_->tlvParser_.loadTagRegistry(manifestPath, scheme);
    if (!tagOk && !apduOk) {
        impl_->lastError_ = "failed to load tag and APDU registry";
    } else if (!tagOk) {
        impl_->lastError_ = "failed to load tag registry";
    } else if (!apduOk) {
        impl_->lastError_ = "failed to load APDU registry";
    }
    return tagOk && apduOk;
}

void EmvInputParser::setLogCallback(LogCallback callback) {
    impl_->logger_.setLogCallback(std::move(callback));
}

std::string EmvInputParser::lastError() const {
    return impl_->lastError_;
}

std::vector<uint8_t> EmvInputParser::hexToBytes(const std::string& hexStr) {
    return parseHexString(hexStr);
}

std::string EmvInputParser::parseHexToJson(const std::string& hexStr) {
    impl_->lastError_.clear();
    std::vector<uint8_t> data;
    if (!hexToBytesOrError(hexStr, data, impl_->lastError_)) {
        return {};
    }
    const auto roots = impl_->tlvParser_.parseBytes(data);
    return TlvTreeFormatter::toJsonArray(roots, impl_->tlvParser_.tagRegistry());
}

std::string EmvInputParser::getTagInfo(const std::string& tagHex) const {
    return impl_->tlvParser_.getTagInfo(tagHex);
}

std::string EmvInputParser::parseHexWithOptions(
    const std::string& hexStr,
    const emvparse::ParseOutputOptions& options
) {
    impl_->lastError_.clear();
    auto data = parseHexString(hexStr);
    if (data.empty() && !hexStr.empty()) {
        impl_->lastError_ = "invalid hex input";
        return {};
    }

    const auto tlvParseFn = [this](const std::vector<uint8_t>& payload) {
        return impl_->tlvParser_.parseBytes(payload);
    };
    const TlvBodyConsumer tlvConsumer = [this](const std::vector<uint8_t>& body) {
        return impl_->tlvParser_.bytesFullyConsumedAsTlv(body);
    };

    // Auto: registry-backed C-APDU match, then R-APDU (body TLV + SW), else raw BER-TLV.
    const ParsedInputKind kind = ApduParser::classifyInput(
        data, options.input, &impl_->apduRegistry_, tlvConsumer);

    ApduParseResult apduResult;
    if (kind == ParsedInputKind::Capdu) {
        apduResult = ApduParser::parseCapdu(data, &impl_->apduRegistry_, tlvParseFn);
    } else if (kind == ParsedInputKind::Rapdu) {
        apduResult = ApduParser::parseRapdu(data, &impl_->apduRegistry_, tlvParseFn);
    } else {
        apduResult.kind = ParsedInputKind::Tlv;
        apduResult.tlvRoots = impl_->tlvParser_.parseBytes(data);
    }

    return ParseOutputPipeline::formatApdu(
        apduResult, impl_->tlvParser_.tagRegistry(), options);
}

std::string EmvInputParser::parseScriptJson(
    const std::string& json,
    const emvparse::ParseOutputOptions& options
) {
    impl_->lastError_.clear();
    TransactionScript script;
    if (!TransactionScriptParser::loadFromJsonString(json, script, impl_->lastError_)) {
        return {};
    }

    // Script may name a scheme overlay different from the parser's initial loadRegistry call.
    if (!script.scheme.empty() && !impl_->lastManifestPath_.empty() &&
        script.scheme != impl_->lastScheme_) {
        loadRegistry(impl_->lastManifestPath_, script.scheme);
    }

    EmvParseContext ctx = EmvInputParserImplAccess::context(*this);
    TransactionScriptParseResult parsed = TransactionScriptParser::parse(ctx, script);
    if (!parsed.error.empty()) {
        impl_->lastError_ = parsed.error;
        return {};
    }

    return ParseOutputPipeline::formatScript(
        parsed, impl_->tlvParser_.tagRegistry(), options);
}

std::string EmvInputParser::parseScriptFile(
    const std::string& path,
    const emvparse::ParseOutputOptions& options
) {
    impl_->lastError_.clear();
    TransactionScript script;
    if (!TransactionScriptParser::loadFromFile(path, script, impl_->lastError_)) {
        return {};
    }

    if (!script.scheme.empty() && !impl_->lastManifestPath_.empty() &&
        script.scheme != impl_->lastScheme_) {
        loadRegistry(impl_->lastManifestPath_, script.scheme);
    }

    EmvParseContext ctx = EmvInputParserImplAccess::context(*this);
    TransactionScriptParseResult parsed = TransactionScriptParser::parse(ctx, script);
    if (!parsed.error.empty()) {
        impl_->lastError_ = parsed.error;
        return {};
    }

    return ParseOutputPipeline::formatScript(
        parsed, impl_->tlvParser_.tagRegistry(), options);
}
