#include "emvparse/transaction_script.h"
#include "tag_registry.h"
#include "tlv_formatter.h"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>

namespace {

std::string formatDolDataLines(
    const ApduParseResult& capdu,
    TagRegistry* registry,
    const emvparse::FormatOptions& options
) {
    std::ostringstream out;
    if (!capdu.dolDataValues.empty()) {
        for (const auto& entry : capdu.dolDataValues) {
            out << "  " << entry.dolEntry.tagHex << " "
                << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
                << static_cast<int>(entry.dolEntry.length) << std::dec;
            if (!entry.dolEntry.name.empty()) {
                out << "  " << entry.dolEntry.name;
            }
            if (!entry.valueHex.empty()) {
                out << "  " << entry.valueHex;
            }
            if (entry.missing) {
                out << "  (truncated)";
            }
            out << "\n";
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
    const auto& tlvSource =
        rapdu.format1Expanded && !rapdu.format1Children.empty() ? rapdu.format1Children
                                                                : rapdu.tlvRoots;
    if (tlvSource.empty()) {
        if (rapdu.tlvRoots.empty() && rapdu.statusWord != 0) {
            if (rapduWireHex.size() <= 4) {
                out << "  (status word only)\n";
            } else {
                out << "  (BER-TLV body could not be fully parsed; see raw R-APDU hex above)\n";
            }
        }
    } else {
        if (rapdu.format1Expanded) {
            out << "  Format 1 (tag 80 on wire); logical fields:\n";
        }
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
        item["capdu"] = nlohmann::json::parse(ApduParser::formatJson(step.capdu, registry));
        item["rapdu"] = nlohmann::json::parse(ApduParser::formatJson(step.rapdu, registry));
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
        out << ApduParser::formatCapduScriptFields(step.capdu);
        if (!step.capdu.tlvRoots.empty() || !step.capdu.dolDataValues.empty() ||
            !step.capdu.capdu.dataHex.empty()) {
            out << "Data:\n";
            out << formatDolDataLines(step.capdu, registry, options);
        }
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
    (void)options;
    std::ostringstream body;
    for (const auto& step : result.steps) {
        body << "<section class=\"script-step\">\n";
        body << "<h2>" << step.stepTitle << "</h2>\n";
        body << "<h3>Send</h3>\n";
        body << ApduParser::formatCapduHtmlSendBlock(step.capdu);
        if (!step.capdu.tlvRoots.empty()) {
            body << "<h4>Data</h4>\n";
            body << "<pre>" << TLVFormatter::toTreeText(step.capdu.tlvRoots, registry, options) << "</pre>\n";
        }
        body << "<h3>Response</h3>\n";
        const auto& tlvSource = step.rapdu.format1Expanded && !step.rapdu.format1Children.empty()
                                    ? step.rapdu.format1Children
                                    : step.rapdu.tlvRoots;
        if (!tlvSource.empty()) {
            body << TLVFormatter::toHtml(tlvSource, registry);
        }
        body << "</section>\n";
    }

    std::ostringstream doc;
    doc << "<!DOCTYPE html>\n<html lang=\"en\">\n<head><meta charset=\"utf-8\"><title>Transaction script</title>"
        << "<style>body{font-family:Segoe UI,Arial,sans-serif;margin:16px;}"
        << ".script-step{margin-bottom:32px;border-bottom:1px solid #ccc;padding-bottom:16px;}</style>"
        << "</head><body><h1>Transaction script</h1>\n"
        << body.str() << "</body></html>\n";
    return doc.str();
}
