# emvparse

**Languages:** English · [简体中文](README.zh-CN.md)

EMV BER-TLV parsing library and CLI: parse hex as **plain TLV**, **C-APDU**, or **R-APDU** (auto-detected by default) and emit structured output via the tag and APDU registries.

## CLI quick start

```bash
emvparse --version

# --format json | table | html   (default: json; html writes a file, often -o report.html)
# --input auto | tlv | capdu | rapdu   (default: auto)
# Add --scheme EMV|Visa|UPI and --registry path/to/manifest.json when needed.

# Plain TLV (e.g. GPO response template 77…)
emvparse parse 770E8202198094081001020120010300 --format json
emvparse parse 770E8202198094081001020120010300 --format table
emvparse parse 770E8202198094081001020120010300 --format html -o report.html

# C-APDU (e.g. READ RECORD Case 2, GET PROCESSING OPTIONS)
emvparse parse 00B2021400 --format json
emvparse parse 00B2021400 --format table
emvparse parse 80A8000002830000 --format json

# R-APDU (FCI body + status word 9000)
emvparse parse 6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000 --format table

# Force whole hex as TLV (no SW split), e.g. for debugging
emvparse parse 6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000 --input tlv --format json

# Single-tag registry lookup
emvparse tag-info 9F33 --scheme EMV
```

**About `--format table`:** stdout is **plain text for reading**, not a strict grid. TLV uses fixed columns (TAG / Name / Length / Value); **C-APDU** uses a labeled block (command name, CLA/INS/P1/P2/…); **R-APDU** mixes a status summary with TLV rows when present. Use **json** for scripts, **html** for a saved report.

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
