#include <gtest/gtest.h>
#include "tlv_parser.h"
#include "tlv.h"
#include "tlv_tree_formatter.h"
#include "tlv_formatter.h"
#include "logger.h"
#include <emvparse/output_options.h>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <functional>
#include <fstream>
#include <mutex>
#include <sstream>

/**
 * @brief TLV 数据解析结构测试
 * 
 * 测试 TLV 格式解析的正确性，包括：
 * - Tag 解析（单字节和多字节）
 * - Length 解析
 * - Value 解析
 * - 嵌套 TLV 解析
 * - Tag 字节数量判断
 * - 复合标签判断
 */
class TlvParserStructureTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<TLVParser>();
    }

    std::unique_ptr<TLVParser> parser;
};

// 测试1: 单字节 Tag 解析
TEST_F(TlvParserStructureTest, SingleByteTag) {
    // 测试数据: Tag=0x95 (单字节), Length=0x02, Value=0x01 0x41
    // Hex: 95 02 01 41
    std::string hexData = "95020141";
    auto tlvList = parser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    // 验证 Tag
    EXPECT_EQ(tlv->getTag().size(), 1);
    EXPECT_EQ(tlv->getTag()[0], 0x95);
    EXPECT_EQ(tlv->tagHex(), "95");
    
    // 验证 Length
    EXPECT_EQ(tlv->getValue().size(), 2);
    
    // 验证 Value
    EXPECT_EQ(tlv->getValue()[0], 0x01);
    EXPECT_EQ(tlv->getValue()[1], 0x41);
    EXPECT_EQ(tlv->valueHex(), "0141");
}

// 测试2: 双字节 Tag 解析
TEST_F(TlvParserStructureTest, TwoByteTag) {
    // 测试数据: Tag=0x9F33 (双字节), Length=0x02, Value=0x01 0x41
    // Hex: 9F 33 02 01 41
    std::string hexData = "9F33020141";
    auto tlvList = parser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    // 验证 Tag
    EXPECT_EQ(tlv->getTag().size(), 2);
    EXPECT_EQ(tlv->getTag()[0], 0x9F);
    EXPECT_EQ(tlv->getTag()[1], 0x33);
    EXPECT_EQ(tlv->tagHex(), "9F33");
    
    // 验证 Length
    EXPECT_EQ(tlv->getValue().size(), 2);
    
    // 验证 Value
    EXPECT_EQ(tlv->getValue()[0], 0x01);
    EXPECT_EQ(tlv->getValue()[1], 0x41);
    EXPECT_EQ(tlv->valueHex(), "0141");
}

// 测试3: 多个 TLV 解析
TEST_F(TlvParserStructureTest, MultipleTLV) {
    // 测试数据: 两个连续的 TLV
    // TLV1: 95 02 01 41
    // TLV2: 9F33 02 01 42
    std::string hexData = "950201419F33020142";
    auto tlvList = parser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 2);
    
    // 验证第一个 TLV
    EXPECT_EQ(tlvList[0]->tagHex(), "95");
    EXPECT_EQ(tlvList[0]->getValue().size(), 2);
    EXPECT_EQ(tlvList[0]->valueHex(), "0141");
    
    // 验证第二个 TLV
    EXPECT_EQ(tlvList[1]->tagHex(), "9F33");
    EXPECT_EQ(tlvList[1]->getValue().size(), 2);
    EXPECT_EQ(tlvList[1]->valueHex(), "0142");
}

// 测试4: 空 Value 解析
TEST_F(TlvParserStructureTest, EmptyValue) {
    // 测试数据: Tag=0x95, Length=0x00 (空值)
    // Hex: 95 00
    std::string hexData = "9500";
    auto tlvList = parser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "95");
    EXPECT_EQ(tlv->getValue().size(), 0);
    EXPECT_EQ(tlv->valueHex(), "");
}

// Long-form BER-TLV length (e.g. READ RECORD response 70 81 B8 ...)
TEST_F(TlvParserStructureTest, LongFormLength81) {
    const std::vector<uint8_t> data = {0x70, 0x81, 0x04, 0x95, 0x02, 0x01, 0x01, 0x00};
    auto tlvList = parser->parseBytes(data);
    ASSERT_EQ(tlvList.size(), 1u);
    EXPECT_EQ(tlvList[0]->tagHex(), "70");
    EXPECT_EQ(tlvList[0]->getValue().size(), 4u);
    ASSERT_EQ(tlvList[0]->getChildren().size(), 1u);
    EXPECT_EQ(tlvList[0]->getChildren()[0]->tagHex(), "95");
}

