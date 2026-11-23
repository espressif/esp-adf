idf_component_register(
    SRCS         "adf_event_hub.c"
                 "adf_vec.c"
                 "adf_mem.c"
    INCLUDE_DIRS "include"
    REQUIRES     freertos log
    PRIV_REQUIRES heap esp_system
)
