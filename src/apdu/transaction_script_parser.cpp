#include "transaction_script.h"
#include "emv_parse_context.h"
#include "hex_util.h"
#include "tlv_parser.h"
#include "tag_registry.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <cctype>

namespace {

// Collect DOL definitions from FCI/GPO/etc. so later GET PROCESSING OPTIONS / GENERATE AC data can split.
void walkTlvUpdateDol(
    const std::shared_ptr<TLV>& tlv,
    ScriptDolContext& ctx,
    TagRegistry* registry
) {
    const std::string tagHex = tlv->tagHex();
    if (tagHex == "9F38" || tagHex == "8C" || tagHex == "8D" || tagHex == "9F49") {
        auto parsed = parseEmvDolList(tlv->getValue());
        enrichDolEntryNames(parsed.entries, registry);
        if (tagHex == "9F38") {
            ctx.pdol = parsed.entries;
        } else if (tagHex == "8C") {
            ctx.cdol1 = parsed.entries;
        } else if (tagHex == "8D") {
            ctx.cdol2 = parsed.entries;
        } else if (tagHex == "9F49") {
            ctx.ddol = parsed.entries;
        }
    }
    for (const auto& child : tlv->getChildren()) {
        walkTlvUpdateDol(child, ctx, registry);
    }
}

bool isPrintableAscii(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return false;
    }
    for (uint8_t b : data) {
        if (b < 0x20 || b > 0x7E) {
            return false;
        }
    }
    return true;
}

std::string asciiFromBytes(const std::vector<uint8_t>& data) {
    return std::string(data.begin(), data.end());
}

} // namespace

void ScriptDolContext::absorbTlvTree(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry
) {
    for (const auto& root : roots) {
        walkTlvUpdateDol(root, *this, registry);
    }
}

bool TransactionScriptParser::loadFromJsonString(
    const std::string& json,
    TransactionScript& out,
    std::string& error
) {
    try {
        const auto root = nlohmann::json::parse(json);
        if (!root.is_object()) {
            error = "script root must be a JSON object";
            return false;
        }
        if (!root.contains("version") || !root["version"].is_number_integer()) {
            error = "missing or invalid version";
            return false;
        }
        out.version = root["version"].get<int>();
        if (out.version != 1) {
            error = "unsupported script version (only 1 supported)";
            return false;
        }
        out.title = root.value("title", std::string());
        out.scheme = root.value("scheme", std::string());
        if (!root.contains("steps") || !root["steps"].is_array()) {
            error = "missing steps array";
            return false;
        }
        if (root["steps"].empty()) {
            error = "steps must not be empty";
            return false;
        }
        out.steps.clear();
        for (const auto& step : root["steps"]) {
            if (!step.is_object()) {
                error = "each step must be an object";
                return false;
            }
            if (!step.contains("capdu") || !step.contains("rapdu")) {
                error = "each step requires capdu and rapdu";
                return false;
            }
            TransactionScriptStepInput input;
            input.capduHex = step["capdu"].get<std::string>();
            input.rapduHex = step["rapdu"].get<std::string>();
            out.steps.push_back(input);
        }
        return true;
    } catch (const std::exception& ex) {
        error = std::string("JSON parse error: ") + ex.what();
        return false;
    }
}

bool TransactionScriptParser::loadFromFile(
    const std::string& path,
    TransactionScript& out,
    std::string& error
) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        error = "cannot open script file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return loadFromJsonString(ss.str(), out, error);
}

std::string TransactionScriptParser::deriveStepTitle(const ApduParseResult& capdu) {
    if (!capdu.command) {
        return "C-APDU";
    }
    const std::string& name = capdu.command->name;
    if (name.find("SELECT") != std::string::npos || capdu.capdu.ins == 0xA4) {
        const auto& data = capdu.capdu.commandData;
        if (capdu.capdu.p1 == 0x04 && !data.empty()) {
            if (isPrintableAscii(data)) {
                const std::string text = asciiFromBytes(data);
                if (text == "1PAY.SYS.DDF01") {
                    return "SELECT PSE (1PAY.SYS.DDF01)";
                }
                return "SELECT (\"" + text + "\")";
            }
            return "SELECT (" + capdu.capdu.dataHex + ")";
        }
        return "SELECT";
    }
    if (capdu.capdu.ins == 0xB2) {
        std::ostringstream oss;
        oss << "READ RECORD (SFI=" << (capdu.sfi >= 0 ? capdu.sfi : 0)
            << ", record " << static_cast<int>(capdu.capdu.p1) << ")";
        return oss.str();
    }
    if (capdu.capdu.ins == 0xA8) {
        return "GET PROCESSING OPTIONS";
    }
    if (capdu.capdu.ins == 0xAE) {
        std::string title = "GENERATE AC";
        if (!capdu.p1Meaning.empty()) {
            title += " (" + capdu.p1Meaning + ")";
        }
        return title;
    }
    if (capdu.capdu.ins == 0x88) {
        return name;
    }
    return name;
}

TransactionScriptParseResult TransactionScriptParser::parse(
    EmvParseContext& context,
    const TransactionScript& script
) {
    TransactionScriptParseResult result;
    result.script = script;

    TagRegistry* tagRegistry = context.tagRegistry();
    ApduRegistry* apduRegistry = &context.apduRegistry;
    TLVParser& tlvParser = context.tlvParser;
    const auto tlvParseFn = [&tlvParser](const std::vector<uint8_t>& payload) {
        return tlvParser.parseBytes(payload);
    };

    ScriptDolContext dolCtx;
    int stepIndex = 0;
    for (const auto& stepInput : script.steps) {
        // dolCtx grows after each step via absorbTlvTree on C-APDU and R-APDU TLV roots.
        TransactionScriptStepResult step;
        step.index = stepIndex++;

        const auto capduBytes = parseHexString(stepInput.capduHex);
        const auto rapduBytes = parseHexString(stepInput.rapduHex);
        if (capduBytes.empty() && !stepInput.capduHex.empty()) {
            result.error = "invalid capdu hex at step " + std::to_string(step.index);
            return result;
        }
        if (rapduBytes.empty() && !stepInput.rapduHex.empty()) {
            result.error = "invalid rapdu hex at step " + std::to_string(step.index);
            return result;
        }

        step.capduWireHex = ApduParser::bytesToHexUpper(capduBytes);
        step.rapduWireHex = ApduParser::bytesToHexUpper(rapduBytes);
        step.capdu = ApduParser::parseCapdu(capduBytes, apduRegistry, tlvParseFn);
        ApduParser::expandCapduDolData(
            step.capdu,
            tagRegistry,
            dolCtx.pdol,
            dolCtx.cdol1,
            dolCtx.cdol2,
            dolCtx.ddol);

        RapduParseContext rapduCtx;
        rapduCtx.requestCapdu = &step.capdu.capdu;
        rapduCtx.requestCommand = step.capdu.command;
        step.rapdu = ApduParser::parseRapdu(rapduBytes, apduRegistry, tlvParseFn, &rapduCtx);

        step.stepTitle = deriveStepTitle(step.capdu);

        dolCtx.absorbTlvTree(step.capdu.tlvRoots, tagRegistry);
        dolCtx.absorbTlvTree(step.rapdu.tlvRoots, tagRegistry);

        result.steps.push_back(step);
    }
    return result;
}
