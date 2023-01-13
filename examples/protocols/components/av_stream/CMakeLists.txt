
set(COMPONENT_SRCDIRS . av_stream_hal)

set(COMPONENT_ADD_INCLUDEDIRS . av_stream_hal)

set(COMPONENT_REQUIRES audio_board audio_stream esp32-camera esp_peripherals esp-adf-libs esp_timer)

if(IDF_TARGET STREQUAL "esp32s3")
    list(APPEND COMPONENT_REQUIRES usb_stream)
endif()

register_component()
