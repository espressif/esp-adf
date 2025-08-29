# Changelog

## v0.5.0

### Features

- Initial version of `esp_ota_service`
- Composable four-layer OTA pipeline: `esp_ota_service_source_t`, `esp_ota_service_checker_t`, `esp_ota_service_verifier_t`, `esp_ota_service_target_t`
- Built-in sources: HTTP/HTTPS (`esp_ota_service_source_http_create()`), local filesystem (`esp_ota_service_source_fs_create()`)
- Optional source: BLE GATT peripheral speaking the official ESP BLE OTA APP protocol (`esp_ota_service_source_ble_create()`) behind Kconfig
- Built-in targets: app partition (`esp_ota_service_target_app_create()`), raw data partition (`esp_ota_service_target_data_create()`)
- Optional target: bootloader (`esp_ota_service_target_bootloader_create()`) behind Kconfig
- Built-in checkers: `esp_ota_service_checker_app_create()`, `esp_ota_service_checker_data_version_create()`, `esp_ota_service_checker_tone_desc_create()`, `esp_ota_service_checker_manifest_create()`
- Built-in verifiers: SHA-256 (`esp_ota_service_verifier_sha256_create()`), MD5 (`esp_ota_service_verifier_md5_create()`)
- NVS-backed resume with HTTP Range / `fseek` checkpointing
- Rollback support with pending-verify state detection (`CONFIG_OTA_ENABLE_ROLLBACK`)
- Pause / resume mid-download via `esp_service_pause()` / `esp_service_resume()`
- Rich event bus covering session begin/end, version check, and per-item begin/progress/end
- Optional MCP tool integration for remote OTA invocation (`CONFIG_ESP_MCP_ENABLE`)
