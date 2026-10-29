#include "tlv_tree_formatter.h"
#include "tlv_formatter.h"
#include <nlohmann/json.hpp>

std::string TlvTreeFormatter::toJsonArray(
    const std::vector<std::shared_ptr<TLV>>& roots,
    TagRegistry* registry
) {
    nlohmann::ordered_json jsonArray = nlohmann::ordered_json::array();
    for (const auto& tlv : roots) {
        jsonArray.push_back(TLVFormatter::toJson(tlv, registry));
    }
    return jsonArray.dump(2);
}
