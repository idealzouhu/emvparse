#ifndef TLV_SEMANTIC_SUMMARY_H
#define TLV_SEMANTIC_SUMMARY_H

#include "emvparse/output_options.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct SemanticSummaryLine {
    std::string text;
    bool inactive = false;
};

std::vector<SemanticSummaryLine> buildSemanticSummaryLines(
    const nlohmann::ordered_json& semantic,
    emvparse::SemanticSummary mode
);

/** --semantic active|full applies to BITFIELD tags only; other types always use Full. */
emvparse::SemanticSummary effectiveSemanticSummaryMode(
    const std::string& tagType,
    emvparse::SemanticSummary requested
);

/** Indented tree output: plain lines, no redundant labels; skips text-only hints. */
std::vector<SemanticSummaryLine> buildTreeSemanticSummaryLines(
    const nlohmann::ordered_json& semantic,
    emvparse::SemanticSummary mode
);

#endif // TLV_SEMANTIC_SUMMARY_H
