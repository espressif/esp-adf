# ESP Video Player Service

- [![组件注册](https://components.espressif.com/components/espressif/esp_video_player_service/badge.svg)](https://components.espressif.com/components/espressif/esp_video_player_service)
- [English](./README.md)

`esp_video_player_service` 是建立在 [`esp_player_service`](../esp_player_service/README_CN.md) 之上的板级**视频（A/V）播放器**。它通过 `esp_board_manager` 查找板载 LCD 与音频 codec，为显示屏构建视频 render，并绑定音频输出上下文，在带屏板卡上播放音频、视频或同步音视频。

返回的句柄是标准的 `esp_player_service_t *`。创建后使用常规的 `esp_player_service` / `esp_media_service` / `esp_service` API 进行 start、stop、link、播控与播放列表操作，并用 `esp_player_service_destroy()` 释放。音频出口既可以在本组件的 `apply_setup()` 中指定，也可以先用 [`esp_audio_player_service`](../esp_audio_player_service/README_CN.md) 的 `apply_setup()` 配置，后者优先。带屏产品只创建本组件，不要为同一喇叭再建音频实例。

## 功能

- 视频播放：播放容器中的视频轨，输出到板载 LCD，或输出到由应用自行构建的显示屏
- 音视频同步播放：同一次会话的音频与视频作为一路 stream 播放，同步时钟源可选
- LCD 输出帧率可配，默认每秒 30 帧，与片源帧率无关
- 轨道选择：报告当前片源有哪些轨道，并播放指定的那一条
- 每路 stream 三种输入（来自父类）：URL（`file:///`、HTTP(S)、HLS `.m3u8`）、应用推入的帧，或来自对端源服务的 media link
- 每路 stream 的播控（来自父类）：播放、暂停、恢复、停止、毫秒定位、倍速、时长、位置、状态，以及播放事件
- 多路混音，三档静态优先级（background、notify、urgent），配合共存压低或独占抢占，按路配置；见 [混音与抢占](../esp_player_service/README_CN.md#混音与抢占)
- 可选 MCP（`CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE`）：常驻 flash 的完整播控工具 schema

## 典型场景

- 带屏产品上播放本地或 HTTP 视频
- 一路 stream 播影片，另一路混入 TTS 或提示音
- 智能屏、门铃、摄像头等在视频片段、提示音与背景音乐之间切换
- 与任意 SRC 服务级联（capture、extractor、RTSP / RTMP / SIP 等）：对端音视频经 media link 接到本 SINK 某一路，画面上屏、音频从 DAC 播放，应用不必转发帧

## 架构

一个实例持有一个视频 render。并发播放是该实例上的 **stream**，各路地位相同：一次会话是影片还是 TTS 提示由它的轨决定，不由 stream 类型决定。两路影片共用同一个视频 render。

`create()` 通过音频子类连接板载 DAC，并绑定视频上下文。LCD 在 `apply_setup()` 中查找，不在 create 中查找。

同一喇叭上两次 `create()` 会建立两套 mixer，因此给已有音频实例加屏应对该句柄调用 `esp_video_player_service_attach()`。

### 头文件

| 头文件 | 内容 |
| --- | --- |
| `esp_video_player_service.h` | `create` / `attach`（身份 + `render_fps`）和容器轨道 API（带 `stream`） |
| `esp_video_player_service_setup.h` | LCD / DAC 回退 `apply_setup`，以及 `set_render()` |
| `esp_video_player_service_mcp.h` | 完整 MCP schema（播控 + 轨道），`CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE` |

播控、混音、播放列表、feed 与 link 都在 `esp_player_service_*`，全部带 `stream` 参数。调度任务名在父类 `esp_player_scheduler.h`（由 `esp_player_service.h` 引入）。

## 快速开始

```c
#include "esp_audio_dec_default.h"
#include "esp_extractor_defaults.h"
#include "esp_player_service_playback.h"
#include "esp_service.h"
#include "esp_video_dec_default.h"
#include "esp_video_player_service.h"
#include "esp_video_player_service_setup.h"

esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
ESP_ERROR_CHECK(esp_extractor_register_default());
ESP_ERROR_CHECK(esp_audio_dec_register_default());
ESP_ERROR_CHECK(esp_video_dec_register_default());

esp_video_player_service_cfg_t cfg = ESP_VIDEO_PLAYER_SERVICE_CFG_DEFAULT();
esp_player_service_t *p = NULL;
ESP_ERROR_CHECK(esp_video_player_service_create(&cfg, &p));

esp_video_player_service_setup_t lcd = ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT();
ESP_ERROR_CHECK(esp_video_player_service_apply_setup(p, &lcd));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(p)));

ESP_ERROR_CHECK(esp_player_service_set_url(p, 0, "file:///sdcard/movie.mp4"));
ESP_ERROR_CHECK(esp_player_service_play(p, 0));
/* stream 1：同一 mixer 上的 TTS / 提示音 */
```

## 视频输出

一个服务只有一个视频 render。两种安装方式都立即生效，**后调用的覆盖先调用的**：

1. `apply_setup(.display_dev_name)`：从已初始化的板载 LCD 创建，服务持有并负责销毁
2. `esp_video_player_service_set_render(handle)`：由调用方创建并持有，服务不销毁

将 `display_dev_name` 置为 NULL，或调用 `set_render(NULL)`，表示不安装视频通路。调用方自建的 render 只需 `set_render()`，不必再调用 `apply_setup()`。

音频子类缓存为空时，使用 `audio_dev_name` 指定的板载 DAC；若此前已调用 `esp_audio_player_service_apply_setup()`，则以该缓存为准。

两个字段的 NULL 含义相同：不使用该设备。板级默认名写在 `ESP_VIDEO_PLAYER_SERVICE_SETUP_DEFAULT()` 中，实现里没有隐藏回退。

最终没有音频出口是允许的：流按纯视频播放，音频轨被丢弃。之后补上出口需要再次 `apply_setup()`，此前建立的 player 会被丢弃，因此 URL 和 feed 轨声明要重新下发。反过来，只有音频出口而没有视频 render 时是纯音频。两者都没有时，`set_url()` 与 `write_frame()` 返回 `ESP_ERR_INVALID_STATE`。

## 配置项

`CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE`（依赖 `CONFIG_ESP_MCP_ENABLE`，默认关闭）编译 MCP handler 及其常驻 flash 的 schema。底层引擎的音频与视频通路由 `esp_player` 的 `CONFIG_ESP_PLAYER_ENABLE_AUDIO` / `CONFIG_ESP_PLAYER_ENABLE_VIDEO` 门控，本组件复用这两个选项而不新增开关。设备名、`render_fps` 与 mixer 设置都是运行时配置，不是 Kconfig。

## 注意事项

- 先用 `esp_board_manager_init_device_by_name()` 初始化 LCD（以及 DAC）。本服务不 init / deinit 面板。
- 播放前注册全局 extractor、音频 decoder 与视频 decoder 表。本服务不调用 `register_default` 或 `unregister_default`。
- `render_fps` 在 create / attach 时固定，修改 LCD 输出帧率需要重新 create 或 attach。
- 混音与抢占（`apply_setup` 之后调用 `set_mix_cfg`）、播控以及每路输入的启停契约属于父类：`esp_player_service_stop()` 不会结束输入，已 link 的 stream 必须先 unlink 才能切换到 URL 或 feed。详见 [`esp_player_service`](../esp_player_service/README_CN.md#混音与抢占)。

## 示例

- `examples/video_player_mix_cli_example` — stream 0 影片 URL（含播放列表）、ES 或 dummy-src link，stream 1 TTS

## MCP 工具

当 `CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE=y` 时，本组件提供以下以 `esp_video_player_service_` 为前缀的工具：

- 播控：`play`、`pause`、`resume`、`stop`、`seek`、`set_speed`、`set_url`
- 播放列表：`playlist_next`、`playlist_prev`、`playlist_play_index`、`set_repeat_mode`
- 音量：`set_volume`、`get_volume`、`set_output_volume`、`get_output_volume`
- 混音与状态：`set_mix_cfg`、`get_preempt_state`、`get_status`
- 容器轨道：`get_track_info`、`enable_track`

通过 `esp_video_player_service_mcp_schema_get()` 与 `esp_video_player_service_tool_invoke()` 向 service manager 注册。媒体 link / unlink 仍属于 `esp_media_service` MCP，媒体帧不会经过 MCP。

## 技术支持

请通过以下渠道获取技术支持：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能请求：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
