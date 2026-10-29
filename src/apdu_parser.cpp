#include "emvparse/apdu_parser.h"
#include "emvparse/dol_parser.h"
#include "tag_registry.h"
#include "tlv_formatter.h"
#include "value_semantic_parser.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>

namespace {

std::string upperHexByte(uint8_t b) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

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

std::string ApduParser::formatJson(const ApduParseResult& result, TagRegistry* tagRegistry) {
    if (result.kind == ParsedInputKind::Tlv) {
        nlohmann::ordered_json jsonArray = nlohmann::ordered_json::array();
        for (const auto& tlv : result.tlvRoots) {
            jsonArray.push_back(TLVFormatter::toJson(tlv, tagRegistry));
        }
        return jsonArray.dump(2);
    }

    nlohmann::ordered_json root;
    if (result.kind == ParsedInputKind::Capdu) {
        root["inputKind"] = "capdu";
        nlohmann::ordered_json command;
        command["name"] = result.command ? result.command->name : "";
        command["cla"] = byteHex(result.capdu.cla);
        command["ins"] = byteHex(result.capdu.ins);
        command["p1"] = byteHex(result.capdu.p1);
        command["p2"] = byteHex(result.capdu.p2);
        if (!result.p1Meaning.empty()) {
            command["p1Meaning"] = result.p1Meaning;
        }
        if (!result.p2Meaning.empty()) {
            command["p2Meaning"] = result.p2Meaning;
        }
        if (result.sfi >= 0) {
            command["sfi"] = result.sfi;
        }
        if (result.capdu.hasLc) {
            command["lc"] = byteHex(result.capdu.lc);
        }
        if (result.capdu.hasLe) {
            command["le"] = byteHex(result.capdu.le);
        }
        root["command"] = command;

        nlohmann::ordered_json dataBlock;
        dataBlock["format"] = result.command ? result.command->dataFormat : "none";
        if (!result.tlvRoots.empty()) {
            nlohmann::ordered_json tlvArray = nlohmann::ordered_json::array();
            for (const auto& tlv : result.tlvRoots) {
                tlvArray.push_back(TLVFormatter::toJson(tlv, tagRegistry));
            }
            dataBlock["tlv"] = tlvArray;
        } else if (!result.capdu.dataHex.empty()) {
            dataBlock["hex"] = result.capdu.dataHex;
        }
        if (!result.dolDataValues.empty()) {
            nlohmann::ordered_json dolArray = nlohmann::ordered_json::array();
            for (const auto& entry : result.dolDataValues) {
                nlohmann::ordered_json item;
                item["tag"] = entry.dolEntry.tagHex;
                item["length"] = static_cast<int>(entry.dolEntry.length);
                if (!entry.dolEntry.name.empty()) {
                    item["name"] = entry.dolEntry.name;
                }
                item["value"] = entry.valueHex;
                if (entry.missing) {
                    item["missing"] = true;
                }
                dolArray.push_back(item);
            }
            dataBlock["dolValues"] = dolArray;
        }
        root["data"] = dataBlock;

        nlohmann::ordered_json raw;
        raw["header"] = result.capdu.headerHex;
        if (!result.capdu.dataHex.empty()) {
            raw["dataHex"] = result.capdu.dataHex;
        }
        root["raw"] = raw;
        return root.dump(2);
    }

    root["inputKind"] = "rapdu";
    nlohmann::ordered_json sw;
    sw["sw"] = byteHex(static_cast<uint8_t>((result.statusWord >> 8) & 0xFF)) +
               byteHex(static_cast<uint8_t>(result.statusWord & 0xFF));
    sw["meaning"] = result.statusMeaning;
    root["statusWord"] = sw;

    nlohmann::ordered_json body;
    nlohmann::ordered_json tlvArray = nlohmann::ordered_json::array();
    for (const auto& tlv : result.tlvRoots) {
        tlvArray.push_back(TLVFormatter::toJson(tlv, tagRegistry));
    }
    body["tlv"] = tlvArray;
    if (result.format1Expanded && !result.format1Children.empty()) {
        nlohmann::ordered_json f1 = nlohmann::ordered_json::array();
        for (const auto& tlv : result.format1Children) {
            f1.push_back(TLVFormatter::toJson(tlv, tagRegistry));
        }
        body["format1"] = f1;
        body["format1Note"] =
            "On-wire body is primitive tag 80 (Format 1); fields below have no on-wire tags.";
    }
    root["body"] = body;
    return root.dump(2);
}

namespace {

std::vector<uint8_t> capduRawBytes(const CapduFields& capdu) {
    std::vector<uint8_t> bytes = {capdu.cla, capdu.ins, capdu.p1, capdu.p2};
    if (capdu.hasLc) {
        bytes.push_back(capdu.lc);
        bytes.insert(bytes.end(), capdu.commandData.begin(), capdu.commandData.end());
    }
    if (capdu.hasLe) {
        bytes.push_back(capdu.le);
    }
    return bytes;
}

std::string spacedCapduHex(const CapduFields& capdu) {
    const auto bytes = capduRawBytes(capdu);
    std::ostringstream oss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i > 0) {
            oss << ' ';
        }
        oss << upperHexByte(bytes[i]);
    }
    return oss.str();
}

