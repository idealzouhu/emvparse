#ifndef TLV_SEMANTIC_FOR_TLV_H
#define TLV_SEMANTIC_FOR_TLV_H

#include <memory>
#include <nlohmann/json.hpp>

class TagRegistry;
struct TagInfo;
class TLV;

nlohmann::ordered_json semanticForTlv(
    const std::shared_ptr<TLV>& tlv,
    TagRegistry* registry,
    const std::shared_ptr<TagInfo>& tagInfo
);

#endif // TLV_SEMANTIC_FOR_TLV_H
