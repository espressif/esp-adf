if(CONFIG_BT_BLUEDROID_ENABLED AND CONFIG_BT_NIMBLE_ENABLED)
    message(FATAL_ERROR "esp_bt_hub: enable only one of Bluedroid or NimBLE")
endif()

set(srcs)
set(requires esp_common freertos log bt)

if(CONFIG_BT_NIMBLE_ENABLED OR CONFIG_BT_BLUEDROID_ENABLED)
    list(APPEND srcs "src/esp_bt_hub.c")
endif()
if(CONFIG_BT_NIMBLE_ENABLED)
    list(APPEND srcs "src/esp_bt_hub_nimble.c")
elseif(CONFIG_BT_BLUEDROID_ENABLED)
    list(APPEND srcs "src/esp_bt_hub_bluedroid.c")
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_inc"
                       REQUIRES ${requires})
