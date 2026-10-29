#ifndef EMVPARSE_TLV_PARSER_H
#define EMVPARSE_TLV_PARSER_H

#include "emvparse/logging.h"
#include "emvparse/tlv.h"
#include <memory>
#include <vector>
#include <string>

class Logger;
class TagRegistry;

/** BER-TLV decode (ISO 8825) with optional TagRegistry for metadata lookup. */
class TLVParser {
public:
    TLVParser();

    explicit TLVParser(const std::string& manifestPath, const std::string& scheme);

    ~TLVParser();

    TLVParser(const TLVParser&) = delete;
    TLVParser& operator=(const TLVParser&) = delete;

    TLVParser(TLVParser&&) noexcept = default;
    TLVParser& operator=(TLVParser&&) noexcept = default;

    void setLogger(Logger* logger);

    bool loadTagRegistry(const std::string& manifestPath, const std::string& scheme);

    std::vector<std::shared_ptr<TLV>> parseBytes(const std::vector<uint8_t>& data);

    std::vector<std::shared_ptr<TLV>> parseHex(const std::string& hexStr);

    /** Used by R-APDU auto-detection: response body must be wholly valid TLV. */
    bool bytesFullyConsumedAsTlv(const std::vector<uint8_t>& data) const;

    std::string getTagInfo(const std::string& tagHex) const;

    TagRegistry* tagRegistry() const { return registry_.get(); }

    static std::vector<uint8_t> hexToBytes(const std::string& hexStr);

    /** Display-only: wire node plus attached children (does not mutate the original TLV). */
    static std::shared_ptr<TLV> displayWithChildren(
        const std::shared_ptr<TLV>& wire,
        const std::vector<std::shared_ptr<TLV>>& children
    );

private:
    std::unique_ptr<TagRegistry> registry_;

    std::shared_ptr<TLV> parseTLV(const std::vector<uint8_t>& data, size_t& offset);

    size_t parseTagBytes(const std::vector<uint8_t>& data, size_t& offset, std::vector<uint8_t>& tagBytes);

    bool isCompositeTag(const std::vector<uint8_t>& tag);

    static bool readTlvLength(const std::vector<uint8_t>& data, size_t& offset, size_t& lengthOut);
};

#endif // EMVPARSE_TLV_PARSER_H
