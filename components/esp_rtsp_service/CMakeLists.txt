set(srcs
    "src/esp_rtsp_service.c"
)

if(CONFIG_ESP_RTSP_SERVICE_SERVER_SUPPORT)
    list(APPEND srcs "src/esp_rtsp_server.c")
endif()

if(CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT)
    list(APPEND srcs "src/esp_rtsp_src.c")
endif()

if(CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT OR CONFIG_ESP_RTSP_SERVICE_SERVER_SUPPORT)
    list(APPEND srcs "src/esp_rtsp_sink.c")
endif()

set(embed_files)

if(CONFIG_ESP_RTSP_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_rtsp_service_mcp.c")
    list(APPEND embed_files "src/mcp/esp_rtsp_service_mcp.json")
endif()

idf_component_register(
    SRCS         ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_inc"
    EMBED_FILES ${embed_files}
)

idf_component_optional_requires(PRIVATE json)

