set(srcs
    "src/esp_llm_service.c"
    "src/esp_llm_event.c"
    "src/esp_llm_media.c"
    "src/esp_llm_audio_ai.c"
)

set(embed_txtfiles)

if(CONFIG_ESP_LLM_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_llm_service_mcp.c")
    list(APPEND embed_txtfiles "src/mcp/esp_llm_service_mcp.json")
endif()

# Component dependencies are declared in idf_component.yml.
idf_component_register(
    SRCS              ${srcs}
    INCLUDE_DIRS      "include"
    PRIV_INCLUDE_DIRS "src" "private_include"
    EMBED_TXTFILES    ${embed_txtfiles}
)

idf_component_optional_requires(PRIVATE json espressif__cjson)
