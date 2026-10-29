#include "transaction_script.h"
#include "apdu_result_formatter.h"
#include "tag_registry.h"
#include "tlv_formatter.h"
#include "hex_util.h"
#include "value_semantic_parser.h"
#include "tlv_semantic_summary.h"
#include <emvparse/tlv.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace {

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

std::string spacedHexUpper(const std::string& hex) {
    std::string out;
    for (size_t i = 0; i < hex.size(); i += 2) {
        if (i > 0) {
            out += ' ';
        }
        if (i + 1 < hex.size()) {
            out += hex[i];
            out += hex[i + 1];
        } else {
            out += hex[i];
        }
    }
    return out;
}

std::string byteHexUpper(uint8_t b) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    return oss.str();
}

void appendCapduHtmlFieldRow(
    std::ostringstream& out,
    const char* label,
    const std::string& hexOrValue,
    const std::string& meaning
) {
    out << "<tr><th>" << htmlEscape(label) << "</th>"
        << "<td class=\"field-hex\">" << htmlEscape(hexOrValue) << "</td>";
    if (!meaning.empty()) {
        out << "<td class=\"field-meaning\">" << htmlEscape(meaning) << "</td>";
    } else {
        out << "<td></td>";
    }
    out << "</tr>\n";
}

bool appendCapduDataRows(
    std::ostringstream& out,
    const ApduParseResult& capdu,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    (void)options;
    if (!capdu.dolDataValues.empty()) {
        for (const auto& entry : capdu.dolDataValues) {
            std::string valueCell = spacedHexUpper(entry.valueHex);
            if (entry.missing) {
                valueCell += " (truncated)";
            }
            std::string meaning = entry.dolEntry.name;
            if (!meaning.empty() && entry.dolEntry.length > 0) {
                meaning = std::to_string(static_cast<int>(entry.dolEntry.length)) + " bytes — " + meaning;
            } else if (entry.dolEntry.length > 0) {
                meaning = std::to_string(static_cast<int>(entry.dolEntry.length)) + " bytes";
            }
            appendCapduHtmlFieldRow(out, entry.dolEntry.tagHex.c_str(), valueCell, meaning);
        }
        return true;
    }
    if (!capdu.tlvRoots.empty()) {
        out << "<tr><th>Data</th><td colspan=\"2\">"
            << TLVFormatter::toHtmlFragment(capdu.tlvRoots, registry, options) << "</td></tr>\n";
        return true;
    }
    if (!capdu.capdu.dataHex.empty()) {
        std::string asciiHint;
        if (capdu.capdu.ins == 0xA4 && capdu.capdu.p1 == 0x04) {
            const auto& raw = capdu.capdu.commandData;
            bool printable = !raw.empty();
            for (uint8_t b : raw) {
                if (b < 0x20 || b > 0x7E) {
                    printable = false;
                    break;
                }
            }
            if (printable) {
                asciiHint = std::string(raw.begin(), raw.end());
            }
        }
        appendCapduHtmlFieldRow(out, "Data", spacedHexUpper(capdu.capdu.dataHex), asciiHint);
        return true;
    }
    return false;
}

std::string formatCapduHtmlSendTable(
    const ApduParseResult& capdu,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    std::string table = ApduResultFormatter::formatCapduHtmlFieldsTable(capdu);
    const auto closePos = table.rfind("</table>");
    if (closePos == std::string::npos) {
        return table;
    }
    std::ostringstream out;
    out << table.substr(0, closePos);
    appendCapduDataRows(out, capdu, registry, options);
    out << "</table>\n";
    return out.str();
}

std::string formatRapduStatusHtml(const ApduParseResult& rapdu) {
    const uint8_t sw1 = static_cast<uint8_t>((rapdu.statusWord >> 8) & 0xFF);
    const uint8_t sw2 = static_cast<uint8_t>(rapdu.statusWord & 0xFF);
    std::ostringstream out;
    out << "<div class=\"apdu-summary\"><strong>Status</strong> "
        << byteHexUpper(sw1) << byteHexUpper(sw2) << " - "
        << htmlEscape(rapdu.statusMeaning) << "</div>\n";
    return out.str();
}

std::string dolEntryLengthHex(uint8_t lengthByte) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
        << static_cast<unsigned>(lengthByte);
    return oss.str();
}

