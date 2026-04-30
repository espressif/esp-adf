# Mock Services Example

A minimal, focused showcase of the six mock services under [`components/`](components/). The example only exercises these services — framework-level tests (service manager, MCP server, HTTP / SSE / WS / UART transports, PC-side simulation harness) live in [`../../test_apps/`](../../test_apps/).

## What It Demonstrates

- How to create, start, exercise, pause/resume, stop and destroy each flavour of `esp_service_t`:
  - `player_service` — ASYNC mode (task-backed, non-blocking API)
  - `led_service`    — SYNC mode (no task, blocking API)
  - `cloud_service`  — QUEUE mode (task + command queue, event-driven)
- How to subscribe to a service's `STATE_CHANGED` events via the event hub.
- Everything runs as a self-contained smoke test on boot; no Wi-Fi required, no transports involved.

## Mock Service Components

| Component | Category | Description |
|-----------|----------|-------------|
| `wifi_service` | network | Simulates Wi-Fi scan/connect with domain events |
| `player_service` | audio | Simulates audio playback with async lifecycle |
| `ota_service` | system | Simulates OTA check/update with progress events |
| `led_service` | display | Simulates LED on/off/blink; reacts to other services' events |
| `cloud_service` | network | Simulates cloud connect/send over a task+queue |
| `button_service` | input | Simulates button press events |

Each network-facing component ships both the service implementation (`xxx_service.c`) and an MCP tool handler (`xxx_service_mcp.c`). This example does not link the MCP handlers; they are consumed by the test_apps runner instead.

## Build and Flash

```bash
cd examples/mock_services
idf.py set-target esp32s3
idf.py build flash monitor
```

On boot you should see `ALL SMOKE TESTS PASSED`.

## Where Did The Transport Demos Go?

Everything that exercises the `esp_service` framework itself — service manager, MCP JSON-RPC, HTTP / SSE / WebSocket / UART transports, the Python MCP clients, the PC-side multi-service simulation — now lives in [`../../test_apps/`](../../test_apps/):

- On-device transport tests: `test_apps/main/`
- PC-side scenarios (sim, tool dispatch, integration, long-running MCP HTTP server): `test_apps/pc/`
- Python MCP clients (on-device transport smoke / hold): `test_apps/scripts/test_mcp_http.py`, `test_mcp_sse.py`, `test_mcp_ws.py`, `test_mcp_uart.py`

See [`../../test_apps/README.md`](../../test_apps/README.md) for how to run those.

## Related Documentation

- Component README: [`../../README.md`](../../README.md)
- Production-style example (real Wi-Fi + OTA + CLI + button services, hosted under `adf_examples/`): [`../../../../adf_examples/services_hub/README.md`](../../../../adf_examples/services_hub/README.md)
