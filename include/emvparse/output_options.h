#ifndef EMVPARSE_OUTPUT_OPTIONS_H
#define EMVPARSE_OUTPUT_OPTIONS_H

namespace emvparse {

enum class SemanticSummary { Active, Full };

enum class OutputFormat { Json, Table, Html };

struct FormatOptions {
    SemanticSummary semantic = SemanticSummary::Active;
};

struct ParseOutputOptions {
    OutputFormat format = OutputFormat::Json;
    FormatOptions formatOptions;
};

} // namespace emvparse

#endif // EMVPARSE_OUTPUT_OPTIONS_H
