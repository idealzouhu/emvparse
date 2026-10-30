# emvparse

**Languages:** English · [简体中文](README.zh-CN.md)

EMV BER-TLV parsing library and CLI: parse hex as **plain TLV**, **C-APDU**, or **R-APDU** (auto-detected by default) and emit structured output via the tag and APDU registries.

## CLI quick start

```bash
emvparse --version

# Run from the repo root. Tag names and APDU command titles need manifest (incl. apdu-commands.json).
# Equivalent: export EMVPARSE_REGISTRY="$PWD/registry/manifest.json"
REG="--registry registry/manifest.json"

# --format json | table | html   (default: json; html writes a file, often -o report.html)
# --input auto | tlv | capdu | rapdu   (default: auto)
# Optional --scheme EMV|Visa|UPI

# Plain TLV (e.g. GPO response template 77…)
emvparse parse 770E8202198094081001020120010300 $REG --format json
emvparse parse 770E8202198094081001020120010300 $REG --format table
emvparse parse 770E8202198094081001020120010300 $REG --format html -o report.html

# C-APDU (e.g. READ RECORD Case 2, GET PROCESSING OPTIONS)
emvparse parse 00B2021400 $REG --format json
emvparse parse 00B2021400 $REG --format table
emvparse parse 80A8000002830000 $REG --format json

# R-APDU (FCI body + status word 9000)
emvparse parse 6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000 $REG --format table

# Force whole hex as TLV (no SW split), e.g. for debugging
emvparse parse 6F29840E325041592E5359532E4444463031A517BF0C1461129F2A038106438701014F07A00000065810109000 $REG --input tlv --format json

# Single-tag registry lookup
emvparse tag-info 9F33 $REG --scheme EMV

# Transaction script (ordered C-APDU / R-APDU steps; table needs APDU registry for step titles)
emvparse script --file docs/examples/transaction-script-v1.json $REG --format table
```

**About `--format table`:** stdout is **plain text for reading**, not a strict grid. TLV uses fixed columns (TAG / Name / Length / Value); **C-APDU** uses a labeled block (command name, CLA/INS/P1/P2/…); **R-APDU** mixes a status summary with TLV rows when present. Use **json** for scripts, **html** for a saved report.

Install layout, environment variables, and exit codes: [CLI guide](docs/en/cli-guide.md).

## Library integration

```cmake
find_package(EmvParse REQUIRED)
target_link_libraries(my_app PRIVATE EmvParse::EmvParse)
```

```cpp
#include <emvparse/input_parser.h>
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

## Build from source

Requires CMake ≥ 3.10 and a C++14 toolchain. Scripts prefer an in-repo **`vcpkg/`** tree (gitignored); if missing, they clone [vcpkg-tool-version](vcpkg-tool-version). Versions are pinned via [vcpkg.json](vcpkg.json) `builtin-baseline` (aligned with the log-decrypt repo layout).

```powershell
# Windows
.\scripts\build.ps1
```

```bash
# Linux / macOS / Git Bash
chmod +x scripts/build.sh
./scripts/build.sh
```

Manual configure: `cmake -B build -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_MANIFEST_MODE=ON -DBUILD_TESTING=ON`

## Requirements

- C++14, CMake 3.10+
- nlohmann/json (vcpkg manifest; FetchContent fallback without vcpkg)

## License

[LICENSE](LICENSE)
