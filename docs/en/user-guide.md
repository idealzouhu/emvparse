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

- C++11, CMake 3.10+
- Compiler: GCC 5+, Clang 3.5+, MSVC 2015+
- nlohmann/json (vcpkg manifest or FetchContent)

Platforms: Windows, Linux, macOS.

---

## 3. Build and install

### Development build (no install)

With vcpkg (recommended):

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_MANIFEST_MODE=ON -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build . --config Release
ctest --output-on-failure
```

Without vcpkg, configure with plain `cmake ..`; dependencies may be fetched via FetchContent.

### Install for downstream `find_package`

Same as above, plus `-DCMAKE_INSTALL_PREFIX=...` and `cmake --install`.

Artifacts: shared library `emvparse`, headers under `include/emvparse/`, registry under `share/emvparse/tag-registry/`, optional CLI `emvparse`.

CMake options: `BUILD_EMVPARSE_CLI` (default ON), `BUILD_TESTING` (default ON).

---

## 4. API reference

### TLVParser

**Constructors**

```cpp
TLVParser();
TLVParser(const std::string& manifestPath, const std::string& scheme);
```

**Methods**

```cpp
bool loadTagRegistry(const std::string& manifestPath, const std::string& scheme);
std::vector<std::shared_ptr<TLV>> parseBytes(const std::vector<uint8_t>& data);
std::vector<std::shared_ptr<TLV>> parseHex(const std::string& hexStr);
std::string parseBytesToJson(const std::vector<uint8_t>& data);
std::string parseHexToJson(const std::string& hexStr);
std::string getTagInfo(const std::string& tagHex) const;
void setLogCallback(LogCallback callback);
```

Nested TLV: EMV **constructed tag** (tag byte bit 6 set) is parsed recursively into `TLV::children` without requiring the registry.

`getTagInfo` returns JSON metadata or empty string if the tag is unknown or no registry is loaded.

### TLV

```cpp
const ByteArray& getTag() const;
const ByteArray& getValue() const;
const Children& getChildren() const;
std::string tagHex() const;
std::string valueHex() const;
std::string lengthHex() const;
```

### Registry ops (optional)

```cpp
#include <emvparse/tag_registry_ops.h>
```

`validateAllSchemes`, `mergeSchemeToFile` — see [tag-registry-data-model.md](tag-registry-data-model.md).

---

## 5. Examples

Parse hex without registry:

```cpp
TLVParser parser;
auto list = parser.parseHex("9F33020141");
```

Parse with semantics (JSON):

```cpp
TLVParser parser("registry/manifest.json", "EMV");
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

**Tag rules** — `loadTagRegistry(".../manifest.json", "EMV"|"Visa"|"UPI")`. See [tag-registry-data-model.md](tag-registry-data-model.md) and [../../registry/tag-registry.schema.json](../../registry/tag-registry.schema.json).

**nlohmann/json** — use vcpkg or allow FetchContent on configure.

---

## See also

- [design.md](design.md) — architecture
- [../zh/设计文档.md](../zh/设计文档.md) — extended design and BER-TLV rules (Chinese)
