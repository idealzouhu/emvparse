#include "cvm_list_parser.h"
#include <iomanip>
#include <sstream>

namespace {

uint32_t readUint32Be(const std::vector<uint8_t>& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        return 0;
    }
    return (static_cast<uint32_t>(bytes[offset]) << 24) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
           static_cast<uint32_t>(bytes[offset + 3]);
}

std::string cvmTypeMeaning(int typeCode) {
    switch (typeCode) {
    case 0x00:
        return "Fail cardholder verification if this CVM is unsuccessful";
    case 0x01:
        return "Plaintext PIN verification performed by ICC";
    case 0x02:
        return "Enciphered PIN verified online";
    case 0x03:
        return "Plaintext PIN verification performed by ICC and signature (paper)";
    case 0x04:
        return "Enciphered PIN verification performed by ICC";
    case 0x05:
        return "Enciphered PIN verification performed by ICC and signature (paper)";
    case 0x1E:
        return "Signature (paper)";
    case 0x1F:
        return "No CVM required";
    default:
        if (typeCode >= 0x06 && typeCode <= 0x1D) {
            return "RFU (CVM type)";
        }
        return "Unknown CVM type";
    }
}

std::string formatHexByte(uint8_t byte) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
    return oss.str();
}

} // namespace

std::string emvCvmCodeMeaning(uint8_t cvmCode, bool* applyNextOnFailOut) {
    // EMV bit numbering: b6 = apply succeeding rule on failure (mask 0x20).
    const bool applyNext = (cvmCode & 0x20) != 0;
    if (applyNextOnFailOut) {
        *applyNextOnFailOut = applyNext;
    }
    const int typeCode = cvmCode & 0x1F;
    return cvmTypeMeaning(typeCode);
}

std::string emvCvmConditionMeaning(uint8_t conditionCode) {
    switch (conditionCode) {
    case 0x00:
        return "Always";
    case 0x01:
        return "If unattended cash";
    case 0x02:
        return "If not unattended cash and not manual cash and not purchase with cashback";
    case 0x03:
        return "If terminal supports the CVM";
    case 0x04:
        return "If manual cash";
    case 0x05:
        return "If purchase with cashback";
    case 0x06:
        return "If transaction is in the application currency and is under X value";
    case 0x07:
        return "If transaction is in the application currency and is over X value";
    case 0x08:
        return "If transaction is in the application currency and is under Y value";
    case 0x09:
        return "If transaction is in the application currency and is over Y value";
    default:
        if (conditionCode >= 0x0A && conditionCode <= 0x7F) {
            return "RFU (condition)";
        }
        if (conditionCode >= 0x80) {
            return "Payment system-specific condition";
        }
        return "Unknown condition";
    }
}

EmvCvmListParseResult parseEmvCvmList(const std::vector<uint8_t>& bytes) {
    EmvCvmListParseResult result;
    if (bytes.size() < 10) {
        result.error = "CVM List length must be at least 10 bytes (Amount X/Y + one rule)";
        return result;
    }
    if ((bytes.size() - 8) % 2 != 0) {
        result.error = "CVM List rule section must be an even number of bytes";
        return result;
    }

    result.amountX = readUint32Be(bytes, 0);
    result.amountY = readUint32Be(bytes, 4);

    int ruleIndex = 0;
    for (std::size_t offset = 8; offset + 1 < bytes.size(); offset += 2) {
        EmvCvmRuleEntry entry;
        entry.index = ++ruleIndex;
        entry.cvmCode = bytes[offset];
        entry.conditionCode = bytes[offset + 1];
        entry.cvmMeaning = emvCvmCodeMeaning(entry.cvmCode, &entry.applyNextOnFail);
        entry.conditionMeaning = emvCvmConditionMeaning(entry.conditionCode);

        std::ostringstream line;
        line << "#" << entry.index << "  " << formatHexByte(entry.cvmCode) << "/"
             << formatHexByte(entry.conditionCode) << "  " << entry.cvmMeaning << "; "
             << entry.conditionMeaning;
        if (entry.applyNextOnFail) {
            line << " (apply next rule on failure)";
        }
        entry.line = line.str();
        result.rules.push_back(entry);
    }

    return result;
}
