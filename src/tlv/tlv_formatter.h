#ifndef TLVFORMATTER_H
#define TLVFORMATTER_H

#include "emvparse/output_options.h"
#include "emvparse/tlv.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

class TagRegistry;
class TagInfo;

struct TableTextRow {
    enum class Kind { Data, Semantic };

    Kind kind = Kind::Data;
    int depth = 0;
    std::string tagHex;
    std::string name;
    std::string length;
    std::string value;
    std::string semanticText;
};

class TLVFormatter {
public:
    static nlohmann::ordered_json toJson(
        const std::shared_ptr<TLV>& tlv,
        TagRegistry* registry = nullptr
    );

    static std::vector<TableTextRow> tableRowsFromRoots(
        const std::vector<std::shared_ptr<TLV>>& roots,
        TagRegistry* registry,
        const emvparse::FormatOptions& options
    );

    static std::string renderTextTable(const std::vector<TableTextRow>& rows);

    static std::string toTable(
        const std::vector<std::shared_ptr<TLV>>& roots,
        TagRegistry* registry,
        const emvparse::FormatOptions& options
    );

    /** HTML table fragment only (no document wrapper). */
    static std::string toHtmlFragment(
        const std::vector<std::shared_ptr<TLV>>& roots,
        TagRegistry* registry,
        const emvparse::FormatOptions& options = emvparse::FormatOptions{}
    );

    static std::string toHtml(
        const std::vector<std::shared_ptr<TLV>>& roots,
        TagRegistry* registry,
        const emvparse::FormatOptions& options = emvparse::FormatOptions{}
    );

    static const char* htmlStyles();
    static const char* htmlToggleScript();

    static std::string toTreeText(
        const std::vector<std::shared_ptr<TLV>>& roots,
        TagRegistry* registry,
        const emvparse::FormatOptions& options
    );
};

#endif // TLVFORMATTER_H
