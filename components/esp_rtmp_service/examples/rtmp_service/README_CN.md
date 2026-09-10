# RTMP 服务例程

- [English Version](./README.md)

- 例程难度：![alt text](../../../../docs/_static/level_regular.png "中级") - 演示 `esp_rtmp_service` 推流、拉流与本地服务器

## 例程简介

- 本例程展示如何使用 `esp_rtmp_service` 作为 **推流端（SINK）**、**拉流端（SRC）** 或 **本地服务器**。可直接复制的用法在 `main/simple_rtmp.c`。推流 / 拉流 URL 以及服务器监听 URL 在 `main/settings.h`（menuconfig）。推流源使用 dummy H264+AAC，无需摄像头或麦克风。
- 技术上演示 `esp_rtmp_service_create` → `setup` / `set_url` → `esp_media_service_link()` → `esp_service_start` / stop。推流将 dummy 源链接到 RTMP sink；拉流将 RTMP 源链接到 dummy sink。串口提示符 `rtmp>` 运行同一套演示。可选 UART MCP 控制预先创建的 dummy 源和 RTMP 推流服务，媒体帧不经过 MCP。

### 预备知识

- 了解 [`esp_rtmp_service`](../../README_CN.md) 与 [`esp_media_service`](../../../esp_media_service/README_CN.md)
- 开发板可加入的 Wi-Fi 网络
- 推流 / 拉流需要可访问的 RTMP URL（`rtmp://host:port/app/stream`）
- 本地服务器需要同一局域网 PC 上的 `ffmpeg` / `ffplay`

### 文件结构

```text
rtmp_service/
├── main/
│   ├── app_main.c          NVS、Wi-Fi、调度器、MCP、控制台
│   ├── simple_rtmp.c       可直接复制的推流 / 拉流 / 服务器用法
│   ├── simple_rtmp.h
│   ├── rtmp_mcp.c          UART MCP 注册（dummy 源 + RTMP sink）
│   ├── rtmp_scheduler.c    服务线程调度
│   ├── settings.h          来自 Kconfig 的 Wi-Fi 与 RTMP URL
│   └── Kconfig.projbuild   例程 Wi-Fi、URL、MCP UART 引脚
└── scripts/
    └── test_rtmp_mcp_uart.py  PC 侧 MCP UART 测试客户端
```

## 环境配置

### 硬件要求

- 带 Wi-Fi 和 PSRAM 的 ESP 开发板（推荐 ESP32-S3 或 ESP32-P4）
- 用于控制台的 USB 线（若使用 MCP UART，还需第二路 USB-UART 适配器）

### 其他要求

- Wi-Fi SSID 和密码
- 远端 RTMP 推流 / 播放 URL，**或** 安装 `ffmpeg` / `ffplay` 的 PC（本地服务器场景）
- MCP UART：安装 `pyserial`（`pip install pyserial`），且 UART 适配器 **不要** 占用控制台串口

## 编译和下载

### 默认 IDF 分支

本例程支持 IDF release/v5.5 及以后的分支，例程默认使用 ADF 的内建分支 `$ADF_PATH/esp-idf`。

### 配置

在 menuconfig 中设置 Wi-Fi 和 RTMP URL（由 `main/settings.h` 映射）：

```text
RTMP service example → WiFi SSID
RTMP service example → WiFi password
RTMP service example → Pusher RTMP URL
RTMP service example → Puller RTMP URL
RTMP service example → Local server listen URL
RTMP service example → Server stream name used in ffmpeg examples
```

MCP UART 引脚（启用 MCP UART 传输时）：

```text
RTMP service example → MCP UART pins
```

`sdkconfig.defaults` 已默认打开 dummy SRC/SINK、RTMP SRC/SINK/SERVER、MCP UART，以及更大的 LwIP TCP 窗口以适配直播流。

### 编译和下载

```bash
cd components/esp_rtmp_service/examples/rtmp_service
idf.py set-target esp32s3
idf.py menuconfig
idf.py build flash monitor
```

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 如何使用例程

### 功能和用法

启动后串口提示符为 `rtmp>`。先阅读 **`main/simple_rtmp.c`**，再用对应控制台命令验证。

| 使用场景 | 函数 | 控制台命令 |
| --- | --- | --- |
| Dummy H264+AAC → 远端 RTMP 推流 | `simple_rtmp_pusher()` | `simple pusher [duration_ms] [url]` |
| 远端 RTMP → dummy sink | `simple_rtmp_puller()` | `simple puller [duration_ms] [url]` |
| 本地 RTMP 服务器（PC 用 ffmpeg 推 / ffplay 拉） | `simple_rtmp_server()` | `simple server [duration_ms] [url]` |

