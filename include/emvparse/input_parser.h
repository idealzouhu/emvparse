#ifndef EMVPARSE_INPUT_PARSER_H
#define EMVPARSE_INPUT_PARSER_H

#include "emvparse/emvparse_global.h"
#include "emvparse/logging.h"
#include "emvparse/output_options.h"
#include "emvparse/tlv.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct EmvInputParserImplAccess;

class EMVPARSE_EXPORT EmvInputParser {
    friend struct EmvInputParserImplAccess;

public:
    EmvInputParser();

    explicit EmvInputParser(const std::string& manifestPath, const std::string& scheme);

    EmvInputParser(const EmvInputParser&) = delete;
    EmvInputParser& operator=(const EmvInputParser&) = delete;

    EmvInputParser(EmvInputParser&&) noexcept;
    EmvInputParser& operator=(EmvInputParser&&) noexcept;

    ~EmvInputParser();

    bool loadRegistry(const std::string& manifestPath, const std::string& scheme);

    std::string parseHexWithOptions(
        const std::string& hexStr,
        const emvparse::ParseOutputOptions& options
    );

    std::string parseHexToJson(const std::string& hexStr);

    std::vector<std::shared_ptr<TLV>> parseHex(const std::string& hexStr);

    std::string getTagInfo(const std::string& tagHex) const;

    std::string parseScriptFile(
        const std::string& path,
        const emvparse::ParseOutputOptions& options
    );

    std::string parseScriptJson(
        const std::string& json,
        const emvparse::ParseOutputOptions& options
    );

    std::string lastError() const;

    static std::vector<uint8_t> hexToBytes(const std::string& hexStr);

    void setLogCallback(LogCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // EMVPARSE_INPUT_PARSER_H
