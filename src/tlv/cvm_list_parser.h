#ifndef CVM_LIST_PARSER_H
#define CVM_LIST_PARSER_H

#include <cstdint>
#include <string>
#include <vector>

/** One CVM List rule: CVM Code byte + CVM Condition Code byte (EMV Book 3). */
struct EmvCvmRuleEntry {
    int index = 0;
    uint8_t cvmCode = 0;
    uint8_t conditionCode = 0;
    bool applyNextOnFail = false;
    std::string cvmMeaning;
    std::string conditionMeaning;
    /** Human-readable single-line summary for formatters. */
    std::string line;
};

struct EmvCvmListParseResult {
    uint32_t amountX = 0;
    uint32_t amountY = 0;
    std::vector<EmvCvmRuleEntry> rules;
    std::string error;
};

/** Parse tag 8E value: 4-byte Amount X, 4-byte Amount Y, then CVM rule pairs. */
EmvCvmListParseResult parseEmvCvmList(const std::vector<uint8_t>& bytes);

std::string emvCvmCodeMeaning(uint8_t cvmCode, bool* applyNextOnFailOut);
std::string emvCvmConditionMeaning(uint8_t conditionCode);

#endif // CVM_LIST_PARSER_H
