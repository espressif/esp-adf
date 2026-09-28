# Changelog

## v0.5.0

First intended public release.

- Lifecycle API: `esp_tongyi_create()` / `destroy()` / `start()` / `stop()`.
- `start()` blocks until the server `Started` event provides a `dialog_id` (session READY).
- Config type is `esp_tongyi_cfg_t`, grouped as `transport` / `reconnect` / `limits` / `upstream` / `downstream` / `client`.
- Events: `CONNECTED`, `DISCONNECTED`, `SESSION_READY`, `AUDIO`, `DIALOG_STATE`, `SPEECH_*`, `RESPONDING_*`, `ERROR`.
- Callback data is `const` and valid only during the callback. Raw WebSocket handles are not exposed.
- Directives are `esp_tongyi_send_directive()` / `esp_tongyi_post_directive()`.
- Text / image / custom payload: `esp_tongyi_send_text()`, `esp_tongyi_send_image()`, `esp_tongyi_send_payload_json()`.
- Control task owns heartbeat and reconnect; timer callbacks only enqueue work.
- WebSocket RX assembler validates fragment bounds, order, and size limits.
- Sends require a full write; partial WebSocket sends are failures.
- Empty `task_id` generates a UUID v4. Placeholder user/device defaults are removed.
