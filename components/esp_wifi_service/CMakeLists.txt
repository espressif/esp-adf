set(srcs
    "src/esp_wifi_service.c"
    "src/provisioning/esp_wifi_service_prov.c"
    "src/profile/esp_wifi_service_profile_mgr.c"
    "src/selector/esp_wifi_service_selector.c"
    "src/selector/esp_wifi_service_sel_blacklist.c"
)

set(reqs esp_http_server esp_netif esp_wifi)

set(priv_reqs bt mbedtls esp_hw_support wpa_supplicant esp_http_client)
set(priv_includes "private_inc")
set(embed_txtfiles)
set(embed_files)

if(CONFIG_WIFI_SERVICE_SELECTOR_PROBE_ENABLE)
    list(APPEND srcs "src/selector/esp_wifi_service_sel_probe.c")
endif()

if(CONFIG_WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE)
    list(APPEND srcs "src/selector/esp_wifi_service_sel_throughput.c")
endif()

if(CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE)
    list(APPEND srcs "src/provisioning/esp_wifi_service_prov_blufi.c")
endif()

if(CONFIG_WIFI_SERVICE_PROV_HTTP_ENABLE)
    list(APPEND srcs
        "src/provisioning/esp_wifi_service_prov_http.c"
        "src/provisioning/esp_wifi_service_prov_softap.c"
    )
    if(CONFIG_WIFI_SERVICE_PROV_HTTP_DEFAULT_WEBUI_ENABLE)
        list(APPEND embed_txtfiles
            "src/provisioning/esp_wifi_service_prov_default_webui.html"
        )
    endif()
endif()

if(CONFIG_WIFI_SERVICE_MCP_ENABLE)
    list(APPEND srcs "src/mcp/esp_wifi_service_mcp.c")
    list(APPEND embed_files "src/mcp/esp_wifi_service_mcp.json")
endif()

idf_component_register(
    SRCS         ${srcs}
    EMBED_TXTFILES ${embed_txtfiles}
    EMBED_FILES ${embed_files}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS ${priv_includes}
    REQUIRES     ${reqs}
    PRIV_REQUIRES ${priv_reqs}
)

idf_component_optional_requires(PUBLIC json)