void appendCapduFieldLine(
    std::ostringstream& out,
    const char* label,
    const std::string& hexValue,
    const std::string& meaning
) {
    out << label << " : " << hexValue;
    if (!meaning.empty()) {
        out << "    " << meaning;
    }
    out << "\n";
}

void appendCapduFieldLineAligned(
    std::ostringstream& out,
    const char* label,
    const std::string& hexValue,
    const std::string& meaning
) {
    out << std::left << std::setw(3) << label << " : " << std::left << std::setw(4) << hexValue;
    if (!meaning.empty()) {
        out << "  " << meaning;
    }
    out << "\n";
}

std::string formatCapduScriptFieldsBlock(const ApduParseResult& result) {
    std::ostringstream out;
    appendCapduFieldLineAligned(out, "CLA", upperHexByte(result.capdu.cla), "");
    {
        const std::string insMeaning = result.command ? result.command->name : "";
        appendCapduFieldLineAligned(out, "INS", upperHexByte(result.capdu.ins), insMeaning);
    }
    appendCapduFieldLineAligned(out, "P1", upperHexByte(result.capdu.p1), result.p1Meaning);
    appendCapduFieldLineAligned(out, "P2", upperHexByte(result.capdu.p2), result.p2Meaning);
    if (result.capdu.hasLc) {
        const std::string lcDec = std::to_string(static_cast<int>(result.capdu.lc)) + " bytes";
        appendCapduFieldLineAligned(out, "Lc", upperHexByte(result.capdu.lc), lcDec);
    }
    if (result.capdu.hasLe) {
        appendCapduFieldLineAligned(out, "Le", upperHexByte(result.capdu.le), "");
    }
    return out.str();
}

std::string formatCapduBlock(const ApduParseResult& result) {
    const std::string commandName = result.command ? result.command->name : "Unknown";
    std::ostringstream out;
    out << commandName << " (" << spacedCapduHex(result.capdu) << ")\n";
    appendCapduFieldLine(out, "CLA", upperHexByte(result.capdu.cla), "");
    {
        std::string insMeaning = result.command ? result.command->name : "";
        appendCapduFieldLine(out, "INS", upperHexByte(result.capdu.ins), insMeaning);
    }
    appendCapduFieldLine(out, "P1", upperHexByte(result.capdu.p1), result.p1Meaning);
    appendCapduFieldLine(out, "P2", upperHexByte(result.capdu.p2), result.p2Meaning);
    if (result.capdu.hasLc) {
        const std::string lcDec = std::to_string(static_cast<int>(result.capdu.lc)) + " bytes";
        appendCapduFieldLine(out, "Lc", upperHexByte(result.capdu.lc), lcDec);
    }
    if (result.capdu.hasLe) {
        appendCapduFieldLine(out, "Le", upperHexByte(result.capdu.le), "");
    }
    return out.str();
}

