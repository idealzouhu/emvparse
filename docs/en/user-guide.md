# emvparse user guide

Full API, build, and install reference (English). For the Chinese edition with the same structure, see [../zh/使用指南.md](../zh/使用指南.md).

## Contents

1. [Quick start](#1-quick-start)
2. [Requirements](#2-requirements)
3. [Build and install](#3-build-and-install)
4. [API reference](#4-api-reference)
5. [Examples](#5-examples)
6. [Running tests](#6-running-tests)
7. [FAQ](#7-faq)

---

## 1. Quick start

CLI details: [cli-guide.md](cli-guide.md). Minimal CMake link: [library-integration.md](library-integration.md).

### Install the library

```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX="/path/to/install"
cmake --build . --config Release    # Windows
cmake --install . --config Release
```

On Linux/macOS use `cmake --build . -j$(nproc)` and `sudo cmake --install .` if needed.

CMake linking and minimal examples: **[library-integration.md](library-integration.md)**.

---

## 2. Requirements

- C++14, CMake 3.10+
- Compiler: GCC 5+, Clang 3.5+, MSVC 2015+
- nlohmann/json (vcpkg manifest or FetchContent)

Platforms: Windows, Linux, macOS.

---

## 3. Build and install

### Development build (no install)

With vcpkg (recommended): run **`scripts/build.ps1`** (Windows) or **`scripts/build.sh`** (Unix). Scripts prefer `./vcpkg/` in the repo (gitignored), cloning per `vcpkg-tool-version` when missing, then configure with manifest mode.

Manual equivalent:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_MANIFEST_MODE=ON -DVCPKG_TARGET_TRIPLET=x64-linux -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Without vcpkg, `cmake -B build` may fetch nlohmann/json via FetchContent; tests may fetch GoogleTest the same way.

### Install for downstream `find_package`

Same as above, plus `-DCMAKE_INSTALL_PREFIX=...` and `cmake --install`.

Artifacts: shared library `emvparse`, headers under `include/emvparse/`, registry under `share/emvparse/tag-registry/`, optional CLI `emvparse`.

CMake options: `BUILD_EMVPARSE_CLI` (default ON), `BUILD_TESTING` (default ON).

---

## 4. API reference

### EmvInputParser

Recommended entry point: loads **tag registry + APDU command manifest**, classifies input (`auto` / `tlv` / `capdu` / `rapdu`), and emits JSON, table, or HTML. For integration, include `input_parser.h`, `output_options.h`, and (when using log callbacks) `logging.h`. `emvparse_global.h` is installed as an ABI helper (export/import macros) and is included transitively—do not use it directly.

```cpp
#include <emvparse/input_parser.h>
#include <emvparse/output_options.h>

EmvInputParser parser("registry/manifest.json", "EMV");
emvparse::ParseOutputOptions opts;
opts.format = emvparse::OutputFormat::Json;
opts.input = emvparse::InputMode::Auto;
std::cout << parser.parseHexWithOptions("00B2021400", opts) << std::endl;
```

| Method | Role |
|--------|------|
| `loadRegistry(manifestPath, scheme)` | Reload tag + APDU manifest |
| `parseHexWithOptions(hex, opts)` | CLI `parse` parity (TLV / C-APDU / R-APDU) |
| `parseHexToJson(hex)` | BER-TLV with registry semantics → JSON |
| `getTagInfo(tagHex)` | Tag metadata JSON or empty if unknown |
| `parseScriptFile(path, opts)` / `parseScriptJson(json, opts)` | Transaction script → formatted string |
| `lastError()` | Error text after failed `loadRegistry`, invalid hex on `parseHexToJson`, empty result from `parseHexWithOptions`, or script helpers |
| `hexToBytes(hex)` | Static hex normalizer (shared with CLI) |
| `setLogCallback(cb)` | Diagnostic logging (`LogLevel`, `LogCallback` in this header) |

On failure, `loadRegistry` returns `false` and sets `lastError()`. Invalid hex makes `parseHexToJson` return an empty result with `lastError()` set. Script helpers and failed `parseHexWithOptions` return an empty string and set `lastError()`.

Nested constructed TLV (tag byte bit 6 set) appears as nested JSON objects under `children` in `parseHexToJson` / `parseHexWithOptions` output.

`TLV`, `TLVParser`, `ApduParser`, and transaction-script types live in internal headers under `src/` (not installed). Use `EmvInputParser` string APIs unless you build from source and need object-level tests.

### Registry ops (source tree / CLI only)

`validateAllSchemes` and `mergeSchemeToFile` are exported from the library but **`tag_registry_ops.h` is not installed** — use the `emvparse registry` CLI or build from source with `src/tlv/tag_registry_ops.h`. See [tag-registry-data-model.md](tag-registry-data-model.md).

---

## 5. Examples

Parse BER-TLV with registry semantics (JSON):

```cpp
EmvInputParser parser("registry/manifest.json", "EMV");
std::cout << parser.parseHexToJson("95058000000000") << std::endl;
```

Log callback: implement `LogCallback` and call `setLogCallback`; default logs go to `stderr`.

---

## 6. Running tests

```bash
cd build && ctest --output-on-failure
```

Binary: `build/bin/emvparse_tests` (or `emvparse_tests.exe` on Windows).

---

## 7. FAQ

**find_package fails** — set `CMAKE_PREFIX_PATH` or `EmvParse_DIR`.

**Windows DLL not found** — keep `emvparse.dll` next to your `.exe` or on `PATH`.

**Nested TLV** — automatic via EMV constructed bit; registry not required for structure.

**Tag rules** — `EmvInputParser::loadRegistry` loads tag + APDU manifests. See [tag-registry-data-model.md](tag-registry-data-model.md) and [../../registry/tag-registry.schema.json](../../registry/tag-registry.schema.json).

**nlohmann/json** — use vcpkg or allow FetchContent on configure.

---

## See also

- [design.md](design.md) — architecture
- [../zh/设计文档.md](../zh/设计文档.md) — extended design and BER-TLV rules (Chinese)
