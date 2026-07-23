set(srcs
    "src/esp_video_capture_service.c"
    "src/esp_video_capture_service_overlay.c"
    "src/esp_video_capture_service_setup.c"
)

set(embed_files)

if(CONFIG_ESP_VIDEO_CAPTURE_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_video_capture_service_mcp.c")
    list(APPEND embed_files "src/mcp/esp_video_capture_service_mcp.json")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS
        "private_inc"
    REQUIRES esp_audio_capture_service
    EMBED_FILES ${embed_files}
)

idf_component_optional_requires(PRIVATE json)
