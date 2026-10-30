cmake_minimum_required(VERSION 3.10)
project(emvparse
    VERSION 1.0.0
    DESCRIPTION "EMV TLV parse library and CLI"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Install path prefix" FORCE)
endif()

add_subdirectory(src)

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/EmvParseConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmvParseConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/EmvParseConfig.cmake"
    INSTALL_DESTINATION lib/cmake/EmvParse
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/EmvParseConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/EmvParseConfigVersion.cmake"
    DESTINATION lib/cmake/EmvParse
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/registry/
    DESTINATION share/emvparse/tag-registry
    FILES_MATCHING
        PATTERN "*.json"
        PATTERN "*.schema.json" EXCLUDE
        PATTERN "README.md" EXCLUDE
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE DESTINATION .)

option(BUILD_EMVPARSE_CLI "Build emvparse command-line tool" ON)
if(BUILD_EMVPARSE_CLI)
    add_subdirectory(cli)
endif()

option(BUILD_TESTING "Build the testing tree" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
