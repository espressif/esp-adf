idf_component_register(
    SRCS "tongyi_app_media.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES
        esp_capture_service
        esp_llm_tongyi
        esp_player_service
    PRIV_REQUIRES
        esp_audio_codec
        esp_audio_capture_service
        esp_audio_player_service
        esp_board_manager
        gmf_core
        esp_lcd
        esp_video
        esp_video_capture_service
        esp_video_codec
        esp_video_player_service
)
