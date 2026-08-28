idf_component_register(
    SRCS
        "app_main.c"
        "simple_rtsp.c"
        "rtsp_mcp.c"
        "rtsp_scheduler.c"
    PRIV_REQUIRES nvs_flash esp_netif
)