说明：

- `duration_ms` 默认值为 `CONFIG_RTMP_EXAMPLE_DURATION_MS`（10000）。
- 省略 `url` 时使用 menuconfig / `sdkconfig` 默认值。`url` 也可以作为唯一可选参数：`simple pusher rtmp://host/live/stream`。
- 需要全局交错缓存时，先链接 sink/server，再给 dummy 源添加 track。
- 先启动 RTMP sink（拉流场景则先启动 dummy sink），再启动源。

#### 本地服务器与 ffmpeg / ffplay

`simple server` 会按 STA IP 打印命令，例如：

```bash
ffmpeg -re -f lavfi -i testsrc=size=320x240:rate=15 -f lavfi -i sine \
  -c:v libx264 -preset ultrafast -tune zerolatency -c:a aac \
  -f flv rtmp://<device-ip>:1935/live/stream0

ffplay rtmp://<device-ip>:1935/live/stream0
```

将 `stream0` 替换为 `CONFIG_RTMP_EXAMPLE_SERVER_STREAM`。服务器 URL 形态为 `rtmp://host:port/app`；推流 / 拉流 URL 需带流名：`rtmp://host:port/app/stream`。

### MCP 操作指南

MCP UART 控制预先创建的 dummy 源（`media_dummy_src`）和 RTMP 推流（`esp_rtmp_service`）。媒体帧通过 `esp_media_service_link()` 在 C 侧传递，不经过 MCP。

启用项（`sdkconfig.defaults` 中已打开）：

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP-RTMP Service → Enable RTMP service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media source
```

默认 MCP UART（见 `RTMP service example → MCP UART pins`）：

- UART 口：`1`
- TX / RX：ESP32 / ESP32-P4 为 GPIO `21` / `22`；ESP32-S3 为 GPIO `17` / `18`（S3 没有 GPIO 22）
- 波特率：`115200`

MCP UART 不要占用控制台串口。烧录后保持开发板运行，在 PC 上执行：

```bash
idf.py -p /dev/ttyACM0 flash monitor
python3 scripts/test_rtmp_mcp_uart.py /dev/ttyUSB1 115200 --push-url rtmp://192.168.1.10/live/stream0
```

脚本会列出工具，然后执行 `setup` → `set_url` → `link` → 启动 RTMP sink → 启动 dummy 源 → 停止 sink → 停止源 → `unlink`。`esp_rtmp_service_query` **仅适用于服务器角色**；本 MCP 例程注册的是推流端，因此脚本允许该工具返回错误。

### 日志输出

```text
I (8708) esp_cli_service: Start 'esp_cli_service': REPL running
I (8708) ESP_SERVICE: [rtmp-cli] Started
I (8708) RTMP_EX: RTMP service example is ready
I (8709) RTMP_EX: Type 'simple pusher', 'simple puller', or 'simple server'
rtmp> simple pusher 10000
I (16905) RTMP_CMD: Set tcUrl rtmp://192.168.1.10:1935/live
I (17140) RTMP: Got peer chunk size 4096
I (17160) RTMP: Publish 0x4801285c Started
I (17168) ESP_SERVICE: [esp_rtmp_service] Started
I (17212) ESP_SERVICE: [media_dummy_src] Started
I (27212) SIMPLE_RTMP: pusher url:rtmp://192.168.1.10/live/stream0 duration_ms:10000
```

### 参考文献

- 组件说明：[esp_rtmp_service](../../README_CN.md)
- 媒体服务：[esp_media_service](../../../esp_media_service/README_CN.md)

## 故障排除

- **Wi-Fi 连接失败 / 远端推拉流失败**：在 menuconfig 中设置 SSID 和密码，确认 RTMP 主机在同一局域网可达。
- **ffplay 无画面**：确认推流在添加 dummy track **之前** 已完成链接（全局缓存），且 ingest 服务器正在接收该流。
- **拉流在 Play.Start 后立即 `receive fail`**：检查流编解码（本协议栈期望 AAC / MP3 / PCM / G.711 与 H264 / MJPEG），并确保 dummy sink 先于 RTMP 源启动。
- **MCP `Timed out waiting for a JSON-RPC response`**：使用独立 UART（不要占用控制台口）；先启动 RTMP sink 再启动 dummy 源；先停 sink 再停源。
- **MCP 上 `esp_rtmp_service_query` 报错**：推流角色下属预期行为，query 仅用于 SERVER。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
