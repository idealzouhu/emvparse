#include "tlv_semantic_summary.h"
#include <cctype>

namespace {

bool isRfuMeaning(const std::string& meaning) {
    if (meaning.empty()) {
        return false;
    }
    if (meaning.size() >= 3 && meaning.compare(0, 3, "RFU") == 0) {
        return true;
    }
    return meaning.find("RFU") != std::string::npos;
}

bool includeBitFieldInActive(const nlohmann::ordered_json& field) {
    if (!field.contains("rawValue") || !field["rawValue"].is_number_unsigned()) {
        if (field.contains("value") && field["value"].is_string()) {
            const std::string v = field["value"].get<std::string>();
            if (v == "1" || v == "true") {
                return true;
            }
        }
        return false;
    }
    const uint64_t raw = field["rawValue"].get<uint64_t>();
    if (raw != 0) {
        return true;
    }
    if (field.contains("meaning") && field["meaning"].is_string()) {
        const std::string meaning = field["meaning"].get<std::string>();
        if (isRfuMeaning(meaning)) {
            return false;
        }
    }
    return false;
}

std::string formatBitFieldLine(const nlohmann::ordered_json& field) {
    int position = field.value("position", 0);
    const int byteNum = position / 8 + 1;
    const int bitNum = 8 - (position % 8);
    std::string valueStr = field.value("value", std::string());
    std::string meaning = field.value("meaning", std::string());
    return "byte " + std::to_string(byteNum) + " bit " + std::to_string(bitNum) + " = " + valueStr +
           "  " + meaning;
}

} // namespace

std::vector<SemanticSummaryLine> buildSemanticSummaryLines(
    const nlohmann::ordered_json& semantic,
    emvparse::SemanticSummary mode
) {
    std::vector<SemanticSummaryLine> lines;
    if (semantic.is_null() || !semantic.is_object()) {
        return lines;
    }

    if (semantic.contains("bitFields") && semantic["bitFields"].is_array()) {
        for (const auto& field : semantic["bitFields"]) {
            if (mode == emvparse::SemanticSummary::Active && !includeBitFieldInActive(field)) {
                continue;
            }
            SemanticSummaryLine line;
            line.text = formatBitFieldLine(field);
            if (field.contains("rawValue") && field["rawValue"].is_number_unsigned()) {
                line.inactive = field["rawValue"].get<uint64_t>() == 0;
            }
            lines.push_back(line);
        }
    }

    if (semantic.contains("numeric") && semantic["numeric"].is_string()) {
        lines.push_back({std::string("numeric: ") + semantic["numeric"].get<std::string>(), false});
    }
    if (semantic.contains("text") && semantic["text"].is_string()) {
        lines.push_back({std::string("text: ") + semantic["text"].get<std::string>(), false});
    }
    if (semantic.contains("date") && semantic["date"].is_string()) {
        lines.push_back({std::string("date: ") + semantic["date"].get<std::string>(), false});
    }
    if (semantic.contains("enum") && semantic["enum"].is_string()) {
        std::string line = "enum: " + semantic["enum"].get<std::string>();
        if (semantic.contains("code") && semantic["code"].is_string()) {
            line += " (" + semantic["code"].get<std::string>() + ")";
        }
        lines.push_back({line, false});
    }

    if (semantic.contains("entries") && semantic["entries"].is_array()) {
        for (const auto& entry : semantic["entries"]) {
            if (!entry.is_object()) {
                continue;
            }
            SemanticSummaryLine line;
            line.text = entry.value("line", std::string());
            if (line.text.empty()) {
                const int sfi = entry.value("sfi", 0);
                const int first = entry.value("firstRecord", 0);
                const int last = entry.value("lastRecord", 0);
                const int offline = entry.value("offlineRecords", 0);
                line.text = "SFI " + std::to_string(sfi) + ": records " + std::to_string(first) + "-" +
                            std::to_string(last) + ", offline auth records " + std::to_string(offline);
            }
            lines.push_back(line);
        }
        if (semantic.contains("summary") && semantic["summary"].is_string()) {
            lines.push_back({semantic["summary"].get<std::string>(), false});
        }
    }

    return lines;
}
