#ifndef EMVPARSE_EMV_PARSE_CONTEXT_H
#define EMVPARSE_EMV_PARSE_CONTEXT_H

class ApduRegistry;
class TagRegistry;
class TLVParser;

/**
 * Non-owning bundle passed into script parsing and in-tree tests.
 * Keeps apdu/ from including the public EmvInputParser header for hex/context only.
 */
struct EmvParseContext {
    TLVParser& tlvParser;
    ApduRegistry& apduRegistry;

    TagRegistry* tagRegistry() const;
};

#endif // EMVPARSE_EMV_PARSE_CONTEXT_H
