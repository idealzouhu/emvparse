#include <gtest/gtest.h>
#include <emvparse/input_parser.h>
#include <emvparse/output_options.h>
#include <nlohmann/json.hpp>
#include <string>

namespace {

const char* kManifestPath = "../../registry/manifest.json";

emvparse::ParseOutputOptions jsonAutoOpts() {
    emvparse::ParseOutputOptions opts;
    opts.format = emvparse::OutputFormat::Json;
    opts.input = emvparse::InputMode::Auto;
    return opts;
}

emvparse::ParseOutputOptions jsonTlvOpts() {
    emvparse::ParseOutputOptions opts;
    opts.format = emvparse::OutputFormat::Json;
    opts.input = emvparse::InputMode::Tlv;
    return opts;
}

} // namespace

TEST(InputParserFacadeTest, CapduAutoReadRecord) {
    EmvInputParser parser(kManifestPath, "EMV");
    const std::string out = parser.parseHexWithOptions("00B2021400", jsonAutoOpts());
    ASSERT_FALSE(out.empty());
    auto root = nlohmann::json::parse(out);
    EXPECT_EQ(root["inputKind"], "capdu");
}

TEST(InputParserFacadeTest, ForcedTlvParse) {
    EmvInputParser parser(kManifestPath, "EMV");
    const std::string hex =
        "6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000";
    const std::string out = parser.parseHexWithOptions(hex, jsonTlvOpts());
    auto root = nlohmann::json::parse(out);
    ASSERT_TRUE(root.is_array());
    EXPECT_EQ(root[0]["tag"], "6F");
}

TEST(InputParserFacadeTest, TagInfo) {
    EmvInputParser parser(kManifestPath, "EMV");
    const std::string info = parser.getTagInfo("9F33");
    EXPECT_FALSE(info.empty());
    EXPECT_NE(info.find("9F33"), std::string::npos);
}

TEST(InputParserFacadeTest, InvalidHexParseHexToJson) {
    EmvInputParser parser;
    const std::string out = parser.parseHexToJson("GG");
    EXPECT_TRUE(out.empty());
    EXPECT_FALSE(parser.lastError().empty());
}

TEST(InputParserFacadeTest, InvalidHexParseHex) {
    EmvInputParser parser;
    const auto roots = parser.parseHex("GG");
    EXPECT_TRUE(roots.empty());
    EXPECT_FALSE(parser.lastError().empty());
}

TEST(InputParserFacadeTest, InvalidManifestLoadRegistry) {
    EmvInputParser parser;
    EXPECT_FALSE(parser.loadRegistry("nonexistent/manifest.json", "EMV"));
    EXPECT_FALSE(parser.lastError().empty());
}

TEST(InputParserFacadeTest, ScriptFileJson) {
    EmvInputParser parser(kManifestPath, "EMV");
    emvparse::ParseOutputOptions opts;
    opts.format = emvparse::OutputFormat::Json;
    const std::string out =
        parser.parseScriptFile("../../docs/examples/transaction-script-v1.json", opts);
    ASSERT_FALSE(out.empty()) << parser.lastError();
    auto root = nlohmann::json::parse(out);
    EXPECT_TRUE(root.is_object());
    EXPECT_TRUE(root.contains("steps"));
}