// 测试5: 长 Value 解析
TEST_F(TlvParserStructureTest, LongValue) {
    // 测试数据: Tag=0x95, Length=0x05, Value=0x01 0x02 0x03 0x04 0x05
    // Hex: 95 05 01 02 03 04 05
    std::string hexData = "95050102030405";
    auto tlvList = parser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "95");
    EXPECT_EQ(tlv->getValue().size(), 5);
    EXPECT_EQ(tlv->valueHex(), "0102030405");
    
    // 验证每个字节
    const auto& value = tlv->getValue();
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(value[i], static_cast<uint8_t>(i + 1));
    }
}

// 测试6: 嵌套 TLV 解析（基于 EMV bit 6 规则）
TEST_F(TlvParserStructureTest, NestedTLV) {
    // 测试数据: 外层 Tag=0x6F (bit 6=1, 复合标签), 内层包含 95 02 01 41
    // 外层: 6F 04 (Length=4)
    // 内层: 95 02 01 41
    // Hex: 6F 04 95 02 01 41
    // 0x6F = 0110 1111, bit 6 = 1 → 复合标签，应该自动解析嵌套 TLV
    std::string hexData = "6F0495020141";
    
    std::cout << "\n[DEBUG] 测试数据: " << hexData << std::endl;
    std::cout << "[DEBUG] 数据长度: " << hexData.length() << " 字符" << std::endl;
    
    // 步骤1: 测试 hexStringToBytes 转换
    std::vector<uint8_t> testBytes;
    for (size_t i = 0; i < hexData.length(); i += 2) {
        std::string byteStr = hexData.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
        testBytes.push_back(byte);
    }
    std::cout << "[DEBUG] 转换后的字节: ";
    for (uint8_t b : testBytes) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << std::endl;
    std::cout << "[DEBUG] 字节数量: " << testBytes.size() << std::endl;
    
    // 步骤2: 测试 parseHex
    std::cout << "[DEBUG] 调用 parseHex..." << std::endl;
    auto tlvList = parser->parseHex(hexData);
    std::cout << "[DEBUG] parseHex 返回的 TLV 数量: " << tlvList.size() << std::endl;
    
    if (tlvList.empty()) {
        // 如果解析失败，尝试直接解析字节数组
        std::cout << "[DEBUG] parseHex 返回空列表，尝试直接使用 parseBytes()" << std::endl;
        std::cout << "[DEBUG] 使用手动转换的字节数组，大小: " << testBytes.size() << std::endl;
        tlvList = parser->parseBytes(testBytes);
        std::cout << "[DEBUG] parse() 返回的 TLV 数量: " << tlvList.size() << std::endl;
        
        if (tlvList.empty()) {
            std::cout << "[DEBUG] 解析完全失败！" << std::endl;
            std::cout << "[DEBUG] 检查数据: ";
            for (uint8_t b : testBytes) {
                std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b) << " ";
            }
            std::cout << std::dec << std::endl;
            if (testBytes.size() >= 2) {
                std::cout << "[DEBUG] 数据解析: Tag=" << std::hex << static_cast<int>(testBytes[0]) 
                          << ", Length=" << static_cast<int>(testBytes[1]) 
                          << ", 期望Value长度=" << static_cast<int>(testBytes[1]) 
                          << ", 实际数据长度=" << testBytes.size() << std::dec << std::endl;
            }
        }
    }

    ASSERT_EQ(tlvList.size(), 1) << "应该解析出 1 个 TLV";
    auto tlv = tlvList[0];
    
    std::cout << "[DEBUG] TLV Tag: " << tlv->tagHex() << std::endl;
    std::cout << "[DEBUG] TLV Value 长度: " << tlv->getValue().size() << std::endl;
    std::cout << "[DEBUG] TLV Value: " << tlv->valueHex() << std::endl;
    
        EXPECT_EQ(tlv->tagHex(), "6F") << "Tag 应该是 6F";
    EXPECT_EQ(tlv->getValue().size(), 4) << "Value 长度应该是 4 字节";
    
    // 由于 0x6F 的 bit 6 = 1（复合标签），应该自动解析嵌套 TLV
    const auto& children = tlv->getChildren();
    std::cout << "[DEBUG] 嵌套 TLV 数量: " << children.size() << std::endl;
    
    EXPECT_EQ(children.size(), 1) << "应该解析出 1 个嵌套 TLV";
    
    if (children.size() > 0) {
        std::cout << "[DEBUG] 嵌套 TLV Tag: " << children[0]->tagHex() << std::endl;
        std::cout << "[DEBUG] 嵌套 TLV Value: " << children[0]->valueHex() << std::endl;
        EXPECT_EQ(children[0]->tagHex(), "95") << "嵌套 TLV 的 Tag 应该是 95";
        EXPECT_EQ(children[0]->getValue().size(), 2) << "嵌套 TLV 的 Value 长度应该是 2 字节";
        EXPECT_EQ(children[0]->valueHex(), "0141") << "嵌套 TLV 的 Value 应该是 0141";
    }
}

