# ESP_TONGYI

[English](./README_EN.md)

## 简介

`esp_tongyi` 是一个轻量级 WebSocket SDK，用于与通义多模态服务进行实时语音/文本/图像交互，
面向 ESP32 等嵌入式设备的低时延双工对话场景。

## Kconfig（`ESP Tongyi`）

组件级参数位于 `menuconfig -> ESP Tongyi`（见 `Kconfig`）：

- `ESP_TONGYI_WS_BUFFER_SIZE`：`transport.buffer_size` 为 0 时使用的 WebSocket 收发缓冲（默认 20480）
- `ESP_TONGYI_WS_CONNECT_TIMEOUT_MS`：`esp_tongyi_start()` 等待 connected / Started 的超时（默认 5000）
- `ESP_TONGYI_WS_RECONNECT_TIMEOUT_MS`：透传给 websocket client 的重连超时（默认 1000）
- `ESP_TONGYI_WS_SEND_TIMEOUT_MS`：发送文本/二进制帧的超时（默认 3000）
- `ESP_TONGYI_WS_NETWORK_TIMEOUT_MS`：网络 I/O 超时（默认 5000）
- `ESP_TONGYI_MAX_JSON_BYTES` / `MAX_BINARY_BYTES` / `MAX_IMAGE_BYTES`：重组与图片上限

WebSocket 任务优先级 / 栈大小通过 `esp_tongyi_cfg_t.transport.task` 在运行时配置。

## 添加到工程

```bash
idf.py add-dependency "espressif/esp_tongyi=*"
```

## 快速开始

1. 从通义平台准备凭证：`API Key`、`Workspace ID`、`Application ID`。
2. 创建并启动。`esp_tongyi_start()` 会阻塞直到会话 READY：

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

3. 持续发送上行音频：

```c
ESP_ERROR_CHECK(esp_tongyi_send_audio(tongyi, audio_data, audio_len));
```

## 说明

- 公开头文件为 `esp_tongyi.h`；UUID / base64 见 `esp_tongyi_utils.h`。
- `task_id` 为空时由 SDK 生成 UUID v4。
- 事件回调在 create 时固定；payload 指针仅在回调期间有效。
- 不要在回调里调用阻塞 send API，改用 `esp_tongyi_post_directive()`。
- 自定义 continue-task payload 使用 caller-retained JSON 字符串：`esp_tongyi_send_payload_json()`。
