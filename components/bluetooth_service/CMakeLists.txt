set(COMPONENT_ADD_INCLUDEDIRS include)

# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES nvs_flash)

set(COMPONENT_SRCS ./bluetooth_service.c ./bt_keycontrol.c)

register_component()
