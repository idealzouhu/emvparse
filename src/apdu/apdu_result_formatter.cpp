#include "apdu_result_formatter.h"
#include "tlv_formatter.h"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>

namespace {

std::string upperHexByte(uint8_t b) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return oss.str();
}

std::string byteHex(uint8_t b) {
    return upperHexByte(b);
}

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

// ISO 7816-4 case 2: … P2 Le; case 4: … Lc Data Le — Le follows command data on the wire.
bool capduLeBeforeCommandData(const CapduFields& capdu) {
    return capdu.hasLe && !capdu.hasLc;
}

std::string formatCapduCase4LeLine(const ApduParseResult& result, bool aligned) {
    if (!result.capdu.hasLe || !result.capdu.hasLc) {
        return "";
    }
    std::ostringstream out;
    if (aligned) {
        appendCapduFieldLineAligned(out, "Le", upperHexByte(result.capdu.le), "");
    } else {
        appendCapduFieldLine(out, "Le", upperHexByte(result.capdu.le), "");
    }
    return out.str();
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
    if (capduLeBeforeCommandData(result.capdu)) {
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
    if (capduLeBeforeCommandData(result.capdu)) {
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

const char* capduHtmlStylesText() {
    return ".capdu-block{margin-bottom:16px;padding:12px 14px;background:#eef3ff;"
           "border:1px solid #ccd6f0;border-radius:4px;}\n"
           ".capdu-title{margin:0 0 10px;font-size:1.15em;}\n"
           ".capdu-raw{font-family:Consolas,monospace;font-weight:normal;"
           "font-size:0.92em;color:#333;}\n"
           ".capdu-fields{border-collapse:collapse;}\n"
           ".capdu-fields th,.capdu-fields td{border:1px solid #333;padding:4px 8px;"
           "vertical-align:top;}\n"
           ".capdu-fields th{text-align:left;font-weight:600;white-space:nowrap;"
           "background:#eee;}\n"
           ".capdu-fields .field-hex{font-family:Consolas,monospace;}\n"
           ".capdu-fields .field-meaning{padding-left:12px;color:#444;}\n"
           ".capdu-fields .tlv-wrap{margin:4px 0 0;}\n"
           ".apdu-summary{margin-bottom:12px;padding:8px;background:#eef3ff;"
           "border:1px solid #ccd6f0;}\n";
}

void appendCapduHtmlFieldRow(
    std::ostringstream& out,
    const char* label,
    const std::string& hexValue,
    const std::string& meaning
) {
    out << "<tr><th>" << htmlEscape(label) << "</th>"
        << "<td class=\"field-hex\">" << htmlEscape(hexValue) << "</td>";
    if (!meaning.empty()) {
        out << "<td class=\"field-meaning\">" << htmlEscape(meaning) << "</td>";
    } else {
        out << "<td></td>";
    }
    out << "</tr>\n";
}

std::string formatCapduHtmlFieldsTableBody(const ApduParseResult& result) {
    std::ostringstream out;
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
    if (capduLeBeforeCommandData(result.capdu)) {
        appendCapduHtmlFieldRow(out, "Le", upperHexByte(result.capdu.le), "");
    }
    if (result.capdu.hasLe && result.capdu.hasLc) {
        appendCapduHtmlFieldRow(out, "Le", upperHexByte(result.capdu.le), "");
    }
    return out.str();
}

std::string formatCapduHtmlCase4LeBlock(const ApduParseResult& result) {
    if (!result.capdu.hasLe || !result.capdu.hasLc) {
        return "";
    }
    return "";
}

std::string formatCapduHtmlBlock(const ApduParseResult& result) {
    const std::string commandName = result.command ? result.command->name : "Unknown";
    std::ostringstream out;
    out << "<div class=\"capdu-block\">\n"
        << "<h2 class=\"capdu-title\">" << htmlEscape(commandName) << " <span class=\"capdu-raw\">("
        << htmlEscape(spacedCapduHex(result.capdu)) << ")</span></h2>\n"
        << "<table class=\"capdu-fields\">\n"
        << formatCapduHtmlFieldsTableBody(result)
        << "</table>\n</div>\n";
    return out.str();
}

std::string wrapCapduOnlyHtmlDocument(const std::string& capduBlockHtml) {
    std::ostringstream doc;
    doc << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
        << "<title>C-APDU Parse</title>\n<style>\n"
        << "body{font-family:Segoe UI,Arial,sans-serif;margin:16px;}\n"
        << capduHtmlStylesText()
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
        html.insert(stylePos, capduHtmlStylesText());
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

std::string ApduResultFormatter::formatCapduSendBlock(const ApduParseResult& result) {
    return formatCapduBlock(result);
}

std::string ApduResultFormatter::formatCapduScriptFields(const ApduParseResult& result) {
    return formatCapduScriptFieldsBlock(result);
}

std::string ApduResultFormatter::formatCapduLeAfterCommandData(const ApduParseResult& result) {
    return formatCapduCase4LeLine(result, true);
}

std::string ApduResultFormatter::formatCapduHtmlLeAfterCommandData(const ApduParseResult& result) {
    return formatCapduHtmlCase4LeBlock(result);
}

std::string ApduResultFormatter::formatCapduHtmlFieldsTable(const ApduParseResult& result) {
    std::ostringstream out;
    out << "<table class=\"capdu-fields\">\n" << formatCapduHtmlFieldsTableBody(result) << "</table>\n";
    return out.str();
}

const char* ApduResultFormatter::capduHtmlStyles() {
    return capduHtmlStylesText();
}

std::string ApduResultFormatter::formatCapduHtmlSendBlock(const ApduParseResult& result) {
    return formatCapduHtmlBlock(result);
}

std::string ApduResultFormatter::formatJson(const ApduParseResult& result, TagRegistry* tagRegistry) {
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

std::string ApduResultFormatter::formatTable(
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
        out << formatCapduCase4LeLine(result, false);
        return out.str();
    }

    if (result.kind == ParsedInputKind::Tlv) {
        return TLVFormatter::toTable(result.tlvRoots, tagRegistry, options);
    }

    std::vector<TableTextRow> rows = rapduTableRows(result);
    const auto tlvSource = ApduParser::displayBodyRoots(result);
    auto tlvRows = TLVFormatter::tableRowsFromRoots(tlvSource, tagRegistry, options);
    rows.insert(rows.end(), tlvRows.begin(), tlvRows.end());
    return TLVFormatter::renderTextTable(rows);
}

std::string ApduResultFormatter::formatHtml(
    const ApduParseResult& result,
    TagRegistry* tagRegistry,
    const emvparse::FormatOptions& options
) {
    if (result.kind == ParsedInputKind::Capdu) {
        const std::string capduBlock = formatCapduHtmlBlock(result);
        if (result.tlvRoots.empty()) {
            return wrapCapduOnlyHtmlDocument(capduBlock);
        }
        std::string html = TLVFormatter::toHtml(result.tlvRoots, tagRegistry, options);
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

    const auto bodyRoots =
        result.kind == ParsedInputKind::Rapdu ? ApduParser::displayBodyRoots(result) : result.tlvRoots;
    std::string html = TLVFormatter::toHtml(bodyRoots, tagRegistry, options);
    const std::string styleNeedle = "body{font-family:";
    const auto stylePos = html.find(styleNeedle);
    if (stylePos != std::string::npos && result.kind == ParsedInputKind::Rapdu) {
        html.insert(stylePos, capduHtmlStylesText());
    }
    const std::string marker = "<h1>EMV TLV</h1>\n";
    const auto pos = html.find(marker);
    if (pos != std::string::npos && result.kind == ParsedInputKind::Rapdu) {
        html.insert(pos + marker.size(), prefix.str());
    }
    return html;
}
