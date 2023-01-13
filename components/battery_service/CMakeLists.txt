set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

set(COMPONENT_SRCS "battery_service.c"
                    "./monitors/voltage_monitor.c")

set(COMPONENT_ADD_INCLUDEDIRS "include"
                                "./monitors/include")

set(COMPONENT_REQUIRES  audio_sal esp_peripherals)

if (idf_version VERSION_GREATER_EQUAL "5.0")
list(APPEND COMPONENT_PRIV_REQUIRES esp_adc)
list(APPEND COMPONENT_REQUIRES esp_timer)
else()
list(APPEND COMPONENT_PRIV_REQUIRES esp_adc_cal)
endif()

register_component()
