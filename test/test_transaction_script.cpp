#include <gtest/gtest.h>
#include <emvparse/transaction_script.h>
#include <emvparse/tlv_parser.h>
#include <emvparse/apdu_parser.h>
#include <nlohmann/json.hpp>
#include <string>

namespace {

const char* kManifestPath = "../../registry/manifest.json";

TEST(TransactionScriptTest, LoadRejectsMissingFields) {
    TransactionScript script;
    std::string error;
    EXPECT_FALSE(TransactionScriptParser::loadFromJsonString(R"({"version":1})", script, error));
    EXPECT_FALSE(error.empty());
}

TEST(TransactionScriptTest, LoadRejectsBadHex) {
    const char* json = R"({
      "version": 1,
      "steps": [{"capdu": "ZZ", "rapdu": "9000"}]
    })";
    TransactionScript script;
    std::string error;
    ASSERT_TRUE(TransactionScriptParser::loadFromJsonString(json, script, error));
    TLVParser parser(kManifestPath, "EMV");
    auto result = TransactionScriptParser::parse(parser, script);
    EXPECT_FALSE(result.error.empty());
}

TEST(TransactionScriptTest, SelectPseStepTitleAndTable) {
    const char* json = R"({
      "version": 1,
      "steps": [
        {
          "capdu": "00A404000E315041592E5359532E444446303100",
          "rapdu": "6F1E840E315041592E5359532E4444463031A50C8801015F2D027A689F1101019000"
        },
        {
          "capdu": "00B2011400",
          "rapdu": "6A83"
        }
      ]
    })";
    TransactionScript script;
    std::string error;
    ASSERT_TRUE(TransactionScriptParser::loadFromJsonString(json, script, error));
    TLVParser parser(kManifestPath, "EMV");
    auto result = TransactionScriptParser::parse(parser, script);
    ASSERT_TRUE(result.error.empty());
    ASSERT_EQ(result.steps.size(), 2u);
    EXPECT_NE(result.steps[0].stepTitle.find("SELECT PSE"), std::string::npos);
    EXPECT_NE(result.steps[1].stepTitle.find("READ RECORD"), std::string::npos);

    emvparse::FormatOptions opts;
    const std::string table =
        TransactionScriptFormatter::formatTable(result, parser.tagRegistry(), opts);
    EXPECT_NE(table.find("Command:"), std::string::npos);
    EXPECT_NE(table.find("Send:"), std::string::npos);
    EXPECT_NE(table.find("Response:"), std::string::npos);
    EXPECT_NE(table.find("6A83"), std::string::npos);
}

TEST(TransactionScriptTest, GpoFormat1WithContext) {
    TLVParser parser(kManifestPath, "EMV");
    const auto capduBytes = TLVParser::hexToBytes("80A8000002830000");
    const auto rapduBytes =
        TLVParser::hexToBytes("80107C000800010001001001010101010101019000");

    const auto tlvParseFn = [&parser](const std::vector<uint8_t>& payload) {
        return parser.parseBytes(payload);
    };
    ApduRegistry* apduReg = parser.apduRegistry();
    auto capdu = ApduParser::parseCapdu(capduBytes, apduReg, tlvParseFn);
    RapduParseContext ctx;
    ctx.requestCapdu = &capdu.capdu;
    ctx.requestCommand = capdu.command;
    auto rapdu = ApduParser::parseRapdu(rapduBytes, apduReg, tlvParseFn, &ctx);
    EXPECT_TRUE(rapdu.format1Expanded);
    ASSERT_FALSE(rapdu.format1Children.empty());
    EXPECT_EQ(rapdu.format1Children[0]->tagHex(), "82");

    const std::string json = ApduParser::formatJson(rapdu, parser.tagRegistry());
    EXPECT_NE(json.find("format1"), std::string::npos);
}

TEST(TransactionScriptTest, RapduWithoutContextKeepsTag80) {
    TLVParser parser(kManifestPath, "EMV");
    const auto rapduBytes =
        TLVParser::hexToBytes("80107C000800010001001001010101010101019000");
    const auto tlvParseFn = [&parser](const std::vector<uint8_t>& payload) {
        return parser.parseBytes(payload);
    };
    auto rapdu = ApduParser::parseRapdu(rapduBytes, parser.apduRegistry(), tlvParseFn, nullptr);
    EXPECT_FALSE(rapdu.format1Expanded);
    ASSERT_EQ(rapdu.tlvRoots.size(), 1u);
    EXPECT_EQ(rapdu.tlvRoots[0]->tagHex(), "80");
}

} // namespace
