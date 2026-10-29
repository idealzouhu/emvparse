#ifndef EMVPARSE_TLV_PARSER_H
#define EMVPARSE_TLV_PARSER_H

#include "tlv.h"
#include "output_options.h"
#include "logging.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

class TagRegistry;
class ApduRegistry;

class EMVPARSE_EXPORT TLVParser {
public:
    TLVParser();

    explicit TLVParser(const std::string& manifestPath, const std::string& scheme);

    ~TLVParser();

    TLVParser(const TLVParser&) = delete;
    TLVParser& operator=(const TLVParser&) = delete;

    TLVParser(TLVParser&&) noexcept;
    TLVParser& operator=(TLVParser&&) noexcept;

    bool loadTagRegistry(const std::string& manifestPath, const std::string& scheme);

    std::vector<std::shared_ptr<TLV>> parseBytes(const std::vector<uint8_t>& data);

    std::vector<std::shared_ptr<TLV>> parseHex(const std::string& hexStr);

    std::string parseBytesToJson(const std::vector<uint8_t>& data);

    std::string parseHexToJson(const std::string& hexStr);

    std::string parseHexWithOptions(const std::string& hexStr, const emvparse::ParseOutputOptions& options);

    bool bytesFullyConsumedAsTlv(const std::vector<uint8_t>& data) const;

    std::string getTagInfo(const std::string& tagHex) const;

    TagRegistry* tagRegistry() const { return registry_; }
    ApduRegistry* apduRegistry() const { return apduRegistry_; }

    static std::vector<uint8_t> hexToBytes(const std::string& hexStr);

    void setLogCallback(LogCallback callback);

private:
    TagRegistry* registry_;
    class ApduRegistry* apduRegistry_;

    std::shared_ptr<TLV> parseTLV(const std::vector<uint8_t>& data, size_t& offset);

    size_t parseTagBytes(const std::vector<uint8_t>& data, size_t& offset, std::vector<uint8_t>& tagBytes);

    bool isCompositeTag(const std::vector<uint8_t>& tag);

    static bool readTlvLength(const std::vector<uint8_t>& data, size_t& offset, size_t& lengthOut);

    std::vector<uint8_t> hexStringToBytes(const std::string& hexStr) const;
};

#endif // EMVPARSE_TLV_PARSER_H
