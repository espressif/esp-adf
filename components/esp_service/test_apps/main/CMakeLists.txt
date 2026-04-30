#
# IDF on-device runner for the esp_service test_apps.  Builds a single firmware
# that registers the six mock services (pulled via EXTRA_COMPONENT_DIRS) with a
# service manager and exposes them over the MCP transports selected in Kconfig.

set(srcs
    "app_main.c"
    "ut_esp_service_core.c"
    "ut_service_manager.c"
)

if(CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS)
    list(APPEND srcs "ut_transport_lifecycle_test.c")
endif()

if(CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT)
    list(APPEND srcs "ut_http_transport.c")
endif()

if(CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT)
    list(APPEND srcs "ut_sse_transport.c")
endif()

if(CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT)
    list(APPEND srcs "ut_ws_transport.c")
endif()

if(CONFIG_ESP_SVC_TEST_ENABLE_UART_TRANSPORT)
    list(APPEND srcs "ut_uart_transport.c")
endif()

idf_component_register(
    SRCS            ${srcs}
    INCLUDE_DIRS    "."
    REQUIRES        esp_service
                    button_service
                    cloud_service
                    led_service
                    ota_service
                    player_service
                    wifi_service
                    json esp_wifi nvs_flash
    PRIV_REQUIRES   unity
)
