# 视频采集例程

- [English Version](./README.md)

- 例程难度：![alt text](../../../../docs/_static/level_regular.png "中级") - 演示 `esp_video_capture_service` 的纯视频、A/V 推流、存储、双 stream、叠加层与 AI 音频采集

## 例程简介

- 本例程展示如何使用 `esp_video_capture_service`，在 board-manager 管理的摄像头 / ADC 设备上完成视频（及可选音频）采集。常用场景请直接参考并可复制 `main/simple_capture.c`（纯视频 / A/V 推流 / 存储 / 链接 / AI / 全速 UVC）；双路、叠加层、自动录制等复杂组合见 `main/video_capture_cases.c`。
- 技术上演示 create → 可选 AI feature 配置 → `apply_setup` → `esp_service_start` / stop，直接使用 `acquire_frame` / `release_frame`，通过 `esp_media_service_link()` 链接到测试 sink，基于 muxer 录制，叠加层刷新，以及可选的源路径全速解码。

### 预备知识

- 了解 [`esp_video_capture_service`](../../README_CN.md) 与 [`esp_capture_service`](../../../esp_capture_service/README_CN.md)
- 具备 `esp_board_manager` 支持的板级定义，并提供 `camera`（A/V 用例还需 `audio_adc`）
- 可选：SD 卡用于存储用例；USB UVC 摄像头用于全速重编码用例

### 文件结构

```text
video_capture/
├── main/
│   ├── app_main.c                板级初始化、CLI、后台录制任务
│   ├── simple_capture.c          可直接复制的常用用法（视频 / A/V / AI / UVC）
│   ├── video_capture_cases.c     复杂用例矩阵（双路 / 叠加层 / 自动录制）
│   ├── video_capture_scheduler.c 采集线程调度桥接
│   └── settings.h                分辨率、编解码与存储路径
├── partitions.csv
├── sdkconfig.defaults
└── README.md
```

## 环境配置

### 硬件要求

- 支持 board-manager 并提供 `camera` 的 ESP 开发板
- 推荐：带 MIPI / 板载摄像头的 ESP32-P4 Function EV Board
- A/V 与 AI 音频用例需要 `audio_adc`
- microSD 卡（存储 / 自动录制用例）
- 可选：USB UVC 摄像头（`simple fullspeed_uvc`）

### 其他要求

- 启用 SPIRAM，以及 `settings.h` 使用的视频编码器 / muxer 选项
- 叠加层用例需要 `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY` 与 painter 字体
- 全速 UVC 用例需要 `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_DECODER`

## 编译和下载

### 默认 IDF 分支

本例程支持 IDF release/v5.5 及以后分支，默认使用 ADF 内建分支 `$ADF_PATH/esp-idf`。

### 配置

先为开发板生成 board-manager 配置，再按需调整：

```text
ESP Capture > Enable video overlay / video decoder
Component config > FAT Filesystem support > Long filename support
```

目标相关默认项见 `sdkconfig.defaults.esp32p4` 等文件。

### 编译和下载

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 如何使用例程

### 推荐阅读顺序

1. 先看 **`main/simple_capture.c`**。这些演示自包含、可直接复制，覆盖 `esp_video_capture_service` 最常用流程。
2. 用对应的 `simple ...` 控制台命令在硬件上验证同一场景。
3. 需要双路推流、叠加层、自动录制组合及更完整 A/V 矩阵时，再看 **`main/video_capture_cases.c`**。

### 常用用法（`simple_capture.c`）

| 使用场景 | 函数 | 控制台命令 |
| --- | --- | --- |
| 纯视频直接拉帧 | `simple_capture_video_only()` | `simple video_only [duration_ms]` |
| A/V 直接拉帧（编码视频 + AAC） | `simple_capture_av_stream()` | `simple av_stream [duration_ms]` |
| 仅存储录制 A/V 到 MP4 | `simple_capture_av_storage()` | `simple av_storage [duration_ms]` |
| A/V 链接到测试 sink | `simple_capture_av_link()` | `simple av_link [duration_ms]` |
| A/V + AI 音频（AEC + VAD） | `simple_capture_av_ai()` | `simple av_ai [duration_ms]` |
| 全速 UVC 解码 + 重编码并存储 | `simple_capture_fullspeed_uvc()` | `simple fullspeed_uvc [duration_ms]` |

### 复杂用例（`video_capture_cases.c`）

需要比简单演示更多组合时，使用完整用例矩阵：

| 使用场景 | 用例名 |
| --- | --- |
| 单路视频推流并统计 | `v_only_stream` |
| 手动录制 A/V 到 MP4 | `av_storage` |
| 推流同时录制 MP4 | `av_stream_storage` |
| 自动录制到 SD 卡目录 | `av_auto_storage` |
| 双路视频（编码 + RGB565） | `v_dual` |
| 双路 A/V（编码 + RGB565 预览） | `av_dual` |
| 双路 A/V，仅编码流存盘 | `av_dual_mixed` |
| 双路 A/V + 共享文字叠加 | `av_dual_overlay` |
| A/V + AI 音频 AEC + VAD | `av_ai_aec_vad` |

