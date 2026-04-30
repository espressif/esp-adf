set(srcs "player_service.c")
set(embed_files "")

if(CONFIG_ESP_MCP_ENABLE)
    list(APPEND srcs        "player_service_mcp.c")
    list(APPEND embed_files "player_service_mcp.json")
endif()

idf_component_register(
    SRCS         ${srcs}
    INCLUDE_DIRS "."
    REQUIRES     esp_service
    EMBED_FILES  ${embed_files}
)
