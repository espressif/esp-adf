idf_component_register(
    SRCS "volc_app_media.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_capture_service
        esp_llm_volc
    PRIV_REQUIRES
        esp_audio_codec
        esp_audio_capture_service
        esp_audio_player_service
)
