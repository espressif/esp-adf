# volc_conv_ai is bundled source, not a registry dependency. Keep the SDK cmake unchanged.
set(VOLC_DIR "${CMAKE_CURRENT_LIST_DIR}/components/volc_conv_ai")
include(${VOLC_DIR}/cmake/common.cmake)

set(VOLC_SRCS ${VOLC_CONV_AI_SRCS}
    "${VOLC_DIR}/osal/src/espressif/volc_osal.c")
set(VOLC_INCS ${VOLC_CONV_AI_INCS}
    "${VOLC_DIR}/osal/inc")

if(CONFIG_VOLC_RTC_MODE)
    list(APPEND VOLC_SRCS ${VOLC_CONV_AI_HIGH_QUALITY_SRCS})
    list(APPEND VOLC_INCS ${VOLC_CONV_AI_HIGH_QUALITY_INCS})
endif()

if(CONFIG_VOLC_WS_MODE)
    list(APPEND VOLC_SRCS ${VOLC_CONV_AI_LOW_LOAD_SRCS})
    list(APPEND VOLC_INCS ${VOLC_CONV_AI_LOW_LOAD_INCS})
endif()

idf_component_register(
    SRCS "src/esp_llm_volc_adapter.c" ${VOLC_SRCS}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ${VOLC_INCS}
    REQUIRES mbedtls lwip esp_netif
)

idf_component_optional_requires(PUBLIC json)

if(CONFIG_VOLC_RTC_MODE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ENABLE_RTC_MODE)
    add_prebuilt_library(volc_engine_rtc_lite
        "${VOLC_DIR}/src/transports/high_quality/third_party/volc_rtc_engine_lite/libs/${CONFIG_IDF_TARGET}/libVolcEngineRTCLite.a"
        REQUIRES mbedtls espressif__zlib lwip)
    target_link_libraries(${COMPONENT_LIB} INTERFACE volc_engine_rtc_lite)
    # Volc's .a embeds http_parser, which collides with IDF http_parser.
    target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--allow-multiple-definition")
    if("${IDF_VERSION_MAJOR}" VERSION_GREATER_EQUAL "6")
        target_link_options(${COMPONENT_LIB} INTERFACE
            "LINKER:--undefined=mbedtls_md5"
            "LINKER:--undefined=mbedtls_sha1"
            "LINKER:--undefined=mbedtls_entropy_init"
            "LINKER:--undefined=mbedtls_entropy_free"
            "LINKER:--undefined=mbedtls_entropy_func"
            "LINKER:--undefined=mbedtls_ssl_conf_rng"
            "LINKER:--undefined=volc_tls_create"
            "LINKER:--undefined=volc_tls_destroy"
            "LINKER:--undefined=volc_tls_start"
            "LINKER:--undefined=volc_tls_start_with_verify"
            "LINKER:--undefined=volc_tls_read"
            "LINKER:--undefined=volc_tls_write"
            "LINKER:--undefined=volc_tls_is_handshake_over"
            "LINKER:--undefined=volc_tls_get_bytes_avail"
        )
    endif()
endif()

if(CONFIG_VOLC_WS_MODE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ENABLE_WS_MODE)
endif()

set_source_files_properties(${VOLC_SRCS} PROPERTIES
    COMPILE_OPTIONS "-w;-Os;-ffunction-sections;-fdata-sections")

idf_component_get_property(esp_llm_service_dir esp_llm_service COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${esp_llm_service_dir}/private_include")
