get_directory_property(defined_defs COMPILE_DEFINITIONS)
message("Defined preprocessor variables-USER: ${defined_defs}")

idf_component_register(SRC_DIRS "./"
                       INCLUDE_DIRS "./"
                       REQUIRES "sdmmc" "fatfs" "esp_wifi" "nvs_flash")
