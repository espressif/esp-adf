# Changelog

## v0.5.0

### Features

- Initial version of `esp_service`
- `esp_service_t` lifecycle base class with `UNINITIALIZED → INITIALIZED → RUNNING ⇄ PAUSED` state machine
- Vtable-based subclassing via `esp_service_ops_t` (`on_init`, `on_start`, `on_stop`, `on_pause`, `on_resume`, low-power hooks)
- `esp_service_manager` dynamic service registry with batch `start_all` / `stop_all` and lookup by name or category
- Automatic JSON-schema tool discovery and dispatch via `esp_service_manager_invoke_tool()`
- Optional MCP (Model Context Protocol) 2024-11-05 server (`CONFIG_ESP_MCP_ENABLE`)
- Pluggable MCP transports: HTTP, SSE, WebSocket, UART, STDIO, SDIO
- Per-service `adf_event_hub_t` for publish/subscribe domain events
