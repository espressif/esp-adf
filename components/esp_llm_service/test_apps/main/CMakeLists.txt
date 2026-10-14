idf_component_register(
    SRCS
        "app_main.c"
        "fake_llm_harness.c"
        "ut_esp_llm_service.c"
        "test_mcp.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES unity
    WHOLE_ARCHIVE
)

idf_component_get_property(esp_llm_service_dir esp_llm_service COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${esp_llm_service_dir}/private_include")

idf_component_optional_requires(PRIVATE json espressif__cjson)
