# Service base layer — always compiled
set(srcs
    "service/esp_service.c"
    "service/esp_service_manager.c"
    "service/esp_service_scheduler.c"
)
set(requires freertos esp_timer log adf_event_hub esp_driver_uart esp_http_server)
# Requirements are resolved before sdkconfig is loaded, so a dependency added
# under if(CONFIG_...) is ignored; esp_driver_sdio must be listed unconditionally.
set(priv_requires esp_driver_sdio)

# MCP layer — compiled only when CONFIG_ESP_MCP_ENABLE=y
if(CONFIG_ESP_MCP_ENABLE)
    list(APPEND srcs
        "mcp/esp_service_mcp_trans.c"
        "mcp/esp_service_mcp_server.c"
    )

    if(CONFIG_ESP_MCP_TRANSPORT_HTTP)
        list(APPEND srcs "mcp/transport/esp_service_mcp_trans_http.c")
    endif()

    if(CONFIG_ESP_MCP_TRANSPORT_SSE)
        list(APPEND srcs "mcp/transport/esp_service_mcp_trans_sse.c")
    endif()

    if(CONFIG_ESP_MCP_TRANSPORT_WS)
        list(APPEND srcs "mcp/transport/esp_service_mcp_trans_ws.c")
    endif()

    if(CONFIG_ESP_MCP_TRANSPORT_UART)
        list(APPEND srcs "mcp/transport/esp_service_mcp_trans_uart.c")
    endif()

    if(CONFIG_ESP_MCP_TRANSPORT_STDIO)
        list(APPEND srcs "mcp/transport/esp_service_mcp_trans_stdio.c")
    endif()

    if(CONFIG_ESP_MCP_TRANSPORT_SDIO)
        list(APPEND srcs "mcp/transport/esp_service_mcp_trans_sdio.c")
    endif()
endif()

idf_component_register(
    SRCS         ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES     ${requires}
    PRIV_REQUIRES ${priv_requires}
)
