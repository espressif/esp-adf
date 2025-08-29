idf_component_register(
    SRCS         "adf_event_hub.c"
                 "adf_vec.c"
                 "adf_mem.c"
    INCLUDE_DIRS "include"
    REQUIRES     freertos esp_timer log heap esp_system
)
