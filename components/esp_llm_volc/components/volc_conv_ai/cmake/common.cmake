set(VOLC_CONV_AI_SRCS "${CMAKE_CURRENT_LIST_DIR}/../src/volc_conv_ai.c"
                "${CMAKE_CURRENT_LIST_DIR}/../src/base/volc_device_manager.c"
                "${CMAKE_CURRENT_LIST_DIR}/../src/util/volc_auth.c"
                "${CMAKE_CURRENT_LIST_DIR}/../src/util/volc_base64.c"
                "${CMAKE_CURRENT_LIST_DIR}/../src/util/volc_http.c"
                "${CMAKE_CURRENT_LIST_DIR}/../src/util/volc_json.c"
                "${CMAKE_CURRENT_LIST_DIR}/../third_party/mbedtls_port/tls_certificate.c"
                "${CMAKE_CURRENT_LIST_DIR}/../third_party/mbedtls_port/tls_client.c"
                "${CMAKE_CURRENT_LIST_DIR}/../third_party/mbedtls_port/volc_mbedtls_compat.c"
                "${CMAKE_CURRENT_LIST_DIR}/../third_party/mbedtls_port/volc_rtc_mbedtls_idf6.c"
                "${CMAKE_CURRENT_LIST_DIR}/../third_party/webclient/src/webclient.c"
                CACHE INTERNAL "ConversationalAI-Embedded-Kit-2.0 common src file")

set(VOLC_CONV_AI_INCS "${CMAKE_CURRENT_LIST_DIR}/../inc"
                "${CMAKE_CURRENT_LIST_DIR}/../src"
                "${CMAKE_CURRENT_LIST_DIR}/../third_party/mbedtls_port"
                "${CMAKE_CURRENT_LIST_DIR}/../third_party/webclient/inc"
                CACHE INTERNAL "ConversationalAI-Embedded-Kit-2.0 common include dir")

set(VOLC_CONV_AI_LOW_LOAD_SRCS "${CMAKE_CURRENT_LIST_DIR}/../src/transports/low_load/src/volc_ws.c"
                        "${CMAKE_CURRENT_LIST_DIR}/../src/transports/low_load/third_party/websocket/websocket.c"
                        CACHE INTERNAL "ConversationalAI-Embedded-Kit-2.0 low load solution src file")
set(VOLC_CONV_AI_LOW_LOAD_INCS "${CMAKE_CURRENT_LIST_DIR}/../src/transports/low_load/inc"
                        "${CMAKE_CURRENT_LIST_DIR}/../src/transports/low_load/third_party/websocket"
                        CACHE INTERNAL "ConversationalAI-Embedded-Kit-2.0 low load solution include dir")

set(VOLC_CONV_AI_HIGH_QUALITY_SRCS "${CMAKE_CURRENT_LIST_DIR}/../src/transports/high_quality/src/volc_rtc.c"
                            CACHE INTERNAL "ConversationalAI-Embedded-Kit-2.0 high quality solution src file")
set(VOLC_CONV_AI_HIGH_QUALITY_INCS "${CMAKE_CURRENT_LIST_DIR}/../src/transports/high_quality/inc"
                                    "${CMAKE_CURRENT_LIST_DIR}/../src/transports/high_quality/third_party/volc_rtc_engine_lite/inc"
                        CACHE INTERNAL "ConversationalAI-Embedded-Kit-2.0 high quality solution include dir")

set(VOLC_CONV_AI_PLATFORM_INCS "${CMAKE_CURRENT_LIST_DIR}/../osal/inc"
                        CACHE INTERNAL "ConversationalAI-Embedded-Kit-2.0 platform include dir")

