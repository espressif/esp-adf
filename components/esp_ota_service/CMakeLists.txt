set(src_root "src")

set(srcs
    "${src_root}/esp_ota_service.c"
    "${src_root}/esp_ota_service_app_desc_reader.c"
    "${src_root}/esp_ota_service_version_utils.c"
    "${src_root}/checker/esp_ota_service_checker_app.c"
    "${src_root}/checker/esp_ota_service_checker_data_version.c"
    "${src_root}/checker/esp_ota_service_checker_tone_desc.c"
    "${src_root}/checker/esp_ota_service_checker_manifest.c"
    "${src_root}/source/esp_ota_service_source_http.c"
    "${src_root}/source/esp_ota_service_source_fs.c"
    "${src_root}/target/esp_ota_service_target_app.c"
    "${src_root}/target/esp_ota_service_target_data.c"
    "${src_root}/verifier/esp_ota_service_verifier_sha256.c"
    "${src_root}/verifier/esp_ota_service_verifier_md5.c"
    "${src_root}/resume/esp_ota_service_resume.c"
)

set(embed_files "")

set(requires
    app_update
    esp_app_format
    esp_http_client
    esp_https_ota
    json
    nvs_flash
)

set(priv_requires
    mbedtls
    bootloader_support
    spi_flash
)

if(CONFIG_ESP_MCP_ENABLE)
    list(APPEND srcs        "${src_root}/esp_ota_service_mcp.c")
    list(APPEND embed_files "${src_root}/esp_ota_service_mcp.json")
endif()

if (CONFIG_OTA_ENABLE_BOOTLOADER_OTA)
    list(APPEND srcs "${src_root}/target/esp_ota_service_target_bootloader.c")
endif()

if (CONFIG_OTA_ENABLE_BLE_SOURCE)
    list(APPEND srcs "${src_root}/source/esp_ota_service_source_ble.c")
endif()

idf_component_register(
    SRCS             ${srcs}
    INCLUDE_DIRS     "include" "include/checker" "include/source" "include/target" "include/verifier"
    PRIV_INCLUDE_DIRS "${src_root}/resume"
    REQUIRES         ${requires}
    PRIV_REQUIRES    ${priv_requires}
    EMBED_FILES      ${embed_files}
)

if (CONFIG_OTA_ENABLE_BLE_SOURCE)
    # BLE source implements the official ESP BLE OTA APP protocol natively
    # on top of the NimBLE stack, so the only Bluetooth dependency is the
    # controller/host bundled with `bt`.
    idf_component_optional_requires(PRIVATE bt)
endif()
