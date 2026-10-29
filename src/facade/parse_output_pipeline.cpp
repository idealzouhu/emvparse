// Thin format dispatch; keeps EmvInputParser free of formatter switch statements.
#include "parse_output_pipeline.h"
#include "apdu_result_formatter.h"
#include "transaction_script.h"

std::string ParseOutputPipeline::formatApdu(
    const ApduParseResult& result,
    TagRegistry* registry,
    const emvparse::ParseOutputOptions& options
) {
    switch (options.format) {
    case emvparse::OutputFormat::Table:
        return ApduResultFormatter::formatTable(result, registry, options.formatOptions);
    case emvparse::OutputFormat::Html:
        return ApduResultFormatter::formatHtml(result, registry, options.formatOptions);
    case emvparse::OutputFormat::Json:
    default:
        return ApduResultFormatter::formatJson(result, registry);
    }
}

std::string ParseOutputPipeline::formatScript(
    const TransactionScriptParseResult& parsed,
    TagRegistry* tagRegistry,
    const emvparse::ParseOutputOptions& options
) {
    switch (options.format) {
    case emvparse::OutputFormat::Table:
        return TransactionScriptFormatter::formatTable(
            parsed, tagRegistry, options.formatOptions);
    case emvparse::OutputFormat::Html:
        return TransactionScriptFormatter::formatHtml(
            parsed, tagRegistry, options.formatOptions);
    case emvparse::OutputFormat::Json:
    default:
        return TransactionScriptFormatter::formatJson(parsed, tagRegistry);
    }
}
