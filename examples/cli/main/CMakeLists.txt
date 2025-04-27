set(COMPONENT_SRCS "console_example.c")
set(COMPONENT_ADD_INCLUDEDIRS .)

IF (CONFIG_SOC_BT_SUPPORTED)
    list(APPEND COMPONENT_SRCS  "ble_gatts_module.c")
ENDIF()

register_component()