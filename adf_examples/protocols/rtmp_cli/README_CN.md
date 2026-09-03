# RTMP CLI 综合示例

- [English Version](./README.md)
- 例程难度：⭐⭐⭐

## 例程简介

本例程在一份固件中提供五种 RTMP 模式，并通过串口 CLI 控制，用户可完成推流、拉流、中继和回环验证，并在停止当前会话后切换模式，无需重新编译或烧录：

- **push**：将摄像头画面和麦克风声音发布到 MediaMTX、nginx-rtmp、YouTube、Twitch 或其他 RTMP 服务器
- **pull**：在开发板 LCD 和扬声器上播放远端 RTMP 流
- **server**：接收 `ffmpeg` 发布的流，并将其转发给多个播放器
- **live**：让网络播放器直接从开发板拉取本机摄像头和麦克风的实时流
- **loopback**：同时运行 server、push 和 pull，在无需 PC 的情况下验证开发板上的完整 RTMP 链路

**server** 模式只负责中继，因此 **live** 组合 server 和 push 对外提供本机摄像头流，**loopback** 再加入 pull；三个独立槽位使这些 service 可以同时运行。

五种模式采用同一套 service 组合方式：应用按需创建采集、播放和 RTMP service，通过 `esp_media_service_link()` 连接媒体生产端与消费端，并使用统一的生命周期 API 控制会话，无需实现 RTMP 协议处理和媒体数据流；串口 CLI 也为脚本和智能体提供可复用的自动化控制入口。

### 典型场景

- 把摄像头推流到 YouTube、Twitch 等 CDN 接入点
- 向中心媒体服务器推流的视频门铃、监控节点
- 无需额外部署服务器、播放器直接从开发板拉流的一体机
- 在本地 LCD 上播放远端 RTMP 流的网络广播屏

### 运行机制

1. `esp_board_manager` 初始化摄像头、LCD、音频 ADC 与 DAC
2. `esp_wifi_service` 连接 Station
3. `esp_cli_service` 启动 `rtmp>` 控制台
4. **push**：`esp_video_capture_service` → `esp_media_service_link()` → `esp_rtmp_service` ROLE_SINK
5. **server**：`esp_rtmp_service` ROLE_SERVER 只做中继，没有媒体 link
6. **pull**：ROLE_SRC → `esp_media_service_link()` → `esp_video_player_service` → LCD 与扬声器

链路的消费端总是先启动。停止时槽位内部顺序相反，并先释放客户端槽位再释放服务器。

### 文件结构

```
rtmp_cli/
├── main/
│   ├── app_main.c              例程入口：NVS、板级设备、编解码器、Wi-Fi、CLI
│   ├── rtmp_example.h          板级与 Wi-Fi 辅助接口
│   ├── rtmp_session.c/h        三槽位的 create、setup、link、start 与停止
│   ├── rtmp_cli.c/h            `rtmp` 与 `wifi` 命令解析
│   ├── rtmp_settings.h         媒体与协议默认值
│   ├── Kconfig.projbuild       Wi-Fi 凭据与默认对端 URL
│   └── idf_component.yml
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── sdkconfig.defaults.esp32s3
├── sdkconfig.defaults.esp32s31
├── sdkconfig.ci
├── partitions.csv
├── pytest_rtmp_cli.py
├── README.md
└── README_CN.md
```

## 环境配置

### 硬件要求

- ESP32-P4 Function EV Board（摄像头、MIPI LCD、音频编解码芯片）
- 与开发板同网段的 PC，用于运行 ffmpeg、ffplay 或 RTMP 服务器
- 无片上 Wi-Fi 的芯片需开发板提供可用的联网能力

摄像头与 LCD 都不是必需的：没有摄像头时 push 槽位仍可用 `-v none` 推纯音频，没有屏幕时 pull 槽位只播放音频。`rtmp loopback -v none` 两者都不需要。

### 默认 IDF 分支

本例程支持 IDF release/v5.4（>= v5.4.3）、release/v5.5（>= v5.5.2）以及 IDF v6.1。

### 软件要求

