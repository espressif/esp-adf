set(srcs app_main.c esp_ota_service_fs_example.c)

if(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC)
    list(APPEND srcs esp_ota_service_fs_example_usb.c)
endif()

set(priv_requires app_update esp_app_format)
if(CONFIG_OTA_FS_EXAMPLE_STORAGE_USB_MSC)
    list(APPEND priv_requires usb)
endif()

idf_component_register(
    SRCS          ${srcs}
    INCLUDE_DIRS  "."
    REQUIRES      nvs_flash fatfs esp_ota_service
    PRIV_REQUIRES ${priv_requires}
)