// 测试7: 字节流解析
TEST_F(TlvParserStructureTest, ByteArrayParse) {
    // 使用字节数组而不是 Hex 字符串
    std::vector<uint8_t> data = {0x95, 0x02, 0x01, 0x41};
    auto tlvList = parser->parseBytes(data);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "95");
    EXPECT_EQ(tlv->getValue().size(), 2);
    EXPECT_EQ(tlv->valueHex(), "0141");
}

// 测试8: 带空格的 Hex 字符串解析
TEST_F(TlvParserStructureTest, HexStringWithSpaces) {
    // 测试带空格的 Hex 字符串
    std::string hexData = "95 02 01 41";
    auto tlvList = parser->parseHex(hexData);

    ASSERT_EQ(tlvList.size(), 1);
    auto tlv = tlvList[0];
    
    EXPECT_EQ(tlv->tagHex(), "95");
    EXPECT_EQ(tlv->valueHex(), "0141");
}

// 测试9: 无效数据测试
TEST_F(TlvParserStructureTest, InvalidData) {
    // 测试数据不完整的情况
    std::string hexData = "95"; // 只有 Tag，没有 Length
    auto tlvList = parser->parseHex(hexData);
    EXPECT_EQ(tlvList.size(), 0);
    
    // 测试 Length 大于实际数据的情况
    hexData = "9505"; // Length=5，但没有 Value
    tlvList = parser->parseHex(hexData);
    EXPECT_EQ(tlvList.size(), 0);
    
    // 测试无效的 Hex 字符
    hexData = "95 02 01 GZ"; // GZ 不是有效的 Hex
    tlvList = parser->parseHex(hexData);
    EXPECT_EQ(tlvList.size(), 0);
}

// 测试10: 边界值测试
TEST_F(TlvParserStructureTest, BoundaryValues) {
    // 测试最大单字节 Length (0x7F = 127)
    std::vector<uint8_t> data;
    data.push_back(0x95);
    data.push_back(0x7F); // Length = 127
    for (int i = 0; i < 127; ++i) {
        data.push_back(static_cast<uint8_t>(i % 256));
    }
    
    auto tlvList = parser->parseBytes(data);
    ASSERT_EQ(tlvList.size(), 1);
    EXPECT_EQ(tlvList[0]->getValue().size(), 127);
}

