set(COMPONENT_SRCS
    app_main.c
    esp_dual_eye_ui.c
    esp_dual_eye_ui_avi.c
    esp_dual_eye_player.c
)

set(COMPONENT_INCLUDEDIRS include  dual_lcd .)

list(APPEND COMPONENT_SRCS
    dual_lcd/esp_dual_display.c
    dual_lcd/esp_dual_lcd.c
)

idf_component_register(SRCS "${COMPONENT_SRCS}"  INCLUDE_DIRS "${COMPONENT_INCLUDEDIRS}")

# If you first use it, need to run the following command to download files into flash
spiffs_create_partition_assets(assets ../images/ FLASH_IN_PROJECT  MMAP_FILE_SUPPORT_FORMAT ".jpg,.png,.ttf,.gif,.avi")
littlefs_create_partition_image(storage ../images/ FLASH_IN_PROJECT)
