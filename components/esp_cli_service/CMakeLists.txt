idf_component_register(
    SRCS
        "esp_cli_service.c"
        "esp_cli_service_cmd_dynamic.c"
        "esp_cli_service_cmd_static.c"
        "esp_cli_service_parse.c"
    INCLUDE_DIRS "include"
    REQUIRES
        console
        vfs
        json
)
