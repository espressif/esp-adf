set(srcs
    "src/esp_rtmp_service.c"
)

if(CONFIG_ESP_RTMP_SERVICE_SERVER_SUPPORT)
    list(APPEND srcs "src/esp_rtmp_server.c")
endif()

if(CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT)
    list(APPEND srcs "src/esp_rtmp_src.c")
endif()

if(CONFIG_ESP_RTMP_SERVICE_SINK_SUPPORT)
    list(APPEND srcs "src/esp_rtmp_sink.c")
endif()

set(embed_files)

if(CONFIG_ESP_RTMP_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_rtmp_service_mcp.c")
    list(APPEND embed_files "src/mcp/esp_rtmp_service_mcp.json")
endif()

idf_component_register(
    SRCS         ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_inc"
    EMBED_FILES ${embed_files}
)

idf_component_optional_requires(PRIVATE json)

