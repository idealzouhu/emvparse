#include "emvparse/tlv_parser.h"
#include "tag_registry.h"
#include "tlv_formatter.h"
#include "logger.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>

TLVParser::TLVParser()
    : registry_(new TagRegistry()) {
}

TLVParser::TLVParser(const std::string& manifestPath, const std::string& scheme)
    : registry_(new TagRegistry()) {
    loadTagRegistry(manifestPath, scheme);
}

TLVParser::~TLVParser() {
    delete registry_;
}

TLVParser::TLVParser(TLVParser&& other) noexcept
    : registry_(other.registry_) {
    other.registry_ = nullptr;
}

TLVParser& TLVParser::operator=(TLVParser&& other) noexcept {
    if (this != &other) {
        delete registry_;
        registry_ = other.registry_;
        other.registry_ = nullptr;
    }
    return *this;
}

bool TLVParser::loadTagRegistry(const std::string& manifestPath, const std::string& scheme) {
    if (!registry_) {
        registry_ = new TagRegistry();
    }
    return registry_->loadManifest(manifestPath, scheme);
}

void TLVParser::setLogCallback(LogCallback callback) {
    // Forward callback to global Logger.
    Logger::getInstance().setLogCallback(callback);
}

std::vector<std::shared_ptr<TLV>> TLVParser::parseBytes(const std::vector<uint8_t>& data) {
    std::vector<std::shared_ptr<TLV>> result;
    size_t offset = 0;
    
    while (offset < data.size()) {
        auto tlv = parseTLV(data, offset);
        if (tlv) {
            result.push_back(tlv);
        } else {
            // parseTLV returned nullptr (truncated or invalid data).
            break;
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<TLV>> TLVParser::parseHex(const std::string& hexStr) {
    auto data = hexStringToBytes(hexStr);
    if (data.empty() && !hexStr.empty()) {
        // Hex conversion failed (odd length or invalid characters).
        return {};
    }
    return parseBytes(data);
}

std::string TLVParser::parseBytesToJson(const std::vector<uint8_t>& data) {
    auto tlvList = parseBytes(data);
    nlohmann::ordered_json jsonArray = nlohmann::ordered_json::array();
    for (const auto& tlv : tlvList) {
        // Attach registry semantics via TLVFormatter.
        jsonArray.push_back(TLVFormatter::toJson(tlv, registry_));
    }
    return jsonArray.dump(2);  // Pretty-print with 2-space indent.
}

std::string TLVParser::parseHexToJson(const std::string& hexStr) {
    auto data = hexStringToBytes(hexStr);
    if (data.empty() && !hexStr.empty()) {
        // Hex conversion failed; empty JSON array.
        return "[]";
    }
    return parseBytesToJson(data);
}

std::string TLVParser::parseHexWithOptions(
    const std::string& hexStr,
    const emvparse::ParseOutputOptions& options
) {
    auto data = hexStringToBytes(hexStr);
    if (data.empty() && !hexStr.empty()) {
        if (options.format == emvparse::OutputFormat::Json) {
            return "[]";
        }
        return {};
    }

    const auto tlvList = parseBytes(data);

    switch (options.format) {
    case emvparse::OutputFormat::Table:
        return TLVFormatter::toTable(tlvList, registry_, options.formatOptions);
    case emvparse::OutputFormat::Html:
        return TLVFormatter::toHtml(tlvList, registry_);
    case emvparse::OutputFormat::Json:
    default:
        return parseBytesToJson(data);
    }
}

std::string TLVParser::getTagInfo(const std::string& tagHex) const {
    if (!registry_) {
        return "";
    }
    
    auto tagInfo = registry_->getTagInfo(tagHex);
    if (!tagInfo) {
        return "";
    }
    
    // Build tag metadata JSON.
    nlohmann::ordered_json info;
    info["tag"] = tagHex;
    
    if (!tagInfo->name.empty()) {
        info["name"] = tagInfo->name;
    }
    
    if (!tagInfo->description.empty()) {
        info["description"] = tagInfo->description;
    }
    
    if (!tagInfo->template_.empty()) {
        info["template"] = tagInfo->template_;
    }
    
    if (!tagInfo->type.empty()) {
        info["type"] = tagInfo->type;
    }
    
    if (!tagInfo->format.empty()) {
        info["format"] = tagInfo->format;
    }
    
    if (!tagInfo->length.empty()) {
        info["length"] = tagInfo->length;
    }
    
    if (!tagInfo->kernel.empty()) {
        info["kernel"] = tagInfo->kernel;
    }
    
    if (!tagInfo->source.empty()) {
        info["source"] = tagInfo->source;
    }
    
    if (!tagInfo->note.empty()) {
        info["note"] = tagInfo->note;
    }
    
    return info.dump(2);
}

std::shared_ptr<TLV> TLVParser::parseTLV(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size()) {
        return nullptr;
    }

    // 1. Parse Tag (EMV single- and multi-byte).
    std::vector<uint8_t> tag;
    size_t tagByteCount = parseTagBytes(data, offset, tag);
    if (tagByteCount == 0 || tag.empty()) {
        // Tag parse failed.
        return nullptr;
    }

    // 2. Parse Length
    if (offset >= data.size()) {
        return nullptr;
    }
    uint8_t lengthByte = data[offset++];
    
    if (lengthByte > 0x7F) {
        return nullptr; // Long-form length not supported yet.
    }
    
    size_t length = lengthByte;

    // 3. Parse Value
    if (offset + length > data.size()) {
        return nullptr;
    }
    std::vector<uint8_t> value(data.begin() + offset, data.begin() + offset + length);
    offset += length;

    // 4. Build TLV object
    auto tlv = std::make_shared<TLV>(tag, value);

    // 5. Constructed tag (EMV tag byte bit 6 set)?
    if (isCompositeTag(tag)) {
        // Parse nested TLVs inside the value.
        size_t nestedOffset = 0;
        while (nestedOffset < value.size()) {
            // Remember offset to detect no-progress loops.
            size_t offsetBefore = nestedOffset;
            auto child = parseTLV(value, nestedOffset);
            if (child) {
                tlv->children.push_back(child);
                // Stalled offset: stop to avoid infinite loop.
                if (nestedOffset == offsetBefore) {
                    break;
                }
            } else {
                // nullptr: end of buffer or parse error; stop if offset did not advance.
                if (nestedOffset == offsetBefore) {
                    break;
                }
                // Advanced offset but child parse failed: try remaining bytes.
            }
        }
    }

    return tlv;
}

size_t TLVParser::parseTagBytes(const std::vector<uint8_t>& data, size_t& offset, std::vector<uint8_t>& tagBytes) {
    if (offset >= data.size()) {
        return 0;
    }

    tagBytes.clear();
    
    // Step 1: read first tag byte (required).
    uint8_t firstByte = data[offset++];
    tagBytes.push_back(firstByte);
    
    // If bits b5-b1 != 0x1F, tag is one byte.
    if ((firstByte & 0x1F) != 0x1F) {
        return 1;
    }
    
    // Multi-byte tag: read continuation bytes (step 2).
    while (offset < data.size()) {
        uint8_t nextByte = data[offset++];
        tagBytes.push_back(nextByte);
        
        // b8=0 marks the last tag byte; b8=1 means more bytes follow.
        if ((nextByte & 0x80) == 0) {
            break;
        }
    }
    
    // Step 3: return total tag byte count.
    return tagBytes.size();
}

bool TLVParser::isCompositeTag(const std::vector<uint8_t>& tag) {
    if (tag.empty()) {
        return false;
    }

    // EMV constructed bit: tag byte bit 6 (1-based); no registry lookup needed.
    uint8_t firstByte = tag[0];
    return (firstByte & 0x20) != 0;  // 0x20: bit 6 set => constructed.
}

std::vector<uint8_t> TLVParser::hexStringToBytes(const std::string& hexStr) {
    std::vector<uint8_t> data;
    
    // Strip whitespace ("9F33 02 01 41" or "9F33020141").
    std::string cleanHex = hexStr;
    cleanHex.erase(std::remove_if(cleanHex.begin(), cleanHex.end(), ::isspace), cleanHex.end());
    
    // Hex string must have even length.
    if (cleanHex.length() % 2 != 0) {
        return {};
    }
    
    // Parse hex pairs into bytes.
    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        std::string byteStr = cleanHex.substr(i, 2);
        try {
            uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
            data.push_back(byte);
        } catch (const std::exception&) {
            return {};  // Invalid hex digit.
        }
    }
    
    return data;
}
