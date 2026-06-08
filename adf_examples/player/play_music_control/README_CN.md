# CLI 多源音乐播放

- [English Version](./README.md)
- 例程难度：⭐⭐

## 例程简介

本例程演示一个可通过串口 CLI 控制的多源音乐播放器。例程可将 microSD 卡本地音频、在线音频 URL 和 Flash 中的提示音加入同一播放列表，用户可在串口中执行命令完成播放、暂停、切歌、音量调节和播放模式切换。

microSD 卡挂载点 **`/sdcard`** 及其一层子目录中的音频会被自动扫描并加入播放列表；播放列表元数据可缓存到 `/sdcard/__playlist/` 以便下次启动快速加载。

本例支持 MP3、WAV、FLAC、AAC、M4A、TS、AMRNB、AMRWB 等常见音频格式。

### 典型场景

- 带串口 CLI 的多源音乐播放与播放列表管理
- SD 卡本地曲目与 HTTP/HTTPS 在线音频混合播放
- 嵌入式设备上的离线提示音（LittleFS）与联网流媒体演示

## 环境配置

### 硬件要求

- microSD 卡（FAT 格式），用于存放待播放音频
- Audio DAC 与扬声器（或耳机）
- 使用 HTTP 在线曲目时需可访问互联网的 Wi-Fi 环境
- 无片上 Wi-Fi 的芯片需开发板提供可用的联网能力

### 默认 IDF 分支

本例程支持 IDF release/v5.4（>= v5.4.3）与 release/v5.5（>= v5.5.2）分支。

### 软件要求

- 在 microSD 卡 **`/sdcard`** 或其一层子目录下放置支持的音频文件
- 在 menuconfig 中配置可访问公网的 Wi-Fi SSID 与密码，用于播放例程内置的 HTTPS 测试 URL

### 提示音资源

例程中的提示音存放在 Flash 的 **LittleFS** 分区（`storage`），运行时挂载为 `/littlefs`，通过 `file://littlefs/...` URI 播放。

`tone/` 目录下提供了四个测试文件：`alarm.mp3`、`dingdong.mp3`、`haode.mp3` 和 `new_message.mp3`。编译时由 `littlefs_create_partition_image()` 打包为 `storage.bin` 并随固件一起烧录。更换提示音只需替换 `tone/` 中的 mp3 文件后重新编译、烧录即可：

```
adf_examples/player/play_music_control/tone/
├── alarm.mp3
├── dingdong.mp3
├── haode.mp3
└── new_message.mp3
```

> 提示音总大小需小于 `partitions.csv` 中 `storage` 分区容量（默认 1 MB）。

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境；若已配置可跳过本段。若未配置，请在 ESP-IDF 根目录执行：

```
./install.sh
. ./export.sh
```

进入本例程工程目录：

```
cd adf_examples/player/play_music_control
```