size_t dolDataNameColumnStart(size_t tagWidth) {
    constexpr size_t kRowIndent = 2;
    constexpr size_t kLenColWidth = 2;
    constexpr size_t kNameArrow = 4; // " -> "
    return kRowIndent + tagWidth + 1 + kLenColWidth + kNameArrow;
}

std::vector<std::string> dolPayloadValueSemanticLines(
    TagRegistry* registry,
    const std::string& tagHex,
    const std::vector<uint8_t>& value,
    emvparse::SemanticSummary mode
) {
    std::vector<std::string> result;
    if (!registry || value.empty()) {
        return result;
    }
    const auto tagInfo = registry->getTagInfo(tagHex);
    if (!tagInfo) {
        return result;
    }
    const std::vector<uint8_t> tagBytes = parseHexString(tagHex);
    if (tagBytes.empty()) {
        return result;
    }
    const auto tlv = std::make_shared<TLV>(tagBytes, value);
    const auto semantic = ValueSemanticParser::parseValueSemantic(tlv, tagInfo);
    const auto lines = buildTreeSemanticSummaryLines(semantic, mode);
    for (const auto& line : lines) {
        if (line.inactive || line.text.empty() || line.text == "UNKNOWN") {
            continue;
        }
        result.push_back(line.text);
    }
    return result;
}

std::string formatDolDataLines(
    const ApduParseResult& capdu,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    std::ostringstream out;
    if (!capdu.dolDataValues.empty()) {
        size_t tagWidth = 2;
        for (const auto& entry : capdu.dolDataValues) {
            tagWidth = std::max(tagWidth, entry.dolEntry.tagHex.size());
        }
        constexpr size_t kLenColWidth = 2; // DOL length byte: one octet as 2 hex digits
        const size_t nameColumnStart = dolDataNameColumnStart(tagWidth);

        for (const auto& entry : capdu.dolDataValues) {
            const std::string lenHex = dolEntryLengthHex(entry.dolEntry.length);
            out << "  " << std::left << std::setfill(' ')
                << std::setw(static_cast<int>(tagWidth)) << entry.dolEntry.tagHex << " "
                << std::setw(static_cast<int>(kLenColWidth)) << lenHex;
            if (!entry.dolEntry.name.empty()) {
                out << " -> " << entry.dolEntry.name;
            }
            if (!entry.valueHex.empty()) {
                out << "  " << entry.valueHex;
            }
            if (entry.missing) {
                out << "  (truncated)";
            }
            out << "\n";

            if (!entry.value.empty()) {
                const auto semanticLines = dolPayloadValueSemanticLines(
                    registry, entry.dolEntry.tagHex, entry.value, options.semantic);
                for (const auto& semLine : semanticLines) {
                    out << std::string(nameColumnStart, ' ') << semLine << "\n";
                }
            }
        }
        return out.str();
    }
    if (!capdu.tlvRoots.empty()) {
        out << TLVFormatter::toTreeText(capdu.tlvRoots, registry, options);
    } else if (!capdu.capdu.dataHex.empty()) {
        out << "  " << capdu.capdu.dataHex;
        if (capdu.capdu.ins == 0xA4 && capdu.capdu.p1 == 0x04) {
            const auto& raw = capdu.capdu.commandData;
            bool printable = !raw.empty();
            for (uint8_t b : raw) {
                if (b < 0x20 || b > 0x7E) {
                    printable = false;
                    break;
                }
            }
            if (printable) {
                out << "  (\"" << std::string(raw.begin(), raw.end()) << "\")";
            }
        }
        out << "\n";
    }
    return out.str();
}

std::string formatResponseBlock(
    const std::string& rapduWireHex,
    const ApduParseResult& rapdu,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    std::ostringstream out;
    const auto tlvSource = ApduParser::displayBodyRoots(rapdu);
    if (tlvSource.empty()) {
        if (rapdu.tlvRoots.empty() && rapdu.statusWord != 0) {
            if (rapduWireHex.size() <= 4) {
                out << "  (status word only)\n";
            } else {
                out << "  (BER-TLV body could not be fully parsed; see raw R-APDU hex above)\n";
            }
        }
    } else {
        out << TLVFormatter::toTreeText(tlvSource, registry, options);
    }

    const uint8_t sw1 = static_cast<uint8_t>((rapdu.statusWord >> 8) & 0xFF);
    const uint8_t sw2 = static_cast<uint8_t>(rapdu.statusWord & 0xFF);
    out << "SW: " << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
        << static_cast<int>(sw1) << std::setw(2) << static_cast<int>(sw2) << std::dec
        << " - " << rapdu.statusMeaning << "\n";
    return out.str();
}

} // namespace

