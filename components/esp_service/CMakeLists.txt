# Service base layer — always compiled
set(srcs
    "service/esp_service.c"
    "service/esp_service_manager.c"
)
set(requires freertos esp_timer log json adf_event_hub esp_http_server esp_driver_uart)
set(priv_requires)

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
