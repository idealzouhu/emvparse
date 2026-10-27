# emvparse

**语言：** [English](README.md) · 简体中文

EMV BER-TLV 解析库与命令行工具：解析十六进制 TLV 流，并按 Tag Registry 输出 JSON 语义。

## CLI 快速上手

```bash
emvparse --version
emvparse parse 9F33020141 --scheme EMV
emvparse tag-info 9F33 --scheme EMV
```

安装包用法、环境变量与退出码：[命令行指南](docs/zh/命令行指南.md) · [CLI guide (English)](docs/en/cli-guide.md).

## 库集成

```cmake
find_package(EmvParse REQUIRED)
target_link_libraries(my_app PRIVATE EmvParse::EmvParse)
```

```cpp
#include <emvparse/tlv_parser.h>
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
