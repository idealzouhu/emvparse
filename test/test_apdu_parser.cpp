#include <gtest/gtest.h>
#include <emvparse/tlv_parser.h>
#include <emvparse/apdu_parser.h>
#include <emvparse/output_options.h>
#include <nlohmann/json.hpp>
#include <string>

namespace {

const char* kManifestPath = "../../registry/manifest.json";

emvparse::ParseOutputOptions jsonOpts() {
    emvparse::ParseOutputOptions opts;
    opts.format = emvparse::OutputFormat::Json;
    opts.input = emvparse::InputMode::Auto;
    return opts;
}

emvparse::ParseOutputOptions tableOpts() {
    emvparse::ParseOutputOptions opts;
    opts.format = emvparse::OutputFormat::Table;
    opts.input = emvparse::InputMode::Auto;
    return opts;
}

emvparse::ParseOutputOptions tlvForcedOpts() {
    emvparse::ParseOutputOptions opts;
    opts.format = emvparse::OutputFormat::Json;
    opts.input = emvparse::InputMode::Tlv;
    return opts;
}

} // namespace

TEST(ApduParserTest, ReadRecordCase2_00B2021400) {
    TLVParser parser(kManifestPath, "EMV");
    const std::string hex = "00B2021400";

    const std::string jsonOut = parser.parseHexWithOptions(hex, jsonOpts());
    auto root = nlohmann::json::parse(jsonOut);
    ASSERT_TRUE(root.is_object());
    EXPECT_EQ(root["inputKind"], "capdu");
    EXPECT_TRUE(root["command"]["name"].get<std::string>().find("READ RECORD") != std::string::npos);
    EXPECT_EQ(root["command"]["cla"], "00");
    EXPECT_EQ(root["command"]["ins"], "B2");
    EXPECT_EQ(root["command"]["p1"], "02");
    EXPECT_EQ(root["command"]["p2"], "14");
    EXPECT_EQ(root["command"]["le"], "00");
    EXPECT_EQ(root["data"]["format"], "none");
    EXPECT_FALSE(root["data"].contains("tlv"));
    EXPECT_EQ(root["command"]["sfi"], 2);
    const std::string p1m = root["command"]["p1Meaning"].get<std::string>();
    EXPECT_NE(p1m.find("2"), std::string::npos);
    const std::string p2m = root["command"]["p2Meaning"].get<std::string>();
    EXPECT_NE(p2m.find("SFI 2"), std::string::npos);

    const std::string tableOut = parser.parseHexWithOptions(hex, tableOpts());
    EXPECT_NE(tableOut.find("READ RECORD"), std::string::npos);
    EXPECT_NE(tableOut.find("00 B2"), std::string::npos);
    EXPECT_EQ(tableOut.find("8202"), std::string::npos);

    ApduRegistry registry;
    ASSERT_TRUE(registry.loadFromManifest(kManifestPath));
    std::vector<uint8_t> data = {0x00, 0xB2, 0x02, 0x14, 0x00};
    const auto consumer = [&parser](const std::vector<uint8_t>& body) {
        return parser.bytesFullyConsumedAsTlv(body);
    };
    EXPECT_EQ(
        ApduParser::classifyInput(data, emvparse::InputMode::Auto, &registry, consumer),
        ParsedInputKind::Capdu);
}

TEST(ApduParserTest, GpoCapdu_80A8000002830000) {
    TLVParser parser(kManifestPath, "EMV");
    const std::string hex = "80A8000002830000";
    const std::string jsonOut = parser.parseHexWithOptions(hex, jsonOpts());
    auto root = nlohmann::json::parse(jsonOut);
    ASSERT_TRUE(root.is_object());
    EXPECT_EQ(root["inputKind"], "capdu");
    EXPECT_TRUE(root["command"]["name"].get<std::string>().find("GET PROCESSING OPTIONS") != std::string::npos);
    ASSERT_TRUE(root["data"].contains("tlv"));
    ASSERT_FALSE(root["data"]["tlv"].empty());
    EXPECT_EQ(root["data"]["tlv"][0]["tag"], "83");
}

TEST(ApduParserTest, PureTlv_770E) {
    TLVParser parser(kManifestPath, "EMV");
    const std::string hex = "770E8202198094081001020120010300";
    const std::string jsonOut = parser.parseHexWithOptions(hex, jsonOpts());
    auto root = nlohmann::json::parse(jsonOut);
    ASSERT_TRUE(root.is_array());
    ASSERT_EQ(root.size(), 1u);
    EXPECT_EQ(root[0]["tag"], "77");
}

TEST(ApduParserTest, Rapdu_Fci9000) {
    TLVParser parser(kManifestPath, "EMV");
    const std::string hex =
        "6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000";
    const std::string jsonOut = parser.parseHexWithOptions(hex, jsonOpts());
    auto root = nlohmann::json::parse(jsonOut);
    ASSERT_TRUE(root.is_object());
    EXPECT_EQ(root["inputKind"], "rapdu");
    EXPECT_EQ(root["statusWord"]["sw"], "9000");
    EXPECT_EQ(root["statusWord"]["meaning"], "Normal processing");
    ASSERT_TRUE(root["body"]["tlv"].is_array());
    EXPECT_EQ(root["body"]["tlv"][0]["tag"], "6F");
}

TEST(ApduParserTest, ForcedTlv_Includes9000Suffix) {
    TLVParser parser(kManifestPath, "EMV");
    const std::string hex =
        "6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000";
    const std::string jsonOut = parser.parseHexWithOptions(hex, tlvForcedOpts());
    auto root = nlohmann::json::parse(jsonOut);
    ASSERT_TRUE(root.is_array());
    EXPECT_GE(root.size(), 1u);
    EXPECT_EQ(root[0]["tag"], "6F");
    if (root.size() >= 2) {
        EXPECT_EQ(root[1]["tag"], "90");
    }
}
