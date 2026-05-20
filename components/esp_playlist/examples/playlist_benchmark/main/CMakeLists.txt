idf_component_register(SRCS "playlist_benchmark.c"
                    "benchmark_common.c"
                    "media_db_benchmark.c"
                    "playlist_benchmark_tests.c"
                    "startup_restore_check.c"
                    "storage.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_timer fatfs esp_playlist)
