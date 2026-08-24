idf_component_register(
    SRCS
        "app_main.c"
        "ut_esp_audio_capture_service.c"
        "capture_fake_aud_src.c"
        "test_mcp.c"
    INCLUDE_DIRS
        "."
        "../../../esp_audio_capture_service/src/ai_audio"
    PRIV_REQUIRES unity
    EMBED_FILES
        "../assets/vad.aac"
        "../assets/vad_expected.txt"
)

if(CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_ENABLED)
    target_sources(${COMPONENT_LIB} PRIVATE "ut_esp_ai_audio.c")
endif()