- 安装 FFmpeg，其中同时包含 `ffmpeg` 与 `ffplay`
- 在 menuconfig 中配置 Wi-Fi SSID 与密码
- 播放开发板提供的流（`server`、`live`）：`ffplay rtmp://<设备 IP>:1935/live/stream`
- 向开发板推流（`server`）：`ffmpeg` 使用 `-f flv rtmp://<设备 IP>:1935/live/stream`
- 接收推流或提供拉流源（`push`、`pull`）时，在 PC 上运行 RTMP 服务器。[MediaMTX](https://github.com/bluenviron/mediamtx) 无需配置即可配合本例程使用：

```
./mediamtx
# RTMP 服务地址为 rtmp://<PC 的 IP>:1935/<路径>
```

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境；若已配置可跳过本段。若未配置，请在 ESP-IDF 根目录执行：

```
./install.sh
. ./export.sh
```

进入本例程工程目录：

```
cd adf_examples/protocols/rtmp_cli
```

本例程使用 [ESP Board Manager](https://github.com/espressif/esp-board-manager) 管理摄像头、LCD、音频编解码等板级外设。推荐安装辅助工具 [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) 作为默认入口。

在已激活的 ESP-IDF Python 环境下安装（同一环境只需安装一次）：

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # 当提示需要更新时执行此命令
```

列出当前可见的开发板：

```bash
idf.py bmgr -l
```

输出示例：

```text
ℹ️  Board Components:
  espressif/esp_boards:
    [1] esp32_c3_lyra
    [2] esp32_lyrat_4_3
    [3] esp32_lyrat_mini_1_1
    [4] esp32_p4_eye
    [5] esp32_p4_function_ev_board
    [6] esp32_s31_function_coreboard_1
    [7] esp32_s31_korvo_1
    [8] esp32_s3_box_3
    [9] esp32_s3_box_lite
    [10] esp32_s3_korvo_2_3
    [11] esp32_s3_lcd_ev_board
    [12] esp_vocat_1_0
    [13] esp_vocat_1_2
```

以上输出示例基于 `esp_boards` 0.5.2 的开发板列表和排序。不同 `esp_boards` 版本或自定义开发板依赖可能会使列表和序号变化，使用时以 `idf.py bmgr -l` 的实际输出为准。

选择开发板：

```bash
idf.py bmgr -b <board_index|board_name>
```

例如选择 `esp32_p4_function_ev_board`：

```bash
idf.py bmgr -b 5
# 或
idf.py bmgr -b esp32_p4_function_ev_board
```

首次执行 `idf.py bmgr` 时，组件会根据本工程 `main/idf_component.yml` 中声明的 `espressif/esp_board_manager` 依赖自动下载。

> [!NOTE]
> 如果切换为其他 `esp_board_manager` 支持的开发板，请按相同步骤执行并替换板型名称/索引。如有需要，请在重新编译前执行 `idf.py fullclean`。
> 所选开发板应提供 `camera`、`display_lcd`、`audio_adc` 和 `audio_dac`，才能完整体验所有槽位。
> 自定义开发板请参考 [创建开发板指南](https://docs.espressif.com/projects/esp-board-manager/zh_CN/latest/create-board/index.html)。
> `esp_board_manager` 更多信息请参考 [ESP_BOARD_MANAGER 入门指南](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README_CN.md)

### 项目配置

本例程默认配置已写入 `sdkconfig.defaults` 与 `sdkconfig.defaults.<target>`。媒体默认值在 `main/rtmp_settings.h`。通常只需按需修改 Wi-Fi：

```bash
idf.py menuconfig
```

在 menuconfig 中配置：

- **RTMP CLI Example Configuration** → **WiFi SSID**
- **RTMP CLI Example Configuration** → **WiFi Password**
- **RTMP CLI Example Configuration** → **Default push URL** / **Default pull URL**（`rtmp push` / `rtmp pull` 未带 URL 时使用）

> CI 构建使用 `sdkconfig.ci` 中的 `${CI_WIFI_SSID}` / `${CI_WIFI_PASSWORD}`。请勿在仓库的 `sdkconfig.defaults` 中提交 Wi-Fi 密码；本地调试请在 menuconfig 中填写密码。

`main/rtmp_settings.h` 中的常用可调项：

- `RTMP_VIDEO_WIDTH`、`RTMP_VIDEO_HEIGHT`、`RTMP_VIDEO_FPS`、`RTMP_VIDEO_CODEC`
- `RTMP_AUDIO_CODEC`、`RTMP_AUDIO_SAMPLE_RATE`、`RTMP_AUDIO_BITRATE`
- `RTMP_SERVER_PORT`、`RTMP_SERVER_APP`、`RTMP_SERVER_STREAM`、`RTMP_SERVER_MAX_CLIENTS`
- `RTMP_CHUNK_SIZE`、`RTMP_RECV_AUDIO_CACHE`、`RTMP_RECV_VIDEO_CACHE`

所有媒体默认值都可以在命令行上按会话覆盖。

配置完成后按 `s` 保存，按 `Esc` 退出。

### 资源优化

默认配置保留五种 CLI 模式使用的全部 RTMP 角色和编解码格式；功能固定的产品可在 menuconfig 中减小固件体积：

- 进入 **Component config → ESP-RTMP Service**，push 使用 `CONFIG_ESP_RTMP_SERVICE_SINK_SUPPORT`、pull 使用 `CONFIG_ESP_RTMP_SERVICE_SRC_SUPPORT`、server 使用 `CONFIG_ESP_RTMP_SERVICE_SERVER_SUPPORT`；live 需要 `SERVER` 和 `SINK`，loopback 需要全部三种角色。
- 进入 **Component config → Audio Codec Configuration** 和 **Component config → Video Codec Configuration**，通过 `CONFIG_AUDIO_ENCODER_*_SUPPORT`、`CONFIG_AUDIO_DECODER_*_SUPPORT`、`CONFIG_VIDEO_ENCODER_*_SUPPORT` 和 `CONFIG_VIDEO_DECODER_*_SUPPORT` 开关编解码器；push 和 live 需要编码器、pull 需要解码器、纯中继 server 无需编解码器、loopback 两者都需要，因此仅保留实际使用的格式，并为每种视频格式保留一种适合目标芯片的实现。

例如，固定使用 H264/AAC 的推流产品通常只需保留 RTMP `SINK` 角色以及 H264/AAC 编码器。关闭角色或编解码格式后，相应 CLI 模式或格式将不可用；修改这些选项后请先执行 `idf.py fullclean` 再重新编译。

运行时执行 `rtmp stop` 会销毁活动的 RTMP、采集和播放 service，但板级设备、编解码器注册和 Wi-Fi 保持初始化，以便无需重启即可启动其他仍受支持的模式。

### 编译与烧录

```
idf.py build
idf.py -p PORT flash monitor
```

退出 monitor：`Ctrl-]`。串口提示符为 `rtmp>`，输入 `help` 查看命令。

## 如何使用例程

### 功能和用法

例程启动后会初始化外设、连接 Wi-Fi 并启动 CLI。如果 menuconfig 中的凭据有误或未填写，可以用 `wifi <ssid> <password>` 在运行时连接。

| 命令 | 说明 |
|------|------|
| `rtmp server [选项]` | 在 `rtmp://<设备 IP>:<端口>/<应用>` 上启动中继服务器 |
| `rtmp push [url] [选项]` | 把摄像头与麦克风推流到 RTMP URL |
| `rtmp pull [url] [选项]` | 拉取 RTMP 流并在 LCD 与扬声器上播放 |
| `rtmp live [选项]` | 服务器加推流端，播放器从开发板拉取本机摄像头 |
| `rtmp loopback [选项]` | 服务器、推流、拉流同时运行，整条链路只在开发板上 |
| `rtmp stop` | 停止并销毁全部活动的 RTMP、采集和播放槽位 |
| `rtmp info` | 打印活动槽位、URL 以及服务器的客户端与会话表 |
| `wifi [ssid] [password]` | 连接 Wi-Fi；不带参数时使用 menuconfig 中的配置 |

`rtmp push` 和 `rtmp pull` 省略 URL 时分别使用 `RTMP_EXAMPLE_PUSH_URL` 与 `RTMP_EXAMPLE_PULL_URL`。

| 选项 | 适用槽位 | 说明 |
|------|---------|------|
| `-p <port>` | server、live、loopback | 监听端口，默认 `1935` |
| `--app <name>` | server、live、loopback | RTMP 应用名，默认 `live` |
| `--stream <name>` | server、live、loopback | 流名，默认 `stream` |
| `--max-clients <n>` | server、live、loopback | 服务器客户端上限，默认 `4` |
| `-v h264\|mjpeg\|none` | push、live、loopback | 视频编码；push/live 默认 `h264`，loopback 默认 `mjpeg`；`none` 表示纯音频 |
| `-a aac\|pcm\|g711a\|g711u\|none` | push、live、loopback | 音频编码，默认 `aac`；`none` 表示纯视频 |
| `--res <WxH>` | push、live、loopback | 采集分辨率，例如 `--res 1280x720` |
| `--fps <n>` | push、live、loopback | 采集帧率 |
| `--bitrate <bps>` | push、live、loopback | 视频码率，不指定时按画面尺寸推导 |
| `--no-video` / `--no-audio` | push、live、loopback | `-v none` / `-a none` 的简写 |
| `--chunk <bytes>` | 全部 | RTMP chunk 大小，默认 `4096` |
| `--cache <bytes>` | pull | 视频接收缓存大小 |
| `--insecure` | push、pull | 接受不校验证书的 `rtmps://` 服务器 |

选择 `g711a` 或 `g711u` 时音频固定为 8 kHz 单声道，这是 G.711 的规定。

自动化测试（`pytest_rtmp_cli.py`）在 CLI 就绪后执行 `rtmp loopback -v none`，并等待 `PREPARING -> PLAYING`。

**把摄像头提供给 PC 播放器** 使用 `rtmp live`，然后在 PC 上：

```
rtmp> rtmp live
```

```
ffplay rtmp://192.168.1.23:1935/live/stream
```

其他组合：

```
rtmp> rtmp live -v h264 --res 1280x720 --fps 15 --bitrate 2000000
rtmp> rtmp live -v none                   # 纯音频流
rtmp> rtmp live -a none -p 8935           # 8935 端口上的纯视频流
rtmp> rtmp live --app app1 --stream cam0  # rtmp://<设备 IP>:1935/app1/cam0
```

**推流到远端 RTMP 服务器。** 在 PC 上启动 MediaMTX，然后：

```
rtmp> rtmp stop
rtmp> rtmp push rtmp://192.168.1.10:1935/live/stream
```

```
ffplay rtmp://192.168.1.10:1935/live/stream
```

CDN 接入点用法相同，串流密钥是 URL 的最后一段：

```
rtmp> rtmp push rtmp://a.rtmp.youtube.com/live2/<your-stream-key>
```

`rtmps://` 会用 IDF 证书包校验服务器证书。自签名测试服务器请加 `--insecure`：

```
rtmp> rtmp push rtmps://192.168.1.10:1936/live/stream --insecure
```

**播放远端流：**

```
rtmp> rtmp stop
rtmp> rtmp pull rtmp://192.168.1.10:1935/live/stream
```

编码格式来自流的元数据，拉流侧无需指定。弱网可用 `--cache` 加大接收缓冲。

**只做中继服务器。** 开发板自身不带媒体。从 PC 推流：

```
ffmpeg -re -f lavfi -i testsrc=size=640x480:rate=15 -f lavfi -i sine -c:v libx264 -preset ultrafast -tune zerolatency -c:a aac -f flv rtmp://192.168.1.23:1935/live/stream
```

```
ffplay rtmp://192.168.1.23:1935/live/stream
```

**整条链路只在开发板上运行** 使用 `rtmp loopback`。数据路径为 capture → SINK → 本机服务器 → SRC → LCD。`rtmp loopback -v none` 在没有摄像头和屏幕的开发板上同样可以运行。

`rtmp loopback` 默认使用 MJPEG，这样板上 LCD 跟得上：ESP32-P4 的 JPEG 解码是硬件的，H264 解码是软件的。`rtmp live` 和 `rtmp push` 仍默认 H264，方便 ffplay 拉流。只有还要用 PC 拉同一路流时，才在 loopback 上加 `-v h264`。

三个槽位彼此独立。每次启动只重启自己那个槽位，因此可以在服务器持续运行的情况下改变推流目标：

```
rtmp> rtmp server
rtmp> rtmp push rtmp://127.0.0.1:1935/live/stream    # 等价于 'rtmp live'
rtmp> rtmp push rtmp://192.168.1.10:1935/live/stream # 改变推流目标，服务器保持运行
rtmp> rtmp stop                                      # 释放全部槽位
```

每种角色同时只能存在一个实例。对端已经断开连接时，`rtmp info` 会把该槽位标记为 `(peer left)`，下一次在该槽位上启动会自动释放它。

### 日志输出

启动过程用 `[ 1 ]` 到 `[ 5 ]` 编号，以 `CLI ready` 结束。会话运行期间每个 RTMP 事件都会连同触发它的槽位一起打印：

- `push: PEER_CLOSED` 或 `pull: PEER_CLOSED`：远端主动断开了连接
- `server: SERVER_CLIENT_CONNECTED`：有客户端接入中继服务器
- `server: SERVER_PULLER_STARTED` / `SERVER_PULLER_STOPPED`：仅在进程内本地 pusher 通过 `esp_rtmp_server_monitor_puller()` 注册时才会抛出。经 socket 接入的客户端（包括本例程在 `127.0.0.1` 上的推流端）不会触发它们。请改看 `SERVER_CLIENT_CONNECTED` 和 `rtmp info`

拉流槽位先启动播放器，再启动源。`rtmp loopback -v none` 这类纯音频流会直接进入 `PREPARING -> PLAYING`。音视频流在视频头晚于音频到达时，可能还会打印 `PLAYER_SERVICE: Stream 0 took a late video track, restarting the feed session`，这是预期行为。紧随其后的一小段 `H264_DEC` 报错表示播放器从 GOP 中间接入。

下面两段来自 ESP32-P4 Function EV 上同一次 `rtmp loopback -v none` 运行。第一段是启动中继服务器，第二段是播放器在收到音频帧后离开 `PREPARING`。第一段里的 `ffmpeg ...` 是固件打印的字面量。

```text
rtmp loopback -v none
I (45589) ADF_EVENT_HUB: Create 'server': domain registered
I (45589) ESP_SERVICE: [server] Initialized
I (45590) ESP_SERVICE: [server] Started
I (45591) RTMP_SESSION: Local RTMP server listening on port 1935, app 'live', up to 4 clients
I (45591) RTMP_SESSION: Publish into it with: ffmpeg ... -f flv rtmp://192.168.3.101:1935/live/stream
I (45592) RTMP_SESSION: Play from it with:    ffplay rtmp://192.168.3.101:1935/live/stream
```

```text
I (47574) RTMP_SESSION: Playing rtmp://127.0.0.1:1935/live/stream
I (47574) RTMP_SESSION: Loopback running: camera -> SINK -> local server -> SRC -> LCD
rtmp>  
rtmp>  I (47584) RTMP_SERVER: Add client 0x48263ab4 count 2
I (47584) RTMP_SERVER: Start close client 0x4827b79c
I (47613) ESP_PLAYER: Set av_mask: 3
I (47613) ESP_PLAYER: Set sync_mode: 1
I (47613) ESP_PLAYER: Set av_mask: 1
I (47614) ESP_PLAYER: set dec cfg, type: 541278529, line: 129
I (47614) ESP_PLAYER_STATE: Handling cmd: PREPARE in state: IDLE
I (47614) ESP_PLAYER_STATE: State transition: IDLE -> PREPARING
I (47615) ESP_PLAYER_STATE: Entering PREPARING state
I (47616) ESP_PLAYER_STATE: Audio decoder started, waiting for ready event
W (47617) ESP_GMF_ASMP_DEC: Not enough memory for out, need:4096, old: 1024, new: 4096
I (47619) ESP_PLAYER_STATE: Handling cmd: REPORT_AUDIO_INFO in state: PREPARING
I (47624) ESP_PLAYER_AUDIO_RENDER: Audio render opened successfully
I (47625) ESP_PLAYER_STATE: Handling cmd: PLAYING in state: PREPARING
I (47625) ESP_PLAYER_STATE: State transition: PREPARING -> PLAYING
I (47625) ESP_PLAYER_STATE: Entering PLAYING state, old_state: PREPARING
```

`PREPARING -> PLAYING` 是数据走过采集、推流、服务器和播放器的证明。`rtmp stop` 结束时会打印 `All RTMP slots released`。

## 故障排除

- 出现 `rtmp push failed: ESP_ERR_NOT_FOUND` 或 `V4L2_SRC: Fail to open device`：摄像头未就绪。检查排线和板型，或用 `-v none` 只推音频。
- `rtmp push` 连接失败：客户端 URL 必须带流名，形如 `rtmp://host:port/app/stream`；服务器 URL 到应用名为止：`rtmp://host:port/app`。
- ffplay 提示 `Connection refused`：没有服务器在监听。先运行 `rtmp server` 或 `rtmp live`，并用 `rtmp info` 打印的地址。
- ffplay 能连上但没有画面，或提示未知编码：流是 MJPEG。改用默认 `-v h264`。MJPEG over RTMP 是乐鑫扩展，第三方播放器无法解码。
- `rtmp push rtmps://...` 握手失败：自签名测试服务器请加 `--insecure`。
- 服务器角色没有鉴权。RTMPS 仅客户端角色可用。

例程默认启用 `CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY`，以便 `--insecure` 跳过证书校验，仅适用于演示环境。

## 技术支持

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈请创建 [esp-adf issues](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
