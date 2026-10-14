idf_component_register(
    SRCS
        "test_app_main.c"
        "test_esp_tongyi_protocol.c"
        "test_esp_tongyi_transport.c"
        "test_esp_tongyi_session.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity esp_llm_tongyi nvs_flash esp_event esp_netif
    WHOLE_ARCHIVE
)

idf_component_get_property(tongyi_dir esp_llm_tongyi COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${tongyi_dir}/components/esp_tongyi/private_include")
