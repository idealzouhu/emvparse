# emvparse library integration (5 minutes)

## Installed package

```cmake
cmake_minimum_required(VERSION 3.10)
project(my_emv_app LANGUAGES CXX)

find_package(EmvParse REQUIRED)

add_executable(my_emv_app main.cpp)
target_link_libraries(my_emv_app PRIVATE EmvParse::EmvParse)
target_compile_features(my_emv_app PRIVATE cxx_std_11)
```

Point CMake at the install prefix:

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/path/to/emvparse/install"
```

## Source tree

```cmake
add_subdirectory(path/to/emvparse)
target_link_libraries(my_emv_app PRIVATE EmvParse::EmvParse)
```

## Minimal example

```cpp
#include <emvparse/tlv_parser.h>
#include <iostream>

int main() {
    TLVParser parser("registry/manifest.json", "EMV");
    std::cout << parser.parseHexToJson("9F33020141") << std::endl;
}
```

## Registry ops

`#include <emvparse/tag_registry_ops.h>` for `validateAllSchemes` and `mergeSchemeToFile`.

Full API, examples, and build options: [user-guide.md](user-guide.md) (English) or [使用指南.md](../zh/使用指南.md) (Chinese).
