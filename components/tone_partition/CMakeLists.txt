set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")
set(COMPONENT_ADD_INCLUDEDIRS include)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES bootloader_support)
set(COMPONENT_PRIV_REQUIRES esp_actions esp_dispatcher audio_sal)

if (idf_version VERSION_GREATER_EQUAL "5.0")
list(APPEND COMPONENT_REQUIRES esp_app_format)
endif()

set(COMPONENT_SRCS ./tone_partition.c)

register_component()