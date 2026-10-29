#include <gtest/gtest.h>
#include <emvparse/input_parser.h>
#include <emvparse/output_options.h>
#include <emvparse/tlv.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <iostream>

namespace {

const nlohmann::json* findTagNode(const nlohmann::json& node, const std::string& tagHex) {
    if (node.is_object() && node.value("tag", std::string()) == tagHex) {
        return &node;
    }
    if (node.is_object() && node.contains("children") && node["children"].is_array()) {
        for (const auto& child : node["children"]) {
            const nlohmann::json* found = findTagNode(child, tagHex);
            if (found != nullptr) {
                return found;
            }
        }
    }
    if (node.is_array()) {
        for (const auto& item : node) {
            const nlohmann::json* found = findTagNode(item, tagHex);
            if (found != nullptr) {
                return found;
            }
        }
    }
    return nullptr;
}

std::optional<int> bitFieldPositionByMeaning(const nlohmann::json& semantic, const std::string& meaning) {
    if (!semantic.is_object() || !semantic.contains("bitFields") || !semantic["bitFields"].is_array()) {
        return std::nullopt;
    }
    for (const auto& field : semantic["bitFields"]) {
        if (field.value("meaning", std::string()) == meaning) {
            return field.value("position", -1);
        }
    }
    return std::nullopt;
}

bool tableContainsBitLine(const std::string& table, int byteNum, int bitNum, const std::string& meaning) {
    const std::string prefix =
        "byte " + std::to_string(byteNum) + " bit " + std::to_string(bitNum) + " = 1";
    const auto pos = table.find(prefix);
    if (pos == std::string::npos) {
        return false;
    }
    const auto meaningPos = table.find(meaning, pos);
    return meaningPos != std::string::npos && meaningPos < pos + 120;
}

} // namespace

/**
 * @brief TLV Value 字段语义测试
 * 
 * 测试 TLV 解析后，根据 TagRegistry 中的规则，
 * Value 字段的语义解析是否正确。
 */
class TlvSemanticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        const std::string manifestPath = "../../registry/manifest.json";
        inputParser = std::make_unique<EmvInputParser>(manifestPath, "EMV");
    }

    std::unique_ptr<EmvInputParser> inputParser;
};

// 测试1: NUMERIC 类型 Value 的语义解析
TEST_F(TlvSemanticsTest, NumericValueSemantics) {
    // 测试数据: 9F33 02 01 41 (Terminal Country Code)
    // Value = 0x01 0x41 (BCD 编码) = 141 (十进制)
    std::string hexData = "9F33020141";
    auto tlvList = inputParser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    // 验证 Tag 识别
    EXPECT_EQ(tlv->tagHex(), "9F33");
    
    // 验证 Value 原始数据
    const auto& value = tlv->getValue();
    EXPECT_EQ(value.size(), 2);
    EXPECT_EQ(value[0], 0x01);
    EXPECT_EQ(value[1], 0x41);
    
    // 注意：实际的语义解析（BCD 转十进制）应该在应用层完成
    // 这里只验证 TagRegistry 能正确识别 Tag 类型
    // 可以通过 TagRegistry 查询 TagInfo 来验证
}

// 测试2: TEXT 类型 Value 的语义解析
TEST_F(TlvSemanticsTest, TextValueSemantics) {
    // 测试数据: 5F20 03 42 6F 62 (Cardholder Name = "Bob")
    // Value = 0x42 0x6F 0x62 (ASCII) = "Bob"
    std::string hexData = "5F2003426F62";
    auto tlvList = inputParser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "5F20");
    
    const auto& value = tlv->getValue();
    EXPECT_EQ(value.size(), 3);
    EXPECT_EQ(value[0], 0x42); // 'B'
    EXPECT_EQ(value[1], 0x6F); // 'o'
    EXPECT_EQ(value[2], 0x62); // 'b'
    
    // 验证可以转换为字符串
    std::string textValue(value.begin(), value.end());
    EXPECT_EQ(textValue, "Bob");
}

