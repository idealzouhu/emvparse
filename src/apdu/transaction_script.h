#ifndef EMVPARSE_TRANSACTION_SCRIPT_H
#define EMVPARSE_TRANSACTION_SCRIPT_H

#include "apdu_parser.h"
#include "dol_parser.h"
#include "emvparse/output_options.h"
#include <string>
#include <vector>

class TagRegistry;
struct EmvParseContext;

struct TransactionScriptStepInput {
    std::string capduHex;
    std::string rapduHex;
};

struct TransactionScript {
    int version = 1;
    std::string title;
    std::string scheme;
    std::vector<TransactionScriptStepInput> steps;
};

/** DOL lists learned from earlier script steps (9F38/8C/8D/9F49 in parsed TLV trees). */
struct ScriptDolContext {
    std::vector<EmvDolEntry> pdol;
    std::vector<EmvDolEntry> cdol1;
    std::vector<EmvDolEntry> cdol2;
    std::vector<EmvDolEntry> ddol;

    void absorbTlvTree(const std::vector<std::shared_ptr<TLV>>& roots, TagRegistry* registry);
};

struct TransactionScriptStepResult {
    int index = 0;
    std::string stepTitle;
    std::string capduWireHex;
    std::string rapduWireHex;
    ApduParseResult capdu;
    ApduParseResult rapdu;
};

struct TransactionScriptParseResult {
    TransactionScript script;
    std::vector<TransactionScriptStepResult> steps;
    std::string error;
};

class TransactionScriptParser {
public:
    static bool loadFromJsonString(const std::string& json, TransactionScript& out, std::string& error);
    static bool loadFromFile(const std::string& path, TransactionScript& out, std::string& error);

    /** Sequential steps: parse C/R-APDU, expand DOL using accumulated ScriptDolContext. */
    static TransactionScriptParseResult parse(
        EmvParseContext& context,
        const TransactionScript& script
    );

    static std::string deriveStepTitle(const ApduParseResult& capdu);
};

class TransactionScriptFormatter {
public:
    static std::string formatJson(
        const TransactionScriptParseResult& result,
        TagRegistry* registry
    );

    static std::string formatTable(
        const TransactionScriptParseResult& result,
        TagRegistry* registry,
        const emvparse::FormatOptions& options
    );

    static std::string formatHtml(
        const TransactionScriptParseResult& result,
        TagRegistry* registry,
        const emvparse::FormatOptions& options
    );
};

#endif // EMVPARSE_TRANSACTION_SCRIPT_H
