# 音频录制例程

- [English Version](./README.md)

- 例程难度：![alt text](../../../../docs/_static/level_regular.png "中级") - 演示 `esp_audio_capture_service` 的推流、存储、双 stream 与 AI 音频采集

## 例程简介

- 本例程展示如何使用 `esp_audio_capture_service`，在 board-manager 管理的音频 ADC 设备上完成录音。常用场景请直接参考并可复制 `main/simple_record.c`（推流 / 直接取帧 / AI / 存储）；双路推流、自动录制、完整 AI 组合与回放校验等复杂场景见 `main/audio_record_cases.c`。
- 技术上演示 create → 可选 AI feature 配置 → `apply_setup` → `esp_service_start` / stop，通过 `esp_media_service_link()` 链接到测试 sink，直接使用 `acquire_frame` / `release_frame` 取帧，以及基于 muxer 的 SD 卡录制。

### 预备知识

- 了解 [`esp_audio_capture_service`](../../README_CN.md) 与 [`esp_capture_service`](../../../esp_capture_service/README_CN.md)
- 具备 `esp_board_manager` 支持的板级定义，并提供 `audio_adc`
- 可选：SD 卡用于存储用例；`audio_dac` 用于 AEC 参考回放与录制文件校验

### 文件结构

```text
audio_record/
├── assets/                 作为 AEC 参考 / 校验回放源的内嵌 AAC
├── main/
│   ├── app_main.c          板级初始化与 CLI 注册
│   ├── simple_record.c     可直接复制的常用用法（stream / direct / AI / storage）
│   ├── audio_record_cases.c 复杂用例矩阵（双路 / 自动录制 / 完整 AI）
│   ├── audio_record_player.c DAC 参考 / 校验播放器
│   ├── settings.h          采样率、编解码与存储路径
│   └── Kconfig.projbuild   AUDIO_RECORD_ENABLE_VERIFY
├── partitions.csv          启用 AI 模型时使用的 8 MB 分区表
├── sdkconfig.defaults
└── README.md
```

## 环境配置

### 硬件要求

- 支持 board-manager 并提供 `audio_adc` 的 ESP 开发板
- 推荐：ESP32-P4 Function EV Board（或其他匹配 board-manager 定义的开发板）
- microSD 卡（存储 / 自动录制 / AI dump 用例）
- 可选：通过 `audio_dac` 连接扬声器 / 耳机，用于 AEC 参考与校验回放

### 其他要求

- 启用 AI 模型时建议至少 8 MB Flash（见 `partitions.csv`）
- AEC 用例要求板级将 DAC 回放路由到 ADC 回声 / 参考通道

## 编译和下载

### 默认 IDF 分支

本例程支持 IDF release/v5.5 及以后分支，默认使用 ADF 内建分支 `$ADF_PATH/esp-idf`。

### 配置

先为开发板生成 board-manager 配置，再按需调整：

```text
Audio record example > Enable recorded-file playback verification
ESP Audio Capture Service > Enable AI audio source / feature supports
Component config > FAT Filesystem support > Long filename support
```

`sdkconfig.defaults` 已包含常用默认项（SPIRAM、FatFS LFN、AI 源功能、`settings.h` 使用的编解码器）。

### 编译和下载

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 如何使用例程

### 推荐阅读顺序

1. 先看 **`main/simple_record.c`**。这些演示自包含、可直接复制，覆盖 `esp_audio_capture_service` 最常用流程。
2. 用对应的 `simple ...` 控制台命令在硬件上验证同一场景。
3. 需要双路推流、自动录制组合、完整 AI 功能矩阵或回放校验时，再看 **`main/audio_record_cases.c`**。

### 常用用法（`simple_record.c`）

| 使用场景 | 函数 | 控制台命令 |
| --- | --- | --- |
| 链接到 sink 并消费推流帧 | `simple_record_stream()` | `simple stream [duration_ms]` |
| 直接使用 `acquire_frame` / `release_frame` 取帧 | `simple_record_direct()` | `simple direct [duration_ms]` |
| AEC + VAD 处理后的 PCM，直接拉取 | `simple_record_ai_direct()` | `simple ai_direct [duration_ms]` |
| 仅存储录制到 MP4（不拉帧） | `simple_record_storage()` | `simple storage [duration_ms]` |

### 复杂用例（`audio_record_cases.c`）

需要比简单演示更多组合时，使用完整用例矩阵：

| 使用场景 | 用例名 |
| --- | --- |
| 单路 AAC 实时推流 | `normal_stream` |
| 手动录制 MP4 | `normal_storage` |
| 自动录制到 SD 卡目录 | `normal_auto_storage` |
| 推流同时录制 MP4 | `normal_stream_storage` |
| 双路 G711A + AAC 实时推流 | `normal_dual_stream` |
| 双路输出，仅 AAC 存盘 | `normal_dual_mixed` |
| AEC + NS PCM 推流 | `ai_afe_stream` |
| AEC + NS PCM 录制 WAV | `ai_afe_storage` |
| 仅 AEC / WakeNet / VAD / DOA | `ai_aec`、`ai_wn`、`ai_vad`、`ai_doa` |
| 对应 AI 功能并存储 WAV | `ai_*_storage` |
| 完整 AI 组合（AEC + NS + WN + VAD + DOA） | `ai_all` |
| AI 源输出 AAC + G711A 并存储 | `ai_dual` |

