#include "apdu_parser.h"
#include "dol_parser.h"
#include "tlv_parser.h"
#include "tag_registry.h"
#include "value_semantic_parser.h"
#include "logger.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

namespace {

std::string upperHexByte(uint8_t b) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

// Structure must match ISO 7816-4 and CLA/INS must appear in the loaded command registry.
bool isCapduCandidate(
    const std::vector<uint8_t>& data,
    CapduFields& fields,
    ApduRegistry* registry
) {
    if (!ApduParser::tryParseCapduStructure(data, fields)) {
        return false;
    }
    if (!registry) {
        return false;
    }
    return registry->findCommand(fields.cla, fields.ins) != nullptr;
}

std::vector<uint8_t> tagBytesFromHex(const std::string& tagHex) {
    std::vector<uint8_t> tag;
    for (size_t i = 0; i + 1 < tagHex.size(); i += 2) {
        tag.push_back(static_cast<uint8_t>(std::stoul(tagHex.substr(i, 2), nullptr, 16)));
    }
    return tag;
}

std::shared_ptr<TLV> makeLogicalTlv(const std::string& tagHex, const std::vector<uint8_t>& value) {
    return std::make_shared<TLV>(tagBytesFromHex(tagHex), value);
}

/*
 * EMV Response Message Template Format 1 uses primitive tag 80; its value is a
 * command-specific field concatenation, not nested BER-TLV on the wire.
 */
std::vector<std::shared_ptr<TLV>> expandFormat1Template(
    const std::vector<std::shared_ptr<TLV>>& tlvRoots,
    const RapduParseContext* context
) {
    std::vector<std::shared_ptr<TLV>> expanded;
    if (!context || !context->requestCommand || !context->requestCapdu) {
        return expanded;
    }
    if (tlvRoots.size() != 1u) {
        return expanded;
    }
    const auto& root = tlvRoots[0];
    if (root->tagHex() != "80") {
        return expanded;
    }
    const auto& value = root->getValue();
    const uint8_t ins = context->requestCapdu->ins;

    if (ins == 0xA8) {
        // GPO Format 1: AIP[2] || AFL[n]
        if (value.size() < 2) {
            return expanded;
        }
        expanded.push_back(makeLogicalTlv("82", std::vector<uint8_t>(value.begin(), value.begin() + 2)));
        if (value.size() > 2) {
            expanded.push_back(
                makeLogicalTlv("94", std::vector<uint8_t>(value.begin() + 2, value.end())));
        }
        return expanded;
    }
    if (ins == 0xAE) {
        // GENERATE AC Format 1: CID[1] ATC[2] AC[8] IAD[n]
        if (value.size() < 11) {
            return expanded;
        }
        expanded.push_back(makeLogicalTlv("9F27", std::vector<uint8_t>(value.begin(), value.begin() + 1)));
        expanded.push_back(makeLogicalTlv("9F36", std::vector<uint8_t>(value.begin() + 1, value.begin() + 3)));
        expanded.push_back(makeLogicalTlv("9F26", std::vector<uint8_t>(value.begin() + 3, value.begin() + 11)));
        if (value.size() > 11) {
            expanded.push_back(
                makeLogicalTlv("9F10", std::vector<uint8_t>(value.begin() + 11, value.end())));
        }
        return expanded;
    }
    if (ins == 0x88) {
        // INTERNAL AUTHENTICATE Format 1: single cryptographic block
        expanded.push_back(makeLogicalTlv("9F4B", value));
        return expanded;
    }
    return expanded;
}

// Trailing SW1-SW2; optional body must parse as TLV and SW must be known or in a standard range.
bool isRapduCandidate(
    const std::vector<uint8_t>& data,
    ApduRegistry* registry,
    const TlvBodyConsumer& tlvConsumesAll
) {
    if (data.size() < 2 || !registry) {
        return false;
    }
    const uint16_t sw =
        static_cast<uint16_t>((static_cast<uint16_t>(data[data.size() - 2]) << 8) |
                              data[data.size() - 1]);
    std::vector<uint8_t> body(data.begin(), data.end() - 2);
    if (body.empty()) {
        return registry->isKnownStatusPattern(sw) || !registry->lookupStatusWord(sw).empty();
    }
    if (!tlvConsumesAll(body)) {
        return false;
    }
    return registry->isKnownStatusPattern(sw) || !registry->lookupStatusWord(sw).empty();
}

} // namespace

ParsedInputKind ApduParser::classifyInput(
    const std::vector<uint8_t>& data,
    emvparse::InputMode mode,
    ApduRegistry* registry,
    const TlvBodyConsumer& tlvConsumesAll
) {
    // Fixed modes skip heuristics; Auto uses registry + structural checks below.
    switch (mode) {
    case emvparse::InputMode::Tlv:
        return ParsedInputKind::Tlv;
    case emvparse::InputMode::Capdu:
        return ParsedInputKind::Capdu;
    case emvparse::InputMode::Rapdu:
        return ParsedInputKind::Rapdu;
    case emvparse::InputMode::Auto:
        break;
    }

    CapduFields capduFields;
    if (isCapduCandidate(data, capduFields, registry)) {
        return ParsedInputKind::Capdu;
    }
    if (isRapduCandidate(data, registry, tlvConsumesAll)) {
        return ParsedInputKind::Rapdu;
    }
    return ParsedInputKind::Tlv;
}

