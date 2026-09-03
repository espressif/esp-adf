# Audio Player Service MCP 例程

- [English](./README.md)
- Regular Example: ⭐⭐

## 例程简介

本例程演示如何通过 **HTTP**（`POST /mcp`）把 `esp_audio_player_service` 暴露为
**MCP tools**：

1. 创建并启动音频播放服务（板级 DAC + 可选 SD 卡）
2. 启动 Wi-Fi SoftAP（默认）或 STA
3. 通过 `esp_service_manager` 注册 MCP tools
4. 启动带 HTTP transport 的 `esp_service_mcp_server`
5. 空闲等待 PC / LLM bridge 远程控制播放

本例程**不**演示混音/抢占（见 `audio_player_mix_cli_example`）。

### 典型场景

通过 HTTP MCP 做远程或 LLM 驱动的音频控制（SoftAP 演示网络，或 STA 加入现有 Wi-Fi）。

## 环境准备

### 硬件要求

- 默认开发板：**ESP32-S3**（**音频 DAC**、原生 Wi-Fi，通过 `esp_board_manager` 管理）；远程 `set_url` / `play` 时再加 **SD 卡**
- PC / 手机能连上板子 SoftAP 即可（MCP 路径**不需要** USB-UART 转接器）

### 默认 IDF 分支

本例程支持 IDF release/v5.4（>= v5.4.3）和 release/v5.5（>= v5.5.2）。

### 软件要求

- 主机冒烟脚本需要 Python `requests`
- 可选：`/sdcard/test.mp3` 用于远程播歌

## 编译与烧录

### 编译准备

```bash
./install.sh
. ./export.sh
cd $ADF_PATH/components/esp_audio_player_service/examples/audio_player_mcp_example
pip install esp-bmgr-assist
idf.py bmgr -l
idf.py bmgr -b <board_index|board_name>
```

### 工程配置

默认 SoftAP 无需密钥。若用 STA：

```bash
idf.py menuconfig
# Audio Player MCP Example → Wi-Fi mode → Station
# 填写 SSID / 密码（不要把真实密码写进 sdkconfig.defaults）
```

| Kconfig | 本例程默认 |
| --- | --- |
| `ESP_MCP_ENABLE` | y |
| `AUDIO_PLAYER_SERVICE_MCP_ENABLE` | y |
| `ESP_MCP_TRANSPORT_HTTP` | y |
| `EXAMPLE_WIFI_MODE_SOFTAP` | y |
| `EXAMPLE_WIFI_SSID` | `esp-audio-mcp` |
| `EXAMPLE_MCP_HTTP_PORT` | 8080 |
| `EXAMPLE_MCP_HTTP_URI` | `/mcp` |

无原生 Wi-Fi 的芯片上，对应的 `sdkconfig.defaults.<target>` 可能启用
`esp_hosted` / `esp_wifi_remote`。

### 编译与烧录

```bash
idf.py build
idf.py -p PORT flash monitor
```

## 如何使用

### 功能与操作

成功日志包含：

```text
Audio player MCP tools registered with service manager
MCP HTTP transport started: http://192.168.4.1:8080/mcp
Ready for MCP tools/list and tools/call over HTTP
```

#### MCP HTTP 冒烟测试（SoftAP）

1. PC Wi-Fi 连上 SSID `esp-audio-mcp`（开放网络，无密码）
2. 执行：

```bash
pip install requests
python3 scripts/test_audio_player_mcp_http.py 192.168.4.1
```

脚本会检查 `tools/list`、volume、`get_status`，并默认对
`file:///sdcard/test.mp3` 做 `set_url` + `play`（需 SD 卡上有该文件）。
跳过播歌：`--no-play`。

STA 模式用日志里打印的 IP：

```bash
python3 scripts/test_audio_player_mcp_http.py <device-ip>
```

#### 可选：远程播歌

将 `/sdcard/test.mp3` 放到卡上（或改 `EXAMPLE_MCP_DEMO_MP3_FILENAME`），再调用例如：

- `esp_audio_player_service_set_url`，参数 `{"url":"file:///sdcard/test.mp3"}`
- `esp_audio_player_service_play`
- `esp_audio_player_service_set_output_volume` / `esp_audio_player_service_set_volume`

### 日志输出

```text
I (xxx) mcp_ex: Audio player MCP tools registered with service manager
I (xxx) mcp_ex: MCP HTTP transport started: http://192.168.4.1:8080/mcp
I (xxx) mcp_ex: Ready for MCP tools/list and tools/call over HTTP
```

## 故障排除

- 连不上 SoftAP：确认 SSID `esp-audio-mcp`，且当前为 SoftAP 模式。
- `set_url` / `play` 失败：在 `/sdcard/` 放置 `test.mp3`，或使用 `--no-play`。
- 芯片无原生 Wi-Fi：在对应的 `sdkconfig.defaults.<target>` 中启用 hosted 协议栈。

## 相关

- 组件 MCP API：`esp_audio_player_service_mcp.h`
- 工具 schema：`../../src/mcp/esp_audio_player_service_mcp.json`
- 混音例程：`../audio_player_mix_cli_example`
- 主机 LLM bridge：`esp_service/tools/mcp_llm_bridge`

## 技术支持

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- Issue 反馈：[ESP-ADF GitHub issues](https://github.com/espressif/esp-adf/issues)
