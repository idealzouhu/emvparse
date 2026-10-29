#ifndef EMVPARSE_PARSE_OUTPUT_PIPELINE_H
#define EMVPARSE_PARSE_OUTPUT_PIPELINE_H

#include "apdu_parser.h"
#include "emvparse/output_options.h"
#include "transaction_script.h"
#include <string>

class TagRegistry;

/** Maps ParseOutputOptions.format to ApduResultFormatter / TransactionScriptFormatter. */
class ParseOutputPipeline {
public:
    static std::string formatApdu(
        const ApduParseResult& result,
        TagRegistry* registry,
        const emvparse::ParseOutputOptions& options
    );

    static std::string formatScript(
        const TransactionScriptParseResult& parsed,
        TagRegistry* tagRegistry,
        const emvparse::ParseOutputOptions& options
    );
};

#endif // EMVPARSE_PARSE_OUTPUT_PIPELINE_H
