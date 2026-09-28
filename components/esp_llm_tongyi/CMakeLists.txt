# Component dependencies are declared in idf_component.yml.
# Tongyi SDK sources are vendored under components/esp_tongyi and are not a separate component.
idf_component_register(
    SRCS
        "src/esp_llm_tongyi_adapter.c"
        "components/esp_tongyi/src/esp_tongyi.c"
        "components/esp_tongyi/src/esp_tongyi_session.c"
        "components/esp_tongyi/src/esp_tongyi_transport.c"
        "components/esp_tongyi/src/esp_tongyi_protocol.c"
        "components/esp_tongyi/src/esp_tongyi_utils.c"
    INCLUDE_DIRS "include" "components/esp_tongyi/include"
    PRIV_INCLUDE_DIRS "components/esp_tongyi/private_include"
    REQUIRES esp_websocket_client
    PRIV_REQUIRES mbedtls esp_timer gmf_core
)

idf_component_optional_requires(PRIVATE json espressif__cjson)

idf_component_get_property(esp_llm_service_dir esp_llm_service COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${esp_llm_service_dir}/private_include")
