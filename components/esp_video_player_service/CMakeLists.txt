set(srcs
    "src/esp_video_player_service.c"
    "src/esp_video_player_service_setup.c"
    "src/esp_video_player_service_ctrl.c"
)

if(CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_video_player_service_mcp.c")
endif()

idf_component_register(
    SRCS         ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS
        "private_inc"
)

idf_component_optional_requires(PUBLIC json)

if(CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE)
    # TEXT so schema_get() can treat the blob as a C string (NUL-terminated).
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/src/mcp/esp_video_player_service_mcp.json" TEXT)
endif()
