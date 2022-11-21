set(COMPONENT_ADD_INCLUDEDIRS .)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES audio_sal ota_service esp_peripherals test_utils)

set(COMPONENT_SRCS test_ota_service.c)

register_component()