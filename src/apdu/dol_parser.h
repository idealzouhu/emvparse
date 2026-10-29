#ifndef EMVPARSE_DOL_PARSER_H
#define EMVPARSE_DOL_PARSER_H

// EMV DOL: tag||length sequences (PDOL/CDOL/DDOL) and splitting command data by DOL layout.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class TagRegistry;

struct EmvDolEntry {
    std::vector<uint8_t> tag;
    std::string tagHex;
    uint8_t length = 0;
    std::string name;
};

struct EmvDolListParseResult {
    std::vector<EmvDolEntry> entries;
    std::size_t consumed = 0;
    std::string error;
};

struct DolPayloadSplitEntry {
    EmvDolEntry dolEntry;
    std::vector<uint8_t> value;
    std::string valueHex;
    bool missing = false;
};

struct DolPayloadSplitResult {
    std::vector<DolPayloadSplitEntry> entries;
    std::vector<uint8_t> leftover;
    bool truncated = false;
};

EmvDolListParseResult parseEmvDolList(const std::vector<uint8_t>& bytes);

DolPayloadSplitResult splitDolPayload(
    const std::vector<EmvDolEntry>& dolEntries,
    const std::vector<uint8_t>& payload
);

void enrichDolEntryNames(std::vector<EmvDolEntry>& entries, TagRegistry* registry);

#endif // EMVPARSE_DOL_PARSER_H
