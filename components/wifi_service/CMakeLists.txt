set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_PRIV_INCLUDEDIRS inc)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES bt mbedtls)
set(COMPONENT_PRIV_REQUIRES audio_sal esp_dispatcher)

set(COMPONENT_SRCS src/esp_wifi_setting.c
                    src/wifi_service.c
                    smart_config/smart_config.c
                    airkiss_config/airkiss_config.c
                    blufi_config/blufi_config.c
                    blufi_config/blufi_security.c)

register_component()

target_link_libraries(${COMPONENT_TARGET} INTERFACE "-L${CMAKE_CURRENT_LIST_DIR}/airkiss_config")
target_link_libraries(${COMPONENT_TARGET} INTERFACE airkiss_aes)
