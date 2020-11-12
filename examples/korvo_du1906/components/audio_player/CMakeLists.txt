set(COMPONENT_SRCDIRS  .)
set(COMPONENT_ADD_INCLUDEDIRS . include)


set(COMPONENT_REQUIRES audio_sal esp-adf-libs audio_pipeline playlist audio_stream audio_hal esp_peripherals bluetooth_service esp_event_cast)

register_component()