// 测试11: Tag 字节数量判断
TEST_F(TlvParserStructureTest, TagByteCount) {
    // 测试单字节 Tag（低5位 ≠ 0x1F）
    // 0x95: 低5位 = 0x15 ≠ 0x1F → 单字节
    std::string hexData1 = "95020141";
    auto tlvList1 = parser->parseHex(hexData1);
    ASSERT_EQ(tlvList1.size(), 1);
    EXPECT_EQ(tlvList1[0]->getTag().size(), 1) << "0x95 应该是单字节Tag";
    EXPECT_EQ(tlvList1[0]->getTag()[0], 0x95);
    
    // 0x6F: 低5位 = 0x0F ≠ 0x1F → 单字节
    std::string hexData2 = "6F0401020304";
    auto tlvList2 = parser->parseHex(hexData2);
    ASSERT_EQ(tlvList2.size(), 1);
    EXPECT_EQ(tlvList2[0]->getTag().size(), 1) << "0x6F 应该是单字节Tag";
    EXPECT_EQ(tlvList2[0]->getTag()[0], 0x6F);
    
    // 测试双字节 Tag（低5位 = 0x1F，第二个字节 b8=0）
    // 0x9F33: 第一个字节低5位 = 0x1F，第二个字节 0x33 的 b8=0 → 双字节
    std::string hexData3 = "9F33020141";
    auto tlvList3 = parser->parseHex(hexData3);
    ASSERT_EQ(tlvList3.size(), 1);
    EXPECT_EQ(tlvList3[0]->getTag().size(), 2) << "0x9F33 应该是双字节Tag";
    EXPECT_EQ(tlvList3[0]->getTag()[0], 0x9F);
    EXPECT_EQ(tlvList3[0]->getTag()[1], 0x33);
    
    // 测试三字节 Tag（低5位 = 0x1F，第二个字节 b8=1，第三个字节 b8=0）
    // 0xDF 81 20: 第一个字节低5位 = 0x1F，第二个字节 0x81 的 b8=1，第三个字节 0x20 的 b8=0 → 三字节
    std::vector<uint8_t> data4;
    data4.push_back(0xDF); // 第一个字节，低5位 = 0x1F
    data4.push_back(0x81); // 第二个字节，b8=1，继续读取
    data4.push_back(0x20); // 第三个字节，b8=0，停止
    data4.push_back(0x02); // Length
    data4.push_back(0x01); // Value
    data4.push_back(0x41);
    auto tlvList4 = parser->parseBytes(data4);
    ASSERT_EQ(tlvList4.size(), 1);
    EXPECT_EQ(tlvList4[0]->getTag().size(), 3) << "0xDF8120 应该是三字节Tag";
    EXPECT_EQ(tlvList4[0]->getTag()[0], 0xDF);
    EXPECT_EQ(tlvList4[0]->getTag()[1], 0x81);
    EXPECT_EQ(tlvList4[0]->getTag()[2], 0x20);
}

// 测试12: 复合标签判断
TEST_F(TlvParserStructureTest, CompositeTagDetection) {
    // 加载 TagRegistry 以便测试基于 TagInfo 的复合标签判断
    parser->loadTagRegistry("../../registry/manifest.json", "EMV");
    
    // 测试1: 基于 bit 6 判断的复合标签
    // 0x6F = 0110 1111, bit 6 = 1 → 复合标签
    std::string hexData1 = "6F0495020141";
    auto tlvList1 = parser->parseHex(hexData1);
    ASSERT_EQ(tlvList1.size(), 1);
    auto tlv1 = tlvList1[0];
    EXPECT_EQ(tlv1->tagHex(), "6F");
    // 验证嵌套 TLV 被解析（说明被识别为复合标签）
    EXPECT_GT(tlv1->getChildren().size(), 0) << "0x6F 是复合标签，应该解析出嵌套 TLV";
    if (tlv1->getChildren().size() > 0) {
        EXPECT_EQ(tlv1->getChildren()[0]->tagHex(), "95");
    }
    
    // 测试2: 基于 bit 6 的复合标签判断（0x6F 的 bit 6 = 1）
    // 0x6F = 0110 1111, bit 6 = 1 → 复合标签
    std::vector<uint8_t> tag2 = {0x6F};
    // 通过解析包含该 Tag 的 TLV 来验证
    std::string hexData2 = "6F0495020141";
    auto tlvList2 = parser->parseHex(hexData2);
    ASSERT_EQ(tlvList2.size(), 1);
    EXPECT_GT(tlvList2[0]->getChildren().size(), 0) << "0x6F 的 bit 6 = 1，应该是复合标签";
    
    // 测试3: 基本标签（bit 6 = 0）
    // 0x95 = 1001 0101, bit 6 = 0 → 基本标签
    std::string hexData3 = "95020141";
    auto tlvList3 = parser->parseHex(hexData3);
    ASSERT_EQ(tlvList3.size(), 1);
    auto tlv3 = tlvList3[0];
    EXPECT_EQ(tlv3->tagHex(), "95");
    // 验证没有嵌套 TLV（说明被识别为基本标签）
    EXPECT_EQ(tlv3->getChildren().size(), 0) << "0x95 是基本标签，不应该有嵌套 TLV";
    
    // 测试4: 双字节基本标签
    // 0x9F33 = 1001 1111 0011 0011, 第一个字节 bit 6 = 0 → 基本标签
    std::string hexData4 = "9F33020141";
    auto tlvList4 = parser->parseHex(hexData4);
    ASSERT_EQ(tlvList4.size(), 1);
    auto tlv4 = tlvList4[0];
    EXPECT_EQ(tlv4->tagHex(), "9F33");
    // 验证没有嵌套 TLV
    EXPECT_EQ(tlv4->getChildren().size(), 0) << "0x9F33 是基本标签，不应该有嵌套 TLV";
    
    // 测试5: 复合标签（bit 6 = 1，但不在 TagRegistry 中）
    // 0xE0 = 1110 0000, bit 6 = 1 → 复合标签
    std::string hexData5 = "E00495020141";
    auto tlvList5 = parser->parseHex(hexData5);
    ASSERT_EQ(tlvList5.size(), 1);
    auto tlv5 = tlvList5[0];
    EXPECT_EQ(tlv5->tagHex(), "E0");
    // 验证嵌套 TLV 被解析（说明通过 bit 6 判断为复合标签）
    EXPECT_GT(tlv5->getChildren().size(), 0) << "0xE0 的 bit 6 = 1，应该被识别为复合标签";
}

