# Muxer Service 例程

- [English Version](./README.md)

- 例程难度：![alt text](../../../../docs/_static/level_regular.png "中级") - 演示 `esp_muxer_service` 将 dummy AAC 或 dummy H264+AAC 封装为 TS 容器

## 例程简介

- 本例程展示 `esp_muxer_service` 给产品开发带来的便利：链接一个源、选择存储 / 流式 / 两者，即可得到 TS 文件和/或封装字节，无需自写封装循环。创建 → setup → link → 先启动 muxer 再启动源 的流程可直接复制 `main/simple_muxer.c`。实际产品把 dummy 源换成采集或 extractor，仍使用同一套 `esp_media_service_link()`。
- 技术上演示 muxer 注册、board-manager 挂载 SD、`esp_muxer_service_setup()`、dummy 源链接以及可选流式读取。可在 menuconfig 中关闭不用的容器以减小固件（见下方配置）。

### 预备知识

- 了解 [`esp_muxer_service`](../../README_CN.md)
- 具备 `esp_board_manager` 支持的板级定义（存储用例需要 SD 卡）
- 不需要摄像头或麦克风；源为 dummy 编码图案

### 文件结构

```text
muxer_service/
├── main/
│   ├── app_main.c           板级 / CLI / MCP 启动
│   ├── simple_muxer.c       可直接复制的音频 / 音视频链接用法
│   ├── muxer_scheduler.c    muxer / dummy 线程调度
│   ├── muxer_mcp.c          UART MCP 注册
│   ├── settings.h           时长、TS 类型、SD 路径、RAM 缓存
│   └── Kconfig.projbuild    MCP UART 引脚
├── scripts/
│   └── test_muxer_mcp_uart.py
├── pytest_esp_muxer_service_example.py
├── sdkconfig.defaults
└── README.md
```

## 环境配置

### 硬件要求

- 支持 board-manager 的 ESP 开发板
- 推荐：ESP32-P4 Function EV Board（或其他匹配 board-manager 定义的开发板）
- `storage` / `both` 用例需要 microSD 卡（路径 `/sdcard/muxed`）
- 仅 streaming 用例不需要 SD 卡

### 其他要求

- 注册 TS muxer（本例程 `sdkconfig.defaults` 已开启）
- 存储目录由服务自动创建（最大深度 2）

### Board manager

本例程通过 `esp_board_manager` 挂载 SD 卡（`ESP_BOARD_DEVICE_NAME_FS_SDCARD`）。首次编译前请为**当前硬件**生成板级配置：

```bash
idf.py set-target esp32p4          # 或 esp32s3 / 你的芯片
idf.py gen-bmgr-config -l          # 列出 board id
idf.py gen-bmgr-config -b <your_board_name>
```

将 `<your_board_name>` 替换为与开发板匹配的 id。更换开发板后需重新执行 `gen-bmgr-config`。SD 挂载为尽力而为：缺少 SD 不阻断启动；`storage` / `both` 会等到卡可用后再成功。

## 编译和下载

### 默认 IDF 分支

本例程支持 IDF release/v5.5 及以后分支，默认使用 ADF 内建分支 `$ADF_PATH/esp-idf`。

### 配置

完成 board-manager 配置后，可按需调整：

```text
Component config → ESP_Muxer Configuration    （只打开真正需要的容器）
Component config → FAT Filesystem support → Long filename support
Muxer service example → MCP UART pins
```

