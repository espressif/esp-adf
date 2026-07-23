set(srcs
    "src/esp_audio_capture_service.c"
    "src/esp_audio_capture_service_setup.c"
    "src/esp_audio_capture_ai_src.c"
)

set(embed_files)

if(CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED)
    list(APPEND srcs
        "src/ai_audio/esp_ai_audio_src.c"
        "src/ai_audio/esp_ai_audio_wrapper_doa.c"
    )
endif()

if(CONFIG_ESP_AUDIO_CAPTURE_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_audio_capture_service_mcp.c")
    list(APPEND embed_files "src/mcp/esp_audio_capture_service_mcp.json")
endif()

set(priv_requires freertos esp_ringbuf)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ${priv_requires}
    EMBED_FILES ${embed_files}
)

idf_component_optional_requires(PRIVATE json)
