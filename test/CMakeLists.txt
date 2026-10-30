find_package(GTest QUIET)
if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(emvparse_tests
    test_tlv_parser.cpp
    test_tlv_semantics.cpp
    test_tag_registry_ops.cpp
    test_apdu_parser.cpp
    test_dol_parser.cpp
    test_transaction_script.cpp
    test_input_parser_facade.cpp
)

target_compile_features(emvparse_tests PRIVATE cxx_std_14)

# Link static lib: use build-side export macros (avoid dllimport on CLI ops symbols).
target_compile_definitions(emvparse_tests PRIVATE EMVPARSE_LIBRARY)

if(MSVC)
    target_compile_options(emvparse_tests PRIVATE /utf-8)
endif()

target_link_libraries(emvparse_tests
    PRIVATE
        EmvParseStatic
        GTest::gtest
        GTest::gtest_main
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(emvparse_tests PRIVATE nlohmann_json::nlohmann_json)
endif()

set(EMVPARSE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_include_directories(emvparse_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${EMVPARSE_SRC}
        ${EMVPARSE_SRC}/tlv
        ${EMVPARSE_SRC}/apdu
        ${EMVPARSE_SRC}/facade
)

set_target_properties(emvparse_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

include(GoogleTest)
gtest_discover_tests(emvparse_tests)
