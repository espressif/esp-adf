set(COMPONENT_SRCS
    "data_queue.c"
    "av_record.c"
    "record_src.c"
    "record_i2s_aud.c"
)

set(COMPONENT_ADD_INCLUDEDIRS ./)

if (CONFIG_DVP_CAMERA_SUPPORT)
    list(APPEND COMPONENT_SRCS "record_dvp_cam.c")
endif()

set(COMPONENT_PRIV_REQUIRES esp-adf-libs esp_peripherals audio_board audio_hal audio_stream)

if (CONFIG_IDF_TARGET STREQUAL "esp32s3")
    list(APPEND COMPONENT_PRIV_REQUIRES esp_h264)
endif()

list(APPEND COMPONENT_REQUIRES usb_stream)

if (CONFIG_USB_CAMERA_SUPPORT)
    list(APPEND COMPONENT_SRCS "record_usb_cam.c")
endif()

register_component()
