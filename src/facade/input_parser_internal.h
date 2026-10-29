#ifndef EMVPARSE_INPUT_PARSER_INTERNAL_H
#define EMVPARSE_INPUT_PARSER_INTERNAL_H

#include "apdu_parser.h"
#include "emv_parse_context.h"
#include "tlv_parser.h"
#include "emvparse/input_parser.h"

class TagRegistry;

/** In-tree only: reaches EmvInputParser::Impl for CLI, tests, and script parsing. */
struct EmvInputParserImplAccess {
    static EmvParseContext context(EmvInputParser& parser);
    static EmvParseContext context(const EmvInputParser& parser);

    static TLVParser& tlvParser(EmvInputParser& parser);
    static const TLVParser& tlvParser(const EmvInputParser& parser);
    static TagRegistry* tagRegistry(EmvInputParser& parser);
    static ApduRegistry* apduRegistry(EmvInputParser& parser);
};

#endif // EMVPARSE_INPUT_PARSER_INTERNAL_H
