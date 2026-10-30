#include "tlv_formatter.h"
#include "tag_registry.h"
#include "value_semantic_parser.h"
#include "tlv.h"
#include <sstream>
#include <iomanip>

nlohmann::ordered_json TLVFormatter::toJson(const std::shared_ptr<TLV>& tlv, TagRegistry* registry) {
    nlohmann::ordered_json json;
    
    // 1. Base fields (tag, length, value).
    json["tag"] = tlv->tagHex();
    json["length"] = tlv->lengthHex();
    json["value"] = tlv->valueHex();

    // 2. Optional registry metadata.
    std::shared_ptr<TagInfo> tagInfo = nullptr;
    if (registry) {
        tagInfo = registry->getTagInfo(tlv->tagHex());
        if (tagInfo) {
            // Name and metadata for primitive and constructed tags.
            json["name"] = tagInfo->name;
            json["type"] = tagInfo->type;
            if (!tagInfo->format.empty()) {
                json["format"] = tagInfo->format;
            }
            if (!tagInfo->length.empty()) {
                json["expectedLength"] = tagInfo->length;
            }
            if (!tagInfo->kernel.empty()) {
                json["kernel"] = tagInfo->kernel;
            }
            if (!tagInfo->source.empty()) {
                json["source"] = tagInfo->source;
            }
            if (!tagInfo->note.empty()) {
                json["note"] = tagInfo->note;
            }
            if (!tagInfo->description.empty()) {
                json["description"] = tagInfo->description;
            }
            if (!tagInfo->template_.empty()) {
                json["template"] = tagInfo->template_;
            }
        }
    }

    // 3. Semantic block before children (stable field order).
    const auto& children = tlv->getChildren();
    if (!children.empty()) {
        // Constructed tag: value holds nested TLVs; skip value semantics.
        if (tagInfo) {
            // Raw value hex only.
            nlohmann::ordered_json semantic;
            semantic["raw"] = tlv->valueHex();
            json["semantic"] = semantic;
        }
    } else {
        // Primitive tag: parse value semantics.
        if (tagInfo) {
            auto semantic = ValueSemanticParser::parseValueSemantic(tlv, tagInfo);
            json["semantic"] = semantic;
        }
    }

    // 4. Recurse into children last.
    if (!children.empty()) {
        nlohmann::ordered_json childrenArray = nlohmann::ordered_json::array();
        for (const auto& child : children) {
            childrenArray.push_back(toJson(child, registry));
        }
        json["children"] = childrenArray;
    }
    
    return json;
}
