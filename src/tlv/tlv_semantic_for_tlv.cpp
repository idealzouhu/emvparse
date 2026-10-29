#include "tlv_semantic_for_tlv.h"
#include "tag_registry.h"
#include "value_semantic_parser.h"
#include <emvparse/tlv.h>

nlohmann::ordered_json semanticForTlv(
    const std::shared_ptr<TLV>& tlv,
    TagRegistry* registry,
    const std::shared_ptr<TagInfo>& tagInfo
) {
    if (!tagInfo) {
        return nlohmann::ordered_json::object();
    }
    if (!tlv->getChildren().empty()) {
        nlohmann::ordered_json semantic;
        semantic["raw"] = tlv->valueHex();
        return semantic;
    }
    auto semantic = ValueSemanticParser::parseValueSemantic(tlv, tagInfo);
    if (registry && tagInfo->type == "LIST" && semantic.contains("dolEntries") &&
        semantic["dolEntries"].is_array()) {
        for (auto& entry : semantic["dolEntries"]) {
            if (!entry.is_object() || !entry.contains("tag")) {
                continue;
            }
            const std::string tag = entry["tag"].get<std::string>();
            if (auto info = registry->getTagInfo(tag)) {
                entry["name"] = info->name;
            }
        }
    }
    return semantic;
}
