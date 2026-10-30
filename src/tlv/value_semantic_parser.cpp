#include "value_semantic_parser.h"
#include "tag_registry.h"
#include "dol_parser.h"
#include "cvm_list_parser.h"
#include "iso_numeric_lookup.h"
#include "tlv.h"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace {

int emvBcdByte(uint8_t byte) {
    return static_cast<int>(((byte >> 4) & 0x0F) * 10 + (byte & 0x0F));
}

std::string hexByteUpper(uint8_t byte) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(byte);
    return oss.str();
}

void enrichNumericWithIsoOrEnumMap(const TagInfo& tagInfo, nlohmann::ordered_json& result) {
    if (!result.contains("numeric") || !result["numeric"].is_string()) {
        return;
    }
    const std::string digits = result["numeric"].get<std::string>();
    std::string label;
    if (!tagInfo.enumRef.empty()) {
        label = emvparse::lookupIsoByEnumRef(tagInfo.enumRef, digits);
    }
    if (label.empty() && !tagInfo.enumMap.empty()) {
        const std::string key = emvparse::isoNumericLookupKey(digits);
        auto it = tagInfo.enumMap.find(key);
        if (it == tagInfo.enumMap.end()) {
            it = tagInfo.enumMap.find(digits);
        }
        if (it != tagInfo.enumMap.end()) {
            label = it->second;
        }
    }
    if (!label.empty()) {
        result["enum"] = label;
    }
}

nlohmann::ordered_json parseTimeValueBcd(const std::vector<uint8_t>& value) {
    nlohmann::ordered_json result;
    if (value.size() != 3) {
        return result;
    }
    const int hour = emvBcdByte(value[0]);
    const int minute = emvBcdByte(value[1]);
    const int second = emvBcdByte(value[2]);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hour << ":" << std::setw(2) << minute << ":"
        << std::setw(2) << second;
    result["time"] = oss.str();
    return result;
}

} // namespace

