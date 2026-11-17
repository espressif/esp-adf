idf_component_register(
    SRCS
        "app_main.c"
        "test_fixtures.c"
        "test_storage.c"
        "test_media_db.c"
        "test_playlist.c"
        "test_playlist_filter.c"
        "test_playlist_ram.c"
        "test_lifecycle_leak.c"
        "test_table_db.c"
        "test_concurrency.c"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS "../../private_inc"
    PRIV_REQUIRES
        esp_playlist
        unity
        fatfs
        wear_levelling
        heap
    WHOLE_ARCHIVE
)

target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=calloc,--wrap=strdup")

fatfs_create_spiflash_image(storage ../fatfsgen FLASH_IN_PROJECT)
