# ESP Extractor Service 例程

- [English Version](./README.md)

- 例程难度：![alt text](../../../../docs/_static/level_regular.png "中级") - 演示 `esp_extractor_service` 从本地文件与网络 / HLS URL 抽取音频、视频帧

## 例程简介

- 本例程展示如何把 `esp_extractor_service` 当作高层媒体源：对 SD 卡或 HTTP(S) / HLS 上的容器解复用，然后自行拉取基本帧，或将抽取器 **link** 到 sink。创建 → 设置 URL → start / link 的流程可直接参考并复制 `main/extractor_demo.c`。
- 技术上演示 extractor 注册、board-manager 挂载 SD、Wi‑Fi 连接、`esp_extractor_service_set_url()`、`esp_media_service_get_provider()` 的 acquire / release，以及通过 `esp_media_service_link()` 接到 `esp_media_dummy_service`（产品中链接播放器 sink 使用同一套 API）。

### 预备知识

- 了解 [`esp_extractor_service`](../../README_CN.md)
- 具备 `esp_board_manager` 支持的板级定义（文件用例需要 SD 卡）
- HTTP / HLS 用例需要 Wi‑Fi 凭据

### 文件结构

```text
extractor_service/
├── main/
│   ├── app_main.c          板级 / Wi‑Fi 初始化，CLI 注册
│   ├── extractor_demo.c    可直接复制的 provider 与链接 sink 用法
│   ├── extractor_mcp.c     可选 UART MCP 服务器
│   ├── settings.h          命名 URL、SD 路径、时长、pool 大小
│   └── Kconfig.projbuild   Wi‑Fi 与 MCP UART 引脚
├── scripts/
│   └── test_extractor_mcp_uart.py
├── pytest_esp_extractor_service_example.py
├── partitions.csv
├── sdkconfig.defaults
└── README.md
```

## 环境配置

### 硬件要求

- 支持 board-manager 的 ESP 开发板
- 推荐：ESP32-P4 Function EV Board（或其他匹配 board-manager 定义、带 SDMMC / SD SPI 的开发板）
- 本地 MP4 用例需要 microSD 卡
- HLS / HTTP 用例需要网络

### 其他要求

- SD 卡上放置可播放测试文件 `/sdcard/video/test1.mp4`（见 `main/settings.h` 中的 `EXTRACTOR_URL_SD_MP4`）
- Flash 至少 8 MB（见 `partitions.csv` / `sdkconfig.defaults`）

### Board manager

本例程通过 `esp_board_manager` 挂载 SD 卡（`ESP_BOARD_DEVICE_NAME_FS_SDCARD`）。首次编译前请为**当前硬件**生成板级配置：

```bash
idf.py set-target esp32p4          # 或 esp32s3 / 你的芯片
idf.py gen-bmgr-config -l          # 列出 board id
idf.py gen-bmgr-config -b <your_board_name>
```

将 `<your_board_name>` 替换为与开发板匹配的 id（例如 ESP32-P4 Function EV 的定义）。更换开发板后需重新执行 `gen-bmgr-config`。若 SD 设备不存在，文件用例会失败但不阻断启动；Wi‑Fi 连通后仍可跑 HTTP / HLS。

## 编译和下载

### 默认 IDF 分支

本例程支持 IDF release/v5.5 及以后分支。

### 配置

完成 board-manager 配置后，可按需调整：

```text
Extractor Service Example > WiFi SSID / password / connect wait
ESP-Extractor Service > file / HTTP / HLS support
Component config > FAT Filesystem support > Long filename support
```

`sdkconfig.defaults` 已包含常用默认项（SPIRAM、FatFS LFN、file + HTTP + HLS、dummy sink、MCP UART）。命名 URL 在 `main/settings.h`：

| 名称 | URL |
|------|-----|
| `hls_aac` | 蜻蜓 FM 直播 AAC HLS |
| `sd_mp4` | `/sdcard/video/test1.mp4` |
| `hls_av` | JW Player oceans AES HLS |

运行 `sd_mp4` 前请将可播放 MP4 放到 `/sdcard/video/test1.mp4`。

### 编译和下载

```bash
idf.py build
idf.py -p 你的设备地址 flash monitor
```

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 如何使用例程

### 推荐阅读顺序

1. 先看 **`main/extractor_demo.c`**。`extractor_demo_run()` 是可复制路径：创建抽取器，设置 URL / mask / pool，然后拉帧或链接 sink。
2. 在硬件上运行对应的 `simple ...` 控制台命令进行验证。
3. 产品中把 dummy sink 换成 [`esp_player_service`](../../../esp_player_service/README.md) / 音频或视频播放服务，仍使用同一套 `esp_media_service_link()`。

