idf_component_register(
    SRCS "app_main.c"
         "test_wifi_service_storage.c"
         "test_mcp.c"
         "test_profile_mgr.c"
         "test_prov_base.c"
         "test_prov_optional.c"
         "test_wifi_service.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity test_utils
                  esp_wifi_service esp_config_manager
                  esp_service nvs_flash esp_wifi esp_netif esp_event
    WHOLE_ARCHIVE
)