nlohmann::ordered_json ValueSemanticParser::parseValueSemantic(
    const std::shared_ptr<TLV>& tlv,
    const std::shared_ptr<TagInfo>& tagInfo
) {
    nlohmann::ordered_json result;
    const auto& value = tlv->getValue();
    
    // No TagInfo: return raw hex only.
    if (!tagInfo || value.empty()) {
        result["raw"] = tlv->valueHex();
        return result;
    }
    
    if (tagInfo->tag == "94" && value.size() >= 4 && value.size() % 4 == 0) {
        result = parseAflValue(value);
        result["raw"] = tlv->valueHex();
        return result;
    }

    // EMV Book 3 Table 15 — Cryptogram Information Data (CID).
    if (tagInfo->tag == "9F27" && value.size() == 1) {
        const uint8_t cid = value[0];
        static const char* const kCryptogramType[] = {
            "AAC (Application Authentication Cryptogram - transaction declined)",
            "TC (Transaction Certificate - approved offline)",
            "ARQC (Authorisation Request Cryptogram - online authorisation requested)",
            "RFU (reserved cryptogram type)"};
        static const char* const kReason[] = {
            "No information given",
            "Service not allowed",
            "PIN Try Limit exceeded",
            "Issuer authentication failed",
            "RFU",
            "RFU",
            "RFU",
            "RFU"};
        nlohmann::ordered_json bitFieldArray = nlohmann::ordered_json::array();
        auto appendField = [&](int position, int length, const std::string& meaning, uint64_t rawValue) {
            nlohmann::ordered_json field;
            field["position"] = position;
            field["length"] = length;
            field["meaning"] = meaning;
            field["rawValue"] = rawValue;
            bitFieldArray.push_back(field);
        };
        const int cryptoType = (cid >> 6) & 0x03;
        appendField(0, 2, kCryptogramType[cryptoType], static_cast<uint64_t>(cryptoType));
        const int paymentBits = (cid >> 4) & 0x03;
        if (paymentBits != 0) {
            appendField(2, 2, "Payment system-specific cryptogram", static_cast<uint64_t>(paymentBits));
        }
        appendField(
            4,
            1,
            (cid & 0x10) ? "Advice required" : "No advice required",
            static_cast<uint64_t>((cid >> 4) & 0x01));
        appendField(5, 3, kReason[cid & 0x07], static_cast<uint64_t>(cid & 0x07));
        result["bitFields"] = bitFieldArray;
        result["raw"] = tlv->valueHex();
        return result;
    }

    if (tagInfo->tag == "9F21" && tagInfo->format == "n" && value.size() == 3) {
        result = parseTimeValueBcd(value);
        result["raw"] = tlv->valueHex();
        return result;
    }

    // EMV Book 3 — CVM List (8E): 8-byte Amount X/Y + 2n-byte rule pairs.
    if (tagInfo->tag == "8E" && value.size() >= 10 && (value.size() - 8) % 2 == 0) {
        const auto cvm = parseEmvCvmList(value);
        result["amountX"] = cvm.amountX;
        result["amountY"] = cvm.amountY;
        nlohmann::ordered_json rules = nlohmann::ordered_json::array();
        for (const auto& rule : cvm.rules) {
            nlohmann::ordered_json item;
            item["index"] = rule.index;
            item["cvmCode"] = hexByteUpper(rule.cvmCode);
            item["conditionCode"] = hexByteUpper(rule.conditionCode);
            item["cvm"] = rule.cvmMeaning;
            item["condition"] = rule.conditionMeaning;
            item["applyNextOnFail"] = rule.applyNextOnFail;
            item["line"] = rule.line;
            rules.push_back(item);
        }
        result["cvmRules"] = rules;
        if (!cvm.error.empty()) {
            result["cvmParseError"] = cvm.error;
        }
        result["raw"] = tlv->valueHex();
        return result;
    }

    // Dispatch by registry type.
    if (tagInfo->type == "NUMERIC") {
        result = parseNumericValue(value, tagInfo->format);
        enrichNumericWithIsoOrEnumMap(*tagInfo, result);
    }
    else if (tagInfo->type == "TEXT") {
        // TEXT: EMV a / an / ans (single-byte characters).
        std::string text(value.begin(), value.end());
        
        // Trim trailing NULs and control characters.
        text.erase(std::find_if(text.rbegin(), text.rend(), 
            [](unsigned char ch) { return ch != '\0' && ch >= 0x20; }).base(), text.end());
        result["text"] = text;
    }
    else if (tagInfo->type == "BITFIELD") {
        result = parseBitFieldValue(value, tagInfo->bitFields);
    }
    else if (tagInfo->type == "DATE") {
        result = parseDateValue(value);
    }
    else if (tagInfo->type == "ENUM") {
        result = parseEnumValue(tlv, tagInfo);
    }
    else if (tagInfo->type == "LIST") {
        // EMV DOL: repeated Tag+Length pairs only (no values); not BER-TLV payload.
        const auto dol = parseEmvDolList(value);
        nlohmann::ordered_json entries = nlohmann::ordered_json::array();
        for (const auto& entry : dol.entries) {
            nlohmann::ordered_json item;
            item["tag"] = entry.tagHex;
            item["length"] = static_cast<int>(entry.length);
            if (!entry.name.empty()) {
                item["name"] = entry.name;
            }
            entries.push_back(item);
        }
        result["dolEntries"] = entries;
        if (!dol.error.empty()) {
            result["dolParseError"] = dol.error;
        }
    }
    else if (tagInfo->type == "BINARY" || tagInfo->type == "b" || tagInfo->type.empty()) {
        // Binary or unknown type: raw hex only.
        result["raw"] = tlv->valueHex();
    }
    
    // Always include raw hex when not already present.
    if (!result.contains("raw")) {
        result["raw"] = tlv->valueHex();
    }
    
    return result;
}

// ============================================================================
// Private helpers
// ============================================================================

nlohmann::ordered_json ValueSemanticParser::parseNumericValue(
    const std::vector<uint8_t>& value,
    const std::string& format
) {
    nlohmann::ordered_json result;
    
    // EMV n (BCD, right-aligned) and cn (compressed numeric, left-aligned, pad F).
    if (format == "n") {
        // n: BCD with leading zeros, e.g. 12345 -> 00 00 00 01 23 45
        std::string numericStr;
        for (uint8_t byte : value) {
            uint8_t high = (byte >> 4) & 0x0F;
            uint8_t low = byte & 0x0F;
            if (high <= 9) numericStr += std::to_string(high);
            if (low <= 9) numericStr += std::to_string(low);
        }
        result["numeric"] = numericStr.empty() ? "0" : numericStr;
    } else if (format == "cn") {
        // cn: e.g. 1234567890123 -> 12 34 56 78 90 12 3F FF
        std::string numericStr;
        for (uint8_t byte : value) {
            uint8_t high = (byte >> 4) & 0x0F;
            uint8_t low = byte & 0x0F;
            if (high <= 9) {
                numericStr += std::to_string(high);
            } else if (high == 0x0F) {
                // Padding F: stop.
                break;
            }
            if (low <= 9) {
                numericStr += std::to_string(low);
            } else if (low == 0x0F) {
                break;
            }
        }
        result["numeric"] = numericStr.empty() ? "0" : numericStr;
    } else {
        // Other formats: big-endian integer.
        uint64_t numeric = 0;
        for (uint8_t byte : value) {
            numeric = (numeric << 8) | byte;
        }
        result["numeric"] = std::to_string(numeric);
    }
    
    return result;
}

