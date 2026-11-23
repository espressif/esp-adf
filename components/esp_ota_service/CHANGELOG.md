# Changelog

## v1.0.0-beta1

### Breaking changes

- A verifier rejection (`verify_begin`, `verify_update` or `verify_finish`) now ends the item with `ESP_OTA_SERVICE_ITEM_STATUS_FAILED` and `ESP_OTA_SERVICE_ITEM_END_REASON_VERIFIER_REJECTED`; `ESP_OTA_SERVICE_ITEM_STATUS_SKIPPED` is no longer emitted. Handlers that matched `SKIPPED` must match `FAILED` + `VERIFIER_REJECTED` instead
- `esp_ota_service_set_upgrade_list()` and `esp_ota_service_check_update()` return `ESP_ERR_INVALID_STATE` unless the service is `INITIALIZED`, idle, and the previous worker has been joined (including MCP `esp_ota_service_check_update` calls while a session runs; poll `esp_ota_service_get_version` for cached metadata during downloads). `esp_ota_service_set_upgrade_list()` also rejects a non-NULL checker whose `check` callback is NULL with `ESP_ERR_INVALID_ARG`
- `esp_ota_service_set_upgrade_list()` no longer clears NVS resume records; they are matched by `(item_index, uri)` and cleared once the write phase finishes by commit, target abort, or verifier rejection (early open/allocation failures and `esp_service_stop()` preserve them)
- `esp_ota_service_checker_app_create(NULL)` takes `require_higher_version` from `CONFIG_OTA_FORCE_VERSION_CHECK`, whose default changed to `y`
- Version strings must be `[v]x.y.z` with each part 0-255; partial, suffixed (`-rc1`) or four-part versions are rejected (`ESP_ERR_INVALID_VERSION` in descriptor/data-header checks and unparsable running versions, or `upgrade_available = false` for unparsable incoming versions in the app/manifest/tone checkers). The manifest checker also requires `project_name` when `check_project_name` and `current_project` are set

### Bugfix

- Return `ESP_ERR_TIMEOUT` from `esp_service_stop()` / `esp_ota_service_destroy()` if the worker has not exited, retaining the instance and its resources for retry; join an already-exited worker immediately and reject worker-context self-stop calls with `ESP_ERR_INVALID_STATE`
- Fixed a stale NVS resume offset being applied to an item with a streaming verifier, which produced a digest over a partial image; such items now restart from 0, drop the record, and skip writing checkpoints
- Fixed a failed item leaving its NVS resume record behind after writing, so a later session resumed into an aborted partition
- Fixed `esp_ota_service_confirm_update()` dereferencing a NULL running partition; it now returns `ESP_ERR_INVALID_STATE`
- Fixed the SHA-256 / MD5 verifiers leaking the hash context when an item was aborted before `verify_finish`, and freed/cleared legacy mbedTLS contexts on `finish()`
- Fixed the data-partition target writing before a successful `open()` (`ESP_ERR_INVALID_STATE`) and returned erase errors from `abort()`
- Fixed `esp_app_desc_t::version` / `project_name` being read past their fixed-size fields in the app and tone-descriptor checkers
- Fixed the data-version checker accepting a header with a non-zero top byte and a `current_version` that does not parse
- Fixed the manifest checker checking `content_length` before HTTP status, ignoring `301/302/303/307/308` redirects, accepting `size` values at or above `UINT32_MAX`, and failing to build when `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE` is disabled
- Fixed the HTTP source leaking its leftover buffer, and fixed the bootloader target missing `esp_flash.h` on IDF 6.x and ignoring staging erase errors in `abort()`
- Fixed the MCP tools composing JSON with `snprintf`, ignoring `cJSON_Add*` OOM failures, reporting `4294967295` instead of `-1` for unknown `total_bytes`/`size`, accessing the update-info cache without a lock, and embedding the schema without a NUL terminator (`EMBED_TXTFILES`)
- Fixed `tools/*.py` using nested same-quote f-strings that fail on Python < 3.12, and set default `x.y.z` project versions in the `ota_http`, `ota_fs` and `ota_ble` examples so bare `idf.py build` images pass strict semver checks without a pre-existing `version.txt`

### Changes

- Documented thread safety, the stop / destroy ordering, resume-record lifetime, accepted version formats and the event contract in `README.md`, `README_CN.md`, user guides and `esp_ota_service.h`
- Added regression tests in `test_apps` covering session restart, PAUSED list replacement, non-blocking `check_update` and busy-state rejection, multi-item verifier halt, resume with a verifier, MCP schema/bounds/cache, and unit checks for version parsing, checkers, verifiers and the data target
- Declared `license: LicenseRef-Espressif-Modified-MIT` and excluded `test_apps/` and `tools/esp_at_esp32c6/` bridge binaries from the component archive
- Required `esp_service` `~1.0.0-beta1`

## v0.5.2~3

### Changes

- Updated depended components `gmf_io` to ~1.1

## v0.5.2~2

### Bug Fixes

- Fixed examples and test app to depend on the local component via `override_path` instead of the whole `components/` tree

## v0.5.2~1

### Bug Fixes

- Updated the `esp-board-manager` dependency version for the test app.

## v0.5.2

### Changes

- Bump `gmf_io` dependency version to v1.0

## v0.5.1

### Changes

- Added support for ESP-IDF v6.0 and later

## v0.1.0~1

### Changes

- Renamed Kconfig menu to `ESP-Service: OTA Service` for consistent menuconfig grouping
- Added Component Registry badge to README.md and README_CN.md

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
