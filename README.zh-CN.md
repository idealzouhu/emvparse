# emvparse

**语言：** [English](README.md) · 简体中文

EMV BER-TLV 解析库与命令行工具：默认识别 **纯 TLV**、**C-APDU**、**R-APDU**，并按 Tag / APDU Registry 输出结构化结果。

## CLI 快速上手

```bash
emvparse --version

# 在仓库根目录执行。Tag + APDU 命令名依赖 manifest（含 apdu-commands.json）。
# 等价：export EMVPARSE_REGISTRY="$PWD/registry/manifest.json"
REG="--registry registry/manifest.json"

# --format json | table | html  （默认 json；html 写文件，常用 -o report.html）
# --input auto | tlv | capdu | rapdu  （默认 auto）
# 可选 --scheme EMV|Visa|UPI

# 纯 TLV（例如 GPO 响应模板 77…）
emvparse parse 770E8202198094081001020120010300 $REG --format json
emvparse parse 770E8202198094081001020120010300 $REG --format table
emvparse parse 770E8202198094081001020120010300 $REG --format html -o report.html

# C-APDU（例如 READ RECORD Case 2、GPO）
emvparse parse 00B2021400 $REG --format json
emvparse parse 00B2021400 $REG --format table
emvparse parse 80A8000002830000 $REG --format json

# R-APDU（FCI 响应体 + 状态字 9000）
emvparse parse 6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000 $REG --format table

# 强制整段按 TLV 解析（不拆 SW），调试用
emvparse parse 6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000 $REG --input tlv --format json

# 单 Tag 注册表查询
emvparse tag-info 9F33 $REG --scheme EMV

# 交易脚本（有序 C-APDU / R-APDU 步骤；table 需 APDU 表才有 SELECT/GPO/GAC 等标题）
emvparse script --file docs/examples/transaction-script-v1.json $REG --format table
```

**关于 `--format table`：** 输出是便于阅读的**纯文本**，不一定是统一网格。TLV 为 TAG/Name/Length/Value 列；**C-APDU** 为命令名 + CLA/INS/P1/P2/… 字段块；**R-APDU** 为状态字摘要 + 下方 TLV 行。脚本用 **json**，存档用 **html**。

安装包用法、环境变量与退出码：[命令行指南](docs/zh/命令行指南.md) · [CLI guide (English)](docs/en/cli-guide.md).

## 库集成

```cmake
find_package(EmvParse REQUIRED)
target_link_libraries(my_app PRIVATE EmvParse::EmvParse)
```

```cpp
#include <emvparse/input_parser.h>
```

5 分钟集成示例：[库集成](docs/zh/库集成.md) · [Library integration (English)](docs/en/library-integration.md).

## 文档

完整索引：[docs/zh/README.md](docs/zh/README.md)

| 文档 | 读者 | 内容 |
|------|------|------|
| [命令行指南.md](docs/zh/命令行指南.md) | 终端用户 | 安装、子命令、`EMVPARSE_REGISTRY` |
| [库集成.md](docs/zh/库集成.md) | 应用开发者 | `find_package` / `add_subdirectory` 最小示例 |
| [使用指南.md](docs/zh/使用指南.md) | 库维护者 | 完整 API、编译与安装 |
| [设计文档.md](docs/zh/设计文档.md) | 架构阅读 | 模块划分与实现原理 |
| [标签注册表数据模型.md](docs/zh/标签注册表数据模型.md) | 数据维护 | Registry JSON 模型 |

## 特性

- BER-TLV（EMV）解析，含嵌套构造 Tag
- JSON Tag Registry（EMV / Visa / UPI overlay）
- 跨平台共享库 `emvparse` + CLI `emvparse`
- 单元测试（GoogleTest）

## 系统要求

- C++11、CMake 3.10+
- 依赖：nlohmann/json（vcpkg manifest 或 FetchContent）

## 许可证

[LICENSE](LICENSE)