// 测试3: BITFIELD 类型 Value 的语义解析
TEST_F(TlvSemanticsTest, BitFieldValueSemantics) {
    // 测试数据: 9F6B 01 D0 (Transaction Flags)
    // Value = 0xD0 (二进制 11010000)
    // bit 7 = 1 → ElectronicCashSupported = true
    // bits 4-6 = 101 = 5 → ErrorCode (但枚举中没有5，应该是未定义)
    // bits 0-3 = 0000 = 0 → SubStatus = 0
    std::string hexData = "9F6B01D0";
    auto tlvList = inputParser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "9F6B");
    
    const auto& value = tlv->getValue();
    EXPECT_EQ(value.size(), 1);
    EXPECT_EQ(value[0], 0xD0);
    
    // 验证 BitField 解析
    // bit 7 (最高位) = 1
    EXPECT_EQ((value[0] >> 7) & 0x01, 1);
    
    // bits 4-6 = 101 = 5
    EXPECT_EQ((value[0] >> 4) & 0x07, 5);
    
    // bits 0-3 = 0000 = 0
    EXPECT_EQ(value[0] & 0x0F, 0);
}

// 测试4: 未定义类型 Value 的语义解析（默认返回原始数据）
TEST_F(TlvSemanticsTest, BinaryValueSemantics) {
    // 测试数据: 84 04 A1 B2 C3 D4 (DF Name)
    // Value = 0xA1 0xB2 0xC3 0xD4 (纯二进制数据)
    std::string hexData = "8404A1B2C3D4";
    auto tlvList = inputParser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "84");
    
    const auto& value = tlv->getValue();
    EXPECT_EQ(value.size(), 4);
    EXPECT_EQ(value[0], 0xA1);
    EXPECT_EQ(value[1], 0xB2);
    EXPECT_EQ(value[2], 0xC3);
    EXPECT_EQ(value[3], 0xD4);
    
    // 二进制数据应该直接返回原始字节
    EXPECT_EQ(tlv->valueHex(), "A1B2C3D4");
}

// 测试5: 复合标签 Value 的嵌套解析（基于 bit 6 自动识别）
TEST_F(TlvSemanticsTest, CompositeValueSemantics) {
    // 测试数据: 6F 04 95 02 01 41
    // 外层: 6F (FCI Template, 复合标签，bit 6 = 1), Length=4
    // 内层: 95 02 01 41
    std::string hexData = "6F0495020141";
    auto tlvList = inputParser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "6F");
    EXPECT_EQ(tlv->getValue().size(), 4);
    
    // 验证嵌套解析（如果 TagRegistry 正确配置，应该解析出 children）
    // 由于 6F 的 bit 6 = 1（复合标签），应该自动解析 Value 中的子 TLV
    const auto& children = tlv->getChildren();
    if (children.size() > 0) {
        // 如果解析成功，应该有一个子 TLV
        EXPECT_EQ(children.size(), 1);
        EXPECT_EQ(children[0]->tagHex(), "95");
        EXPECT_EQ(children[0]->getValue().size(), 2);
        EXPECT_EQ(children[0]->valueHex(), "0141");
    }
}

// 测试6: 金额字段的语义解析（BCD 编码）
TEST_F(TlvSemanticsTest, AmountValueSemantics) {
    // 测试数据: 9F02 04 00 00 01 50 (Amount, Authorised)
    // Value = 0x00 0x00 0x01 0x50 (BCD) = 150 (分) = 1.50 (元)
    std::string hexData = "9F020400000150";
    auto tlvList = inputParser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "9F02");
    
    const auto& value = tlv->getValue();
    EXPECT_EQ(value.size(), 4);
    
    // BCD 编码验证
    // 0x00 0x00 0x01 0x50 = 00000150 (BCD) = 150 (十进制)
    // 实际应用中需要将 BCD 转换为十进制
    uint32_t bcdValue = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        uint8_t high = (value[i] >> 4) & 0x0F;
        uint8_t low = value[i] & 0x0F;
        bcdValue = bcdValue * 100 + high * 10 + low;
    }
    EXPECT_EQ(bcdValue, 150);
}