// 测试13: 解析复杂 TLV 数据结构和语义
TEST_F(TlvParserStructureTest, ComplexTLVStructureAndSemantic) {
    // 加载 TagRegistry 以便进行语义解析
    // 工作目录 build/test：../../registry/manifest.json
    bool loaded = parser->loadTagRegistry("../../registry/manifest.json", "EMV");
    ASSERT_TRUE(loaded) << "TagRegistry 加载失败，请检查文件路径";
    
    // TLV 数据: 6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000
    std::string hexData = "6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000";
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试13: 复杂 TLV 数据结构和语义解析" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "原始 Hex 数据: " << hexData << std::endl;
    std::cout << "数据长度: " << hexData.length() << " 字符 (" << hexData.length() / 2 << " 字节)" << std::endl;
    std::cout << std::endl;
    
    // 解析 TLV 数据
    auto tlvList = parser->parseHex(hexData);
    std::cout << "解析结果: 共 " << tlvList.size() << " 个顶级 TLV 对象" << std::endl;
    std::cout << std::endl;
    
    // 输出结构信息
    std::function<void(const std::shared_ptr<TLV>&, int)> printTLV = 
        [&printTLV](const std::shared_ptr<TLV>& tlv, int indent) {
            std::string indentStr(indent * 2, ' ');
            std::cout << indentStr << "Tag: " << tlv->tagHex() << std::endl;
            std::cout << indentStr << "Length: " << tlv->lengthHex() << std::endl;
            std::cout << indentStr << "Value (Hex): " << tlv->valueHex() << std::endl;
            
            const auto& children = tlv->getChildren();
            if (!children.empty()) {
                std::cout << indentStr << "Children (" << children.size() << "):" << std::endl;
                for (const auto& child : children) {
                    printTLV(child, indent + 1);
                }
            }
            std::cout << std::endl;
        };
    
    for (size_t i = 0; i < tlvList.size(); ++i) {
        std::cout << "--- TLV #" << (i + 1) << " ---" << std::endl;
        printTLV(tlvList[i], 0);
    }
    
    // 输出 JSON 格式（包含语义信息）
    std::cout << "========================================" << std::endl;
    std::cout << "JSON 格式输出（包含语义信息）:" << std::endl;
    std::cout << "========================================" << std::endl;
    std::string jsonOutput =
        TlvTreeFormatter::toJsonArray(tlvList, parser->tagRegistry());
    std::cout << jsonOutput << std::endl;
    std::cout << std::endl;
    
    // 基本验证
    ASSERT_GT(tlvList.size(), 0) << "应该至少解析出 1 个 TLV 对象";
    
    // 验证第一个 TLV 的 Tag（应该是 0x6F）
    if (tlvList.size() > 0) {
        EXPECT_EQ(tlvList[0]->tagHex(), "6F") << "第一个 TLV 的 Tag 应该是 6F";
        // 0x6F 是复合标签，应该有子元素
        EXPECT_GT(tlvList[0]->getChildren().size(), 0) << "0x6F 是复合标签，应该有子元素";
    }
}

