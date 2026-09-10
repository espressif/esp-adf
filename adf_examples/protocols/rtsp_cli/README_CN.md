# RTSP CLI 综合示例

- [English Version](./README.md)
- 例程难度：⭐⭐⭐

## 例程简介

本例程在一份固件中提供 RTSP server、push 和 pull 三种角色，并通过串口 CLI 控制，用户可验证常见 RTSP 工作流，并在停止当前会话后切换角色，无需重新编译或烧录：

- **server**：让 VLC 或 ffplay 从 `rtsp://<设备 IP>:554/live` 拉取摄像头画面和麦克风声音
- **push**：将摄像头画面和麦克风声音发布到 MediaMTX 等远端 RTSP 服务器（ANNOUNCE + RECORD）
- **pull**：在开发板 LCD 和扬声器上播放远端 RTSP 流（DESCRIBE + SETUP + PLAY）

三种角色采用同一套 service 组合方式：应用通过 `esp_media_service_link()` 连接采集、播放和 RTSP service，并使用统一的生命周期 API 控制会话，无需实现 RTSP 协议处理和媒体数据流；串口 CLI 也为脚本和智能体提供可复用的自动化控制入口。

如需先学习基础推流链路，请参阅 [`RTSP 推流`](../rtsp_push/README_CN.md) 示例。

### 典型场景

- 供手机 App 或 NVR 拉流的 IP 摄像头、视频门铃
- 向中心 RTSP 服务器推流的视频监控节点
- 在本地 LCD 上播放远端摄像头画面的实时监控屏
- 工业或楼宇自动化网络中的流媒体客户端

### 运行机制

1. `esp_board_manager` 初始化摄像头、LCD、音频 ADC 与 DAC
2. `esp_wifi_service` 连接 Station
3. `esp_cli_service` 启动 `rtsp>` 控制台
4. **server / push**：`esp_video_capture_service` → `esp_media_service_link()` → `esp_rtsp_service` ROLE_SERVER 或 ROLE_SINK
5. **pull**：ROLE_SRC → `esp_media_service_link()` → `esp_video_player_service` → LCD 与扬声器

链路的消费端总是先启动。停止时顺序相反：先停生产端，消费端把残留数据排空，而不是阻塞在空队列上。

### 文件结构

```
rtsp_cli/
├── main/
│   ├── app_main.c              例程入口：NVS、板级设备、编解码器、Wi-Fi、CLI
│   ├── rtsp_example.h          板级与 Wi-Fi 辅助接口
│   ├── rtsp_session.c/h        三种角色的 create、setup、link、start 与停止
│   ├── rtsp_cli.c/h            `rtsp` 与 `wifi` 命令解析
│   ├── rtsp_settings.h         媒体与协议默认值
│   ├── Kconfig.projbuild       Wi-Fi 凭据与默认对端 URL
│   └── idf_component.yml
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── sdkconfig.defaults.esp32s3
├── sdkconfig.defaults.esp32s31
├── sdkconfig.ci
├── partitions.csv
├── pytest_rtsp_cli.py
├── README.md
└── README_CN.md
```

## 环境配置

### 硬件要求

- ESP32-P4 Function EV Board（摄像头、MIPI LCD、音频编解码芯片）
- 与开发板同网段的 PC，用于运行 ffplay、VLC 或 RTSP 服务器
- 无片上 Wi-Fi 的芯片需开发板提供可用的联网能力

摄像头与 LCD 都不是必需的：没有摄像头时发送角色仍可用 `-v none` 推纯音频，没有屏幕时 pull 角色只播放音频。

### 默认 IDF 分支

本例程支持 IDF release/v5.4（>= v5.4.3）、release/v5.5（>= v5.5.2）以及 IDF v6.1。

### 软件要求