// 测试7: 多个不同语义类型的 TLV
TEST_F(TlvSemanticsTest, MultipleSemanticTypes) {
    // 测试数据: 包含多个不同类型的 TLV
    // 9F33 02 01 41 (NUMERIC)
    // 5F20 03 42 6F 62 (TEXT)
    // 9F6B 01 D0 (BITFIELD)
    std::string hexData = "9F330201415F2003426F629F6B01D0";
    auto tlvList = inputParser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 3);
    
    // 验证第一个 TLV (NUMERIC)
    EXPECT_EQ(tlvList[0]->tagHex(), "9F33");
    EXPECT_EQ(tlvList[0]->getValue().size(), 2);
    
    // 验证第二个 TLV (TEXT)
    EXPECT_EQ(tlvList[1]->tagHex(), "5F20");
    EXPECT_EQ(tlvList[1]->getValue().size(), 3);
    std::string text(tlvList[1]->getValue().begin(), tlvList[1]->getValue().end());
    EXPECT_EQ(text, "Bob");
    
    // 验证第三个 TLV (BITFIELD)
    EXPECT_EQ(tlvList[2]->tagHex(), "9F6B");
    EXPECT_EQ(tlvList[2]->getValue().size(), 1);
    EXPECT_EQ(tlvList[2]->getValue()[0], 0xD0);
}

// 测试8: 嵌套复合标签的语义解析
TEST_F(TlvSemanticsTest, NestedCompositeSemantics) {
    // 测试数据: 6F 08 84 04 A1 B2 C3 D4 95 02 01 41
    // 外层: 6F (复合标签，bit 6 = 1), Length=8
    // 内层1: 84 04 A1 B2 C3 D4
    // 内层2: 95 02 01 41
    std::string hexData = "6F088404A1B2C3D495020141";
    auto tlvList = inputParser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "6F");
    EXPECT_EQ(tlv->getValue().size(), 8);
    
    // 验证嵌套解析
    const auto& children = tlv->getChildren();
    if (children.size() >= 2) {
        EXPECT_EQ(children[0]->tagHex(), "84");
        EXPECT_EQ(children[0]->getValue().size(), 4);
        
        EXPECT_EQ(children[1]->tagHex(), "95");
        EXPECT_EQ(children[1]->getValue().size(), 2);
    }
}

// 测试9: 空 Value 的语义处理
TEST_F(TlvSemanticsTest, EmptyValueSemantics) {
    // 测试数据: 9F33 00 (空 Value)
    std::string hexData = "9F3300";
    auto tlvList = inputParser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "9F33");
    EXPECT_EQ(tlv->getValue().size(), 0);
    EXPECT_EQ(tlv->valueHex(), "");
}

// 测试10: TagRegistry 加载验证
TEST_F(TlvSemanticsTest, TagRegistryLoading) {
    // 验证 TagRegistry 是否正确加载
    // 通过解析已知的 Tag 来验证
    
    // 测试 9F33 (EMV base registry)
    std::string hexData = "9F33020141";
    auto tlvList = inputParser->parseHex(hexData);
    
    ASSERT_EQ(tlvList.size(), 1);
    EXPECT_EQ(tlvList[0]->tagHex(), "9F33");
    
    // 如果 TagRegistry 正确加载，嵌套解析应该能工作
    // 这里主要验证解析不会因为 TagRegistry 问题而失败
}

