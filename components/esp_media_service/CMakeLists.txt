set(srcs
    "src/esp_media_service.c"
    "src/esp_media_track_mngr.c"
    "src/esp_media_track.c"
    "src/esp_media_provider.c"
    "src/dummy_service/esp_media_dummy_service.c"
)

set(embed_files)

if(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SRC_SUPPORT)
    list(APPEND srcs
        "src/dummy_service/media_dummy_src.c"
        "src/dummy_service/media_dummy_patterns.c"
    )
    list(APPEND embed_files
        "src/dummy_service/assets/s.aac"
        "src/dummy_service/assets/s.opus"
        "src/dummy_service/assets/s.mp3"
        "src/dummy_service/assets/s.h264"
        "src/dummy_service/assets/s.mjpeg"
    )
endif()

if(CONFIG_ESP_MEDIA_DUMMY_SERVICE_SINK_SUPPORT)
    list(APPEND srcs "src/dummy_service/media_dummy_sink.c")
endif()

if(CONFIG_ESP_MEDIA_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_media_service_mcp.c")
    list(APPEND embed_files
        "src/mcp/esp_media_service_mcp.json"
        "src/mcp/esp_media_dummy_service_mcp.json"
    )
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_inc" "src/dummy_service"
    EMBED_FILES ${embed_files}
)

idf_component_optional_requires(PRIVATE json)
