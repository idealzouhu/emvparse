#include "tlv_parser.h"
#include "hex_util.h"
#include "tag_registry.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

TLVParser::TLVParser()
    : registry_(std::make_unique<TagRegistry>()) {
}

TLVParser::TLVParser(const std::string& manifestPath, const std::string& scheme)
    : registry_(std::make_unique<TagRegistry>()) {
    loadTagRegistry(manifestPath, scheme);
}

TLVParser::~TLVParser() = default;

void TLVParser::setLogger(Logger* logger) {
    if (registry_) {
        registry_->setLogger(logger);
    }
}

bool TLVParser::loadTagRegistry(const std::string& manifestPath, const std::string& scheme) {
    if (!registry_) {
        registry_ = std::make_unique<TagRegistry>();
    }
    return registry_->loadManifest(manifestPath, scheme);
}

std::vector<std::shared_ptr<TLV>> TLVParser::parseBytes(const std::vector<uint8_t>& data) {
    std::vector<std::shared_ptr<TLV>> result;
    size_t offset = 0;
    
    while (offset < data.size()) {
        auto tlv = parseTLV(data, offset);
        if (tlv) {
            result.push_back(tlv);
        } else {
            break;
        }
    }
    
    return result;
}

std::vector<std::shared_ptr<TLV>> TLVParser::parseHex(const std::string& hexStr) {
    auto data = parseHexString(hexStr);
    if (data.empty() && !hexStr.empty()) {
        return {};
    }
    return parseBytes(data);
}

bool TLVParser::bytesFullyConsumedAsTlv(const std::vector<uint8_t>& data) const {
    // parseTLV is non-const; copy buffer so this probe stays logically const.
    if (data.empty()) {
        return true;
    }
    size_t offset = 0;
    while (offset < data.size()) {
        const size_t before = offset;
        std::vector<uint8_t> mutableData = data;
        auto tlv = const_cast<TLVParser*>(this)->parseTLV(mutableData, offset);
        if (!tlv || offset == before) {
            return false;
        }
    }
    return true;
}

std::string TLVParser::getTagInfo(const std::string& tagHex) const {
    if (!registry_) {
        return "";
    }
    
    auto tagInfo = registry_->getTagInfo(tagHex);
    if (!tagInfo) {
        return "";
    }
    
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

// BER-TLV length: short form (0–127), long form (0x81…0x84 + big-endian octets). 0x80 invalid.
bool TLVParser::readTlvLength(const std::vector<uint8_t>& data, size_t& offset, size_t& lengthOut) {
    if (offset >= data.size()) {
        return false;
    }
    const uint8_t first = data[offset++];
    if (first <= 0x7F) {
        lengthOut = static_cast<size_t>(first);
        return true;
    }
    if (first == 0x80) {
        return false; // indefinite length not supported (not used in EMV data objects)
    }
    const size_t numOctets = static_cast<size_t>(first & 0x7F);
    if (numOctets == 0 || numOctets > 4 || offset + numOctets > data.size()) {
        return false;
    }
    size_t len = 0;
    for (size_t i = 0; i < numOctets; ++i) {
        len = (len << 8) | static_cast<size_t>(data[offset++]);
    }
    lengthOut = len;
    return true;
}

std::shared_ptr<TLV> TLVParser::parseTLV(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset >= data.size()) {
        return nullptr;
    }

    std::vector<uint8_t> tag;
    size_t tagByteCount = parseTagBytes(data, offset, tag);
    if (tagByteCount == 0 || tag.empty()) {
        return nullptr;
    }

    size_t length = 0;
    if (!readTlvLength(data, offset, length)) {
        return nullptr;
    }

    if (offset + length > data.size()) {
        return nullptr;
    }
    std::vector<uint8_t> value(data.begin() + offset, data.begin() + offset + length);
    offset += length;

    auto tlv = std::make_shared<TLV>(tag, value);

    // EMV constructed data objects: value field holds nested TLVs (tag bit 6).
    if (isCompositeTag(tag)) {
        size_t nestedOffset = 0;
        while (nestedOffset < value.size()) {
            size_t offsetBefore = nestedOffset;
            auto child = parseTLV(value, nestedOffset);
            if (child) {
                tlv->children.push_back(child);
                if (nestedOffset == offsetBefore) {
                    break;
                }
            } else {
                if (nestedOffset == offsetBefore) {
                    break;
                }
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
    
    uint8_t firstByte = data[offset++];
    tagBytes.push_back(firstByte);
    
    // Low 5 bits 0x1F → multi-byte tag continues until a byte with bit 8 clear.
    if ((firstByte & 0x1F) != 0x1F) {
        return 1;
    }

    while (offset < data.size()) {
        uint8_t nextByte = data[offset++];
        tagBytes.push_back(nextByte);
        
        if ((nextByte & 0x80) == 0) {
            break;
        }
    }
    
    return tagBytes.size();
}

bool TLVParser::isCompositeTag(const std::vector<uint8_t>& tag) {
    if (tag.empty()) {
        return false;
    }

    return (tag[0] & 0x20) != 0; // ISO 7816 / EMV: bit 6 = constructed
}

std::vector<uint8_t> TLVParser::hexToBytes(const std::string& hexStr) {
    return parseHexString(hexStr);
}

std::shared_ptr<TLV> TLVParser::displayWithChildren(
    const std::shared_ptr<TLV>& wire,
    const std::vector<std::shared_ptr<TLV>>& children
) {
    if (!wire) {
        return nullptr;
    }
    auto display = std::make_shared<TLV>(wire->getTag(), wire->getValue());
    for (const auto& child : children) {
        display->children.push_back(child);
    }
    return display;
}
