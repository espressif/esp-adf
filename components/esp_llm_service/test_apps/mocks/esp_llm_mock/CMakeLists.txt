# esp_llm_service is declared in idf_component.yml.
idf_component_register(
    SRCS         "src/esp_llm_mock_adapter.c"
    INCLUDE_DIRS "include"
)

idf_component_get_property(esp_llm_service_dir esp_llm_service COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${esp_llm_service_dir}/private_include")
