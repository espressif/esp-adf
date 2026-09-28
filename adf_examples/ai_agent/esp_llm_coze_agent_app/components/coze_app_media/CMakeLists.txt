idf_component_register(
    SRCS "coze_app_media.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_capture_service
        esp_llm_coze
        esp_player_service
    PRIV_REQUIRES
        esp_audio_codec
        esp_audio_capture_service
        esp_audio_player_service
)
