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

set(EMVPARSE_SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(EMVPARSE_SOURCES
    tlv/tlv.cpp
    tlv/hex_util.cpp
    tlv/iso_numeric_lookup.cpp
    tlv/tlv_parser.cpp
    tlv/tag_registry.cpp
    tlv/tag_registry_ops.cpp
    tlv/cvm_list_parser.cpp
    tlv/value_semantic_parser.cpp
    tlv/tlv_semantic_summary.cpp
    tlv/tlv_semantic_for_tlv.cpp
    tlv/tlv_formatter.cpp
    tlv/tlv_formatter_table_html.cpp
    tlv/tlv_tree_formatter.cpp
    apdu/apdu_registry.cpp
    apdu/apdu_parser.cpp
    apdu/apdu_result_formatter.cpp
    apdu/dol_parser.cpp
    apdu/transaction_script_parser.cpp
    apdu/transaction_script_formatter.cpp
    facade/input_parser.cpp
    facade/parse_output_pipeline.cpp
    facade/emv_parse_context.cpp
    facade/logger.cpp
    facade/registry_io.cpp
)

set(_emv_expected_cpp)
foreach(_emv_rel ${EMVPARSE_SOURCES})
    list(APPEND _emv_expected_cpp "${EMVPARSE_SRC_ROOT}/${_emv_rel}")
endforeach()

file(GLOB _emv_root_cpp "${EMVPARSE_SRC_ROOT}/*.cpp")
if(_emv_root_cpp)
    message(FATAL_ERROR "Source files must not live in src/ root: ${_emv_root_cpp}")
endif()

file(GLOB _emv_src_subdirs RELATIVE ${EMVPARSE_SRC_ROOT} ${EMVPARSE_SRC_ROOT}/*)
set(_emv_allowed_subdirs tlv apdu facade)
foreach(_emv_subdir ${_emv_src_subdirs})
    if(IS_DIRECTORY "${EMVPARSE_SRC_ROOT}/${_emv_subdir}")
        list(FIND _emv_allowed_subdirs "${_emv_subdir}" _emv_dir_idx)
        if(_emv_dir_idx LESS 0)
            message(FATAL_ERROR "Unexpected directory under src/: ${_emv_subdir}")
        endif()
    endif()
endforeach()

file(GLOB_RECURSE _emv_actual_cpp CONFIGURE_DEPENDS
    "${EMVPARSE_SRC_ROOT}/tlv/*.cpp"
    "${EMVPARSE_SRC_ROOT}/apdu/*.cpp"
    "${EMVPARSE_SRC_ROOT}/facade/*.cpp"
)
foreach(_emv_cpp ${_emv_actual_cpp})
    if(NOT _emv_cpp IN_LIST _emv_expected_cpp)
        message(FATAL_ERROR "Unexpected source file (not in EMVPARSE_SOURCES): ${_emv_cpp}")
    endif()
endforeach()
foreach(_emv_cpp ${_emv_expected_cpp})
    if(NOT EXISTS "${_emv_cpp}")
        message(FATAL_ERROR "EMVPARSE_SOURCES lists missing file: ${_emv_cpp}")
    endif()
endforeach()

add_library(EmvParseCore OBJECT ${EMVPARSE_SOURCES})

add_library(EmvParse SHARED $<TARGET_OBJECTS:EmvParseCore>)
add_library(EmvParseStatic STATIC $<TARGET_OBJECTS:EmvParseCore>)

set(EMVPARSE_INTERNAL_INCLUDES
    ${EMVPARSE_SRC_ROOT}
    ${EMVPARSE_SRC_ROOT}/tlv
    ${EMVPARSE_SRC_ROOT}/apdu
    ${EMVPARSE_SRC_ROOT}/facade
)

foreach(_emv_target EmvParseCore EmvParse EmvParseStatic)
    target_compile_definitions(${_emv_target}
        PRIVATE EMVPARSE_LIBRARY
    )
    target_include_directories(${_emv_target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${EMVPARSE_INTERNAL_INCLUDES}
    )
    target_compile_features(${_emv_target} PUBLIC cxx_std_11)
    if(MSVC)
        target_compile_options(${_emv_target} PRIVATE /utf-8)
    endif()
endforeach()

target_compile_definitions(EmvParse
    PUBLIC $<$<CONFIG:Debug>:EMVPARSE_DEBUG>
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(EmvParseCore PUBLIC nlohmann_json::nlohmann_json)
elseif(TARGET json)
    target_link_libraries(EmvParseCore PUBLIC json)
else()
    if(EXISTS "${json_SOURCE_DIR}/include")
        target_include_directories(EmvParseCore PRIVATE ${json_SOURCE_DIR}/include)
    endif()
endif()

target_link_libraries(EmvParse PRIVATE EmvParseCore)
target_link_libraries(EmvParseStatic PUBLIC EmvParseCore)

set_target_properties(EmvParse EmvParseStatic PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

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

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/emvparse/emvparse_global.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/emvparse/input_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/emvparse/logging.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/emvparse/output_options.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/emvparse/tlv.h
    DESTINATION include/emvparse
)

install(EXPORT EmvParseTargets
    FILE EmvParseTargets.cmake
    NAMESPACE EmvParse::
    DESTINATION lib/cmake/EmvParse
)