- 从开发板拉流（server 角色）：安装 FFmpeg 后使用 `ffplay`，或直接使用 VLC
- 在 menuconfig 中配置 Wi-Fi SSID 与密码
- 接收推流或提供拉流源（push 与 pull 角色）时，在 PC 上运行 RTSP 服务器。[MediaMTX](https://github.com/bluenviron/mediamtx) 无需配置即可配合本例程使用：

```
./mediamtx
# RTSP 服务地址为 rtsp://<PC 的 IP>:8554/<任意路径>
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
cd adf_examples/protocols/rtsp_cli
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
> 所选开发板应提供 `camera`、`display_lcd`、`audio_adc` 和 `audio_dac`，才能完整体验三种角色。
> 自定义开发板请参考 [创建开发板指南](https://docs.espressif.com/projects/esp-board-manager/zh_CN/latest/create-board/index.html)。
> `esp_board_manager` 更多信息请参考 [ESP_BOARD_MANAGER 入门指南](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README_CN.md)

### 项目配置

本例程默认配置已写入 `sdkconfig.defaults` 与 `sdkconfig.defaults.<target>`。媒体默认值在 `main/rtsp_settings.h`。通常只需按需修改 Wi-Fi：

```bash
idf.py menuconfig
```

在 menuconfig 中配置：

- **RTSP CLI Example Configuration** → **WiFi SSID**
- **RTSP CLI Example Configuration** → **WiFi Password**
- **RTSP CLI Example Configuration** → **Default push URL** / **Default pull URL**（`rtsp push` / `rtsp pull` 未带 URL 时使用）

> CI 构建使用 `sdkconfig.ci` 中的 `${CI_WIFI_SSID}` / `${CI_WIFI_PASSWORD}`。请勿在仓库的 `sdkconfig.defaults` 中提交 Wi-Fi 密码；本地调试请在 menuconfig 中填写密码。

`main/rtsp_settings.h` 中的常用可调项：

- `RTSP_VIDEO_WIDTH`、`RTSP_VIDEO_HEIGHT`、`RTSP_VIDEO_FPS`、`RTSP_VIDEO_CODEC`
- `RTSP_AUDIO_CODEC`、`RTSP_AUDIO_SAMPLE_RATE`、`RTSP_AUDIO_BITRATE`
- `RTSP_SERVER_PORT`、`RTSP_SERVER_PATH`、`RTSP_DEFAULT_TRANSPORT`
- `RTSP_RECV_AUDIO_CACHE`、`RTSP_RECV_VIDEO_CACHE`

所有媒体默认值都可以在命令行上按会话覆盖。

配置完成后按 `s` 保存，按 `Esc` 退出。

### 资源优化

默认配置保留 server、push 和 pull 三种 RTSP 角色及其编解码格式，以便通过 CLI 切换；功能固定的产品可在 menuconfig 中减小固件体积：

- 进入 **Component config → ESP-RTSP Service**，server 使用 `CONFIG_ESP_RTSP_SERVICE_SERVER_SUPPORT`、push 使用 `CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT`、pull 使用 `CONFIG_ESP_RTSP_SERVICE_SRC_SUPPORT`。
- 进入 **Component config → Audio Codec Configuration** 和 **Component config → Video Codec Configuration**，通过 `CONFIG_AUDIO_ENCODER_*_SUPPORT`、`CONFIG_AUDIO_DECODER_*_SUPPORT`、`CONFIG_VIDEO_ENCODER_*_SUPPORT` 和 `CONFIG_VIDEO_DECODER_*_SUPPORT` 开关编解码器；server 和 push 需要编码器，pull 需要解码器，因此仅保留实际使用的格式，并为每种视频格式保留一种适合目标芯片的实现。

例如，固定使用 MJPEG/AAC 的推流产品通常只需保留所需的 `SERVER` 或 `SINK` 角色以及 MJPEG/AAC 编码器。关闭角色或编解码格式后，相应 CLI 角色或格式将不可用；修改这些选项后请先执行 `idf.py fullclean` 再重新编译。

运行时执行 `rtsp stop` 会销毁活动的 RTSP 以及采集或播放 service，但板级设备、编解码器注册和 Wi-Fi 保持初始化，以便无需重启即可启动其他仍受支持的角色。

### 编译与烧录

```
idf.py build
idf.py -p PORT flash monitor
```

退出 monitor：`Ctrl-]`。串口提示符为 `rtsp>`，输入 `help` 查看命令。

## 如何使用例程

### 功能和用法

例程启动后会初始化外设、连接 Wi-Fi 并启动 CLI。如果 menuconfig 中的凭据有误或未填写，可以用 `wifi <ssid> <password>` 在运行时连接。

| 命令 | 说明 |
|------|------|
| `rtsp server [url] [选项]` | 在指定的本地 RTSP URL 上提供摄像头与麦克风的音视频流 |
| `rtsp push [url] [选项]` | 把摄像头与麦克风推流到远端 RTSP 服务器 |
| `rtsp pull [url] [选项]` | 拉取远端 RTSP 流并在 LCD 与扬声器上播放 |
| `rtsp stop` | 停止并销毁活动的 RTSP 以及采集或播放会话 |
| `rtsp info` | 打印当前角色、RTSP 状态、设备 IP 以及可直接复制的 ffplay 命令 |
| `wifi [ssid] [password]` | 连接 Wi-Fi；不带参数时使用 menuconfig 中的配置 |

省略 `url` 时，server 使用 `rtsp://0.0.0.0:554/live`，push 和 pull 分别使用 `RTSP_EXAMPLE_PUSH_URL` 与 `RTSP_EXAMPLE_PULL_URL`。
指定 `url` 时，必须紧跟在角色名称之后。
server 从该 URL 中解析监听端口。

