set(COMPONENT_SRCS
    "main.c"
    "data_queue.c"
    "av_record.c"
    "rtmp_server_app.c"
    "rtmp_src_app.c"
    "rtmp_push_app.c"
    "rtmp_app_setting.c"
    "record_src.c"
    "record_i2s_aud.c"
    "record_spi_cam.c"
)

if (CONFIG_RTMP_USB_CAMERA_SUPPORT)
    list(APPEND COMPONENT_SRCS "record_usb_cam.c")
endif()

register_component()