nlohmann::ordered_json ValueSemanticParser::parseDateValue(
    const std::vector<uint8_t>& value
) {
    nlohmann::ordered_json result;
    
    // Infer layout from byte length:
    // 3: YYMMDD or HHMMSS; 4: YYMMDDHH or MMDD (BCD); 6: YYMMDDHHMMSS (BCD).
    if (value.size() == 3) {
        // EMV n 3: YYMMDD (BCD).
        const int yy = emvBcdByte(value[0]);
        const int mm = emvBcdByte(value[1]);
        const int dd = emvBcdByte(value[2]);
        std::ostringstream oss;
        oss << "20" << std::setfill('0') << std::setw(2) << yy << "-" << std::setw(2) << mm << "-"
            << std::setw(2) << dd;
        result["date"] = oss.str();
    } else if (value.size() == 4) {
        // EMV n 4: YYMMDDHH (BCD).
        const int yy = emvBcdByte(value[0]);
        const int mm = emvBcdByte(value[1]);
        const int dd = emvBcdByte(value[2]);
        const int hh = emvBcdByte(value[3]);
        std::ostringstream oss;
        oss << "20" << std::setfill('0') << std::setw(2) << yy << "-" << std::setw(2) << mm << "-"
            << std::setw(2) << dd << " " << std::setw(2) << hh << ":00:00";
        result["date"] = oss.str();
    } else if (value.size() == 6) {
        // BCD YYMMDDHHMMSS.
        std::ostringstream oss;
        uint8_t year = ((value[0] >> 4) & 0x0F) * 10 + (value[0] & 0x0F);
        uint8_t month = ((value[1] >> 4) & 0x0F) * 10 + (value[1] & 0x0F);
        uint8_t day = ((value[2] >> 4) & 0x0F) * 10 + (value[2] & 0x0F);
        uint8_t hour = ((value[3] >> 4) & 0x0F) * 10 + (value[3] & 0x0F);
        uint8_t minute = ((value[4] >> 4) & 0x0F) * 10 + (value[4] & 0x0F);
        uint8_t second = ((value[5] >> 4) & 0x0F) * 10 + (value[5] & 0x0F);
        oss << "20" << std::setfill('0') << std::setw(2) << static_cast<int>(year)
            << "-" << std::setw(2) << static_cast<int>(month)
            << "-" << std::setw(2) << static_cast<int>(day)
            << " " << std::setw(2) << static_cast<int>(hour)
            << ":" << std::setw(2) << static_cast<int>(minute)
            << ":" << std::setw(2) << static_cast<int>(second);
        result["date"] = oss.str();
    } else if (value.size() == 2) {
        // MMDD (BCD).
        uint8_t month = ((value[0] >> 4) & 0x0F) * 10 + (value[0] & 0x0F);
        uint8_t day = ((value[1] >> 4) & 0x0F) * 10 + (value[1] & 0x0F);
        std::ostringstream oss;
        oss << "MM-DD: " << std::setfill('0') << std::setw(2) << static_cast<int>(month)
            << "-" << std::setw(2) << static_cast<int>(day);
        result["date"] = oss.str();
    }
    
    return result;
}

nlohmann::ordered_json ValueSemanticParser::parseEnumValue(
    const std::shared_ptr<TLV>& tlv,
    const std::shared_ptr<TagInfo>& tagInfo
) {
    nlohmann::ordered_json result;
    const auto& value = tlv->getValue();
    
    // Map code string via enumMap.
    std::string codeStr;
    
    // EMV n (BCD) and cn formats.
    if (tagInfo->format == "n") {
        for (uint8_t byte : value) {
            uint8_t high = (byte >> 4) & 0x0F;
            uint8_t low = byte & 0x0F;
            if (high <= 9) codeStr += std::to_string(high);
            if (low <= 9) codeStr += std::to_string(low);
        }
    } else if (tagInfo->format == "cn") {
        for (uint8_t byte : value) {
            uint8_t high = (byte >> 4) & 0x0F;
            uint8_t low = byte & 0x0F;
            if (high <= 9) {
                codeStr += std::to_string(high);
            } else if (high == 0x0F) {
                break;
            }
            if (low <= 9) {
                codeStr += std::to_string(low);
            } else if (low == 0x0F) {
                break;
            }
        }
    } else if (tagInfo->format == "a" || tagInfo->format == "an" || tagInfo->format == "ans") {
        // Text enum (e.g. country code letters).
        codeStr = std::string(value.begin(), value.end());
        codeStr.erase(std::find_if(codeStr.rbegin(), codeStr.rend(), 
            [](unsigned char ch) { return ch != '\0' && ch >= 0x20; }).base(), codeStr.end());
    } else if (tagInfo->format == "b") {
        codeStr = tlv->valueHex();
    } else {
        uint64_t code = 0;
        for (uint8_t byte : value) {
            code = (code << 8) | byte;
        }
        codeStr = std::to_string(code);
    }
    
    auto it = tagInfo->enumMap.find(codeStr);
    if (it != tagInfo->enumMap.end()) {
        result["enum"] = it->second;
        result["code"] = codeStr;
    } else {
        result["code"] = codeStr;
        result["enum"] = "UNKNOWN";
    }
    
    return result;
}

