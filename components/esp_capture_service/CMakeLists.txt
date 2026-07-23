set(srcs
    "src/esp_capture_service.c"
    "src/esp_capture_service_setup.c"
)

idf_component_register(
    SRCS         ${srcs}
    INCLUDE_DIRS "include"
)
