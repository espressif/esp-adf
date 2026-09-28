# REQUIRES is resolved before project Kconfig, so video deps cannot be scene-gated
# here. C ifdefs still drop the unused paths at compile time.
idf_component_register(
    SRCS "baidu_app_media.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES
        esp_capture_service
        esp_llm_baidu
        esp_player_service
    PRIV_REQUIRES
        esp_audio_codec
        esp_audio_capture_service
        esp_audio_player_service
        esp_board_manager
        esp_video_capture_service
        esp_video_codec
        esp_video_player_service
)
