#ifndef VALUE_SEMANTIC_PARSER_H
#define VALUE_SEMANTIC_PARSER_H

#include "tag_registry.h"
#include <emvparse/tlv.h>
#include <nlohmann/json.hpp>
#include <memory>

/**
 * @brief Value semantic parser (internal).
 *
 * Interprets TLV values using TagInfo business rules from the registry.
 */
class ValueSemanticParser {
public:
    /**
     * @brief Parse semantic meaning of a TLV value.
     * @param tlv TLV object.
     * @param tagInfo Registry rules (may be null).
     * @return JSON object with semantic fields.
     */
    static nlohmann::ordered_json parseValueSemantic(
        const std::shared_ptr<TLV>& tlv,
        const std::shared_ptr<TagInfo>& tagInfo
    );

private:
    /**
     * @brief Parse NUMERIC value.
     * @param value Raw value bytes.
     * @param format EMV format (n, cn, etc.).
     * @return Parsed JSON object.
     */
    static nlohmann::ordered_json parseNumericValue(
        const std::vector<uint8_t>& value,
        const std::string& format
    );

    /**
     * @brief Parse DATE value.
     * @param value Raw value bytes.
     * @return Parsed JSON object.
     */
    static nlohmann::ordered_json parseDateValue(
        const std::vector<uint8_t>& value
    );

    /**
     * @brief Parse ENUM value.
     * @param tlv TLV object.
     * @param tagInfo Tag metadata and enum map.
     * @return Parsed JSON object.
     */
    static nlohmann::ordered_json parseEnumValue(
        const std::shared_ptr<TLV>& tlv,
        const std::shared_ptr<TagInfo>& tagInfo
    );

    /**
     * @brief Parse BITFIELD value.
     * @param value Raw value bytes.
     * @param bitFields Bit-field rule list.
     * @return Parsed JSON object.
     */
    static nlohmann::ordered_json parseBitFieldValue(
        const std::vector<uint8_t>& value,
        const std::vector<BitFieldRule>& bitFields
    );

    static nlohmann::ordered_json parseAflValue(const std::vector<uint8_t>& value);
};

#endif // VALUE_SEMANTIC_PARSER_H
