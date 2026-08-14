# ESP Player Service

- [![组件注册](https://components.espressif.com/components/espressif/esp_player_service/badge.svg)](https://components.espressif.com/components/espressif/esp_player_service)
- [English](./README.md)

`esp_player_service` 是 ESP-ADF 中的媒体 **SINK** 播放服务。一个实例对应一块板子上的播放出口，在其上播放音频、视频或同步音视频。背景音乐、TTS、影片等并发内容，都是该实例上的一路 **stream**。

面向板卡的播放组件 [`esp_audio_player_service`](../esp_audio_player_service/README_CN.md) 与 [`esp_video_player_service`](../esp_video_player_service/README_CN.md) 都建立在本组件之上。板级产品选用两者之一；PCM 不经过板载 DAC 时，直接使用本组件。

## 功能

- 每路 stream 三种互斥输入：URL（`file:///`、HTTP(S)、HLS `.m3u8`）、应用推入并**拷贝**进播放器的帧，或来自对端源服务的**零拷贝** media link；见 [每路输入](#每路输入)
- 每路 stream 的播控：播放、暂停、恢复、停止、毫秒定位、倍速、时长、位置和状态
- 每路可以是纯音频、纯视频或音视频，以实际安装的出口为准：喇叭 mixer 与视频 render，至少要有一个
- 多路混音，路数可配：每路独立音量，外加一个设备总音量
- 三档静态优先级（background、notify、urgent），配合共存压低或独占抢占，按路配置；见 [混音与抢占](#混音与抢占)
- 每路独立播放列表：上一首、下一首、跳转到序号、循环模式和自动切歌
- 同一会话内的音视频：每路最多一条音频轨和一条视频轨，同步时钟可选，extractor 缓冲池和预缓冲可配
- 播放事件送到每个服务一个回调，覆盖播放结束、缓冲、切歌和错误
- 可选解析裸 MP3 URL 的 ID3
- 可选视频输出，由视频子类接入：每个实例一块屏，各路 stream 地位相同
- 音频通路和视频通路可在编译期分别裁掉（`CONFIG_ESP_PLAYER_ENABLE_AUDIO` / `CONFIG_ESP_PLAYER_ENABLE_VIDEO`）

本组件不提供 MCP。产品侧 MCP schema 在音/视频子类中（`CONFIG_AUDIO_PLAYER_SERVICE_MCP_ENABLE` / `CONFIG_VIDEO_PLAYER_SERVICE_MCP_ENABLE`）。

## 典型场景

- PCM 不经过板载 DAC 的产品：自定义 writer，或由应用持有的 codec 句柄
- 背景音乐、TTS 和提示音需要在同一喇叭上共存或互相抢占
- 与任意 SRC 服务级联（capture、extractor、RTSP / RTMP / SIP 等）：对端的音频或音视频经 media link 接到本 SINK 的某一路直接播放，应用不必转发帧
- 板级产品间接使用：它们创建两个子类之一，子类返回本父类句柄

## 架构

板载 DAC 和 LCD 的查找在子类 [`esp_audio_player_service`](../esp_audio_player_service/README_CN.md) 与 [`esp_video_player_service`](../esp_video_player_service/README_CN.md) 中完成。两个 `create()` 都返回本父类句柄。销毁一律调用 `esp_player_service_destroy()`。

生命周期由 `esp_service` 统一：通过 `ESP_SERVICE_BASE(player)` 进行 start / stop。stop 和 destroy 会等到 media-link 桥接任务退出。`apply_setup` 成功后，角色变为 `ESP_MEDIA_ROLE_SINK`。

```text
            esp_service  (lifecycle: init / start / stop / deinit)
                  ^
            esp_media_service  (role, set_provider, link)
                  ^
            esp_player_service  (this component, ROLE_SINK)
                  |
   +--------------+----------------------------------+
   |  [DAC mixer (esp_audio_render)] [LCD render]    |
   |  +-- stream 0: this session A and/or V          |
   |  +-- stream 1: typically TTS / BGM (audio)      |
   +-------------------------------------------------+
```

各路 stream 地位相同。本次会话是音频、视频还是音视频，由 URL 解析、feed `set_track` 或 link 的 SRC 轨决定，再与已安装的出口取交集。每个实例一块屏；两路影片共用该视频 render。

板级产品应调用 `esp_audio_player_service_create()` 或 `esp_video_player_service_create()`，不要直接调用父类 `create()`，DAC / LCD 才会自动连接。同一喇叭上不要同时创建音频实例和视频实例。加屏调用 `esp_video_player_service_attach()`。

纯音频固件不要链接 `esp_video_render`。视频子类会把 GMF 视频元素注册进 pool。全局 decoder 表仍由应用注册。

### 头文件

| 头文件 | 内容 |
| --- | --- |
| `esp_player_service.h` | `create` / `destroy` / `set_deinit_cb`；句柄快照 `get_info` / `get_stream_info` |
| `esp_player_scheduler.h` | 供 `esp_service_scheduler_set_cb()` 使用的任务名宏与数据流 |
| `esp_player_service_setup.h` | 出口 `apply_setup`（PCM writer / 格式） |
| `esp_player_service_playback.h` | 每路 URL / feed / 音量 / 混音 / 播放列表 / ID3 / buffer / A/V 同步 |

## 快速开始

```c
#include "esp_audio_dec_default.h"
#include "esp_extractor_defaults.h"
#include "esp_player_service.h"
#include "esp_player_service_playback.h"
#include "esp_player_service_setup.h"
#include "esp_service.h"

ESP_ERROR_CHECK(esp_extractor_register_default());
ESP_ERROR_CHECK(esp_audio_dec_register_default());

esp_player_service_cfg_t cfg = ESP_PLAYER_SERVICE_CFG_DEFAULT();
cfg.max_stream_num = 2;
esp_player_service_t *p = NULL;
ESP_ERROR_CHECK(esp_player_service_create(&cfg, &p));

esp_player_service_setup_t setup = ESP_PLAYER_SERVICE_SETUP_DEFAULT();
setup.out_writer = my_pcm_writer;   /* 或 codec_dev；板载 DAC 使用音频子类 */
ESP_ERROR_CHECK(esp_player_service_apply_setup(p, &setup));
ESP_ERROR_CHECK(esp_service_start(ESP_SERVICE_BASE(p)));

ESP_ERROR_CHECK(esp_player_service_set_url(p, 0, "file:///sdcard/music.mp3"));
ESP_ERROR_CHECK(esp_player_service_play(p, 0));

esp_service_stop(ESP_SERVICE_BASE(p));
esp_player_service_destroy(p);
```

## 每路输入

每个 stream 同一时间只有一种输入。

| 路径 | 应用侧调用 | 数据 | 典型用途 |
| --- | --- | --- | --- |
| URL | `set_url` + `play` | 播放器自己拉取 | `file:///`、HTTP(S)、HLS、播放列表 |
| Feed | `set_track` + `write_frame` | **拷贝**进播放器 | 应用线程推 PCM / ES（TTS、自解析） |
| Link | `esp_media_service_link` 或 `set_provider` | **零拷贝** | 对端 SRC（如 capture）或 `esp_media_track_mngr`；**不要**调 `set_track` |

每条路径各有自己的启动和停止方式，见下面的示例。`esp_player_service_play()` / `_pause()` / `_resume()` / `_stop()` 是单路的播放传输（对 `esp_player_*` 的薄封装），不会创建或拆除生产者。停止整个实例要用另一组接口：`esp_service_stop()` / `esp_player_service_destroy()`。

Feed 和 Link **不要**调用 `play()`：`play()` 要求已经设置 URL；feed 在第一帧 `write_frame` 时启动；已 link 的 stream 由桥接任务启动。每路最多一条音频轨加一条视频轨；额外的音频（TTS）用另一路。PCM 需填写 `sample_rate` / `channel` / `bits`；ADTS AAC 和 MP3 只需 `codec`。音视频两路各 `set_track` 一次，再用 `frame->type` 分流。已 link 时 `write_frame` 返回 `ESP_ERR_INVALID_STATE`。下一会话只要音频：先 `esp_player_service_stop()`，再 `set_track` 一次音频（视频轨被丢弃），本次 `av_mask` 为 A。`set_url` 会按出口恢复 A/V mask。`esp_player_service_stop()` 成功后会清除已记录的输入种类，PAUSE 与 DROP 按新路径重新校验。

**URL：**

```c
/* 启动 */
ESP_ERROR_CHECK(esp_player_service_set_url(p, stream, "file:///sdcard/music.mp3"));
ESP_ERROR_CHECK(esp_player_service_play(p, stream));

/* 停止这路输入 */
ESP_ERROR_CHECK(esp_player_service_stop(p, stream));
```

**Feed：**

```c
/* 启动：先声明一次轨，再写第一帧，会话才会开始 */
esp_media_track_info_t track = {
    .id = 1,
    .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    .info.audio = {
        .codec = ESP_FOURCC_PCM,  /* PCM：同时填 sample_rate / bits / channel */
        .sample_rate = 8000,
        .bits_per_sample = 16,
        .channel = 2,
    },
};
ESP_ERROR_CHECK(esp_player_service_set_track(p, stream, &track));

esp_media_frame_t frame = {
    .track_id = 1,
    .type = ESP_MEDIA_TRACK_TYPE_AUDIO,
    .data = pcm,
    .size = pcm_bytes,
    .pts = pts_ms,
};
ESP_ERROR_CHECK(esp_player_service_write_frame(p, stream, &frame));
/* 最后一帧带 ESP_MEDIA_FRAME_FLAG_EOS */

/* 停止这路输入：应用停止写帧。stop() 立刻结束当前会话；
   之后再写会用同一套轨开新会话（只有要改轨集合时才需要
   重新 set_track）。 */
ESP_ERROR_CHECK(esp_player_service_stop(p, stream));
```

**Link：** 已有 SRC 服务时用 `link`；手里已有 `esp_media_provider_t`（例如来自 track manager）时用 `set_provider`。对端用 `esp_media_track_write_frame()` 写数据，不要用播放器的 `write_frame` / `set_track`。换 SRC 再 `link` / `set_provider` 一次，轨集合以新 SRC 为准（音视频换成纯音频即可）。重新 link 同一个 SRC 不必再声明轨。

```c
/* 启动：桥接任务在此处启动；若服务尚未 start，
   则由 esp_service_start() 启动 */
ESP_ERROR_CHECK(esp_media_service_link(ESP_SERVICE_BASE(src), src_stream,
                                       ESP_SERVICE_BASE(p), play_stream));
/* 或：ESP_ERROR_CHECK(esp_media_service_set_provider(
 *         ESP_SERVICE_BASE(p), play_stream, &provider)); */

/* 停止这路输入：unlink 会 abort provider 并等待桥接任务退出。
   只调 esp_player_service_stop() 只停播放器，桥接任务仍在拉流，
   下一帧 SRC 数据会再次开始播放。
   这一路要改成 URL 或 feed 前也必须先 unlink。 */
ESP_ERROR_CHECK(esp_media_service_unlink(ESP_SERVICE_BASE(src), src_stream,
                                         ESP_SERVICE_BASE(p), play_stream));
/* 或：ESP_ERROR_CHECK(esp_media_service_set_provider(
 *         ESP_SERVICE_BASE(p), play_stream, NULL)); */
```

拆除时 SRC 侧应调用 `esp_media_track_write_abort()`。SINK 侧 `unlink` / `set_provider(NULL)` 以及 `esp_service_stop()` / `destroy` 会 abort provider 并等待桥接任务退出；`esp_player_service_stop()` 不会，它只是播放传输。

三路同时接到一个 mixer 的完整示例见 `audio_player_mix_cli_example`。

## 混音与抢占

在 `apply_setup` 之后、该路开始播放之前，对每路调用一次 `esp_player_service_set_mix_cfg()`。只有配置过 mix 的 stream 才参与仲裁。第一次调用成功时，还会启动执行仲裁的服务线程。

每路有一个静态 `priority`：`ESP_PLAYER_PRIO_BACKGROUND`、`ESP_PLAYER_PRIO_NOTIFY` 或 `ESP_PLAYER_PRIO_URGENT`。更高优先级的一路处于 preparing、playing 或 paused 时，按**它自己的** `preempt_mode` 压制更低优先级的路：

| `preempt_mode` | 对更低优先级各路的效果 |
| --- | --- |
| `ESP_PLAYER_PREEMPT_COEXIST` | 音量压到 `duck_gain`，继续播放 |
| `ESP_PLAYER_PREEMPT_EXCLUSIVE` | 整路让出（暂停或丢弃，见 `on_preempt`） |

`on_preempt` 是**被抢占一方**的策略：更高优先级且为 EXCLUSIVE 的一路占用出口时生效。必须与该路的输入路径匹配：

| `on_preempt` | 允许的输入 | 行为 |
| --- | --- | --- |
| `ESP_PLAYER_ON_PREEMPT_PAUSE` | 仅 URL | 冻结时间轴，抢占结束后恢复 |
| `ESP_PLAYER_ON_PREEMPT_DROP` | 仅 feed 或 link | 被压制期间丢弃输出 |

feed/link 配置 `PAUSE`，或 URL 配置 `DROP`，在 mix 配置和输入路径都已知时返回 `ESP_ERR_INVALID_ARG`。`active_gain` 必须大于 0，且不小于 `duck_gain`。mixer 已经在运行且无法接受新的增益时，`set_mix_cfg` 返回 `ESP_ERR_INVALID_STATE`。当前是否被压制可用 `esp_player_service_get_preempt_state()` 查询。

背景音乐加 TTS，写法与 `audio_player_mix_cli_example` 相同：

```c
esp_player_mix_cfg_t bgm = {
    .active_gain = 1.0f,
    .duck_gain = 0.2f,
    .transition_ms = 80,
    .priority = ESP_PLAYER_PRIO_BACKGROUND,
    .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
    .on_preempt = ESP_PLAYER_ON_PREEMPT_PAUSE,  /* URL */
};
ESP_ERROR_CHECK(esp_player_service_set_mix_cfg(p, 0, &bgm));

esp_player_mix_cfg_t tts = {
    .active_gain = 1.0f,
    .duck_gain = 0.2f,
    .transition_ms = 80,
    .priority = ESP_PLAYER_PRIO_NOTIFY,
    .preempt_mode = ESP_PLAYER_PREEMPT_COEXIST,
    .on_preempt = ESP_PLAYER_ON_PREEMPT_DROP,  /* feed 或 link */
};
ESP_ERROR_CHECK(esp_player_service_set_mix_cfg(p, 1, &tts));
```

需要让其他所有路静音的告警类 feed，使用 `ESP_PLAYER_PRIO_URGENT`、`ESP_PLAYER_PREEMPT_EXCLUSIVE` 和 `ESP_PLAYER_ON_PREEMPT_DROP`。

## 注意事项

- 全局 extractor / decoder 表由应用持有。播放前调用 `esp_extractor_register_default()` 和 `esp_audio_dec_register_default()`，带屏产品再加 `esp_video_dec_register_default()`。本服务不会注册或注销这些表。
- `esp_player_service_apply_setup()` 会丢弃按旧出口建立的播放器，调用之后需要重新下发 URL 和 feed 轨声明。
- `esp_player_service_stop()` 只是播放传输，不会结束输入。每路输入有各自的停止方式，见 [每路输入](#每路输入)。

## 技术支持

请通过以下渠道获取技术支持：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能请求：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