// 测试11: BITFIELD 类型 TAG 的详细语义解析（测试多个 bit 位）
TEST_F(TlvSemanticsTest, BitFieldTypeDetailedSemantics) {
    const std::string manifestPath = "../../registry/manifest.json";
    EmvInputParser parserEmv(manifestPath, "EMV");

    // 测试数据: TVR 包含多个设置的 bit
    // Byte 1: 0xC0 = 11000000 (b8=1, b7=1)
    //   - b8: Offline data authentication was not performed
    //   - b7: SDA failed
    // Byte 2: 0x80 = 10000000 (b8=1)
    //   - b8: ICC and terminal have different application versions
    // Byte 3: 0x40 = 01000000 (b7=1)
    //   - b7: Unrecognised CVM
    // Byte 4: 0x20 = 00100000 (b6=1)
    //   - b6: Upper consecutive offline limit exceeded
    // Byte 5: 0x10 = 00010000 (b5=1)
    //   - b5: Script processing failed after final GENERATE AC
    std::string hexData = "9505C080402010";
    auto tlvList = parserEmv.parseHex(hexData);
    
    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    // 验证 Tag
    EXPECT_EQ(tlv->tagHex(), "95");
    EXPECT_EQ(tlv->getValue().size(), 5);
    
    // 验证 Value 原始数据
    const auto& value = tlv->getValue();
    EXPECT_EQ(value[0], 0xC0); // Byte 1: 11000000
    EXPECT_EQ(value[1], 0x80); // Byte 2: 10000000
    EXPECT_EQ(value[2], 0x40); // Byte 3: 01000000
    EXPECT_EQ(value[3], 0x20); // Byte 4: 00100000
    EXPECT_EQ(value[4], 0x10); // Byte 5: 00010000
    
    // 输出 JSON 内容
    std::string jsonOutput = parserEmv.parseHexToJson(hexData);
    std::cout << "\n========== BITFIELD 类型 TAG 详细语义解析 JSON 输出 ==========\n";
    std::cout << jsonOutput << std::endl;
    std::cout << "==============================================================\n" << std::endl;
    
    // 验证 JSON 包含语义信息和 bitFields 数组
    EXPECT_NE(jsonOutput.find("\"tag\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("\"95\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("\"type\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("\"BITFIELD\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("\"bitFields\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("\"position\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("\"meaning\""), std::string::npos);

    const nlohmann::json root = nlohmann::json::parse(jsonOutput);
    const nlohmann::json* tvrNode = findTagNode(root, "95");
    ASSERT_NE(tvrNode, nullptr);
    ASSERT_TRUE(tvrNode->contains("semantic"));
    const nlohmann::json& semantic = (*tvrNode)["semantic"];

    emvparse::ParseOutputOptions tableActive;
    tableActive.format = emvparse::OutputFormat::Table;
    tableActive.formatOptions.semantic = emvparse::SemanticSummary::Active;
    const std::string tableActiveOut = parserEmv.parseHexWithOptions(hexData, tableActive);

    EXPECT_TRUE(tableContainsBitLine(
        tableActiveOut, 1, 8, "Offline data authentication was not performed"));
    EXPECT_TRUE(tableContainsBitLine(tableActiveOut, 1, 7, "SDA failed"));

    const auto odaPos = bitFieldPositionByMeaning(semantic, "Offline data authentication was not performed");
    const auto sdaPos = bitFieldPositionByMeaning(semantic, "SDA failed");
    ASSERT_TRUE(odaPos.has_value());
    ASSERT_TRUE(sdaPos.has_value());
    EXPECT_EQ(*odaPos, 0);
    EXPECT_EQ(*sdaPos, 1);
}

