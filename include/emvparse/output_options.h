#ifndef EMVPARSE_OUTPUT_OPTIONS_H
#define EMVPARSE_OUTPUT_OPTIONS_H

namespace emvparse {

enum class SemanticSummary { Active, Full };

/** Compact: primitive lines omit value hex (LIST never shows raw hex). Verbose: primitives show value hex. */
enum class TreeStyle { Compact, Verbose };

enum class OutputFormat { Json, Table, Html };

enum class InputMode { Auto, Tlv, Capdu, Rapdu };

struct FormatOptions {
    SemanticSummary semantic = SemanticSummary::Active;
    TreeStyle tree = TreeStyle::Compact;
};

struct ParseOutputOptions {
    OutputFormat format = OutputFormat::Json;
    FormatOptions formatOptions;
    InputMode input = InputMode::Auto;
};

} // namespace emvparse

#endif // EMVPARSE_OUTPUT_OPTIONS_H