本例程只封装 **TS**。为减小 Flash，请保留 `Support TS Muxer`（`CONFIG_ESP_MUXER_TS_SUPPORT`），并**取消勾选**不用的类型（MP4 / OGG / WAV / FLV / CAF / AVI）。之后 `esp_muxer_register_default()` 只会链接仍启用的 muxer。详见组件 [优化体积](../../README_CN.md#优化体积)。

`sdkconfig.defaults` 已包含常用默认项（dummy 源、TS muxer、MCP UART）。存储路径与时长在 `main/settings.h`（`MUXER_EXAMPLE_FAKE_STORAGE_DIR` = `/sdcard/muxed`）。

### 编译和下载

```bash
idf.py build
idf.py -p 你的设备地址 flash monitor
```

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 如何使用例程

### 推荐阅读顺序

1. 先看 **`main/simple_muxer.c`**。这些走读是可复制路径：dummy SRC → muxer SINK，通过 `esp_media_service_link()` 连接。
2. 在硬件上运行对应的 `simple ...` 控制台命令进行验证。
3. 产品中把 dummy 源换成 [`esp_audio_capture_service`](../../../esp_audio_capture_service/README.md)、[`esp_video_capture_service`](../../../esp_video_capture_service/README.md) 或 [`esp_extractor_service`](../../../esp_extractor_service/README.md)，仍使用同一套 link 调用。

### 应用场景

| 用户场景 | 函数 | 控制台命令 |
| --- | --- | --- |
| Dummy AAC → TS muxer | `simple_muxer_audio()` | `simple audio [duration_ms] [streaming\|storage\|both]` |
| Dummy H264+AAC → TS muxer | `simple_muxer_av()` | `simple av [duration_ms] [streaming\|storage\|both]` |

`duration_ms` 默认 5000。模式默认 **streaming**。`storage` 与 `both` 写入 `/sdcard/muxed`。仅存储模式会跳过流式排空。

### 用法

启动后串口提示符为 `muxer>`。

```text
simple audio
simple audio 5000 streaming
simple av 5000 both
simple av 8000 storage
```

说明：

- 启动顺序是先 muxer，再 dummy 源。
- streaming 模式会打印 `esp_muxer_service_read_streaming_data()` 的包数 / 字节数。
- 存储模式依赖 `/sdcard` 上的 FatFS。

### 参考

- 组件 README：[esp_muxer_service](../../README_CN.md)
- 产品中常见源：[esp_extractor_service](../../../esp_extractor_service/README.md)、[esp_audio_capture_service](../../../esp_audio_capture_service/README.md)

## 故障排除

- **SD 卡不可用 / `storage` 失败**：确认已执行 `idf.py gen-bmgr-config -b <board>`，FatFS 已挂载，且 `/sdcard` 可写。
- **流式读取一直为空**：先启动 muxer 再启动源；使用支持流式的容器（本例为 TS）。
- **`setup` / `set_storage_url` 返回 `ESP_ERR_INVALID_STATE`**：在 muxer 处于 stop 时调用。

## MCP 操作指南

本例程可通过 UART MCP 暴露 muxer 配置/控制、dummy 源 start/stop 以及媒体 link/unlink。封装字节不会经过 MCP，始终在 C 侧通过 `esp_media_service_link()` 传递。

### 1. 启用组件 MCP 选项

在 `menuconfig` 中配置（或直接使用 `sdkconfig.defaults`）：

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP-Muxer Service → Enable muxer service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media source service
```

例程 UART 引脚选项在 `Muxer service example → MCP UART pins`。默认值：

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

启动后例程会创建一个 dummy 源（`media_dummy_src`）和一个 muxer（`esp_muxer_service`），注册 MCP 工具并启动 UART MCP 服务器。普通 `muxer>` 控制台仍使用 IDF 控制台 UART。

### 3. 运行 PC 端 UART 脚本

使用第二路 USB-UART 适配器连接到 MCP 引脚：

```bash
python3 scripts/test_muxer_mcp_uart.py /dev/ttyUSB1 115200
```

典型覆盖范围：

1. `tools/list`
2. `esp_muxer_service_setup`
3. `esp_media_service_link`
4. 先启动 muxer 再启动 dummy 源，等待，再 stop
5. unlink 与 `esp_muxer_service_set_storage_url`

### 故障排除

- 若 `tools/list` 超时，请确认 MCP UART 引脚，并确认控制台日志未占用同一 UART
- 若流式读取一直为空，请确认 dummy 源在 muxer 之后启动
- `set_storage_url` / `setup` 必须在 muxer 处于 stop 时调用
- MCP UART 上的非 JSON 日志行会被脚本忽略

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
