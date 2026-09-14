set(srcs
    "src/esp_extractor_service.c"
)

set(embed_files)

if(CONFIG_ESP_EXTRACTOR_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_extractor_service_mcp.c")
    list(APPEND embed_files "src/mcp/esp_extractor_service_mcp.json")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_inc"
    EMBED_FILES ${embed_files}
)

idf_component_optional_requires(PRIVATE json)
