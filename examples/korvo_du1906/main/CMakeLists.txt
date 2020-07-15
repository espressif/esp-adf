set(COMPONENT_SRCS  main.c
                    app_sys_tools.c
                    app_player_init.c
                    app_control.c
                    app_bt_init.c
                    app_ota_upgrade.c
                    )


set(COMPONENT_ADD_INCLUDEDIRS . )


set(COMPONENT_REQUIRES audio_sal audio_hal esp_dispatcher esp_peripherals display_service input_key_service bluetooth_service wifi_service audio_player audio_flash_tone bdsc_engine bds_light ota_service mqtt json)

register_component()
