idf_component_register(
    SRCS
        "app_main.c"
        "simple_rtmp.c"
        "rtmp_mcp.c"
        "rtmp_scheduler.c"
    PRIV_REQUIRES nvs_flash esp_netif
)
