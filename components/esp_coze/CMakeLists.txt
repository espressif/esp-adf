set(public_include_dirs "./include")

set(srcs
    "src/coze_envelope.c"
    "src/coze_router.c"
    "src/coze_transport.c"
    "src/esp_coze_chat.c"
    "src/esp_coze_chat_event.c"
    "src/esp_coze_chat_protocol.c"
    "src/esp_coze_tts.c"
    "src/esp_coze_asr.c"
    "src/esp_coze_trace.c"
    "src/esp_coze_jwt.c"
    "src/esp_coze_http.c"
)

set(esp_coze_priv_requires esp_http_client mbedtls)
if("${IDF_VERSION_MAJOR}" VERSION_LESS "6")
    list(APPEND esp_coze_priv_requires json)
endif()

idf_component_register(INCLUDE_DIRS "${public_include_dirs}"
                       SRCS ${srcs}
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES esp_common esp_websocket_client
                       PRIV_REQUIRES ${esp_coze_priv_requires}
                       WHOLE_ARCHIVE)