本例程使用 [ESP Board Manager](https://github.com/espressif/esp-board-manager) 管理 SD 卡、音频输出等板级外设。安装辅助工具 [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) 后，可通过下文命令选择开发板：

```bash
pip install esp-bmgr-assist
```

查看支持的开发板：

```bash
idf.py bmgr -l
```

选择开发板（会生成 `components/gen_bmgr_codes/` 并设置 target；**该目录为本地生成，请勿提交 Git**）：

```bash
idf.py bmgr -b esp32_p4_function_ev
```

> 切换其他板型时，使用 `idf.py bmgr -b <board_name>` 重新生成即可。自定义板型请参考 [ESP Board Manager 自定义指南](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/docs/how_to_customize_board_cn.md)。

### 项目配置

本例程默认配置已写入 `sdkconfig.defaults` 与 `sdkconfig.defaults.<target>`（GMF IO、Simple Player 重采样、自定义分区表等）。通常只需按需修改 Wi-Fi：

```bash
idf.py menuconfig
```

在 menuconfig 中配置：

- **Example Configuration** → **WiFi SSID**
- **Example Configuration** → **WiFi Password**

> CI 构建使用 `sdkconfig.ci` 中的 `${CI_WIFI_SSID}` / `${CI_WIFI_PASSWORD}`。请勿在仓库的 `sdkconfig.defaults` 中提交 Wi-Fi 密码；本地调试请在 menuconfig 中填写密码。

默认配置确认项（一般 defaults 已满足）：

- **Audio Simple Player**：采样率 / 声道 / 位宽转换目标参数
- **GMF IO**：File / HTTP Reader，Codec Dev TX

本例使用自定义分区表 `partitions.csv`（`factory` 约 2.5 MB + `storage` LittleFS 1 MB），请确保开发板 Flash 容量不低于 `sdkconfig.defaults` 中的配置（默认 4 MB）。

配置完成后按 `s` 保存，按 `Esc` 退出。

### 编译与烧录

```
idf.py build
idf.py -p PORT flash monitor
```

`idf.py flash` 会同时烧录应用固件与 `storage` LittleFS 镜像（`build/storage.bin`）。退出 monitor：`Ctrl-]`。串口提示符为 `gmf>`，输入 `help` 查看命令。

## 如何使用例程

### 功能和用法

例程启动后会初始化外设、挂载 LittleFS、加载或扫描播放列表、连接 Wi-Fi（用于 HTTP 曲目），并启动 CLI 控制台。用户可使用以下命令：

| 命令          | 说明                                          |
|--------------|-----------------------------------------------|
| `scan`       | 扫描 SD 卡 `/sdcard` 下的一层子目录（含 `audio`） |
| `list`       | 列出播放列表中的所有音乐文件和 URL               |
| `play [N]`   | 播放当前或指定歌曲（N = 文件索引号）             |
| `pause`      | 暂停当前歌曲                                    |
| `resume`     | 恢复暂停的歌曲                                  |
| `stop`       | 停止当前歌曲                                    |
| `next`       | 播放下一首歌曲                                  |
| `prev`       | 播放上一首歌曲                                  |
| `get_vol`    | 获取当前音量                                     |
| `set_vol V`  | 设置音量（0-100）                                |
| `mute`       | 静音                                           |
| `unmute`     | 取消静音                                        |
| `status`     | 显示当前播放器状态                               |
| `mode [M]`   | 查询或设置曲目结束后的播放模式（见下方说明）       |

### 播放模式

当前曲目播放结束后，行为由**播放模式**决定：

- **stop**（默认）：播放后停止，不自动下一首。
- **single**：单曲循环，播完后重复当前曲目。
- **list**：列表循环，播完后播放下一首；列表末尾回到第一首。

无参数执行 `mode` 可查看当前模式；执行 `mode stop`、`mode single` 或 `mode list` 可设置模式。`status` 命令也会显示当前播放模式。

自动化测试（`pytest_play_music_control.py`）在 CLI 就绪后执行 `play 1` 并等待 `Song finished` log；需可访问公网以下载内置 HTTPS 测试 URL。

### 日志输出

例程启动后会加载播放列表（若存在 `/sdcard/__playlist/` 缓存则直接加载，否则扫描 **`/sdcard`**），添加 HTTP 与 LittleFS 提示音 URL，连接 Wi-Fi 后进入 CLI。以下为首次运行时的关键日志示例（时间戳、文件数量与 Wi-Fi 细节因 SD 卡内容与网络环境而异）：

```c
I (1327) main_task: Calling app_main()
I (1328) PLAY_MUSIC_CONTROL: === Play Music Control (multi-source) ===
I (1328) PLAY_MUSIC_CONTROL: [ 1 ] Setup peripheral for audio codec and SD card
Name: SC32G
Type: SDHC
Speed: 40.00 MHz (limit: 40.00 MHz)
Size: 30436MB
CSD: ver=2, sector_size=512, capacity=62333952 read_bl_len=9
SSR: bus_width=4
I (1411) PLAY_MUSIC_CONTROL: [ 2 ] Mount LittleFS tone partition
I (1419) PLAY_MUSIC_CONTROL: Partition size: total: 1048576, used: 90112
I (1419) PLAY_MUSIC_CONTROL: [ 3 ] Initialize simple player
I (1420) PLAY_MUSIC_CONTROL: [ 4 ] Initialize CLI music player and register commands
I (1462) CLI_MUSIC_CONTROL: Loaded 23 entries from playlist JSON (skip import)
I (1463) CLI_MUSIC_CONTROL: Music library ready! Use 'list' to see all files.
I (1463) CLI_MUSIC_CONTROL: ================================================
I (1463) CLI_MUSIC_CONTROL:      ESP-GMF Music Player - CLI Control
I (1463) CLI_MUSIC_CONTROL: ================================================
I (1463) CLI_MUSIC_CONTROL: Available commands:
I (1463) CLI_MUSIC_CONTROL:   scan      - Scan for music files in SD card
I (1463) CLI_MUSIC_CONTROL:   list      - List all music files and URLs
I (1464) CLI_MUSIC_CONTROL:   play [N]  - Start playing current or specified song (N=file index)
I (1464) CLI_MUSIC_CONTROL:   pause     - Pause current song
I (1464) CLI_MUSIC_CONTROL:   resume    - Resume paused song
I (1464) CLI_MUSIC_CONTROL:   stop      - Stop current song
I (1464) CLI_MUSIC_CONTROL:   next      - Play next song
I (1464) CLI_MUSIC_CONTROL:   prev      - Play previous song
I (1464) CLI_MUSIC_CONTROL:   get_vol   - Get current volume level
I (1465) CLI_MUSIC_CONTROL:   set_vol   - Set volume level (0-100)
I (1465) CLI_MUSIC_CONTROL:   mute      - Mute audio output
I (1465) CLI_MUSIC_CONTROL:   unmute    - Unmute audio output
I (1465) CLI_MUSIC_CONTROL:   status    - Show current player status
I (1465) CLI_MUSIC_CONTROL:   mode [stop|single|list] - Get/set play mode (stop/single loop/list loop)
I (1465) CLI_MUSIC_CONTROL:   help      - Show help information
I (1465) CLI_MUSIC_CONTROL:   restart   - Restart system
I (1465) CLI_MUSIC_CONTROL:   free      - Show free memory
I (1466) CLI_MUSIC_CONTROL: ================================================
I (1466) CLI_MUSIC_CONTROL: Found 23 music files. Use 'list' to view them.
I (1466) CLI_MUSIC_CONTROL: Use 'play' to start playing the first song.
I (1466) CLI_MUSIC_CONTROL: ================================================

I (1467) CLI_MUSIC_CONTROL: Audio muted (volume remains: 80%)
I (1467) CLI_MUSIC_CONTROL: Adding 3 source URLs to media DB and playlist...
I (1506) CLI_MUSIC_CONTROL: Added to media DB (count=23): https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.mp3
I (1544) CLI_MUSIC_CONTROL: Added to media DB (count=23): https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.aac
I (1581) CLI_MUSIC_CONTROL: Added to media DB (count=23): https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.wav
I (1606) CLI_MUSIC_CONTROL: Successfully added 3/3 source URLs (playlist count=23)
I (1606) CLI_MUSIC_CONTROL: Adding 4 source URLs to media DB and playlist...
I (1642) CLI_MUSIC_CONTROL: Added to media DB (count=23): file://littlefs/alarm.mp3
I (1680) CLI_MUSIC_CONTROL: Added to media DB (count=23): file://littlefs/dingdong.mp3
I (1720) CLI_MUSIC_CONTROL: Added to media DB (count=23): file://littlefs/haode.mp3
I (1757) CLI_MUSIC_CONTROL: Added to media DB (count=23): file://littlefs/new_message.mp3
I (1778) CLI_MUSIC_CONTROL: Successfully added 4/4 source URLs (playlist count=23)
I (1778) PLAY_MUSIC_CONTROL: [ 5 ] Connect to WiFi
I (9756) PLAY_MUSIC_CONTROL: WiFi connected

Type 'help' to get the list of commands.
Use UP/DOWN arrows to navigate through command history.
Press TAB when typing command name to auto-complete.
I (9802) PLAY_MUSIC_CONTROL: CLI ready (type 'help' for commands)
gmf>
gmf>  list
I (11859) CLI_MUSIC_CONTROL: === Music File List (Total: 23) ===
I (11859) CLI_MUSIC_CONTROL: * [1] [URL]  https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.mp3
I (11860) CLI_MUSIC_CONTROL:   [2] [FILE] sdcard/esp_gmf_rec001.aac
I (11861) CLI_MUSIC_CONTROL:   [3] [FILE] sdcard/audio/Solitary_brave.mp3
I (11861) CLI_MUSIC_CONTROL:   [4] [FILE] sdcard/audio/boot.mp3
I (11861) CLI_MUSIC_CONTROL:   [5] [FILE] sdcard/audio/esp_gmf_rec001.aac
I (11862) CLI_MUSIC_CONTROL:   [6] [FILE] sdcard/audio/ff-16b-2c-44100hz.aac
I (11862) CLI_MUSIC_CONTROL:   [7] [FILE] sdcard/audio/ff-16b-2c-44100hz.flac
I (11863) CLI_MUSIC_CONTROL:   [8] [FILE] sdcard/audio/ff-16b-2c-44100hz.mp3
I (11863) CLI_MUSIC_CONTROL:   [9] [FILE] sdcard/audio/ff-16b-2c-44100hz.wav
I (11864) CLI_MUSIC_CONTROL:   [10] [FILE] sdcard/audio/music.mp3
I (11864) CLI_MUSIC_CONTROL:   [11] [FILE] sdcard/audio/test.aac
I (11865) CLI_MUSIC_CONTROL:   [12] [FILE] sdcard/audio/test.flac
I (11865) CLI_MUSIC_CONTROL:   [13] [FILE] sdcard/audio/test.mp3
I (11865) CLI_MUSIC_CONTROL:   [14] [FILE] sdcard/audio/test.wav
I (11866) CLI_MUSIC_CONTROL:   [15] [FILE] sdcard/test.wav
I (11866) CLI_MUSIC_CONTROL:   [16] [FILE] sdcard/test.aac
I (11867) CLI_MUSIC_CONTROL:   [17] [FILE] sdcard/test.flac
I (11867) CLI_MUSIC_CONTROL:   [18] [URL]  https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.aac
I (11868) CLI_MUSIC_CONTROL:   [19] [URL]  https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.wav
I (11868) CLI_MUSIC_CONTROL:   [20] [FILE] littlefs/alarm.mp3
I (11869) CLI_MUSIC_CONTROL:   [21] [FILE] littlefs/dingdong.mp3
I (11869) CLI_MUSIC_CONTROL:   [22] [FILE] littlefs/haode.mp3
I (11869) CLI_MUSIC_CONTROL:   [23] [FILE] littlefs/new_message.mp3
I (11870) CLI_MUSIC_CONTROL: =============================

gmf>  set_vol 50
I (16335) CLI_MUSIC_CONTROL: Volume set to 50%
gmf>  play
I (18404) CLI_MUSIC_CONTROL: Audio unmuted (volume: 50%)
I (18404) CLI_MUSIC_CONTROL: Playing song [1/23]: https://dl.espressif.com/dl/audio/gs-16b-2c-44100hz.mp3
gmf>  I (23253) PLAY_MUSIC_CONTROL: Music info - rate:48000, channels:2, bits:16
I (23254) PLAY_MUSIC_CONTROL: Player state: ESP_AUD_SIMPLE_PLAYER_RUNNING
gmf>  next
I (27422) PLAY_MUSIC_CONTROL: Player state: ESP_AUD_SIMPLE_PLAYER_STOPPED
I (27423) CLI_MUSIC_CONTROL: Playing song [2/23]: file://sdcard/esp_gmf_rec001.aac
```

## 故障排除

- 若扫描未找到音乐文件，请确认 microSD 卡已挂载，且在 **`/sdcard/audio`**（或 **`/sdcard`** 下一层子目录）下放置了支持的音频文件；也可执行 `scan` 重新扫描。
- 若 LittleFS 提示音无法播放，请确认 `tone/` 目录中有 mp3 文件且已重新编译烧录（含 `storage.bin`）；启动日志中应出现 `Mount LittleFS tone partition` 及分区用量信息。
- 若 HTTP 曲目无法播放，请检查 **Example Configuration** 中的 Wi-Fi SSID/密码是否正确，启动日志中是否出现 `WiFi connected`，以及设备是否已获取 IP。
- 若播放失败，请检查音频文件是否损坏、格式是否受支持，以及 DAC 是否正常初始化。
- 例程默认启用 `CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY` 以便访问内置 HTTPS 测试 URL，仅适用于演示环境。

## 技术支持

请通过以下方式获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈请创建 [esp-adf issues](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
