#
# IDF on-device runner for the esp_service test_apps.  Builds a single firmware
# that registers the six mock services (pulled via EXTRA_COMPONENT_DIRS) with a
# service manager and exposes them over the MCP transports selected in Kconfig.

get_filename_component(COMPONENTS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
idf_build_get_property(ESP_SERVICE_TEST_FAULT_INJECTION ESP_SERVICE_TEST_FAULT_INJECTION)

if(ESP_SERVICE_TEST_FAULT_INJECTION)
    if(NOT CMAKE_BUILD_EARLY_EXPANSION)
        if(NOT CONFIG_ESP_MCP_ENABLE OR CONFIG_ESP_MCP_TRANSPORT_HTTP OR
           CONFIG_ESP_MCP_TRANSPORT_SSE OR CONFIG_ESP_MCP_TRANSPORT_WS OR
           CONFIG_ESP_MCP_TRANSPORT_UART OR CONFIG_ESP_MCP_TRANSPORT_STDIO OR
           CONFIG_ESP_MCP_TRANSPORT_SDIO)
            message(FATAL_ERROR "Fault injection requires MCP enabled and real transports disabled; use sdkconfig.emu")
        endif()
    endif()
    set(HUB_EXAMPLES "${COMPONENTS_ROOT}/adf_event_hub/examples/common")
    idf_component_register(
        SRCS "app_main.c" "ut_transport_serial_fault.c" "ut_esp_service_mcp_server.c"
             "ut_event_hub_demo.c" "ut_transport_web_fault.c" "ut_scheduler_and_mock_tools.c"
             "${HUB_EXAMPLES}/demo_button_service.c"
             "${HUB_EXAMPLES}/demo_wifi_service.c"
             "${HUB_EXAMPLES}/demo_ota_service.c"
             "${HUB_EXAMPLES}/demo_player_service.c"
             "${HUB_EXAMPLES}/demo_led_service.c"
        PRIV_INCLUDE_DIRS "." "stubs" "${HUB_EXAMPLES}"
        REQUIRES unity esp_service adf_event_hub esp_timer heap
                 led_service player_service wifi_service
    )
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ESP_SERVICE_TEST_FAULT_INJECTION=1)
    return()
endif()

set(srcs
    "app_main.c"
    "ut_esp_service.c"
    "ut_service_manager.c"
    "ut_derived_destroy.c"
    "${COMPONENTS_ROOT}/esp_rtmp_service/examples/rtmp_service/main/simple_rtmp.c"
    "${COMPONENTS_ROOT}/esp_rtsp_service/examples/rtsp_service/main/simple_rtsp.c"
    "${COMPONENTS_ROOT}/esp_extractor_service/examples/extractor_service/main/extractor_demo.c"
)

if(CONFIG_ESP_MCP_ENABLE)
    list(APPEND srcs "ut_esp_service_mcp_server.c")
endif()

if(CONFIG_ESP_SVC_TEST_ENABLE_LIFECYCLE_STRESS)
    list(APPEND srcs "ut_transport_lifecycle.c")
endif()

if(CONFIG_ESP_SVC_TEST_ENABLE_HTTP_TRANSPORT OR CONFIG_ESP_SVC_TEST_ENABLE_SSE_TRANSPORT OR
   CONFIG_ESP_SVC_TEST_ENABLE_WS_TRANSPORT)
    list(APPEND srcs "ut_transport_web_smoke.c")
endif()

if(CONFIG_ESP_SVC_TEST_ENABLE_UART_TRANSPORT)
    list(APPEND srcs "ut_transport_uart_smoke.c")
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
                    esp_wifi nvs_flash
                    esp_rtmp_service esp_rtsp_service esp_extractor_service
                    esp_media_service esp_netif
    PRIV_REQUIRES   unity esp_timer heap
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    CONFIG_RTMP_EXAMPLE_PUSH_URL="rtmp://127.0.0.1/live/test"
    CONFIG_RTMP_EXAMPLE_PULL_URL="rtmp://127.0.0.1/live/test"
    CONFIG_RTMP_EXAMPLE_SERVER_URL="rtmp://127.0.0.1/live/test"
    CONFIG_RTMP_EXAMPLE_SERVER_STREAM="test"
    CONFIG_RTSP_EXAMPLE_PUSH_URL="rtsp://127.0.0.1/live/test"
    CONFIG_RTSP_EXAMPLE_PULL_URL="rtsp://127.0.0.1/live/test"
    CONFIG_RTSP_EXAMPLE_SERVER_URL="rtsp://127.0.0.1/live/test"
)
