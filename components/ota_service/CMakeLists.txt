set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")
set(COMPONENT_ADD_INCLUDEDIRS include)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES app_update esp_https_ota)
set(COMPONENT_PRIV_REQUIRES esp_peripherals audio_pipeline audio_sal audio_stream)

if (idf_version VERSION_GREATER_EQUAL "5.0")
list(APPEND COMPONENT_REQUIRES esp_app_format)
endif()

set(COMPONENT_SRCS ./esp_fs_ota.c
                    ./ota_service.c
                    ./ota_proc_default.c)

register_component()
