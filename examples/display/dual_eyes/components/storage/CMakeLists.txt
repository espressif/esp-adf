idf_component_register(SRCS "benchmark.c" "flash_littlefs.c" "flash_mmap.c" "sdcard.c"
                       INCLUDE_DIRS "./"
                       REQUIRES "sdmmc" "fatfs" "esp_mmap_assets" "littlefs")