nlohmann::ordered_json ValueSemanticParser::parseBitFieldValue(
    const std::vector<uint8_t>& value,
    const std::vector<BitFieldRule>& bitFields
) {
    nlohmann::ordered_json result;
    
    if (value.empty() || bitFields.empty()) {
        return result;
    }
    
    // Multi-byte bit fields; position 0 = MSB of value[0].
    nlohmann::ordered_json bitFieldArray = nlohmann::ordered_json::array();
    
    for (const auto& rule : bitFields) {
        int totalBits = static_cast<int>(value.size() * 8);
        int startBit = rule.position;
        int endBit = startBit + rule.length - 1;
        
        if (startBit < 0 || endBit >= totalBits) {
            continue; // Rule out of range.
        }
        
        uint64_t fieldValue = 0;
        int bitsRemaining = rule.length;
        int currentBit = startBit;
        
        while (bitsRemaining > 0 && currentBit < totalBits) {
            int byteIndex = currentBit / 8;
            // position 0 = value[0] MSB (EMV b8); increase toward LSB then next byte
            int bitFromMsb = currentBit % 8;
            int bitsToExtract = std::min(bitsRemaining, 8 - bitFromMsb);
            
            uint8_t byteValue = value[byteIndex];
            const int shift = 7 - bitFromMsb - bitsToExtract + 1;
            int mask = ((1 << bitsToExtract) - 1) << shift;
            int extractedBits = (byteValue & mask) >> shift;
            
            fieldValue |= (static_cast<uint64_t>(extractedBits) << (rule.length - bitsRemaining));
            
            bitsRemaining -= bitsToExtract;
            currentBit += bitsToExtract;
        }
        
        nlohmann::ordered_json fieldInfo;
        fieldInfo["position"] = startBit;
        fieldInfo["length"] = rule.length;
        fieldInfo["range"] = std::to_string(startBit) + "-" + std::to_string(endBit);
        
        std::string valueStr;
        std::string meaning;
        
        if (rule.type == "ENUM") {
            auto it = rule.enumMap.find(std::to_string(fieldValue));
            if (it != rule.enumMap.end()) {
                valueStr = std::to_string(fieldValue);
                meaning = it->second;
            } else {
                continue; // Unknown enum: omit from output.
            }
        }
        else if (rule.type == "NUMERIC") {
            valueStr = std::to_string(fieldValue);
            meaning = valueStr;
        }
        else {
            continue; // Unsupported rule type.
        }
        
        fieldInfo["value"] = valueStr;
        fieldInfo["rawValue"] = static_cast<uint64_t>(fieldValue);
        fieldInfo["meaning"] = meaning;
        
        bitFieldArray.push_back(fieldInfo);
    }
    
    if (!bitFieldArray.empty()) {
        result["bitFields"] = bitFieldArray;
    }
    
    return result;
}

nlohmann::ordered_json ValueSemanticParser::parseAflValue(const std::vector<uint8_t>& value) {
    nlohmann::ordered_json result;
    nlohmann::ordered_json entries = nlohmann::ordered_json::array();

    int totalRecords = 0;
    int fileCount = 0;

    for (size_t offset = 0; offset + 4 <= value.size(); offset += 4) {
        const uint8_t b1 = value[offset];
        const uint8_t firstRecord = value[offset + 1];
        const uint8_t lastRecord = value[offset + 2];
        const uint8_t offlineRecords = value[offset + 3];

        if (b1 == 0x00 && firstRecord == 0x00 && lastRecord == 0x00 && offlineRecords == 0x00) {
            continue;
        }

        const int sfi = (b1 >> 3) & 0x1F;
        nlohmann::ordered_json entry;
        entry["sfi"] = sfi;
        entry["firstRecord"] = static_cast<int>(firstRecord);
        entry["lastRecord"] = static_cast<int>(lastRecord);
        entry["offlineRecords"] = static_cast<int>(offlineRecords);
        entry["line"] = "SFI " + std::to_string(sfi) + ": records " + std::to_string(firstRecord) + "-" +
                        std::to_string(lastRecord) + ", offline auth records " +
                        std::to_string(offlineRecords);
        entries.push_back(entry);

        ++fileCount;
        if (lastRecord >= firstRecord) {
            totalRecords += static_cast<int>(lastRecord - firstRecord + 1);
        }
    }

    result["entries"] = entries;
    if (!entries.empty()) {
        result["summary"] =
            std::to_string(fileCount) + " file(s), " + std::to_string(totalRecords) + " record(s) referenced";
    }
    return result;
}