### 常用用法（`extractor_demo.c`）

| 用户场景 | 控制台命令 |
| --- | --- |
| 通过 `get_provider` + acquire / release 拉帧 | `simple provider [name\|url] [duration_ms]` |
| 将抽取器链接到 dummy sink（与播放器 sink 同一 API） | `simple link [name\|url] [duration_ms]` |
| 重复播放直到 N 次 EOS（或出错 / 超时） | `repeat <provider\|link> [name\|url] [repeat_count] [duration_ms]` |

命名 URL（`hls_aac`、`sd_mp4`、`hls_av`）见 `main/settings.h`。省略名称则使用第一项。示例：`simple provider sd_mp4 5000`。

Provider 模式会打印每帧（`type`、`pts`、`size`、前 8 字节载荷），并在墙钟时长、全部轨 EOS 或出错时停止。Link 模式由 sink 消费，并打印 dummy-sink 的音视频帧数与字节数。

### 例程功能

启动后串口提示符为 `extractor>`。

```text
simple provider
simple provider sd_mp4 5000
simple link hls_aac 10000
simple link https://example.com/live.m3u8 15000
repeat provider sd_mp4 3
repeat link hls_aac 2 60000
```

说明：

- `duration_ms` 默认取 `settings.h` 中的 `EXTRACTOR_TEST_DURATION_MS`。
- `repeat_count` 默认取 `EXTRACTOR_REPEAT_COUNT`。重复模式开启 `set_auto_loop(true)`，由例程统计 `ESP_EXTRACTOR_SERVICE_EVENT_EOS`，在出错或达到次数后停止。
- SD 挂载与 Wi‑Fi 连接均为尽力而为。文件用例需要 `/sdcard`；HTTP / HLS 用例需要 Wi‑Fi。
- 每次 `simple link` 结束后的统计来自 `esp_media_dummy_service`。

### 参考

- 组件 README：[esp_extractor_service](../../README_CN.md)
- 播放器 sink（产品中常见的链接目标）：[esp_player_service](../../../esp_player_service/README.md)

## 故障排除

- **SD 卡不可用 / `sd_mp4` 失败**：确认已执行 `idf.py gen-bmgr-config -b <board>`，FatFS 已挂载，且存在 `/sdcard/video/test1.mp4`。
- **HTTP / HLS 立刻失败**：检查 menuconfig 中的 Wi‑Fi SSID / 密码，以及 URL 是否可达。
- **未编译 HLS**：打开 `CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT`（本例程 `sdkconfig.defaults` 中已开启）。
- **无帧 / pool 过小**：增大 `EXTRACTOR_OUT_POOL_SIZE` / `esp_extractor_service_set_out_pool_size()`，使 pool 能容纳最大访问单元。

## MCP 操作指南

本例程可通过 UART MCP 暴露抽取器配置/控制、dummy-sink 统计以及媒体 link/unlink。媒体帧不会经过 MCP，始终在 C 侧通过 `esp_media_service_link()` 传递。

### 1. 启用组件 MCP 选项

在 `menuconfig` 中配置（或直接使用 `sdkconfig.defaults`）：

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP-Extractor Service → Enable extractor service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media sink service
```

例程 UART 引脚选项在 `Extractor Service Example → MCP UART pins`。默认值：

- UART 端口：`UART_NUM_1`
- TX GPIO：`21`（接到 USB-UART 适配器 RX）
- RX GPIO：`22`（接到 USB-UART 适配器 TX）
- 波特率：`115200`

### 2. 编译、下载并保持开发板运行

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

启动后例程会创建 `esp_extractor_service` 与 `media_dummy_sink`，注册 MCP 工具并启动 UART MCP 服务器。普通 `extractor>` 控制台仍使用 IDF 控制台 UART。

### 3. 运行 PC 端 UART 脚本

使用第二路 USB-UART 适配器连接到 MCP 引脚：

```bash
idf.py -p /dev/ttyACM0 flash monitor
python3 scripts/test_extractor_mcp_uart.py /dev/ttyUSB1 115200
```

典型覆盖范围：

1. `set_extract_mask`、`set_out_pool_size`、`set_url`
2. link，先启动 sink 再启动 extractor
3. dummy 统计
4. stop、unlink

使用 `/sdcard/video/test1.mp4` 中的可播放文件，或通过 `--url` 指定 HTTP/HLS。

### 故障排除

- 若 `tools/list` 超时，请确认 MCP UART 引脚，并确认控制台日志未占用同一 UART。
- 若统计始终为 0，请先启动 sink 再启动 extractor，并确认 URL 可读。
- MCP UART 上的非 JSON 日志行会被脚本忽略。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
