add_executable(emvparse_cli
    main.cpp
    manifest_resolve.cpp
    hex_util.cpp
    commands_parse.cpp
    commands_registry.cpp
    commands_script.cpp
)

set_target_properties(emvparse_cli PROPERTIES OUTPUT_NAME emvparse)

target_compile_definitions(emvparse_cli
    PRIVATE
        EMVPARSE_VERSION="${PROJECT_VERSION}"
        EMVPARSE_DEFAULT_REGISTRY_DIR="${CMAKE_INSTALL_PREFIX}/share/emvparse/tag-registry"
)

target_link_libraries(emvparse_cli PRIVATE EmvParse)

set(EMVPARSE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_include_directories(emvparse_cli
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${EMVPARSE_SRC}
        ${EMVPARSE_SRC}/tlv
)

if(MSVC)
    target_compile_options(emvparse_cli PRIVATE /utf-8)
endif()

if(UNIX AND NOT APPLE)
    set_target_properties(emvparse_cli PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

install(TARGETS emvparse_cli RUNTIME DESTINATION bin)