// 测试14: 查询 Tag 信息
TEST_F(TlvParserStructureTest, GetTagInfo) {
    // 加载 TagRegistry
    bool loaded = parser->loadTagRegistry("../../registry/manifest.json", "EMV");
    ASSERT_TRUE(loaded) << "TagRegistry 加载失败，请检查文件路径";
    
    // 测试查询存在的 tag
    std::string tagInfoJson = parser->getTagInfo("9F33");
    ASSERT_FALSE(tagInfoJson.empty()) << "查询 9F33 tag 信息应该返回非空结果";
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试14: 查询 Tag 信息" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tag 9F33 的信息:" << std::endl;
    std::cout << tagInfoJson << std::endl;
    std::cout << std::endl;
    
    // 验证 JSON 包含基本字段
    // 注意：这里我们只验证 JSON 不为空，具体字段验证可以通过解析 JSON 进行
    EXPECT_NE(tagInfoJson.find("9F33"), std::string::npos) << "返回的 JSON 应该包含 tag 字段";
    
    // 测试查询另一个存在的 tag
    std::string tagInfoJson2 = parser->getTagInfo("6F");
    ASSERT_FALSE(tagInfoJson2.empty()) << "查询 6F tag 信息应该返回非空结果";
    std::cout << "Tag 6F 的信息:" << std::endl;
    std::cout << tagInfoJson2 << std::endl;
    std::cout << std::endl;
    
    // 测试查询不存在的 tag
    std::string tagInfoJson3 = parser->getTagInfo("FF00");
    EXPECT_TRUE(tagInfoJson3.empty()) << "查询不存在的 tag 应该返回空字符串";
    
    // 测试查询 95 tag（Terminal Verification Results）
    std::string tagInfoJson4 = parser->getTagInfo("95");
    ASSERT_FALSE(tagInfoJson4.empty()) << "查询 95 tag 信息应该返回非空结果";
    std::cout << "Tag 95 的信息:" << std::endl;
    std::cout << tagInfoJson4 << std::endl;
    std::cout << std::endl;
    
    // 验证返回的 JSON 包含 name、description、template 等字段
    EXPECT_NE(tagInfoJson4.find("name"), std::string::npos) << "返回的 JSON 应该包含 name 字段";
    EXPECT_NE(tagInfoJson4.find("description"), std::string::npos) << "返回的 JSON 应该包含 description 字段";
}

// 测试15: 加载外部项目的 Tag 规则文件（用于调试）
TEST_F(TlvParserStructureTest, LoadExternalTagRuleFile) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试15: 加载外部项目的 Tag 规则文件" << std::endl;
    std::cout << "========================================" << std::endl;
    
    const std::string manifestPath = "../../registry/manifest.json";
    
    std::cout << "尝试加载 manifest: " << manifestPath << " (scheme Visa)" << std::endl;
    
    bool fileExists = false;
    {
        std::ifstream testFile(manifestPath, std::ios::binary);
        fileExists = testFile.is_open() && testFile.good();
    }
    
    bool loaded = parser->loadTagRegistry(manifestPath, "Visa");
    
    if (loaded) {
        std::cout << "✓ 成功加载 Tag 规则文件!" << std::endl;
        
        // 验证加载的 Tag 数量
        // 通过查询一些已知的 Tag 来验证
        std::vector<std::string> testTags = {"9F33", "95", "6F", "9F01"};
        int foundCount = 0;
        
        for (const auto& tag : testTags) {
            std::string tagInfo = parser->getTagInfo(tag);
            if (!tagInfo.empty()) {
                foundCount++;
                std::cout << "  ✓ 找到 Tag: " << tag << std::endl;
            } else {
                std::cout << "  ✗ 未找到 Tag: " << tag << std::endl;
            }
        }
        
        std::cout << "找到的 Tag 数量: " << foundCount << "/" << testTags.size() << std::endl;
        
        // 验证至少找到一些 Tag
        EXPECT_GT(foundCount, 0) << "应该至少找到一些 Tag";
        
        // 详细输出一个 Tag 的信息作为示例
        std::string tagInfo = parser->getTagInfo("9F33");
        if (!tagInfo.empty()) {
            std::cout << "\nTag 9F33 的详细信息:" << std::endl;
            std::cout << tagInfo << std::endl;
        }
        
    } else {
        std::cout << "✗ 加载 Tag 规则文件失败!" << std::endl;
        std::cout << "可能的原因:" << std::endl;
        std::cout << "  1. 文件不存在或路径错误" << std::endl;
        std::cout << "  2. 文件权限问题" << std::endl;
        std::cout << "  3. JSON 格式错误" << std::endl;
        std::cout << "  4. 文件被其他程序锁定" << std::endl;
    }
    
    // 这个测试在文件不存在时不应该失败（因为这是外部路径）
    // 但如果文件存在，应该能够成功加载
    if (fileExists) {
        ASSERT_TRUE(loaded) << "文件存在但加载失败，请检查文件格式和权限";
    } else {
        std::cout << "注意: 文件不存在，跳过加载验证" << std::endl;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
}

