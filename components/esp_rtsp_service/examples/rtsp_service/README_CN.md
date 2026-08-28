# RTSP 服务例程

- [English Version](./README.md)

- 例程难度：![alt text](../../../../docs/_static/level_regular.png "中级") - 演示 `esp_rtsp_service` 推流、拉流与本地服务器

## 例程简介

- 本例程展示如何将 `esp_rtsp_service` 用作 **推流端（SINK）**、**拉流端（SRC）** 或 **本地服务器**。可直接复制的流程位于 `main/simple_rtsp.c`，Wi-Fi 与默认 RTSP URL 通过 menuconfig 配置，并由 `main/settings.h` 映射。
- Dummy H264+AAC 用于生成及接收媒体，因此无需摄像头、麦克风、显示屏或扬声器。`rtsp>` 控制台以异步任务启动各场景。可选 UART MCP 控制预创建的 dummy 源与 RTSP 服务器，媒体帧不经过 MCP。

### 预备知识

- 了解 [`esp_rtsp_service`](../../README_CN.md) 与 [`esp_media_service`](../../../esp_media_service/README_CN.md)
- 开发板可加入的 Wi-Fi 网络
- 推流 / 拉流需要可访问的 RTSP 地址（`rtsp://host:port/path`）
- 本地服务器测试需要同一局域网 PC 上的 `ffplay` 或 VLC

### 文件结构

```text
rtsp_service/
├── main/
│   ├── app_main.c          NVS、Wi-Fi、调度器、MCP 与控制台
│   ├── simple_rtsp.c       可直接复制的推流、拉流与服务器流程
│   ├── simple_rtsp.h
│   ├── rtsp_mcp.c          UART MCP 注册（dummy 源 + RTSP 服务器）
│   ├── rtsp_scheduler.c    服务线程调度器
│   ├── settings.h          来自 Kconfig 的 Wi-Fi 与 RTSP URL
│   └── Kconfig.projbuild   Wi-Fi、URL、时长及 UART 引脚
├── scripts/
│   └── test_rtsp_mcp_uart.py  PC 侧 MCP UART 测试客户端
└── pytest_esp_rtsp_service_example.py  启动冒烟测试
```

## 环境配置

### 硬件要求

- 带 Wi-Fi 与 PSRAM 的 ESP 开发板（推荐 ESP32-S3 或 ESP32-P4）
- 控制台 USB 线；MCP 测试还需要第二路 USB-UART 适配器

### 其他要求

- Wi-Fi SSID 与密码
- 远端 RTSP 服务器 / 客户端，或安装 `ffplay`/VLC 的 PC
- MCP UART 需要安装 `pyserial`（`pip install pyserial`），且不能占用控制台 UART

## 编译和下载

本例程支持 ESP-IDF release/v5.5 及后续分支，默认使用 ADF 内置的 `$ADF_PATH/esp-idf`。

在 `RTSP service example` 下配置：

```text
WiFi SSID
WiFi password
Default run duration (ms)
Pusher RTSP URL
Puller RTSP URL
Local server URL
MCP UART pins
```

编译并烧录：

```bash
cd components/esp_rtsp_service/examples/rtsp_service
idf.py set-target esp32s3
idf.py menuconfig
idf.py build flash monitor
```

完整步骤见 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 如何使用例程

启动后串口提示符为 `rtsp>`。`simple` 命令在后台任务中执行，因此控制台仍可响应。

| 使用场景 | 函数 | 控制台命令 |
| --- | --- | --- |
| Dummy H264 和/或 AAC → 远端 RTSP 服务器 | `simple_rtsp_pusher()` | `simple pusher [duration_ms] [url] [a\|v\|av]` |
| 远端 RTSP 服务器 → dummy sink | `simple_rtsp_puller()` | `simple puller [duration_ms] [url]` |
| Dummy H264 和/或 AAC → 本地 RTSP 服务器 | `simple_rtsp_server()` | `simple server [duration_ms] [url] [a\|v\|av]` |
| 重连 Wi-Fi STA | `esp_wifi_service_request_connect()` | `wifi [ssid] [password]` |

说明：

- `duration_ms` 默认为 `CONFIG_RTSP_EXAMPLE_DURATION_MS`（10000）。
- 省略 `url` 时使用 menuconfig 默认值；也可只提供 URL：`simple pusher rtsp://host:8554/live`。
- pusher/server 可用 `a` / `v` / `av` 选择音频、视频或两者（默认 `av`）。case 名之后参数顺序不限：`simple pusher v`、`simple server 20000 a`。
- 同一时间只能运行一个 `simple` 任务。
- `wifi` 无参数时使用 menuconfig 凭据；`wifi <ssid>` 保留默认密码；`wifi <ssid> -` 连接开放 AP。
- `assert` 会故意向零地址写入，用于 panic / watchdog 调试；它会重启设备，不属于正常功能。

### 本地服务器

执行 `simple server` 后，例程会打印远端播放 URL：

```bash
ffplay -fflags nobuffer -flags low_delay rtsp://<device-ip>:554/live
```

服务器当前仅支持一个远端拉流端。请先启动服务器，再打开播放器。

### MCP 操作指南

MCP UART 控制预创建的 dummy 源（`media_dummy_src`）与 RTSP 服务器（`esp_rtsp_service`）。媒体帧通过 `esp_media_service_link()` 在 C 侧传递。

`sdkconfig.defaults` 已启用所需的 MCP、RTSP MCP、媒体服务 MCP 与 dummy source 选项。

默认 MCP UART 为端口 1、TX GPIO 21、RX GPIO 22、115200 波特率。请勿与控制台 UART 共用。

```bash
idf.py -p /dev/ttyACM0 flash monitor
python3 scripts/test_rtsp_mcp_uart.py /dev/ttyUSB1 115200
```

脚本依次执行：列出工具 → `setup` → `set_url` → `link` → 启动 RTSP 服务器 → 启动 dummy 源 → 停止 dummy 源 → 停止服务器 → `unlink`。

### 日志输出

```text
I (...) ESP_SERVICE: [rtsp-cli] Started
I (...) RTSP_EX: RTSP service example is ready
I (...) RTSP_EX: RTSP_SERVICE_EXAMPLE_READY
rtsp> simple server 10000
simple server started in background
I (...) RTSP_EX: async simple server start duration_ms=10000 url=(default)
I (...) SIMPLE_RTSP: remote pull: ffplay rtsp://192.168.1.20:554/live
```

## 自动冒烟测试

`pytest_esp_rtsp_service_example.py` 会烧录例程，并在 ESP32-S3 与 ESP32-P4 上验证两个启动就绪日志：

```bash
pytest --target esp32s3 pytest_esp_rtsp_service_example.py
```

## 故障排除

- **Wi-Fi 连接或远端推拉流失败**：检查 menuconfig 凭据及 RTSP 地址可达性。
- **推流在 ANNOUNCE 后停止**：确认目标支持 RTSP 发布（`ANNOUNCE`/`RECORD`），而不只是播放。
- **播放器无媒体**：先启动 RTSP server/sink，再启动 dummy 源；服务器只允许一个客户端。
- **音视频漂移**：确认重建的 `esp_media_protocols` 库包含 AAC 帧时长与空帧处理修复。
- **MCP 响应超时**：使用独立 UART，并在停止 RTSP 服务器前先停止 dummy 源。

## 技术支持

- 技术问题请访问 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 功能需求或缺陷请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)
