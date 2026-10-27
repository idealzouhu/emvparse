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

class TLVFormatter {
public:
    static nlohmann::ordered_json toJson(
        const std::shared_ptr<TLV>& tlv,
        TagRegistry* registry = nullptr
    );

    static std::string toTable(
        const std::vector<std::shared_ptr<TLV>>& roots,
        TagRegistry* registry,
        const emvparse::FormatOptions& options
    );

    static std::string toHtml(
        const std::vector<std::shared_ptr<TLV>>& roots,
        TagRegistry* registry
    );
};

#endif // TLVFORMATTER_H