std::string htmlEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

const char* capduHtmlStyles() {
    return ".capdu-block{margin-bottom:16px;padding:12px 14px;background:#eef3ff;"
           "border:1px solid #ccd6f0;border-radius:4px;}\n"
           ".capdu-title{margin:0 0 10px;font-size:1.15em;}\n"
           ".capdu-raw{font-family:Consolas,monospace;font-weight:normal;"
           "font-size:0.92em;color:#333;}\n"
           ".capdu-fields{border-collapse:collapse;}\n"
           ".capdu-fields th{text-align:left;padding:3px 16px 3px 0;font-weight:600;"
           "white-space:nowrap;vertical-align:top;}\n"
           ".capdu-fields td{padding:3px 0;vertical-align:top;}\n"
           ".capdu-fields .field-hex{font-family:Consolas,monospace;}\n"
           ".capdu-fields .field-meaning{padding-left:12px;color:#444;}\n";
}

void appendCapduHtmlFieldRow(
    std::ostringstream& out,
    const char* label,
    const std::string& hexValue,
    const std::string& meaning
) {
    out << "<tr><th>" << htmlEscape(label) << " :</th>"
        << "<td class=\"field-hex\">" << htmlEscape(hexValue) << "</td>";
    if (!meaning.empty()) {
        out << "<td class=\"field-meaning\">" << htmlEscape(meaning) << "</td>";
    } else {
        out << "<td></td>";
    }
    out << "</tr>\n";
}

std::string formatCapduHtmlBlock(const ApduParseResult& result) {
    const std::string commandName = result.command ? result.command->name : "Unknown";
    std::ostringstream out;
    out << "<div class=\"capdu-block\">\n"
        << "<h2 class=\"capdu-title\">" << htmlEscape(commandName) << " <span class=\"capdu-raw\">("
        << htmlEscape(spacedCapduHex(result.capdu)) << ")</span></h2>\n"
        << "<table class=\"capdu-fields\">\n";
    appendCapduHtmlFieldRow(out, "CLA", upperHexByte(result.capdu.cla), "");
    appendCapduHtmlFieldRow(
        out,
        "INS",
        upperHexByte(result.capdu.ins),
        result.command ? result.command->name : "");
    appendCapduHtmlFieldRow(out, "P1", upperHexByte(result.capdu.p1), result.p1Meaning);
    appendCapduHtmlFieldRow(out, "P2", upperHexByte(result.capdu.p2), result.p2Meaning);
    if (result.capdu.hasLc) {
        appendCapduHtmlFieldRow(
            out,
            "Lc",
            upperHexByte(result.capdu.lc),
            std::to_string(static_cast<int>(result.capdu.lc)) + " bytes");
    }
    if (result.capdu.hasLe) {
        appendCapduHtmlFieldRow(out, "Le", upperHexByte(result.capdu.le), "");
    }
    out << "</table>\n</div>\n";
    return out.str();
}

std::string wrapCapduOnlyHtmlDocument(const std::string& capduBlockHtml) {
    std::ostringstream doc;
    doc << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
        << "<title>C-APDU Parse</title>\n<style>\n"
        << "body{font-family:Segoe UI,Arial,sans-serif;margin:16px;}\n"
        << capduHtmlStyles()
        << "</style>\n</head>\n<body>\n"
        << "<h1>C-APDU</h1>\n"
        << capduBlockHtml
        << "</body>\n</html>\n";
    return doc.str();
}

void injectCapduBlockIntoTlvHtml(std::string& html, const std::string& capduBlockHtml) {
    const std::string styleNeedle = "body{font-family:";
    const auto stylePos = html.find(styleNeedle);
    if (stylePos != std::string::npos) {
        html.insert(stylePos, capduHtmlStyles());
    }
    const std::string marker = "<h1>EMV TLV</h1>\n";
    const auto pos = html.find(marker);
    if (pos != std::string::npos) {
        html.insert(pos + marker.size(), capduBlockHtml);
    }
}

