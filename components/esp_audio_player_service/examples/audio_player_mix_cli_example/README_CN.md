# Audio Player Mix CLI 例程

- [English Version](./README.md)
- Regular Example: ⭐⭐

## 例程简介

本例程演示在同一个 `esp_audio_player_service` 上并发混音三条不同输入路径的
stream，并通过串口 CLI（`esp_cli_service`）控制哪一路加入或退出。stream 0
是 URL 播放列表，不是单个文件。

| Stream | 角色 | 优先级 | 输入路径 | 默认素材 |
|--------|------|--------|----------|----------|
| 0 `url` | 背景音乐 | `BACKGROUND` | URL + playlist（`set_playlist` + `play_index`） | `test.mp3` / `.aac` / `.wav` / `.opus` |
| 1 `link` | 通知档 | `NOTIFY` | Link（`esp_media_service_link`） | dummy SRC，pattern AAC |
| 2 `feed` | 紧急播报 | `URGENT` | Feed（`set_track` + `write_frame`） | `/sdcard/test_8000hz_16bit_2ch_10000ms.pcm` |

混音与抢占策略在启动时写死：URL / LINK 为 `COEXIST`（压低共存），FEED 为
`EXCLUSIVE`（完全让路）。FEED 起播时 URL 按 `PAUSE` 冻结续播，LINK 按 `DROP`
丢帧让路；结束后自动恢复。播放列表命令（`list` / `play` / `next` / `prev` /
`mode`）只作用于 stream 0。Link 用法请参考 `link_source.c`。dummy SRC 会一直循环，
直到 `stop link`。

### 典型场景

需要在同一 DAC 上混播 BGM、通知档叠加与紧急播报，并按优先级压低或抢占，
同时 BGM 带本地播放列表的产品。本例通知档用 dummy SRC 占位，产品里可换成
TTS 或其它 SRC。

## 环境准备

### 硬件要求

- 默认开发板：**ESP32-S3**（**音频 DAC**、**SD 卡**、PSRAM，通过 `esp_board_manager` 管理）
- FAT 格式的 microSD 卡

### 默认 IDF 分支

本例程支持 IDF release/v5.4（>= v5.4.3）和 release/v5.5（>= v5.5.2）。

### 软件要求

将以下文件放到 SD 卡挂载根目录（例如 `/sdcard/`）：

| 文件 | 说明 |
|------|------|
| `test.mp3` | 播放列表 index 0 |
| `test.aac` | 播放列表 index 1 |
| `test.wav` | 播放列表 index 2 |
| `test.opus` | 播放列表 index 3 |
| `test_8000hz_16bit_2ch_10000ms.pcm` | `start feed` 用的原始 PCM：16-bit 小端，8 kHz 立体声（约 10 s）；格式见 `main/settings.h` |

缺 PCM 只会导致 `start feed` 失败，URL 与 link 仍可用。

在 PC 上生成该 PCM 文件（示例）：

```bash
edge-tts --text "你好，欢迎使用混音 CLI 例程" --write-media test.mp3
ffmpeg -i test.mp3 -ar 8000 -ac 2 -t 10 -f s16le test_8000hz_16bit_2ch_10000ms.pcm
```

文件名、播放列表 URL 和 PCM 格式可在 `main/settings.h` 中修改。

## 编译与烧录

### 编译准备

先完成 ESP-IDF 环境：

```bash
./install.sh
. ./export.sh
```

进入例程目录：

```bash
cd $ADF_PATH/components/esp_audio_player_service/examples/audio_player_mix_cli_example
```

本例程使用 [ESP Board Manager](https://github.com/espressif/esp-board-manager)。
在已激活的 IDF Python 环境中安装助手：

```bash
pip install esp-bmgr-assist
```

列出并选择板型：

```bash
idf.py bmgr -l
idf.py bmgr -b <board_index|board_name>
```

### 编译与烧录

```bash
idf.py build
idf.py -p PORT flash monitor
```

## 如何使用

### 功能与操作

启动后会执行一次 `help`，列出全部已注册命令。
提示符为 `mix>`：

| 命令 | 说明 |
|------|------|
| `start url` | 加入 stream 0 并播放当前列表项 |
| `start link` | 加入 stream 1（dummy SRC，循环 pattern AAC） |
| `start feed` | 加入 stream 2（feed / PCM） |
| `stop url` / `stop link` / `stop feed` | 退出该路并释放生产者 |
| `list` / `list <start>` | 从 `start` 列出 URL 播放列表（`*` 为当前） |
| `play` / `play <index>` | 播当前 URL 项（暂停则 resume），或指定 index |
| `next` / `prev` | URL 列表下一首 / 上一首 |
| `mode <none\|one\|all\|shuffle>` | URL 列表循环模式 |
| `pause` / `resume` | 只暂停 / 继续 stream 0 |
| `status` | 打印角色、播放状态与当前 URL 曲目 |
| `help` | 列出控制台命令 |

典型操作：

```text
mix> start url
mix> list
mix> play 1
mix> next
mix> start link
mix> start feed
mix> status
mix> stop feed
mix> stop url
```

优先级为 URL < LINK < FEED。起播 LINK 会压低 URL（`COEXIST`）；起播 FEED 会
`EXCLUSIVE` 抢占 URL / LINK（URL 暂停续播，LINK 丢帧）。

### 循环模式（`mode`）

| 模式 | 含义 |
|------|------|
| `none` | 到列表两端停止；next/prev 与自动切歌不回绕 |
| `one` | 单曲循环 |
| `all` | 列表循环（本例程默认） |
| `shuffle` | next/prev / 自动切歌时随机选另一首 |

## 故障排除

- 无声音：确认 DAC 初始化日志，以及 SD 卡上存在播放列表文件 / PCM。
- `start feed` 打不开 PCM：检查路径与 `settings.h` 中的格式。
- `play` / `next` 失败：当前列表项对应的文件不在卡上。
- 看不到 CLI：检查板子的 USB-Serial-JTAG / UART 控制台配置。

## 技术支持

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- Issue 反馈：[ESP-ADF GitHub issues](https://github.com/espressif/esp-adf/issues)
