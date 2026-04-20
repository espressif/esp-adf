# Changelog

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
