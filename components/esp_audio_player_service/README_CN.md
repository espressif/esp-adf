# ESP Audio Player Service

- [![组件注册](https://components.espressif.com/components/espressif/esp_audio_player_service/badge.svg)](https://components.espressif.com/components/espressif/esp_audio_player_service)
- [English](./README.md)

`esp_audio_player_service` 是建立在 [`esp_player_service`](../esp_player_service/README_CN.md) 之上的板级**音频播放器**。它通过 `esp_board_manager` 查找板载 DAC，将其连接到 mixer，使背景音乐、TTS、提示音等多个来源作为不同 stream 从同一出口播放。

返回的句柄是标准的 `esp_player_service_t *`。创建后使用常规的 `esp_player_service` / `esp_media_service` / `esp_service` API 进行 start、stop、link、播控与播放列表操作，并用 `esp_player_service_destroy()` 释放。后续加屏不需要第二个实例：对同一句柄调用 [`esp_video_player_service`](../esp_video_player_service/README_CN.md) 的 `attach()` 即可。

## 功能

- 通过 `esp_board_manager` 发现板载 DAC 输出；后续 setup 不指定设备时，保留当前出口
- 在 setup 时设定 mixer 采样格式、处理周期和初始 DAC 音量
- 每路 stream 三种输入（来自父类）：URL（`file:///`、HTTP(S)、HLS `.m3u8`）、应用推入的 PCM 或编码帧，或来自对端源服务的 media link
- 每路 stream 的播控（来自父类）：播放、暂停、恢复、停止、毫秒定位、倍速、时长、位置、状态，以及播放事件
- 多路混音，三档静态优先级（background、notify、urgent），配合共存压低或独占抢占，按路配置；见 [混音与抢占](../esp_player_service/README_CN.md#混音与抢占)
- 每路独立音量，外加一个面向 DAC 的设备输出音量
- 每路独立播放列表：上一首、下一首、跳转到序号、循环模式和自动切歌
- 可选解析裸 MP3 URL 的 ID3
- 可选 MCP（`CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE`）：常驻 flash 的完整播控工具 schema

## 典型场景

- 音乐播放器、音效、提示音和 TTS 播放
- 语音助手中 TTS 回答需要压低或抢占背景音乐
- 与任意 SRC 服务级联（capture、extractor、RTSP / RTMP / SIP 等）：对端音频经 media link 接到本 SINK 某一路，从板载 DAC 播放，应用不必转发帧
- 音箱等无屏产品，需要在一个 DAC 上承载多个音频来源

## 架构

本组件是一层很薄的板级适配：只增加板载 DAC 查找，其余能力都来自父类。自定义 PCM writer 或由应用持有的 codec 句柄，仍使用父类 `esp_player_service_apply_setup()`。

同一喇叭上创建两个实例会各自建立一套 mixer，因此给已有实例加屏应调用 `esp_video_player_service_attach()`，而不是再次 `create()`。

### 头文件

| 头文件 | 内容 |
| --- | --- |
| `esp_audio_player_service.h` | `create`（返回父句柄） |
| `esp_audio_player_service_setup.h` | 板载 DAC `apply_setup` |
| `esp_audio_player_service_mcp.h` | 完整 MCP schema（播控 + ID3），`CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE` |

播控、混音、播放列表、ID3 与事件在 `esp_player_service_playback.h`。调度任务名在父类 `esp_player_scheduler.h`（由 `esp_player_service.h` 引入）。

## 快速开始

```c
#include "esp_audio_dec_default.h"
#include "esp_audio_player_service.h"
#include "esp_audio_player_service_setup.h"
#include "esp_extractor_defaults.h"
#include "esp_player_service_playback.h"
#include "esp_service.h"

ESP_ERROR_CHECK(esp_extractor_register_default());
ESP_ERROR_CHECK(esp_audio_dec_register_default());

esp_audio_player_service_cfg_t cfg = ESP_AUDIO_PLAYER_SERVICE_CFG_DEFAULT();
cfg.max_stream_num = 2;
esp_player_service_t *p = NULL;
ESP_ERROR_CHECK(esp_audio_player_service_create(&cfg, &p));

esp_audio_player_service_setup_t dac = ESP_AUDIO_PLAYER_SERVICE_SETUP_DEFAULT();
dac.dev_name = ESP_BOARD_DEVICE_NAME_AUDIO_DAC;
ESP_ERROR_CHECK(esp_audio_player_service_apply_setup(p, &dac));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(p)));

ESP_ERROR_CHECK(esp_player_service_set_url(p, 0, "file:///sdcard/music.mp3"));
ESP_ERROR_CHECK(esp_player_service_play(p, 0));

esp_service_stop(ESP_SERVICE_BASE(p));
esp_player_service_destroy(p);
```

## 配置项

`CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE`（依赖 `CONFIG_ESP_MCP_ENABLE`，默认关闭）编译 MCP handler 及其常驻 flash 的 schema。底层引擎的音频与视频通路由 `esp_player` 的 `CONFIG_ESP_PLAYER_ENABLE_AUDIO` / `CONFIG_ESP_PLAYER_ENABLE_VIDEO` 门控，本组件复用这两个选项而不新增开关。mixer 格式、处理周期、DAC 音量与 stream 路数都是运行时配置，不是 Kconfig。

## 注意事项

- 调用这些接口前，先用 `esp_board_manager` 初始化板级设备。
- 播放前注册全局 extractor 与音频 decoder 表。本服务不调用 `register_default`。
- `esp_audio_player_service_apply_setup()` 会丢弃此前建立的 player，因此 URL 或 feed 轨声明需要重新下发。
- 混音与抢占（`apply_setup` 之后调用 `set_mix_cfg`）、播控以及每路输入的启停契约属于父类：`esp_player_service_stop()` 不会结束输入，已 link 的 stream 必须先 unlink 才能切换到 URL 或 feed。详见 [`esp_player_service`](../esp_player_service/README_CN.md#混音与抢占)。

## 示例

- `examples/audio_player_mix_cli_example` — 三路混音（URL 播放列表 / link / feed）
- `examples/audio_player_mcp_example` — HTTP MCP

## MCP 工具

当 `CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE=y` 时，本组件提供以下以 `esp_audio_player_service_` 为前缀的工具：

- 播控：`play`、`pause`、`resume`、`stop`、`seek`、`set_speed`、`set_url`
- 播放列表：`playlist_next`、`playlist_prev`、`playlist_play_index`、`set_repeat_mode`
- 音量：`set_volume`、`get_volume`、`set_output_volume`、`get_output_volume`
- 混音与状态：`set_mix_cfg`、`get_preempt_state`、`get_status`
- ID3：`enable_id3_parse`、`get_id3_info`

通过 `esp_audio_player_service_mcp_schema_get()` 与 `esp_audio_player_service_tool_invoke()` 向 service manager 注册。媒体 link / unlink 仍属于 `esp_media_service` MCP，媒体帧不会经过 MCP。

## 技术支持

请通过以下渠道获取技术支持：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能请求：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
