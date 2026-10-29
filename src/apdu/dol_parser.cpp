#include "dol_parser.h"
#include "tag_registry.h"
#include <sstream>
#include <iomanip>

namespace {

std::string bytesToTagHex(const std::vector<uint8_t>& tag) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (uint8_t b : tag) {
        oss << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::string bytesToValueHex(const std::vector<uint8_t>& value) {
    return bytesToTagHex(value);
}

// EMV BER tag bytes (same rules as TLVParser::parseTagBytes).
bool readBerTag(const std::vector<uint8_t>& data, std::size_t& offset, std::vector<uint8_t>& tagOut) {
    if (offset >= data.size()) {
        return false;
    }
    tagOut.clear();
    uint8_t firstByte = data[offset++];
    tagOut.push_back(firstByte);
    if ((firstByte & 0x1F) != 0x1F) {
        return true;
    }
    while (offset < data.size()) {
        uint8_t nextByte = data[offset++];
        tagOut.push_back(nextByte);
        if ((nextByte & 0x80) == 0) {
            break;
        }
    }
    return true;
}

} // namespace

EmvDolListParseResult parseEmvDolList(const std::vector<uint8_t>& bytes) {
    EmvDolListParseResult result;
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        EmvDolEntry entry;
        if (!readBerTag(bytes, offset, entry.tag)) {
            result.error = "truncated DOL tag";
            break;
        }
        entry.tagHex = bytesToTagHex(entry.tag);
        if (offset >= bytes.size()) {
            result.error = "truncated DOL length";
            break;
        }
        entry.length = bytes[offset++];
        result.entries.push_back(entry);
    }
    result.consumed = offset;
    return result;
}

DolPayloadSplitResult splitDolPayload(
    const std::vector<EmvDolEntry>& dolEntries,
    const std::vector<uint8_t>& payload
) {
    DolPayloadSplitResult result;
    std::size_t pos = 0;
    for (const auto& dol : dolEntries) {
        DolPayloadSplitEntry split;
        split.dolEntry = dol;
        if (pos + dol.length > payload.size()) {
            split.missing = true;
            if (pos < payload.size()) {
                split.value.assign(payload.begin() + static_cast<std::ptrdiff_t>(pos), payload.end());
                split.valueHex = bytesToValueHex(split.value);
            }
            result.truncated = true;
            result.entries.push_back(split);
            pos = payload.size();
            break;
        }
        split.value.assign(
            payload.begin() + static_cast<std::ptrdiff_t>(pos),
            payload.begin() + static_cast<std::ptrdiff_t>(pos + dol.length));
        split.valueHex = bytesToValueHex(split.value);
        pos += dol.length;
        result.entries.push_back(split);
    }
    if (pos < payload.size()) {
        result.leftover.assign(payload.begin() + static_cast<std::ptrdiff_t>(pos), payload.end());
    }
    return result;
}

void enrichDolEntryNames(std::vector<EmvDolEntry>& entries, TagRegistry* registry) {
    if (!registry) {
        return;
    }
    for (auto& entry : entries) {
        if (auto info = registry->getTagInfo(entry.tagHex)) {
            entry.name = info->name;
        }
    }
}
