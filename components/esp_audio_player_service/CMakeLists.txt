set(srcs
    "src/esp_audio_player_service.c"
    "src/esp_audio_player_service_setup.c"
)

if(CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_audio_player_service_mcp.c")
endif()

idf_component_register(
    SRCS         ${srcs}
    INCLUDE_DIRS "include"
)

idf_component_optional_requires(PUBLIC json)

if(CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE)
    # TEXT so schema_get() can treat the blob as a C string (NUL-terminated).
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/src/mcp/esp_audio_player_service_mcp.json" TEXT)
endif()
