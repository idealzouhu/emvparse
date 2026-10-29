#include <gtest/gtest.h>
#include <emvparse/dol_parser.h>
#include <emvparse/tlv_parser.h>

namespace {

const char* kManifestPath = "../../registry/manifest.json";

TEST(DolParserTest, ParsePdolListFrom9F38) {
    // 9F02 06 9F03 06 9F1A 02
    const std::vector<uint8_t> dol = {0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06, 0x9F, 0x1A, 0x02};
    const auto parsed = parseEmvDolList(dol);
    ASSERT_EQ(parsed.entries.size(), 3u);
    EXPECT_EQ(parsed.entries[0].tagHex, "9F02");
    EXPECT_EQ(parsed.entries[0].length, 6);
    EXPECT_EQ(parsed.entries[2].tagHex, "9F1A");
    EXPECT_TRUE(parsed.error.empty());
}

TEST(DolParserTest, SplitPayloadMatchesLengths) {
    std::vector<EmvDolEntry> dol;
    EmvDolEntry e1;
    e1.tagHex = "9F02";
    e1.length = 6;
    dol.push_back(e1);
    EmvDolEntry e2;
    e2.tagHex = "5F2A";
    e2.length = 2;
    dol.push_back(e2);

    const std::vector<uint8_t> payload = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x40};
    const auto split = splitDolPayload(dol, payload);
    ASSERT_EQ(split.entries.size(), 2u);
    EXPECT_EQ(split.entries[0].valueHex, "000000000100");
    EXPECT_EQ(split.entries[1].valueHex, "0840");
    EXPECT_FALSE(split.truncated);
}

TEST(DolParserTest, ParseLongTagInDol) {
    const std::vector<uint8_t> dol = {0x9F, 0x02, 0x06};
    const auto parsed = parseEmvDolList(dol);
    ASSERT_EQ(parsed.entries.size(), 1u);
    EXPECT_EQ(parsed.entries[0].tagHex, "9F02");
}

} // namespace
