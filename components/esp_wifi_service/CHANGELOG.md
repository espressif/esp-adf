# Changelog

## v0.5.2

### Features

- Added `esp_wifi_service_request_connect()` for saving or updating a profile and requesting an immediate selector-driven connection
- Added `ESP_WIFI_SERVICE_EVENT_STA_CONFIG` so applications can adjust STA config before HTTP provisioning, BluFi provisioning, selector switching, or direct connect applies it

### Changes

- Added `esp_wifi_service` test apps and CI coverage for profile management, provisioning, storage, and direct connect flows
- Added a shared Wi-Fi scan agent for HTTP provisioning, BluFi provisioning, and selector reevaluation, with scan requests batched by configuration and BluFi Wi-Fi list requests always receiving results, an empty list, or an explicit failure

## v0.5.1

### Changes

- Added support for ESP-IDF v6.0 and later

## v0.5.0

### Features

- Initial version of `esp_wifi_service`
- ADF service integration for Wi-Fi lifecycle, provisioning, connection selection, and events
- Profile manager for multiple Wi-Fi credentials with add, update, enable, disable, delete, and cleanup operations
- Replaceable profile storage media including NVS, file system, raw dual-partition flash, and custom adapters
- HTTP SoftAP/Web UI provisioning with captive portal support and cached scan results
- BluFi provisioning support using the shared profile manager
- Automatic startup policy that selects saved networks or starts configured provisioning agents
- Wi-Fi selector with priority, RSSI, probe health, temporary blocklist, and re-evaluation support
- Optional MCP tools for remote status query and profile/provisioning management
