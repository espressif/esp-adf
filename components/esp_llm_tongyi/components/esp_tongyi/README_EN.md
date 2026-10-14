# ESP_TONGYI

[中文](./README.md)

## Introduction

`esp_tongyi` is a lightweight WebSocket SDK for realtime interaction with Tongyi multimodal services (audio/text/image).
It targets embedded devices such as ESP32 and focuses on low-latency duplex conversation.

## Kconfig (`ESP Tongyi`)

Component-level runtime options are exposed in `menuconfig -> ESP Tongyi` (see `Kconfig`):

- `ESP_TONGYI_WS_BUFFER_SIZE`: WebSocket RX/TX buffer used when `transport.buffer_size` is 0 (default 20480)
- `ESP_TONGYI_WS_CONNECT_TIMEOUT_MS`: wait timeout for connected / Started in `esp_tongyi_start()` (default 5000)
- `ESP_TONGYI_WS_RECONNECT_TIMEOUT_MS`: reconnect timeout forwarded to websocket client config (default 1000)
- `ESP_TONGYI_WS_SEND_TIMEOUT_MS`: send timeout for text/binary WebSocket frames (default 3000)
- `ESP_TONGYI_WS_NETWORK_TIMEOUT_MS`: network I/O timeout (default 5000)
- `ESP_TONGYI_MAX_JSON_BYTES` / `MAX_BINARY_BYTES` / `MAX_IMAGE_BYTES`: reassembly and image limits

WebSocket task priority / stack size are configured at runtime via `esp_tongyi_cfg_t.transport.task`.

## Add to Project

```bash
idf.py add-dependency "espressif/esp_tongyi=*"
```

## Quick Start

1. Prepare credentials from Tongyi platform: `API Key`, `Workspace ID`, `Application ID`.
2. Create and start. `esp_tongyi_start()` blocks until the session is READY:

```c
#include "esp_tongyi.h"

void app_tongyi_demo(void)
{
    esp_tongyi_cfg_t cfg = ESP_TONGYI_CFG_DEFAULT();
    cfg.api_key      = "your_api_key";
    cfg.workspace_id = "your_workspace_id";
    cfg.app_id       = "your_app_id";

    esp_tongyi_handle_t tongyi = NULL;
    ESP_ERROR_CHECK(esp_tongyi_create(&cfg, &tongyi));
    ESP_ERROR_CHECK(esp_tongyi_start(tongyi));
}
```

3. Send uplink audio:

```c
ESP_ERROR_CHECK(esp_tongyi_send_audio(tongyi, audio_data, audio_len));
```

## Notes

- Public API entry is `esp_tongyi.h`; UUID / base64 helpers live in `esp_tongyi_utils.h`.
- A UUID v4 `task_id` is generated when the field is empty.
- The event callback is fixed at create time; payload pointers are valid only during the callback.
- Do not call blocking send APIs from the callback; use `esp_tongyi_post_directive()`.
- Custom continue-task payloads use a caller-retained JSON object string: `esp_tongyi_send_payload_json()`.
