find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(json)
    set(nlohmann_json_FOUND TRUE)
endif()

add_library(EmvParse SHARED
    tlv.cpp
    tlv_parser.cpp
    tag_registry.cpp
    tag_registry_ops.cpp
    tlv_formatter.cpp
    tlv_formatter_table_html.cpp
    tlv_semantic_summary.cpp
    value_semantic_parser.cpp
    logger.cpp
)

target_compile_definitions(EmvParse
    PRIVATE EMVPARSE_LIBRARY
    PUBLIC $<$<CONFIG:Debug>:EMVPARSE_DEBUG>
)

target_include_directories(EmvParse
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(EmvParse PRIVATE nlohmann_json::nlohmann_json)
elseif(TARGET json)
    target_link_libraries(EmvParse PRIVATE json)
else()
    if(EXISTS "${json_SOURCE_DIR}/include")
        target_include_directories(EmvParse PRIVATE ${json_SOURCE_DIR}/include)
    endif()
endif()

target_compile_features(EmvParse PUBLIC cxx_std_11)

if(MSVC)
    target_compile_options(EmvParse PRIVATE /utf-8)
endif()

set_target_properties(EmvParse PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "emvparse"
    PUBLIC_HEADER ""
)

install(TARGETS EmvParse
    EXPORT EmvParseTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/emvparse/
    DESTINATION include/emvparse
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT EmvParseTargets
    FILE EmvParseTargets.cmake
    NAMESPACE EmvParse::
    DESTINATION lib/cmake/EmvParse
)
