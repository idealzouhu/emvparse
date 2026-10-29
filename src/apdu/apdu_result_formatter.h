#ifndef EMVPARSE_APDU_RESULT_FORMATTER_H
#define EMVPARSE_APDU_RESULT_FORMATTER_H

#include "apdu_parser.h"
#include "emvparse/output_options.h"
#include <string>

class TagRegistry;

/** Serializes ApduParseResult (TLV / C-APDU / R-APDU) to JSON, table, or HTML. */
class ApduResultFormatter {
public:
    static std::string formatCapduSendBlock(const ApduParseResult& result);
    static std::string formatCapduScriptFields(const ApduParseResult& result);
    /** Case 4 C-APDU: Le byte after command data (ISO 7816-4 wire order). */
    static std::string formatCapduLeAfterCommandData(const ApduParseResult& result);
    static std::string formatCapduHtmlSendBlock(const ApduParseResult& result);
    /** C-APDU field rows only (includes case-4 Le); no outer block or title. */
    static std::string formatCapduHtmlFieldsTable(const ApduParseResult& result);
    static std::string formatCapduHtmlLeAfterCommandData(const ApduParseResult& result);
    static const char* capduHtmlStyles();

    static std::string formatJson(const ApduParseResult& result, TagRegistry* tagRegistry);
    static std::string formatTable(
        const ApduParseResult& result,
        TagRegistry* tagRegistry,
        const emvparse::FormatOptions& options
    );
    static std::string formatHtml(
        const ApduParseResult& result,
        TagRegistry* tagRegistry,
        const emvparse::FormatOptions& options = emvparse::FormatOptions{}
    );
};

#endif // EMVPARSE_APDU_RESULT_FORMATTER_H
