set(srcs
    "app_main.c"
    "ut_esp_sip_service.c"
)

if(CONFIG_ESP_SIP_SERVICE_MCP_ENABLE)
    list(APPEND srcs "test_mcp.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity esp_timer esp_wifi nvs_flash
)