### 功能和用法

启动后串口提示符为 `video-capture>`。

`record` 用例在后台任务中运行，CLI 可继续响应。后台采集进行中可用 `i` 查看内存 / 系统状态。

```text
cases
simple video_only 10000
simple av_stream 10000
simple av_storage 10000
simple av_link 10000
simple av_ai 10000
record av_storage 10000
record av_dual_overlay 10000
run_all 5000
run_all 5000 with_trace
i
```

说明：

- `duration_ms` 默认 10000。
- 推流统计来自通过 `esp_media_service_link()` 连接的 `esp_media_dummy_service`。
- 叠加层用例会启用共享源叠加，并可能启动定时刷新以更新日期时间文字。
- AI 用例在视频采集句柄上、于 `apply_setup()` 前调用 `esp_capture_service_ai_audio_src_set_feature()`。
- 录制文件写入 `/sdcard/video_capture/`（见 `settings.h`）。

### 日志输出

```text
I (xxx) VIDEO_CAPTURE: Video capture example is ready
I (xxx) VIDEO_CAPTURE: Type 'cases' to list examples, or 'simple video_only 10000'
video-capture> simple av_stream 3000
I (xxx) SIMPLE_CAPTURE: av_stream: video ... audio ...
video-capture> record av_storage 5000
I (xxx) VIDEO_CAPTURE: Started background record for 'av_storage' (5000 ms)
I (xxx) VIDEO_CAPTURE: Capture case 'av_storage' finished
```

### 参考文献

- 组件文档：[esp_video_capture_service](../../README_CN.md)
- 核心采集服务：[esp_capture_service](../../../esp_capture_service/README_CN.md)
- 音频 AI API：[esp_audio_capture_service](../../../esp_audio_capture_service/README_CN.md)

## 故障排除

- **摄像头初始化 / apply_setup 失败**：确认 board-manager 摄像头设备已初始化，且目标提供 V4L2 摄像头路径。
- **SD 卡不可用 / 存储用例失败**：确认 FatFS 已挂载，且可创建 `/sdcard/video_capture`。
- **叠加层返回 `ESP_ERR_NOT_SUPPORTED`**：开启 `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY` 与 painter 字体后重新编译。
- **全速 UVC 用例失败**：开启视频解码支持，连接 UVC 摄像头，并确认 UVC 设备相关 Kconfig。
- **后台采集仍在运行**：等待上一次 `record` 任务结束，或使用 `i` 查看状态。

## MCP 操作指南

本例程可通过 UART MCP 暴露视频采集、媒体 link/unlink 与 dummy-sink 统计，便于 PC 侧验证。媒体帧不会经过 MCP，仍通过 `esp_media_service_link()` 在 C 路径中传输。

### 1. 启用组件 MCP 选项

在 `menuconfig` 中开启（或依赖 `sdkconfig.defaults`）：

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP Video Capture Service → Enable video capture service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media sink service
```

当视频 MCP 与 UART 传输启用后，例程 UART 引脚选项会出现在 `Video capture example → MCP UART pins` 下。默认值：

- UART 端口：`UART_NUM_1`
- TX GPIO：`17`（接 USB-UART 适配器 RX）
- RX GPIO：`18`（接 USB-UART 适配器 TX）
- 波特率：`115200`

### 2. 编译、烧录并保持板端运行

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

启动后，例程会创建一个真实视频采集服务（`video-rec`）和一个 `media_dummy_sink`，注册 MCP 工具并启动 UART MCP 服务器。普通 `video-capture>` 控制台仍使用 IDF 控制台 UART。

### 3. 运行 PC 端 UART 脚本

使用第二路 USB-UART 适配器连接到 MCP 引脚：

```bash
python3 scripts/test_video_capture_mcp_uart.py /dev/ttyUSB1 115200
```

典型覆盖范围：

1. `tools/list`
2. `esp_video_capture_service_get_status` / `apply_setup`
3. `esp_media_service_link`
4. 启动 sink + capture，等待，再 stop
5. `esp_media_dummy_service_get_stats`（期望 video 或 audio 帧数 > 0）
6. unlink，再以 muxer + overlay 重新配置，然后执行存储录制 / `enable_stream` / overlay redraw

### 故障排除

- 若 `tools/list` 超时，请确认 MCP UART 引脚，并确认控制台日志未占用同一 UART。
- 若统计始终为 0，请确认摄像头 / ADC 初始化，并确认 sink 在 capture 之前已启动。
- 存储 / 录制工具需要第二次 `apply_setup` 并带上 `muxer_type`（例如 `MP4`），在 stop 状态下调用 `set_storage_url`，再执行 `start` → `start_record` → `stop_record`。未配置 muxer 时直接 `start_record` 会返回 `ESP_ERR_INVALID_STATE`。
- `enable_stream` 应在采集运行中验证；stop 之后可能返回 `ESP_ERR_NOT_SUPPORTED`。
- 若板端未提供 overlay 支持或字体，overlay redraw 可能返回工具错误；脚本在 setup 中启用 overlay 后允许该情况。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
