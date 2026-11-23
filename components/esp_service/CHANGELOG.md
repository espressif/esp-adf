# Changelog

## v1.0.0-beta1

### Features

- Added manager-owned tool snapshots for MCP requests and concurrent unregister
- Added optional bearer-token authentication to HTTP, SSE and WebSocket transports
- Added host, ESP32-S3 EMU and PC-driven transport regressions, plus authenticated Python clients compatible with Python 3.8 and later

### Bugfix

- Serialized lifecycle transitions with an instance-owned mutex; ran hooks and synchronous state notifications outside the lock and rejected overlapping calls on the same instance
- Rejected invalid pause/resume and low-power calls on uninitialized or closing instances
- Ran teardown hooks outside the lifecycle lock, closed publish/subscribe admission without blocking joined workers, and drained event users before private cleanup
- Rejected duplicate service/tool names, rolled back failed auto-start registration, returned the first start/stop error and stopped services in reverse registration order
- Copied tool data before invocation and NUL-terminated full result buffers
- Validated JSON-RPC IDs, propagated response-allocation failures, and retained MCP servers and handler bindings when transport stop fails
- Published scheduler callback/context pairs under a short lock, validated thread settings and preserved defaults on callback failure or invalid output
- Bounded HTTP/SSE body reads and WebSocket frame reads by size and deadline; drained or closed rejected request bodies
- Normalized bearer-scheme matching, rejected overlong tokens and included CORS headers on error responses
- Made SSE session IDs unpredictable, validated sessions before dispatch, respected payload lengths and synchronized keepalive shutdown with client lifetime
- Registered and removed WebSocket clients across supported ESP-IDF handshake variants, rejected unsupported authentication builds, and validated Origin and control frames
- Answered WebSocket PING/CLOSE frames, rejected malformed controls with CLOSE 1002, consumed unsupported frame payloads and removed closed broadcast sessions
- Validated UART/STDIO request-size floors, removed the unused UART pattern interrupt and declared the SDIO driver dependency
- Made serial stop/destroy retryable without deleting active readers, preserved UART cleanup failures, and released handler responses on dispatch errors
- Discarded complete oversized UART/STDIO lines and SDIO packets instead of dispatching their tails; assembled SDIO fragments without overwriting receive data
- Kept at most one outstanding SDIO DMA send and reclaimed it only after completion or driver teardown
- Rejected non-object tool arguments and invalid example-tool numbers/strings; serialized Wi-Fi results with JSON escaping
- Removed raw request, response and argument logging from transports and example adapters
- Made bridge discovery read-only, required explicit execution authorization and restricted SSE endpoints to the configured origin without redirects
- Used native Gemini content/function responses, preserved model signatures and call IDs, and executed tools only from the selected model candidate
- Exposed `esp_http_server` as a public dependency for the public HTTP/SSE/WS headers

### Changes

- Used application-level `override_path` for in-tree lifecycle tests without upgrading published dependency version constraints
- Documented lifecycle ownership, teardown retry, manager dispatch, scheduler and transport stop/error contracts
- Clarified that MCP server `max_response_size` bounds tool-call results and added component packaging metadata

### Breaking changes

- Reject lifecycle calls from synchronous Event Hub callbacks; post requests to the owner task instead
- Service base layout changed; rebuild dependent components. `esp_service_init()` zeroes the instance and supports a single initialization per instance (start/stop cycles remain repeatable)
- Return `on_stop` and `on_deinit` failures from `esp_service_deinit()` instead of freeing base resources unconditionally; keep failed instances in `ERROR` or `STOPPING` for retry, and treat `UNINITIALIZED` as an idempotent no-op
- Destruction is single-owner: stop external callers, unlink borrowed endpoints and return borrowed deliveries/frames first; event callbacks and same-instance lifecycle hooks must request destruction from the owner task
- MCP tool providers return heap-owned flat snapshots; removed `max_tools` and changed `esp_service_mcp_build_tools_list()` to accept a flat array
- CORS defaults to disabled, HTTP/SSE POST requests require `Content-Type: application/json`, and SSE clients must use the emitted 32-character session ID verbatim
- Removed unused MCP server `max_request_size` and `timeout_ms`; request size/deadline limits belong to transports

## v0.5.3

### Features

- Added scheduler support for all services

## v0.5.2

### Bugfix

- Fixed build errors across IDFv5.x and IDFv6.x

## v0.5.1

### Changes

- Added support for ESP-IDF v6.0 and later

## v0.5.0~2

### Bugfix

- Fixed a build issue

## v0.5.0~1

### Changes

- Renamed Kconfig menu to `ESP-Service: ESP Service` for consistent menuconfig grouping
- Added Component Registry badge to README.md and README_CN.md

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