### 功能和用法

启动后串口提示符为 `audio-record>`。

`record` 用例在后台任务中运行，CLI 可继续响应。后台录制进行中可用 `i` 查看内存 / 系统状态。

```text
cases
simple stream 10000
simple direct 10000
simple ai_direct 10000
simple storage 10000
record normal_stream 10000
record normal_storage 10000 verify
record ai_all 10000
run_all 5000
run_all 5000 with_trace
i
```

说明：

- `duration_ms` 默认 10000。
- 在启用 `CONFIG_AUDIO_RECORD_ENABLE_VERIFY` 时，可追加 `verify` 或 `1` 回放录制文件。
- `i` 可查询 GMF 内存与 FreeRTOS 任务 CPU 占用。
- 堆泄漏追踪仅用于 `run_all ... with_trace`：先不带 `with_trace` 跑一轮以沉降常驻分配，再带 `with_trace` 跑一轮查看残留。
- 普通用例使用 codec 设备源；AI 用例在 `apply_setup()` 前调用 `esp_capture_service_ai_audio_src_set_feature()`，从而选择 AI 源。
- 含 AEC 的用例会通过板载 DAC 循环播放 `assets/music.aac` 作为立体声参考信号。
- 各用例结束后的推流统计来自通过 `esp_media_service_link()` 连接的 `esp_media_dummy_service`。
- 仅存储用例会禁用 provider track，同时保留 muxer track。
- AI 存储用例可将未处理源 PCM 转储到 `/sdcard/audio_record/src.pcm`，便于与处理后录音对比。

### 日志输出

```text
I (xxx) AUDIO_RECORD: Audio record example is ready
I (xxx) AUDIO_RECORD: Type 'cases' to list examples, or 'record normal_stream 10000'
audio-record> record normal_stream 3000
I (xxx) RECORD_CASE: Running case 'normal_stream'
I (xxx) TEST_SINK: stream0 frames=... bytes=...
I (xxx) RECORD_CASE: Case 'normal_stream' finished
```

### 参考文献

- 组件文档：[esp_audio_capture_service](../../README_CN.md)
- 核心采集服务：[esp_capture_service](../../../esp_capture_service/README_CN.md)

## 故障排除

- **SD 卡不可用 / 存储用例失败**：确认 FatFS 已挂载，且可创建 `/sdcard/audio_record`。
- **AEC 效果弱或无效**：确认 DAC 初始化成功，且板级将回放路由到 ADC 参考通道。
- **AI 功能返回 `ESP_ERR_NOT_SUPPORTED`**：开启对应的 `ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_*` Kconfig 后重新编译。
- **校验回放被跳过**：开启 `AUDIO_RECORD_ENABLE_VERIFY`，并确认存在 `audio_dac`。
- **Flash / 模型分区错误**：使用提供的 `partitions.csv`，并选用 8 MB 及以上 Flash。
- **后台录制仍在运行**：等待上一次 `record` 任务结束，或用 `i` 查看状态。

## MCP 操作指南

本例程可通过 UART MCP 暴露音频采集、媒体 link/unlink 与 dummy-sink 统计，便于 PC 侧验证。媒体帧不会经过 MCP，仍通过 `esp_media_service_link()` 在 C 路径中传输。

### 1. 启用组件 MCP 选项

在 `menuconfig` 中开启（或依赖 `sdkconfig.defaults`）：

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP Audio Capture Service → Enable audio capture service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media sink service
```

当音频 MCP 与 UART 传输启用后，例程 UART 引脚选项会出现在 `Audio record example → MCP UART pins` 下。默认值：

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

启动后，例程会创建一个真实音频采集服务（`audio-rec`）和一个 `media_dummy_sink`，注册 MCP 工具并启动 UART MCP 服务器。普通 `audio-record>` 控制台仍使用 IDF 控制台 UART。

### 3. 运行 PC 端 UART 脚本

使用第二路 USB-UART 适配器连接到 MCP 引脚：

```bash
python3 scripts/test_audio_capture_mcp_uart.py /dev/ttyUSB1 115200
```

典型覆盖范围：

1. `tools/list`
2. `esp_audio_capture_service_get_status` / `apply_setup`
3. `esp_media_service_link`
4. 启动 sink + capture，等待，再 stop
5. `esp_media_dummy_service_get_stats`（期望 `audio_frame_count > 0`）
6. unlink，再以 muxer 重新配置，然后执行存储录制 + `enable_stream`

### 故障排除

- 若 `tools/list` 超时，请确认 MCP UART 引脚，并确认控制台日志未占用同一 UART。
- 若统计始终为 0，请检查板载 ADC 初始化，并确认 sink 在 capture 之前已启动。
- 存储 / 录制工具需要第二次 `apply_setup` 并带上 `muxer_type`（例如 `MP4`），在 stop 状态下调用 `set_storage_url`，再执行 `start` → `start_record` → `stop_record`。
- `enable_stream` 应在采集运行中验证；stop 之后可能返回 `ESP_ERR_NOT_SUPPORTED`。
- MCP UART 上的非 JSON 日志行会被脚本忽略。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