std::vector<TableTextRow> rapduTableRows(const ApduParseResult& result) {
    std::vector<TableTextRow> rows;

    TableTextRow swRow;
    swRow.kind = TableTextRow::Kind::Data;
    swRow.tagHex = "R-APDU";
    swRow.name = "Status word";
    swRow.length = "2";
    swRow.value = upperHexByte(static_cast<uint8_t>((result.statusWord >> 8) & 0xFF)) + " " +
                  upperHexByte(static_cast<uint8_t>(result.statusWord & 0xFF)) + " - " +
                  result.statusMeaning;
    rows.push_back(swRow);

    return rows;
}

} // namespace

std::string ApduParser::bytesToHexUpper(const std::vector<uint8_t>& data) {
    return bytesHex(data);
}

std::string ApduParser::formatCapduSendBlock(const ApduParseResult& result) {
    return formatCapduBlock(result);
}

std::string ApduParser::formatCapduScriptFields(const ApduParseResult& result) {
    return formatCapduScriptFieldsBlock(result);
}

std::string ApduParser::formatCapduHtmlSendBlock(const ApduParseResult& result) {
    return formatCapduHtmlBlock(result);
}

std::string ApduParser::formatTable(
    const ApduParseResult& result,
    TagRegistry* tagRegistry,
    const emvparse::FormatOptions& options
) {
    if (result.kind == ParsedInputKind::Capdu) {
        std::ostringstream out;
        out << formatCapduBlock(result);
        if (!result.tlvRoots.empty()) {
            out << "\n" << TLVFormatter::toTable(result.tlvRoots, tagRegistry, options);
        }
        return out.str();
    }

    if (result.kind == ParsedInputKind::Tlv) {
        return TLVFormatter::toTable(result.tlvRoots, tagRegistry, options);
    }

    std::vector<TableTextRow> rows = rapduTableRows(result);
    const auto& tlvSource =
        result.format1Expanded && !result.format1Children.empty() ? result.format1Children
                                                                  : result.tlvRoots;
    auto tlvRows = TLVFormatter::tableRowsFromRoots(tlvSource, tagRegistry, options);
    rows.insert(rows.end(), tlvRows.begin(), tlvRows.end());
    return TLVFormatter::renderTextTable(rows);
}

std::string ApduParser::formatHtml(const ApduParseResult& result, TagRegistry* tagRegistry) {
    if (result.kind == ParsedInputKind::Capdu) {
        const std::string capduBlock = formatCapduHtmlBlock(result);
        if (result.tlvRoots.empty()) {
            return wrapCapduOnlyHtmlDocument(capduBlock);
        }
        std::string html = TLVFormatter::toHtml(result.tlvRoots, tagRegistry);
        injectCapduBlockIntoTlvHtml(html, capduBlock);
        return html;
    }

    std::ostringstream prefix;
    if (result.kind == ParsedInputKind::Rapdu) {
        prefix << "<div class=\"apdu-summary\"><strong>Status</strong> "
               << byteHex(static_cast<uint8_t>((result.statusWord >> 8) & 0xFF))
               << byteHex(static_cast<uint8_t>(result.statusWord & 0xFF)) << " - "
               << htmlEscape(result.statusMeaning) << "</div>\n";
    }

    std::string html = TLVFormatter::toHtml(result.tlvRoots, tagRegistry);
    const std::string styleNeedle = "body{font-family:";
    const auto stylePos = html.find(styleNeedle);
    if (stylePos != std::string::npos && result.kind == ParsedInputKind::Rapdu) {
        html.insert(stylePos, ".apdu-summary{margin-bottom:12px;padding:8px;background:#eef3ff;border:1px solid #ccd6f0;}\n");
    }
    const std::string marker = "<h1>EMV TLV</h1>\n";
    const auto pos = html.find(marker);
    if (pos != std::string::npos && result.kind == ParsedInputKind::Rapdu) {
        html.insert(pos + marker.size(), prefix.str());
    }
    return html;
}
