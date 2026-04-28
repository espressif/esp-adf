set(COMPONENT_SRCS
    src/esp_media_db.c
    src/epl_table_db.c
    src/epl_db_storage_memory.c
    src/epl_db_storage_vfs.c
    src/epl_media_lib.c
    src/esp_playlist.c
    helper/epl_cvector.c
)

set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_PRIV_INCLUDEDIRS private_inc)

idf_component_register(
    SRCS "${COMPONENT_SRCS}"
    INCLUDE_DIRS "${COMPONENT_ADD_INCLUDEDIRS}"
    PRIV_INCLUDE_DIRS "${COMPONENT_PRIV_INCLUDEDIRS}"
    PRIV_REQUIRES esp_timer freertos
)

idf_component_optional_requires(PRIVATE json cjson)
