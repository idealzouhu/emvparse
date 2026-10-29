#include <gtest/gtest.h>
#include "transaction_script.h"
#include "apdu_parser.h"
#include "apdu_result_formatter.h"
#include "tlv_parser.h"
#include <emvparse/input_parser.h>
#include "input_parser_internal.h"
#include <emvparse/output_options.h>
#include <nlohmann/json.hpp>
#include <string>

namespace {

const char* kManifestPath = "../../registry/manifest.json";

} // namespace

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
    EmvInputParser parser(kManifestPath, "EMV");
    auto ctx = EmvInputParserImplAccess::context(parser);
    auto result = TransactionScriptParser::parse(ctx, script);
    EXPECT_FALSE(result.error.empty());
}

TEST(TransactionScriptTest, FormatHtmlLayout) {
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
    EmvInputParser parser(kManifestPath, "EMV");
    emvparse::ParseOutputOptions opts;
    opts.format = emvparse::OutputFormat::Html;
    const std::string html = parser.parseScriptJson(json, opts);
    ASSERT_FALSE(html.empty()) << parser.lastError();

    EXPECT_EQ(html.find("<!DOCTYPE html>"), html.rfind("<!DOCTYPE html>"));
    EXPECT_NE(html.find("<h2>SELECT</h2>"), std::string::npos);
    EXPECT_NE(html.find("<p class=\"send-summary\">SELECT PSE"), std::string::npos);
    EXPECT_EQ(html.find("EMV TLV Parse</title>"), std::string::npos);
    EXPECT_NE(html.find("class=\"tlv-gutter\""), std::string::npos);
    const auto tag5f2d = html.find("<td class=\"tlv-tag\">5F2D</td>");
    ASSERT_NE(tag5f2d, std::string::npos);
    const auto rowStart = html.rfind("<tr>", tag5f2d);
    ASSERT_NE(rowStart, std::string::npos);
    const std::string row5f2d = html.substr(rowStart, tag5f2d - rowStart);
    const std::string kGutterCell = R"(<td class="tlv-gutter"></td>)";
    size_t gutterCells = 0;
    for (size_t pos = 0; (pos = row5f2d.find(kGutterCell, pos)) != std::string::npos; ++gutterCells) {
        pos += kGutterCell.size();
    }
    EXPECT_GE(gutterCells, 2u);
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
    EmvInputParser parser(kManifestPath, "EMV");
    emvparse::ParseOutputOptions opts;
    opts.format = emvparse::OutputFormat::Table;
    const std::string table = parser.parseScriptJson(json, opts);
    ASSERT_FALSE(table.empty()) << parser.lastError();
    EXPECT_NE(table.find("Command:"), std::string::npos);
    EXPECT_NE(table.find("Send:"), std::string::npos);
    EXPECT_NE(table.find("Response:"), std::string::npos);
    EXPECT_NE(table.find("6A83"), std::string::npos);
    EXPECT_NE(table.find("SELECT PSE"), std::string::npos);
    EXPECT_NE(table.find("READ RECORD"), std::string::npos);
}

TEST(TransactionScriptTest, GpoFormat1WithContext) {
    EmvInputParser parser(kManifestPath, "EMV");
    const auto capduBytes = EmvInputParser::hexToBytes("80A8000002830000");
    const auto rapduBytes =
        EmvInputParser::hexToBytes("80107C000800010001001001010101010101019000");

    TLVParser tlvParser;
    ASSERT_TRUE(tlvParser.loadTagRegistry(kManifestPath, "EMV"));
    const auto tlvParseFn = [&tlvParser](const std::vector<uint8_t>& payload) {
        return tlvParser.parseBytes(payload);
    };
    ApduRegistry apduReg;
    ASSERT_TRUE(apduReg.loadFromManifest(kManifestPath));
    auto capdu = ApduParser::parseCapdu(capduBytes, &apduReg, tlvParseFn);
    RapduParseContext ctx;
    ctx.requestCapdu = &capdu.capdu;
    ctx.requestCommand = capdu.command;
    auto rapdu = ApduParser::parseRapdu(rapduBytes, &apduReg, tlvParseFn, &ctx);
    EXPECT_TRUE(rapdu.format1Expanded);
    ASSERT_FALSE(rapdu.format1Children.empty());
    EXPECT_EQ(rapdu.format1Children[0]->tagHex(), "82");

    const std::string json = ApduResultFormatter::formatJson(rapdu, tlvParser.tagRegistry());
    EXPECT_NE(json.find("format1"), std::string::npos);
}

TEST(TransactionScriptTest, RapduWithoutContextKeepsTag80) {
    TLVParser tlvParser;
    ASSERT_TRUE(tlvParser.loadTagRegistry(kManifestPath, "EMV"));
    ApduRegistry apduReg;
    ASSERT_TRUE(apduReg.loadFromManifest(kManifestPath));
    const auto rapduBytes =
        EmvInputParser::hexToBytes("80107C000800010001001001010101010101019000");
    const auto tlvParseFn = [&tlvParser](const std::vector<uint8_t>& payload) {
        return tlvParser.parseBytes(payload);
    };
    auto rapdu = ApduParser::parseRapdu(rapduBytes, &apduReg, tlvParseFn, nullptr);
    EXPECT_FALSE(rapdu.format1Expanded);
    ASSERT_EQ(rapdu.tlvRoots.size(), 1u);
    EXPECT_EQ(rapdu.tlvRoots[0]->tagHex(), "80");
}
