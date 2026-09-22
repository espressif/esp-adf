set(srcs
    "src/esp_sip_service.c"
    "src/esp_sip_signaling.c"
)

if(CONFIG_ESP_SIP_SERVICE_UPLINK_SUPPORT)
    list(APPEND srcs "src/esp_sip_uplink.c")
endif()

if(CONFIG_ESP_SIP_SERVICE_DOWNLINK_SUPPORT)
    list(APPEND srcs "src/esp_sip_downlink.c")
endif()

set(embed_txtfiles)

if(CONFIG_ESP_SIP_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_sip_service_mcp.c")
    list(APPEND embed_txtfiles "src/mcp/esp_sip_service_mcp.json")
endif()

idf_component_register(
    SRCS         ${srcs}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private_inc"
    EMBED_TXTFILES ${embed_txtfiles}
)

idf_component_optional_requires(PRIVATE json)