// 测试16: 日志回调功能测试
TEST_F(TlvParserStructureTest, LogCallback) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "测试16: 日志回调功能测试" << std::endl;
    std::cout << "========================================" << std::endl;

    // 用于收集日志消息的结构
    struct LogCollector {
        mutable std::mutex mutex;  // mutable 允许在 const 方法中使用
        std::vector<std::pair<LogLevel, std::string>> logs;

        void clear() {
            std::lock_guard<std::mutex> lock(mutex);
            logs.clear();
        }

        void add(LogLevel level, const std::string& message) {
            std::lock_guard<std::mutex> lock(mutex);
            logs.push_back({level, message});
        }

        size_t count() const {
            std::lock_guard<std::mutex> lock(mutex);
            return logs.size();
        }

        bool hasLevel(LogLevel level) const {
            std::lock_guard<std::mutex> lock(mutex);
            for (const auto& log : logs) {
                if (log.first == level) {
                    return true;
                }
            }
            return false;
        }

        std::vector<std::string> getMessages(LogLevel level) const {
            std::lock_guard<std::mutex> lock(mutex);
            std::vector<std::string> result;
            for (const auto& log : logs) {
                if (log.first == level) {
                    result.push_back(log.second);
                }
            }
            return result;
        }
    };

    LogCollector collector;
    Logger logger;

    // 定义日志回调函数
    auto logCallback = [&collector](LogLevel level, const std::string& message) {
        collector.add(level, message);
        // 同时输出到控制台，方便调试
        const char* levelStr = (level == LogLevel::ERROR) ? "ERROR" :
                               (level == LogLevel::WARNING) ? "WARNING" : "INFO";
        std::cout << "[Callback] [" << levelStr << "] " << message << std::endl;
    };

    parser->setLogger(&logger);
    logger.setLogCallback(logCallback);

    // 测试1: 加载存在的 Tag 规则文件（应该产生 INFO 日志）
    collector.clear();
    bool loaded = parser->loadTagRegistry("../../registry/manifest.json", "EMV");
    ASSERT_TRUE(loaded) << "应该成功加载 Tag 规则文件";

    // 验证回调被调用（应该至少有一些日志）
    size_t logCount = collector.count();
    EXPECT_GT(logCount, 0) << "日志回调应该被调用";

    // 验证有 INFO 级别的日志
    EXPECT_TRUE(collector.hasLevel(LogLevel::INFO)) << "应该有 INFO 级别的日志";

    // 输出收集到的日志统计
    std::cout << "\n收集到的日志统计:" << std::endl;
    std::cout << "  总日志数: " << logCount << std::endl;
    std::cout << "  INFO: " << collector.getMessages(LogLevel::INFO).size() << " 条" << std::endl;
    std::cout << "  WARNING: " << collector.getMessages(LogLevel::WARNING).size() << " 条" << std::endl;
    std::cout << "  ERROR: " << collector.getMessages(LogLevel::ERROR).size() << " 条" << std::endl;

    // 输出一些示例日志
    if (logCount > 0) {
        std::cout << "\n示例日志内容:" << std::endl;
        auto infoLogs = collector.getMessages(LogLevel::INFO);
        for (size_t i = 0; i < std::min(size_t(3), infoLogs.size()); ++i) {
            std::cout << "  " << infoLogs[i] << std::endl;
        }
    }

    // 测试2: 尝试加载不存在的文件（应该产生 ERROR 日志）
    collector.clear();
    bool failed = parser->loadTagRegistry("nonexistent_manifest.json", "EMV");
    ASSERT_FALSE(failed) << "加载不存在的文件应该失败";

    // 验证有 ERROR 级别的日志
    EXPECT_TRUE(collector.hasLevel(LogLevel::ERROR)) << "加载失败时应该有 ERROR 级别的日志";

    // 验证 ERROR 日志包含错误信息
    auto errorLogs = collector.getMessages(LogLevel::ERROR);
    EXPECT_GT(errorLogs.size(), 0) << "应该有 ERROR 日志";
    if (!errorLogs.empty()) {
        // 检查错误日志是否包含相关关键词
        bool hasErrorKeyword = false;
        for (const auto& msg : errorLogs) {
            if (msg.find("Error") != std::string::npos || 
                msg.find("Failed") != std::string::npos ||
                msg.find("not exist") != std::string::npos) {
                hasErrorKeyword = true;
                break;
            }
        }
        EXPECT_TRUE(hasErrorKeyword) << "ERROR 日志应该包含错误相关信息";
    }

    std::cout << "\n错误日志示例:" << std::endl;
    for (const auto& msg : errorLogs) {
        std::cout << "  " << msg << std::endl;
    }

    // 测试3: 禁用日志回调（传入 nullptr）
    collector.clear();
    logger.setLogCallback({});
    
    // 再次加载文件，应该不会调用回调
    loaded = parser->loadTagRegistry("../../registry/manifest.json", "EMV");
    ASSERT_TRUE(loaded) << "应该成功加载 Tag 规则文件";
    
    // 验证回调没有被调用（或者调用但使用默认的 std::cerr 输出）
    // 注意：禁用回调后，日志会输出到 std::cerr，但不会调用我们的回调函数
    // 所以 collector 应该是空的
    EXPECT_EQ(collector.count(), 0) << "禁用回调后，回调函数不应该被调用";

    // 测试4: 重新设置回调并验证正常工作
    logger.setLogCallback(logCallback);
    collector.clear();
    loaded = parser->loadTagRegistry("../../registry/manifest.json", "EMV");
    ASSERT_TRUE(loaded) << "应该成功加载 Tag 规则文件";
    EXPECT_GT(collector.count(), 0) << "重新设置回调后，应该能正常接收日志";

    std::cout << "\n========================================" << std::endl;
    std::cout << "日志回调功能测试完成" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
}

