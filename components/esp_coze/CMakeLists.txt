set(public_include_dirs "./include")
idf_component_register(INCLUDE_DIRS "${public_include_dirs}"
                       SRCS "http_client_request.c"
                       PRIV_REQUIRES esp_http_client
                       WHOLE_ARCHIVE)
add_prebuilt_library(esp_coze "${CMAKE_CURRENT_LIST_DIR}/libs/${CONFIG_IDF_TARGET}/libcoze_websocket.a"
                     PRIV_REQUIRES mbedtls espressif__esp_websocket_client json)
target_link_libraries(${COMPONENT_LIB} INTERFACE esp_coze)
