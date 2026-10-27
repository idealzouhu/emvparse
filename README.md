# emvparse

**Languages:** English · [简体中文](README.zh-CN.md)

EMV BER-TLV parsing library and CLI: parse hex TLV streams and emit JSON semantics via the tag registry.

## CLI quick start

```bash
emvparse --version
emvparse parse 9F33020141 --scheme EMV
emvparse tag-info 9F33 --scheme EMV
```

Install layout, environment variables, and exit codes: [CLI guide](docs/en/cli-guide.md).

## Library integration

```cmake
find_package(EmvParse REQUIRED)
target_link_libraries(my_app PRIVATE EmvParse::EmvParse)
```

```cpp
#include <emvparse/tlv_parser.h>
```

Five-minute examples: [Library integration](docs/en/library-integration.md).

## Documentation map

| Document | Audience | Scope |
|----------|----------|--------|
| [docs/en/README.md](docs/en/README.md) | Everyone | Documentation index |
| [docs/en/cli-guide.md](docs/en/cli-guide.md) | CLI users | Install, subcommands, `EMVPARSE_REGISTRY` |
| [docs/en/library-integration.md](docs/en/library-integration.md) | App developers | `find_package` / `add_subdirectory` |
| [docs/en/user-guide.md](docs/en/user-guide.md) | Maintainers | Full API, build, and install |
| [docs/en/design.md](docs/en/design.md) | Architecture | Modules and design |
| [docs/en/tag-registry-data-model.md](docs/en/tag-registry-data-model.md) | Data maintainers | Registry JSON layout and merge rules |

## Features

- BER-TLV (EMV) parsing, including nested constructed tags
- JSON tag registry with EMV / Visa / UPI overlays
- Cross-platform shared library `emvparse` and CLI `emvparse`
- Unit tests (GoogleTest)

## Requirements

- C++11, CMake 3.10+
- Dependency: nlohmann/json (vcpkg manifest or FetchContent)

## License

[LICENSE](LICENSE)
