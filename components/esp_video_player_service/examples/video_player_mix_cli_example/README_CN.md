# 视频播放器混音 CLI 示例

- [English](./README.md)
- 普通示例：⭐⭐

## 示例简介

本示例演示**一个** `esp_video_player_service` 句柄在同一 DAC 上混播影片与 TTS。
`create()` 返回 `esp_player_service_t *`。串口 CLI（`esp_cli_service`）控制各路启停。

stream 0 播影片。影片 URL（播放列表）、ES 喂帧和 media link 在该槽上互斥。
stream 1 的 TTS 可以叠加在三种片源上。

| Stream | 角色 | 优先级 | 输入 | 默认素材 |
|--------|------|--------|------|----------|
| 0 `movie` | 影片 | `BACKGROUND` | URL + playlist | `test.mp4` / `test1.mp4` / `test.mp3` |
| 0 `es` | 影片 | `BACKGROUND` | Feed（`set_track` + `write_frame`） | `/sdcard/feed.aac` + `feed.mjpeg` |
| 0 `link` | 影片 | `BACKGROUND` | Link（`esp_media_service_link`） | dummy SRC，弹跳小球 H264 + AAC |
| 1 `tts` | 提示 | `NOTIFY` | PCM feed | `/sdcard/test_8000hz_16bit_2ch_10000ms.pcm` |

`start movie` 时若 ES 或 link 在跑，会先停该路。`start es` / `start link` 时若
stream 0 已有其它片源，会先停掉。播放列表命令（`list` 始终可用；`play` /
`next` / `seek` / `pause`）只在 movie 模式下有效，ES 或 link 模式下返回
`INVALID_STATE`。Feed 用法请参考 `feed_source.c` 里的 `set_track` / `write_frame` /
PTS / EOS 契约，不必照搬 ADTS/JPEG 解析。Link 用法请参考 `link_source.c`。

不要再为 TTS 单独 `audio_create` 第二套 mixer。影片在 TTS 播放时按 `COEXIST` 压低。

初始化 LCD 之前，示例会把板级帧缓冲数量改成 2。`esp_video_render` 只有在面板拥有
多个帧缓冲时才直接画到面板缓冲；单缓冲会逐帧拷贝。

### 典型场景

带屏产品在同一喇叭上播容器影片、应用自持的 ES，或 link 过来的 SRC，同时播提示音 / TTS。

## 环境配置

### 硬件需求

- 默认开发板：**ESP32-S3**（**320x240 LCD**、**音频 DAC**、**SD 卡**、PSRAM，通过 `esp_board_manager` 管理）
- FAT 格式 microSD 卡，上述文件放在挂载根目录

若板子没有显示设备，示例仍可启动。影片音频与 TTS 仍可混音；无视频输出时 ES 只喂音频。

### 默认 IDF 分支

本示例支持 IDF release/v5.4（>= v5.4.3）与 release/v5.5（>= v5.5.2）。

### 软件需求

将以下文件放到 SD 卡挂载根目录（例如 `/sdcard/`）：

| 文件 | 说明 |
|------|------|
| `test.mp4` | 播放列表 index 0 |
| `test1.mp4` | 播放列表 index 1 |
| `test.mp3` | 播放列表 index 2（故意纯音频） |
| `feed.aac` | `start es` 用的 AAC ADTS |
| `feed.mjpeg` | `start es` 用的 MJPEG |
| `test_8000hz_16bit_2ch_10000ms.pcm` | 原始 PCM：16-bit LE，8 kHz 立体声（约 10 s） |

缺 ES 文件只会导致 `start es` 失败，movie URL、link 与 TTS 仍可用。

生成 PCM：

```bash
edge-tts --text "Hello from the video player mix CLI" --write-media tts.mp3
ffmpeg -i tts.mp3 -ar 8000 -ac 2 -t 10 -f s16le test_8000hz_16bit_2ch_10000ms.pcm
```

从任意片子生成 ES。请参考 feed 契约，不必照搬分帧解析：

```bash
ffmpeg -i input.mp4 -vn -c:a aac -b:a 96k -ar 44100 -ac 2 -f adts feed.aac
ffmpeg -i input.mp4 -an -c:v mjpeg -q:v 6 -r 15 -pix_fmt yuvj420p \
       -vf scale=320:240 -f mjpeg feed.mjpeg
```

`-vf scale=` 必须与 LCD 一致（本例程 ESP32-S3 SPI 屏为 320x240）。
`-r` 必须与 `main/settings.h` 里的 `EXAMPLE_VIDEO_FPS` 一致。文件名也可在该头文件修改。

## 编译和烧录

### 编译准备

```bash
./install.sh
. ./export.sh
cd $ADF_PATH/components/esp_video_player_service/examples/video_player_mix_cli_example
pip install esp-bmgr-assist
idf.py bmgr -l
idf.py bmgr -b <board_index|board_name>
```

### 编译与烧录命令

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

## 如何使用示例

### 功能与用法

启动后会执行一次 `help`，列出全部已注册命令。
命令提示符为 `vpm>`：

| 命令 | 说明 |
|------|------|
| `start movie` | 用 URL 播放列表播 stream 0 |
| `start es` | 在 stream 0 喂 AAC + MJPEG（会先停 movie 或 link） |
| `start link` | 把 dummy SRC（弹跳小球 H264 + AAC）link 到 stream 0（会先停 movie 或 ES） |
| `start tts` | 在 stream 1 喂 PCM（movie / ES / link 下都能开） |
| `stop movie` / `stop es` / `stop link` | 停 stream 0（三种输入都停） |
| `stop tts` | 停 stream 1 |
| `list` / `list <start>` | 从 `start` 列出 movie 播放列表（`*` 为当前） |
| `play` / `play <index>` | 播当前 movie 项，或指定 index |
| `next` / `prev` | movie 列表下一首 / 上一首 |
| `seek <ms>` | 在当前 movie 项内 seek |
| `mode <none\|one\|all\|shuffle>` | movie 列表循环模式 |
| `pause` / `resume` | 暂停 / 继续 movie URL |
| `status` | 打印角色、播放状态、列表、ES 计数或 link |
| `help` | 列出控制台命令 |

典型会话：

```text
vpm> start movie
vpm> list
vpm> play 1
vpm> start tts
vpm> status
vpm> stop tts
vpm> start es
vpm> stop es
vpm> start link
vpm> stop link
```

### 循环模式（`mode`）

| 模式 | 含义 |
|------|------|
| `none` | 到列表两端停止；next/prev 与自动切歌不回绕 |
| `one` | 单曲循环 |
| `all` | 列表循环（本示例默认） |
| `shuffle` | next/prev / 自动切歌时随机选另一首 |

## 故障排除

- `start movie failed: ESP_ERR_NOT_FOUND` — 把 `test.mp4` 放到 `/sdcard/`。
- `start es failed` — 把 `feed.aac` / `feed.mjpeg` 放到 `/sdcard/`，或继续用 movie / link。
- `play` / `next` 打印 `stream 0 is not in movie mode` — 先 `stop es` 或 `stop link` 再用 movie 命令。
- `start tts failed` / `Failed to open PCM` — 把 PCM 文件放到 `/sdcard/`。
- 有声音但屏幕全黑 — 板子没有拉起显示；启动日志会打印 `No display came up`。
- 画面撕裂并刷 `dma2d_configure_color_space_conversion` — 面板只有一块帧缓冲。
  启动时应看到 `Using 2 LCD frame buffers`。

## 技术支持

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈：[ESP-ADF GitHub issues](https://github.com/espressif/esp-adf/issues)
