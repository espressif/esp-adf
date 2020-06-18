set(COMPONENT_SRCS "audio_player.c" "audio_player_helper_bt.c" "audio_player_helper_default.c" "audio_player_helper_raw.c" "audio_player_helper_sd.c" "ble_gatts_module.c" "esp_audio_helper.c" "audio_player_manager.c")

set(COMPONENT_ADD_INCLUDEDIRS . include)


set(COMPONENT_REQUIRES audio_sal esp-adf-libs audio_pipeline playlist audio_stream audio_hal esp_peripherals bluetooth_service esp_event_cast)

register_component()
