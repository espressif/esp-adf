# Copyright (2025) Beijing Volcano Engine Technology Ltd.
# SPDX-License-Identifier: Apache-2.0

# ConversationalAI SDK ESP-IDF component configuration
# Converted from standard CMake to ESP-IDF component format

# Source files
set(VOLC_SRCS ${VOLC_CONV_AI_SRCS}
            "${CMAKE_CURRENT_LIST_DIR}/../osal/src/espressif/volc_osal.c"
)

set(VOLC_INCS ${VOLC_CONV_AI_INCS}
            "${CMAKE_CURRENT_LIST_DIR}/../osal/inc"
)

if(CONFIG_VOLC_RTC_MODE)
    message(STATUS "CONFIG_VOLC_RTC_MODE: ${CONFIG_VOLC_RTC_MODE}")
    list(APPEND VOLC_SRCS
        ${VOLC_CONV_AI_HIGH_QUALITY_SRCS}
    )
    list(APPEND VOLC_INCS
        ${VOLC_CONV_AI_HIGH_QUALITY_INCS}
    )
endif()

if(CONFIG_VOLC_WS_MODE)
    message(STATUS "CONFIG_VOLC_WS_MODE: ${CONFIG_VOLC_WS_MODE}")
    list(APPEND VOLC_SRCS
        ${VOLC_CONV_AI_LOW_LOAD_SRCS}
    )
    list(APPEND VOLC_INCS
        ${VOLC_CONV_AI_LOW_LOAD_INCS}
    )
endif()

idf_component_register(
    SRCS ${VOLC_SRCS}
    INCLUDE_DIRS ${VOLC_INCS}
    REQUIRES mbedtls lwip esp_netif
)

idf_component_optional_requires(PUBLIC json)

if(CONFIG_VOLC_RTC_MODE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ENABLE_RTC_MODE)

    add_prebuilt_library(volc_engine_rtc_lite
        "${CMAKE_CURRENT_LIST_DIR}/../src/transports/high_quality/third_party/volc_rtc_engine_lite/libs/${CONFIG_IDF_TARGET}/libVolcEngineRTCLite.a"
        REQUIRES mbedtls espressif__zlib lwip)
    target_link_libraries(${COMPONENT_LIB} INTERFACE volc_engine_rtc_lite)
    # First-cut: Volc's .a embeds http_parser, which collides with IDF http_parser
    # (pulled in by esp-tls). Keep Volc's extra APIs (http_status_str) and let
    # the first definition win for the shared symbols.
    target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:--allow-multiple-definition")

    # Force-pull our IDF6 objects from libvolc_conv_ai.a before the prebuilt
    # archive is scanned; otherwise the linker takes volc_tls.c.obj from
    # libVolcEngineRTCLite.a and then fails on missing mbedtls 3.x symbols.
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

# Compiler flags
target_compile_options(${COMPONENT_LIB} PRIVATE
    -w
    -Os
    -ffunction-sections
    -fdata-sections
)

# Export the library for use by other components
set(VOLC_CONV_AI_SDK_LIBRARIES ${COMPONENT_LIB} PARENT_SCOPE)
set(VOLC_CONV_AI_SDK_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../inc PARENT_SCOPE)
