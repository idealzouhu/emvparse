#ifndef EMVPARSE_APDU_PARSER_H
#define EMVPARSE_APDU_PARSER_H

#include "emvparse/emvparse_global.h"
#include "emvparse/output_options.h"
#include "emvparse/tlv.h"
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class TagRegistry;

struct ApduParamRule {
    std::string name;
    std::string description;
    std::map<std::string, std::string> enumMap;
    bool numericRecord = false;
    bool tagReference = false;
    int sfiShift = -1;
};

struct ApduCommandInfo {
    uint8_t cla = 0;
    uint8_t ins = 0;
    std::string name;
    std::string dataFormat;
    ApduParamRule p1;
    ApduParamRule p2;
};

class EMVPARSE_EXPORT ApduRegistry {
public:
    bool loadFromManifest(const std::string& manifestPath);

    const ApduCommandInfo* findCommand(uint8_t cla, uint8_t ins) const;
    std::string lookupStatusWord(uint16_t sw) const;
    bool isKnownStatusPattern(uint16_t sw) const;

    std::size_t commandCount() const { return commands_.size(); }

private:
    std::vector<ApduCommandInfo> commands_;
    std::unordered_map<uint32_t, std::size_t> commandIndex_;
    std::unordered_map<uint16_t, std::string> statusWords_;
};

enum class ParsedInputKind { Tlv, Capdu, Rapdu };

struct CapduFields {
    uint8_t cla = 0;
    uint8_t ins = 0;
    uint8_t p1 = 0;
    uint8_t p2 = 0;
    bool hasLc = false;
    uint8_t lc = 0;
    bool hasLe = false;
    uint8_t le = 0;
    std::vector<uint8_t> commandData;
    std::string headerHex;
    std::string dataHex;
};

struct ApduParseResult {
    ParsedInputKind kind = ParsedInputKind::Tlv;
    const ApduCommandInfo* command = nullptr;
    CapduFields capdu;
    std::string p1Meaning;
    std::string p2Meaning;
    int sfi = -1;
    uint16_t statusWord = 0;
    std::string statusMeaning;
    std::vector<std::shared_ptr<TLV>> tlvRoots;
};

using TlvBodyConsumer = std::function<bool(const std::vector<uint8_t>& body)>;

class EMVPARSE_EXPORT ApduParser {
public:
    static ParsedInputKind classifyInput(
        const std::vector<uint8_t>& data,
        emvparse::InputMode mode,
        ApduRegistry* registry,
        const TlvBodyConsumer& tlvConsumesAll
    );

    static bool tryParseCapduStructure(const std::vector<uint8_t>& data, CapduFields& out);

    static ApduParseResult parseCapdu(
        const std::vector<uint8_t>& data,
        ApduRegistry* registry,
        const std::function<std::vector<std::shared_ptr<TLV>>(const std::vector<uint8_t>&)>& parseTlv
    );

    static ApduParseResult parseRapdu(
        const std::vector<uint8_t>& data,
        ApduRegistry* registry,
        const std::function<std::vector<std::shared_ptr<TLV>>(const std::vector<uint8_t>&)>& parseTlv
    );

    static std::string formatJson(const ApduParseResult& result, TagRegistry* tagRegistry);
    static std::string formatTable(
        const ApduParseResult& result,
        TagRegistry* tagRegistry,
        const emvparse::FormatOptions& options
    );
    static std::string formatHtml(const ApduParseResult& result, TagRegistry* tagRegistry);

private:
    static void enrichCommandSemantics(ApduParseResult& result);
    static std::string resolveParamMeaning(const ApduParamRule& rule, uint8_t value, int* sfiOut);
    static std::string byteHex(uint8_t b);
    static std::string bytesHex(const std::vector<uint8_t>& data);
};

#endif // EMVPARSE_APDU_PARSER_H
