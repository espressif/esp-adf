idf_component_register(
    SRCS
        "app_main.c"
        "simple_sip.c"
    PRIV_REQUIRES nvs_flash esp_netif esp_wifi
)