| 选项 | 适用角色 | 说明 |
|------|---------|------|
| `-v h264\|mjpeg\|none` | server、push、pull | 视频编码，默认 `mjpeg`；`none` 表示只发音频。pull 时表示期望的解包格式 |
| `-a aac\|g711a\|g711u\|none` | server、push | 音频编码，`none` 表示只发视频 |
| `--res <WxH>` | server、push | 采集分辨率，例如 `--res 640x480` |
| `--fps <n>` | server、push | 采集帧率 |
| `--bitrate <bps>` | server、push | 视频码率，不指定时按画面尺寸推导 |
| `--no-video` / `--no-audio` | pull | 忽略远端流中的对应轨道 |
| `--cache <bytes>` | pull | 视频接收缓存大小 |

选择 `g711a` 或 `g711u` 时音频固定为 8 kHz 单声道，这是 RTP 载荷类型 8 与 0 的规定。

自动化测试（`pytest_rtsp_cli.py`）在 CLI 就绪后启动并停止 `rtsp server`。

**把摄像头提供给 PC 播放器：**

```
rtsp> rtsp server
```

```
ffplay -rtsp_transport udp rtsp://192.168.1.23:554/live
```

其他组合：

```
rtsp> rtsp server -v h264 -a g711a --res 640x480 --fps 10
rtsp> rtsp server -v none  # 纯音频流
rtsp> rtsp server rtsp://0.0.0.0:8554/live -a none  # 8554 端口上的纯视频流
```

**推流到远端 RTSP 服务器。** 在 PC 上启动 MediaMTX，然后：

```
rtsp> rtsp stop
rtsp> rtsp push rtsp://192.168.1.10:8554/live
```

```
ffplay -rtsp_transport udp rtsp://192.168.1.10:8554/live
```

**拉取并播放远端流。** 把开发板指向 IP 摄像头、MediaMTX，或另一块运行 `rtsp server` 的开发板：

```
rtsp> rtsp stop
rtsp> rtsp pull rtsp://192.168.1.10:8554/live
```

默认按 MJPEG 解包，与本例程 server / push 默认编码一致；拉 H264 源时请加 `-v h264`。用 `--no-audio` 或 `--no-video` 可以忽略其中一路。

同一时刻只能运行一种角色。切换角色前请先执行 `rtsp stop`。如果对端已经主动断开会话，下一条 `rtsp` 命令会自动释放它。

### 日志输出

启动过程用 `[ 1 ]` 到 `[ 5 ]` 编号，以 `CLI ready` 结束。会话运行期间每一次 RTSP 状态跳转都会按名字打印出来。

```text
I (1274) RTSP_EXAMPLE: === RTSP Example ===
I (1277) RTSP_EXAMPLE: [ 1 ] Initialize NVS and the media adapter
I (1284) RTSP_EXAMPLE: [ 2 ] Initialize board devices (camera / LCD / audio)
I (2801) RTSP_EXAMPLE: [ 3 ] Register audio and video codecs
I (2810) RTSP_EXAMPLE: [ 4 ] Connect to WiFi
I (2815) RTSP_EXAMPLE: Connecting to Wi-Fi SSID:my-ap
I (5120) RTSP_EXAMPLE: Wi-Fi connected
I (5125) RTSP_EXAMPLE: [ 5 ] Start CLI
I (5130) RTSP_EXAMPLE: CLI ready
I (5133) RTSP_EXAMPLE: Type 'help' to list all commands

rtsp> rtsp server
I (20130) RTSP_SESSION: server session running on rtsp://0.0.0.0:554/live (udp, video: mjpeg, audio: aac)
I (20138) RTSP_SESSION: Pull it with: ffplay -rtsp_transport udp rtsp://192.168.1.23:554/live
I (31502) RTSP_SESSION: OPTIONS (state: options)
I (31510) RTSP_SESSION: DESCRIBE (state: describe)
I (31530) RTSP_SESSION: SETUP (state: setup)
I (31544) RTSP_SESSION: PLAY (state: play)

rtsp> rtsp info
device ip  : 192.168.1.23
session    : server
rtsp state : play
url        : rtsp://0.0.0.0:554/live
transport  : udp
video      : mjpeg 640x480@10fps
audio      : aac 16000 Hz 1 ch
remote pull: ffplay -rtsp_transport udp rtsp://192.168.1.23:554/live
```

## 故障排除

- 出现 `rtsp server failed: ESP_ERR_NOT_FOUND` 或 `V4L2_SRC: Fail to open device`：摄像头未就绪。检查排线和板型，或用 `-v none` 只推音频。
- ffplay 提示 `Connection refused`：设备尚未拿到 IP，或端口不对。使用 `rtsp info` 打印的地址。
- ffplay 能连上但没有画面：确认与设备在同一局域网，且防火墙没有拦截 RTP 的 UDP 端口。
- 提示 `A server session is running`：先执行 `rtsp stop`。同一时刻只能运行一种角色。
- 不支持组播、RTSP over TLS 和认证。Server 角色不接受远端 `ANNOUNCE` / `RECORD`；板子向板子推流请用 push 角色对接 PC 上的 RTSP 服务器。

## 技术支持

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈请创建 [esp-adf issues](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
