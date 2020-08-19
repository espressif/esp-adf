set(COMPONENT_REQUIRES audio_flash_tone audio_stream audio_pipeline audio_hal esp_peripherals)

set(COMPONENT_SRCS "play_tone_mp3_example.c")
set(COMPONENT_ADD_INCLUDEDIRS .)

register_component()