// ISO 7816-4 cases 1–4 (Lc=0 rejected; no extended Lc/Le here).
bool ApduParser::tryParseCapduStructure(const std::vector<uint8_t>& data, CapduFields& out) {
    if (data.size() < 4) {
        return false;
    }

    out.cla = data[0];
    out.ins = data[1];
    out.p1 = data[2];
    out.p2 = data[3];
    out.hasLc = false;
    out.hasLe = false;
    out.lc = 0;
    out.le = 0;
    out.commandData.clear();

    if (data.size() == 4) {
        out.headerHex = bytesHex(std::vector<uint8_t>(data.begin(), data.begin() + 4));
        out.dataHex.clear();
        return true;
    }

    if (data.size() == 5) {
        out.hasLe = true;
        out.le = data[4];
        out.headerHex = bytesHex(std::vector<uint8_t>(data.begin(), data.begin() + 4));
        out.dataHex.clear();
        return true;
    }

    const uint8_t lc = data[4];
    if (lc == 0) {
        return false;
    }
    const size_t case3Len = 5u + static_cast<size_t>(lc);
    if (data.size() == case3Len) {
        out.hasLc = true;
        out.lc = lc;
        out.commandData.assign(data.begin() + 5, data.begin() + 5 + lc);
        out.headerHex = bytesHex(std::vector<uint8_t>(data.begin(), data.begin() + 5));
        out.dataHex = bytesHex(out.commandData);
        return true;
    }
    if (data.size() == case3Len + 1) {
        out.hasLc = true;
        out.lc = lc;
        out.commandData.assign(data.begin() + 5, data.begin() + 5 + lc);
        out.hasLe = true;
        out.le = data[case3Len];
        out.headerHex = bytesHex(std::vector<uint8_t>(data.begin(), data.begin() + 5));
        out.dataHex = bytesHex(out.commandData);
        return true;
    }

    return false;
}

ApduParseResult ApduParser::parseCapdu(
    const std::vector<uint8_t>& data,
    ApduRegistry* registry,
    const std::function<std::vector<std::shared_ptr<TLV>>(const std::vector<uint8_t>&)>& parseTlv
) {
    ApduParseResult result;
    result.kind = ParsedInputKind::Capdu;

    if (!tryParseCapduStructure(data, result.capdu)) {
        return result;
    }

    if (registry) {
        result.command = registry->findCommand(result.capdu.cla, result.capdu.ins);
    }
    enrichCommandSemantics(result);

    if (result.command && result.command->dataFormat == "ber_tlv" && !result.capdu.commandData.empty()) {
        result.tlvRoots = parseTlv(result.capdu.commandData);
    }

    return result;
}

ApduParseResult ApduParser::parseRapdu(
    const std::vector<uint8_t>& data,
    ApduRegistry* registry,
    const std::function<std::vector<std::shared_ptr<TLV>>(const std::vector<uint8_t>&)>& parseTlv,
    const RapduParseContext* context
) {
    ApduParseResult result;
    result.kind = ParsedInputKind::Rapdu;

    if (data.size() < 2) {
        return result;
    }

    result.statusWord =
        static_cast<uint16_t>((static_cast<uint16_t>(data[data.size() - 2]) << 8) | data[data.size() - 1]);
    std::vector<uint8_t> body(data.begin(), data.end() - 2);
    if (!body.empty()) {
        result.tlvRoots = parseTlv(body);
    }

    if (registry) {
        result.statusMeaning = registry->lookupStatusWord(result.statusWord);
        if (result.statusMeaning.empty()) {
            const uint8_t hi = static_cast<uint8_t>((result.statusWord >> 8) & 0xFF);
            if (hi == 0x61) {
                const uint8_t extra = static_cast<uint8_t>(result.statusWord & 0xFF);
                result.statusMeaning =
                    "Normal processing; " + std::to_string(static_cast<int>(extra)) + " bytes available";
            } else if (hi == 0x6C) {
                const uint8_t correct = static_cast<uint8_t>(result.statusWord & 0xFF);
                result.statusMeaning =
                    "Wrong length Le; correct Le=" + upperHexByte(correct);
            } else if (registry->isKnownStatusPattern(result.statusWord)) {
                result.statusMeaning = "Status warning or processing state";
            } else {
                result.statusMeaning = "unknown";
            }
        }
    } else {
        result.statusMeaning = "unknown";
    }

    // With paired C-APDU context only: expand Format 1 template 80 into logical fields.
    if (context) {
        result.format1Children = expandFormat1Template(result.tlvRoots, context);
        result.format1Expanded = !result.format1Children.empty();
    }

    return result;
}

