
idf_component_register(
    SRCS "app_main.c"
         "esp_ota_service_test_mocks.c"
         "esp_ota_service_test_fixtures.c"
         "test_esp_ota_service.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../src/resume"
    PRIV_REQUIRES unity test_utils
                  esp_ota_service mbedtls
                  nvs_flash
                  fatfs vfs
                  espressif__esp_board_manager
    WHOLE_ARCHIVE
)
