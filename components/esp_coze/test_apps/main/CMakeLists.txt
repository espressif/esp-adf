set(priv_reqs unity esp_coze nvs_flash esp_event esp_netif esp_websocket_client)
if("${IDF_VERSION_MAJOR}" VERSION_LESS "6")
    list(APPEND priv_reqs json)
endif()

idf_component_register(SRC_DIRS .
                       PRIV_INCLUDE_DIRS .
                       PRIV_REQUIRES ${priv_reqs}
                       WHOLE_ARCHIVE TRUE
                       )