TEST(TlvTreeTextTest, CompactCdolOmitsRawHexOnTagLine) {
    TLVParser parser;
    ASSERT_TRUE(parser.loadTagRegistry("../../registry/manifest.json", "EMV"));
    const auto roots = parser.parseHex("8C049F02069F0306");
    ASSERT_EQ(roots.size(), 1u);

    emvparse::FormatOptions compact;
    compact.tree = emvparse::TreeStyle::Compact;
    const std::string tree = TLVFormatter::toTreeText(roots, parser.tagRegistry(), compact);
    EXPECT_EQ(tree.find("9F02069F0306"), std::string::npos);
    EXPECT_NE(tree.find("9F02"), std::string::npos);
    EXPECT_NE(tree.find("->"), std::string::npos);
}

TEST(TlvTreeTextTest, CompactOmitsPrimitiveHexVerboseShows) {
    TLVParser parser;
    ASSERT_TRUE(parser.loadTagRegistry("../../registry/manifest.json", "EMV"));
    const auto roots = parser.parseHex("95020141");
    ASSERT_EQ(roots.size(), 1u);

    emvparse::FormatOptions compact;
    compact.tree = emvparse::TreeStyle::Compact;
    const std::string compactTree = TLVFormatter::toTreeText(roots, parser.tagRegistry(), compact);
    EXPECT_NE(compactTree.find("95"), std::string::npos);
    EXPECT_EQ(compactTree.find("  0141\n"), std::string::npos);

    emvparse::FormatOptions verbose;
    verbose.tree = emvparse::TreeStyle::Verbose;
    const std::string verboseTree = TLVFormatter::toTreeText(roots, parser.tagRegistry(), verbose);
    EXPECT_NE(verboseTree.find("  0141"), std::string::npos);
}