std::vector<std::shared_ptr<TLV>> ApduParser::displayBodyRoots(const ApduParseResult& result) {
    if (result.format1Expanded && !result.format1Children.empty() && result.tlvRoots.size() == 1u &&
        result.tlvRoots[0]->tagHex() == "80") {
        return {TLVParser::displayWithChildren(result.tlvRoots[0], result.format1Children)};
    }
    if (result.format1Expanded && !result.format1Children.empty()) {
        return result.format1Children;
    }
    return result.tlvRoots;
}

void ApduParser::expandCapduDolData(
    ApduParseResult& capduResult,
    TagRegistry* tagRegistry,
    const std::vector<EmvDolEntry>& pdol,
    const std::vector<EmvDolEntry>& cdol1,
    const std::vector<EmvDolEntry>& cdol2,
    const std::vector<EmvDolEntry>& ddol
) {
    if (!capduResult.command || capduResult.capdu.commandData.empty()) {
        return;
    }
    const uint8_t ins = capduResult.capdu.ins;
    std::vector<uint8_t> payload;
    const std::vector<EmvDolEntry>* dol = nullptr;

    if (ins == 0xA8) {
        if (!pdol.empty()) {
            dol = &pdol;
            if (!capduResult.tlvRoots.empty() && capduResult.tlvRoots[0]->tagHex() == "83") {
                payload = capduResult.tlvRoots[0]->getValue();
            } else {
                payload = capduResult.capdu.commandData;
            }
        }
    } else if (ins == 0xAE) {
        const uint8_t p1 = capduResult.capdu.p1;
        if ((p1 & 0xC0) == 0x40 && !cdol2.empty()) {
            dol = &cdol2;
        } else if (!cdol1.empty()) {
            dol = &cdol1;
        }
        if (dol) {
            if (!capduResult.tlvRoots.empty()) {
                payload = capduResult.capdu.commandData;
            } else {
                payload = capduResult.capdu.commandData;
            }
        }
    } else if (ins == 0x88 && !ddol.empty()) {
        dol = &ddol;
        payload = capduResult.capdu.commandData;
    }

    if (!dol || dol->empty() || payload.empty()) {
        return;
    }
    auto split = splitDolPayload(*dol, payload);
    for (auto& entry : split.entries) {
        std::vector<EmvDolEntry> one = {entry.dolEntry};
        enrichDolEntryNames(one, tagRegistry);
        entry.dolEntry = one[0];
    }
    capduResult.dolDataValues = split.entries;
}

std::string ApduParser::resolveParamMeaning(const ApduParamRule& rule, uint8_t value, int* sfiOut) {
    const std::string key = upperHexByte(value);
    const auto enumIt = rule.enumMap.find(key);
    if (enumIt != rule.enumMap.end()) {
        return enumIt->second;
    }
    if (rule.numericRecord) {
        return "Record number " + std::to_string(static_cast<int>(value));
    }
    if (rule.tagReference) {
        return "Tag reference byte " + key;
    }
    if (rule.sfiShift >= 0) {
        const int sfi = static_cast<int>(value) >> rule.sfiShift;
        if (sfiOut) {
            *sfiOut = sfi;
        }
        std::ostringstream oss;
        oss << "SFI " << sfi;
        const int mode = value & ((1 << rule.sfiShift) - 1);
        oss << " (read mode bits " << mode << ")";
        return oss.str();
    }
    if (!rule.description.empty()) {
        return rule.description;
    }
    if (!rule.name.empty()) {
        return rule.name + " " + key;
    }
    return key;
}

void ApduParser::enrichCommandSemantics(ApduParseResult& result) {
    if (!result.command) {
        return;
    }
    int sfi = -1;
    result.p1Meaning = resolveParamMeaning(result.command->p1, result.capdu.p1, nullptr);
    result.p2Meaning = resolveParamMeaning(result.command->p2, result.capdu.p2, &sfi);
    if (sfi >= 0) {
        result.sfi = sfi;
    }
    if (result.command->p1.tagReference && result.command->p2.tagReference) {
        const std::string tagRef = upperHexByte(result.capdu.p1) + upperHexByte(result.capdu.p2);
        result.p1Meaning = "Tag " + tagRef;
        result.p2Meaning.clear();
    }
}

std::string ApduParser::byteHex(uint8_t b) {
    return upperHexByte(b);
}

std::string ApduParser::bytesHex(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); ++i) {
        oss << upperHexByte(data[i]);
    }
    return oss.str();
}

std::string ApduParser::bytesToHexUpper(const std::vector<uint8_t>& data) {
    return bytesHex(data);
}

void ApduRegistry::setLogger(Logger* logger) {
    logger_ = logger;
}
