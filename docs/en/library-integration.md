# emvparse library integration (5 minutes)

## Installed package

```cmake
cmake_minimum_required(VERSION 3.10)
project(my_emv_app LANGUAGES CXX)

find_package(EmvParse REQUIRED)

add_executable(my_emv_app main.cpp)
target_link_libraries(my_emv_app PRIVATE EmvParse::EmvParse)
target_compile_features(my_emv_app PRIVATE cxx_std_14)
```

Point CMake at the install prefix:

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/path/to/emvparse/install"
```

After `cmake --install`, application code should include only `input_parser.h`, `output_options.h`, and (when using log callbacks) `logging.h` from `include/emvparse/`. The install tree also contains `emvparse_global.h` (`EMVPARSE_EXPORT` for shared-library ABI on Windows and similar platforms). Public headers include it automatically—**do not `#include <emvparse/emvparse_global.h>` in your project**.

## Source tree

```cmake
add_subdirectory(path/to/emvparse)
target_link_libraries(my_emv_app PRIVATE EmvParse::EmvParse)
```

## Minimal example

BER-TLV with semantics:

```cpp
#include <emvparse/input_parser.h>
#include <iostream>

int main() {
    EmvInputParser parser("registry/manifest.json", "EMV");
    std::cout << parser.parseHexToJson("9F33020141") << std::endl;
}
```

C-APDU / R-APDU or mixed CLI-style output:

```cpp
#include <emvparse/input_parser.h>
#include <emvparse/output_options.h>
#include <iostream>

int main() {
    EmvInputParser parser("registry/manifest.json", "EMV");
    emvparse::ParseOutputOptions opts;
    std::cout << parser.parseHexWithOptions("00B2021400", opts) << std::endl;
}
```

## Registry validate / merge

Not part of the installed header set. Use the `emvparse registry` CLI, or when building from source include `src/tlv/tag_registry_ops.h` and add `src/tlv` (or the in-tree CLI private include layout) to your target’s private include path.

Full API and build options: [user-guide.md](user-guide.md) (English) or [使用指南.md](../zh/使用指南.md) (Chinese).
