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

#endif // TLV_SEMANTIC_SUMMARY_H
