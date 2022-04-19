set(COMPONENT_ADD_INCLUDEDIRS include)

set(COMPONENT_REQUIRES bt audio_sal audio_pipeline esp_peripherals audio_stream audio_hal)
set(COMPONENT_PRIV_REQUIRES nvs_flash)

set(COMPONENT_SRCS ./bluetooth_service.c ./bt_keycontrol.c ./a2dp_stream.c ./hfp_stream.c)

register_component()