std::string TransactionScriptFormatter::formatJson(
    const TransactionScriptParseResult& result,
    TagRegistry* registry
) {
    nlohmann::ordered_json root;
    if (!result.script.title.empty()) {
        root["title"] = result.script.title;
    }
    nlohmann::ordered_json steps = nlohmann::ordered_json::array();
    for (const auto& step : result.steps) {
        nlohmann::ordered_json item;
        item["index"] = step.index;
        item["stepTitle"] = step.stepTitle;
        item["capdu"] = nlohmann::json::parse(ApduResultFormatter::formatJson(step.capdu, registry));
        item["rapdu"] = nlohmann::json::parse(ApduResultFormatter::formatJson(step.rapdu, registry));
        steps.push_back(item);
    }
    root["steps"] = steps;
    return root.dump(2);
}

std::string TransactionScriptFormatter::formatTable(
    const TransactionScriptParseResult& result,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    std::ostringstream out;
    bool first = true;
    for (const auto& step : result.steps) {
        if (!first) {
            out << "----------------------------------------\n\n";
        }
        first = false;
        out << "Command: " << step.stepTitle << "\n\n";
        out << "Send: " << step.capduWireHex << "\n----\n";
        out << ApduResultFormatter::formatCapduScriptFields(step.capdu);
        if (!step.capdu.tlvRoots.empty() || !step.capdu.dolDataValues.empty() ||
            !step.capdu.capdu.dataHex.empty()) {
            out << "Data:\n";
            out << formatDolDataLines(step.capdu, registry, options);
        }
        out << ApduResultFormatter::formatCapduLeAfterCommandData(step.capdu);
        out << "\nResponse: " << step.rapduWireHex << "\n----\n";
        out << formatResponseBlock(step.rapduWireHex, step.rapdu, registry, options);
        out << "\n";
    }
    return out.str();
}

std::string TransactionScriptFormatter::formatHtml(
    const TransactionScriptParseResult& result,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    std::ostringstream body;
    for (const auto& step : result.steps) {
        const std::string commandHeading =
            step.capdu.command ? step.capdu.command->name : "C-APDU";
        body << "<section class=\"script-step\">\n";
        body << "<h2>" << htmlEscape(commandHeading) << "</h2>\n";
        body << "<h3>Send</h3>\n";
        body << "<p class=\"send-summary\">" << htmlEscape(step.stepTitle) << "</p>\n";
        body << formatCapduHtmlSendTable(step.capdu, registry, options);
        body << "<h3>Response</h3>\n";
        const auto tlvSource = ApduParser::displayBodyRoots(step.rapdu);
        if (!tlvSource.empty()) {
            body << TLVFormatter::toHtmlFragment(tlvSource, registry, options);
        } else if (step.rapdu.tlvRoots.empty() && step.rapdu.statusWord != 0) {
            if (step.rapduWireHex.size() <= 4) {
                body << "<p class=\"response-note\">(status word only)</p>\n";
            } else {
                body << "<p class=\"response-note\">(BER-TLV body could not be fully parsed)</p>\n";
            }
        }
        body << formatRapduStatusHtml(step.rapdu);
        body << "</section>\n";
    }

    const std::string title = result.script.title.empty() ? "Transaction script" : result.script.title;
    std::ostringstream doc;
    doc << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>"
        << htmlEscape(title) << "</title>\n<style>\n"
        << "body{font-family:Segoe UI,Arial,sans-serif;margin:16px;}\n"
        << ".script-step{margin-bottom:32px;border-bottom:1px solid #ccc;padding-bottom:16px;}\n"
        << ".send-summary{margin:8px 0;font-size:1.02em;color:#222;}\n"
        << ".response-note{margin:8px 0;color:#555;font-style:italic;}\n"
        << ApduResultFormatter::capduHtmlStyles()
        << TLVFormatter::htmlStyles()
        << "</style>\n<script>\n"
        << TLVFormatter::htmlToggleScript()
        << "</script>\n</head>\n<body>\n<h1>" << htmlEscape(title) << "</h1>\n"
        << body.str() << "</body></html>\n";
    return doc.str();
}