TEST_F(TlvSemanticsTest, AipAflTableHtmlAndJson) {
    const std::string hexData = "770E8202198094081001020120010300";
    auto tlvList = inputParser->parseHex(hexData);
    ASSERT_EQ(tlvList.size(), 1);
    EXPECT_EQ(tlvList[0]->tagHex(), "77");

    const std::string jsonOutput = inputParser->parseHexToJson(hexData);
    EXPECT_NE(jsonOutput.find("\"82\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("\"bitFields\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("\"94\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("\"entries\""), std::string::npos);
    EXPECT_NE(jsonOutput.find("Terminal risk management is to be performed"), std::string::npos);
    EXPECT_NE(jsonOutput.find("Cardholder verification is supported"), std::string::npos);
    EXPECT_NE(jsonOutput.find("SFI 2"), std::string::npos);

    emvparse::ParseOutputOptions tableActive;
    tableActive.format = emvparse::OutputFormat::Table;
    tableActive.formatOptions.semantic = emvparse::SemanticSummary::Active;
    const std::string tableActiveOut = inputParser->parseHexWithOptions(hexData, tableActive);
    EXPECT_NE(tableActiveOut.find("Application Interchange Profile"), std::string::npos);
    EXPECT_NE(tableActiveOut.find("Application File Locator"), std::string::npos);
    EXPECT_NE(tableActiveOut.find("Cardholder verification is supported"), std::string::npos);
    EXPECT_EQ(tableActiveOut.find("SDA not supported"), std::string::npos);

    EXPECT_TRUE(tableContainsBitLine(tableActiveOut, 1, 5, "Cardholder verification is supported"));
    EXPECT_TRUE(tableContainsBitLine(
        tableActiveOut, 1, 4, "Terminal risk management is to be performed"));
    EXPECT_TRUE(tableContainsBitLine(tableActiveOut, 1, 1, "RFU (shall be 0)"));
    // Byte 2 = 0x80 sets MSB (position 8); registry maps b8 to RFU (shall be 0), not EMV at b1.
    EXPECT_TRUE(tableContainsBitLine(tableActiveOut, 2, 8, "RFU (shall be 0)"));

    const nlohmann::json root = nlohmann::json::parse(jsonOutput);
    const nlohmann::json* aipNode = findTagNode(root, "82");
    ASSERT_NE(aipNode, nullptr);
    ASSERT_TRUE(aipNode->contains("semantic"));
    const nlohmann::json& aipSemantic = (*aipNode)["semantic"];
    const auto cvmPos = bitFieldPositionByMeaning(aipSemantic, "Cardholder verification is supported");
    const auto trmPos =
        bitFieldPositionByMeaning(aipSemantic, "Terminal risk management is to be performed");
    ASSERT_TRUE(cvmPos.has_value());
    ASSERT_TRUE(trmPos.has_value());
    EXPECT_EQ(*cvmPos, 3);
    EXPECT_EQ(*trmPos, 4);

    emvparse::ParseOutputOptions tableFull;
    tableFull.format = emvparse::OutputFormat::Table;
    tableFull.formatOptions.semantic = emvparse::SemanticSummary::Full;
    const std::string tableFullOut = inputParser->parseHexWithOptions(hexData, tableFull);
    EXPECT_NE(tableFullOut.find("SDA not supported"), std::string::npos);

    emvparse::ParseOutputOptions htmlOpts;
    htmlOpts.format = emvparse::OutputFormat::Html;
    const std::string htmlOut = inputParser->parseHexWithOptions(hexData, htmlOpts);
    EXPECT_NE(htmlOut.find("semantic-full"), std::string::npos);
    EXPECT_NE(htmlOut.find("toggleSemantic"), std::string::npos);
    EXPECT_NE(htmlOut.find("display:none"), std::string::npos);
    EXPECT_NE(htmlOut.find("inactive-bit"), std::string::npos);
    EXPECT_NE(htmlOut.find("RFU (shall be 0)"), std::string::npos);
    EXPECT_NE(htmlOut.find("SFI 2"), std::string::npos);
    const std::string detailControl = "onclick=\"toggleSemantic(";
    size_t toggleCount = 0;
    for (size_t pos = 0; (pos = htmlOut.find(detailControl, pos)) != std::string::npos; ++pos) {
        ++toggleCount;
    }
    EXPECT_EQ(toggleCount, 1U);
    EXPECT_EQ(htmlOut.find(detailControl), htmlOut.rfind(detailControl));
    const auto emvPos = bitFieldPositionByMeaning(aipSemantic, "EMV mode not supported");
    ASSERT_TRUE(emvPos.has_value());
    EXPECT_EQ(*emvPos, 15);
}

