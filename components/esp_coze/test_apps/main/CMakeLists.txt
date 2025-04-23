idf_component_register(SRC_DIRS .
                       PRIV_INCLUDE_DIRS .
                       PRIV_REQUIRES unity esp_coze nvs_flash esp_event esp_wifi
                       WHOLE_ARCHIVE TRUE
                       EMBED_FILES nihao.pcm
                       )
