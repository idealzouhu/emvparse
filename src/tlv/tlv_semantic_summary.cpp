#include "tlv_semantic_summary.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace {

std::string dolEntryLengthHex(int lengthByte) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(2)
        << (lengthByte & 0xFF);
    return oss.str();
}

std::vector<SemanticSummaryLine> buildDolEntrySummaryLines(
    const nlohmann::ordered_json& dolEntries
) {
    std::vector<SemanticSummaryLine> lines;
    if (!dolEntries.is_array()) {
        return lines;
    }

    size_t tagWidth = 2;
    for (const auto& entry : dolEntries) {
        if (entry.is_object() && entry.contains("tag") && entry["tag"].is_string()) {
            tagWidth = std::max(tagWidth, entry["tag"].get<std::string>().size());
        }
    }
    constexpr size_t kLenColWidth = 2;

    for (const auto& entry : dolEntries) {
        if (!entry.is_object()) {
            continue;
        }
        const std::string tag = entry.value("tag", std::string());
        if (tag.empty()) {
            continue;
        }
        std::ostringstream line;
        line << std::left << std::setfill(' ') << std::setw(static_cast<int>(tagWidth)) << tag << " ";
        if (entry.contains("length")) {
            line << std::setw(static_cast<int>(kLenColWidth))
                 << dolEntryLengthHex(entry["length"].get<int>());
        }
        if (entry.contains("name") && entry["name"].is_string()) {
            const std::string name = entry["name"].get<std::string>();
            if (!name.empty()) {
                line << " -> " << name;
            }
        }
        lines.push_back({line.str(), false});
    }
    return lines;
}


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

// EMV bit index: position 0 = b8 (MSB of byte 1).
std::string formatEmvBitRangeLabel(int position, int length) {
    const int bHigh = 8 - position;
    const int bLow = 8 - (position + length - 1);
    if (length <= 1) {
        return "b" + std::to_string(bHigh);
    }
    return "b" + std::to_string(bHigh) + "-b" + std::to_string(bLow);
}

bool hasResolvedEnumLabel(const nlohmann::ordered_json& semantic) {
    if (!semantic.contains("enum") || !semantic["enum"].is_string()) {
        return false;
    }
    const std::string label = semantic["enum"].get<std::string>();
    return !label.empty() && label != "UNKNOWN";
}

bool includeBitFieldInTree(const nlohmann::ordered_json& field, emvparse::SemanticSummary mode) {
    const int position = field.value("position", -1);
    const int length = field.value("length", 0);
    // CID (9F27): always show cryptogram type and advice lines in the tree.
    if (position == 0 && length == 2) {
        return true;
    }
    if (position == 4 && length == 1) {
        return true;
    }
    if (position == 5 && length == 3) {
        if (mode == emvparse::SemanticSummary::Full) {
            return true;
        }
        if (field.contains("meaning") && field["meaning"].is_string()) {
            const std::string meaning = field["meaning"].get<std::string>();
            if (meaning == "No information given" || meaning.find("RFU") != std::string::npos) {
                return false;
            }
        }
        return true;
    }
    return includeBitFieldInActive(field);
}

std::string formatTreeBitFieldLine(const nlohmann::ordered_json& field) {
    const int position = field.value("position", 0);
    const int length = field.value("length", 1);
    const std::string meaning = field.value("meaning", std::string());
    return formatEmvBitRangeLabel(position, length) + ": " + meaning;
}

} // namespace

emvparse::SemanticSummary effectiveSemanticSummaryMode(
    const std::string& tagType,
    emvparse::SemanticSummary requested
) {
    if (tagType == "BITFIELD") {
        return requested;
    }
    return emvparse::SemanticSummary::Full;
}

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

    if (semantic.contains("numeric") && semantic["numeric"].is_string() && !hasResolvedEnumLabel(semantic)) {
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
    if (semantic.contains("advice") && semantic["advice"].is_string()) {
        const std::string advice = semantic["advice"].get<std::string>();
        if (mode == emvparse::SemanticSummary::Full || advice != "No advice required") {
            lines.push_back({std::string("advice: ") + advice, false});
        }
    }
    if (semantic.contains("reason") && semantic["reason"].is_string()) {
        const std::string reason = semantic["reason"].get<std::string>();
        const bool neutral = reason == "No information given";
        const bool rfu = reason.find("RFU") != std::string::npos;
        if (mode == emvparse::SemanticSummary::Full || (!neutral && !rfu)) {
            lines.push_back({std::string("reason: ") + reason, false});
        }
    }

    if (semantic.contains("dolEntries") && semantic["dolEntries"].is_array()) {
        const auto dolLines = buildDolEntrySummaryLines(semantic["dolEntries"]);
        lines.insert(lines.end(), dolLines.begin(), dolLines.end());
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

std::vector<SemanticSummaryLine> buildTreeSemanticSummaryLines(
    const nlohmann::ordered_json& semantic,
    emvparse::SemanticSummary mode
) {
    std::vector<SemanticSummaryLine> lines;
    if (semantic.is_null() || !semantic.is_object()) {
        return lines;
    }

    if (semantic.contains("bitFields") && semantic["bitFields"].is_array()) {
        for (const auto& field : semantic["bitFields"]) {
            if (!includeBitFieldInTree(field, mode)) {
                continue;
            }
            if (!field.contains("meaning") || !field["meaning"].is_string()) {
                continue;
            }
            const std::string meaning = field["meaning"].get<std::string>();
            if (meaning.empty()) {
                continue;
            }
            SemanticSummaryLine line;
            line.text = formatTreeBitFieldLine(field);
            if (field.contains("rawValue") && field["rawValue"].is_number_unsigned()) {
                line.inactive = field["rawValue"].get<uint64_t>() == 0;
            }
            lines.push_back(line);
        }
    }

    if (semantic.contains("numeric") && semantic["numeric"].is_string() && !hasResolvedEnumLabel(semantic)) {
        lines.push_back({std::string("numeric: ") + semantic["numeric"].get<std::string>(), false});
    }
    if (semantic.contains("date") && semantic["date"].is_string()) {
        lines.push_back({semantic["date"].get<std::string>(), false});
    }
    if (semantic.contains("time") && semantic["time"].is_string()) {
        lines.push_back({semantic["time"].get<std::string>(), false});
    }
    if (semantic.contains("enum") && semantic["enum"].is_string()) {
        lines.push_back({semantic["enum"].get<std::string>(), false});
    }
    if (semantic.contains("advice") && semantic["advice"].is_string()) {
        lines.push_back({semantic["advice"].get<std::string>(), false});
    }
    if (semantic.contains("reason") && semantic["reason"].is_string()) {
        const std::string reason = semantic["reason"].get<std::string>();
        const bool neutral = reason == "No information given";
        const bool rfu = reason.find("RFU") != std::string::npos;
        if (mode == emvparse::SemanticSummary::Full || (!neutral && !rfu)) {
            lines.push_back({reason, false});
        }
    }

    if (semantic.contains("dolEntries") && semantic["dolEntries"].is_array()) {
        const auto dolLines = buildDolEntrySummaryLines(semantic["dolEntries"]);
        lines.insert(lines.end(), dolLines.begin(), dolLines.end());
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
