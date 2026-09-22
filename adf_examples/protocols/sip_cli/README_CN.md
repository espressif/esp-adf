# SIP CLI 综合示例

- [English Version](./README.md)
- 复杂示例：⭐⭐⭐

## 示例简介

本示例把开发板变成一个由串口 CLI 控制的 SIP 终端：注册到 PBX、拨出与接听来电，并同时双向传输音频（可选视频）。整个通话流程都通过控制台命令驱动，无需重新烧写固件。

与 RTSP、RTMP 不同，SIP 通话是全双工的，`esp_sip_service` 因此上报 `ESP_MEDIA_ROLE_SRC_SINK`：单个实例服务两条媒体流，上行流消费采集服务的数据，下行流则向播放服务供数。

```
esp_video_capture_service ──link(0 → 上行)──> esp_sip_service ──link(下行 → 0)──> esp_video_player_service
      麦克风 + 摄像头                          SIP + RTP + SRTP                       扬声器 + LCD
```

除媒体通路外，本示例还演示了实际部署所需的信令能力：带外 DTMF、文本 SIP MESSAGE 以及 SDES-SRTP 协商。

### 典型应用场景

- 呼叫室内机或软电话的可视门铃与对讲机
- 楼宇、厂区 PBX 上的音频对讲终端
- 拨打固定分机的电梯与紧急求助话机
- 通过 SIP MESSAGE 向值班人员上报状态的设备

### 运行流程

1. `esp_board_manager` 初始化音频 ADC、音频 DAC，以及可选的摄像头与 LCD
2. `esp_wifi_service` 连接 station
3. `esp_cli_service` 启动 `sip>` 控制台
4. `sip start` 创建三个服务、双向建链，并注册到服务器
5. `sip call <user>` 或收到 INVITE 后建立通话，两条流同时开始传输媒体
6. `sip bye` 结束通话，会话保持在线以便发起下一次呼叫

注册并不等于出流：`sip start` 只是让设备上线。媒体仅在通话期间存在，因此下行 track 在通话应答时创建、在通话结束时释放。

链路的消费端先启动。拆除时顺序相反：先停采集，再停 SIP 服务，最后停播放器，这样每个消费端都能把数据排空，而不会阻塞在空队列上。

### 文件结构

```
sip_cli/
├── main/
│   ├── app_main.c              程序入口：NVS、板级、编解码器、Wi-Fi、CLI
│   ├── sip_example.h           板级与 Wi-Fi 辅助接口
│   ├── sip_session.c/h         创建、配置、建链、启动与通话动作
│   ├── sip_cli.c/h             `sip` 与 `wifi` 命令解析
│   ├── sip_settings.h          账号与媒体默认值
│   ├── Kconfig.projbuild       Wi-Fi 凭据与 SIP 账号
│   └── idf_component.yml
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── sdkconfig.defaults.esp32s3
├── sdkconfig.ci
├── partitions.csv
├── pytest_sip_cli.py
├── README.md
└── README_CN.md
```

## 环境配置

### 硬件需求

- 带麦克风与扬声器的开发板，例如 ESP32-S3-Korvo-2 或 ESP32-P4 Function EV Board
- 同网络内的 SIP 服务器，或用于 P2P 通话的第二块开发板
- 对于不带片上 Wi-Fi 的芯片，开发板需提供可用的网络连接

摄像头与 LCD 是可选的，纯音频通话无需它们。视频通话需要本端有摄像头，且对端接受视频协商。

### 默认 IDF 分支

本示例支持 IDF release/v5.4（>= v5.4.3）、release/v5.5（>= v5.5.2）与 IDF v6.1。

### 软件需求

- 一个 SIP 注册服务器。[Asterisk](https://www.asterisk.org/) 与 [FreeSWITCH](https://signalwire.com/freeswitch) 均可；如需快速验证，可用 [Linphone](https://www.linphone.org/) 或 [MicroSIP](https://www.microsip.org/) 等软电话注册到同一 PBX，再按分机号互拨
- 该 PBX 上的两个分机：一个给开发板，一个给软电话
- 在 menuconfig 中配置 Wi-Fi 与 SIP 账号

P2P 通话不需要服务器：两块开发板，或开发板与工作在直连 IP 模式的软电话，可以直接按地址互相呼叫。

## 编译和烧写

### 编译前准备

编译前请确认已配置好 ESP-IDF 环境。若尚未配置，请在 ESP-IDF 根目录下执行：

```
./install.sh
. ./export.sh
```

进入本示例目录：

```
cd adf_examples/protocols/sip_cli
```

本示例通过 [ESP Board Manager](https://github.com/espressif/esp-board-manager) 管理音频编解码器、摄像头、LCD 等板级外设，推荐使用 [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) 作为默认入口。

在已激活的 ESP-IDF Python 环境中安装（每个环境只需一次）：

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # 提示需要更新时执行
```

列出当前可见的开发板：

```bash
idf.py bmgr -l
```

选择开发板：

```bash
idf.py bmgr -b <board_index|board_name>
```

例如：

```bash
idf.py bmgr -b esp32_s3_korvo_2_3
```

首次执行 `idf.py bmgr` 时，会依据 `main/idf_component.yml` 中声明的 `espressif/esp_board_manager` 依赖自动下载该组件。

> [!NOTE]
> 切换到 `esp_board_manager` 支持的其他开发板时，用新的板名或序号重复上述步骤即可；必要时先执行 `idf.py fullclean` 再重新编译。
> 所选开发板必须提供 `audio_adc` 与 `audio_dac`；`camera` 与 `display_lcd` 仅在视频通话时需要。
> 自定义开发板请参考[创建开发板指南](https://docs.espressif.com/projects/esp-board-manager/zh_CN/latest/create-board/index.html)。
> 关于 `esp_board_manager` 的更多信息，请参考 [ESP Board Manager 入门指南](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md)。

### 项目配置

默认选项位于 `sdkconfig.defaults` 与 `sdkconfig.defaults.<target>`，媒体默认值位于 `main/sip_settings.h`。通常只需配置 Wi-Fi 与账号：

```bash
idf.py menuconfig
```

需要配置：

- **SIP Example Configuration** → **WiFi SSID** / **WiFi Password**
- **SIP Example Configuration** → **SIP user** / **SIP password** / **SIP server** / **SIP server port**
- **SIP Example Configuration** → **Default peer**（`sip call` 不带参数时呼叫的对端）
- **SIP Example Configuration** → **SIP transport**（`udp`、`tcp` 或 `tls`）

> CI 中请在 `sdkconfig.ci` 里使用 `${CI_WIFI_SSID}` / `${CI_WIFI_PASSWORD}`。请勿把 Wi-Fi 或 SIP 密码提交到 `sdkconfig.defaults`，应在本地 menuconfig 中设置。

`main/sip_settings.h` 中的常用可调项：

- `SIP_AUDIO_CODEC`、`SIP_AUDIO_SAMPLE_RATE`、`SIP_AUDIO_BITRATE`
- `SIP_VIDEO_CODEC`、`SIP_VIDEO_WIDTH`、`SIP_VIDEO_HEIGHT`、`SIP_VIDEO_FPS`
- `SIP_DEFAULT_SRTP_MODE`、`SIP_DTMF_VOLUME`、`SIP_DTMF_DURATION_MS`
- `SIP_RECV_AUDIO_CACHE`、`SIP_RECV_VIDEO_CACHE`、`SIP_MIC_GAIN`

所有账号与媒体默认值同样可以在命令行中按会话覆盖。

按 `s` 保存，按 `Esc` 退出 menuconfig。

### 编译和烧写命令

```
idf.py build
idf.py -p PORT flash monitor
```

使用 `Ctrl-]` 退出监视器。CLI 提示符为 `sip>`，输入 `help` 查看命令。

## 如何使用示例

### 功能和用法

启动后示例会初始化外设、连接 Wi-Fi 并启动 CLI。若 menuconfig 中的凭据有误或缺失，可在运行时执行 `wifi <ssid> <password>` 连接。

| 命令 | 说明 |
|------|------|
| `sip start [options]` | 建立 采集 → SIP → 播放 链路并注册到服务器 |
| `sip stop` | 挂断、拆除链路，并释放麦克风、扬声器与网络资源 |
| `sip call [user]` | 发起呼叫；不带参数时呼叫 menuconfig 中的默认对端 |
| `sip answer` | 接听正在振铃的来电 |
| `sip bye` | 挂断当前通话，或取消正在拨出的呼叫 |
| `sip dtmf <digit>` | 发送一个带外 DTMF 按键：`0`-`9`、`*`、`#` 或 `A`-`D` |
| `sip msg [-t <uri>] <text...>` | 发送文本 SIP MESSAGE，默认经由已配置的服务器 |
| `sip auto <on\|off>` | 自动接听来电，无需等待 `sip answer` |
| `sip info` | 打印账号、注册、通话、SRTP 与媒体状态 |
| `wifi [ssid] [password]` | 连接 Wi-Fi；不带参数时沿用 menuconfig 中的配置 |

`sip start` 的选项：

| 选项 | 说明 |
|------|------|
| `-u <user>` | SIP 用户名或分机号，默认取自 menuconfig |
| `-w <password>` | SIP 密码；服务器不做鉴权时传 `-` |
| `-s <server[:port]>` | 注册服务器地址，P2P 模式下为对端地址 |
| `-t udp\|tcp\|tls` | 信令传输方式，默认 `udp` |
| `-a g711a\|g711u\|opus\|none` | 音频编码，默认 `g711a`；`none` 表示纯视频通话 |
| `-v h264\|mjpeg\|none` | 视频编码，默认 `none`，即纯音频通话 |
| `--p2p` | 跳过注册，直接向对端发起 INVITE |
| `--srtp off\|prefer\|required` | SDES-SRTP 协商模式，默认 `off` |
| `--port <local>` | 固定本地 SIP 端口；P2P 模式下会被忽略，此时监听 `-s` 中的端口 |
| `--res <WxH>`、`--fps <n>`、`--bitrate <bps>` | 视频采集参数 |

选择 `g711a` 或 `g711u` 会把音频固定为 8 kHz 单声道，因为 RTP 载荷类型 8 与 0 不允许其他取值；`opus` 运行在 16 kHz。

这些开发板上麦克风与扬声器共用一路 I2S 时钟，而通话会同时使用两者，因此扬声器按所选编码的采样率打开，而不是常见的 48 kHz。也就是说，改变 `-a` 同时会改变播放时钟。

**注册并发起呼叫。** 账号已在 menuconfig 中配置好时：

```
sip> sip start
sip> sip call 1002
```

注册为 `1002` 的软电话开始振铃，对方接听后音频即双向流动。

**接听来电。** 从软电话拨打开发板的分机号，然后执行：

```
sip> sip answer
```

也可以让它自动接听，这通常是对讲设备想要的行为：

```
sip> sip auto on
```

**通话中发送 DTMF 与文本：**

```
sip> sip dtmf 5
sip> sip msg "door opened"
```

**加密媒体。** `prefer` 在对端不提供 crypto 行时回退到明文 RTP，而 `required` 会直接拒绝该通话：

```
sip> sip stop
sip> sip start --srtp required
```

**视频通话。** 需要开发板带摄像头，且对端接受视频协商：

```
sip> sip stop
sip> sip start -v h264 --res 640x480 --fps 15
```

**两块开发板之间的 P2P 通话。** 不涉及注册服务器，两端互指对方地址。P2P 模式下协议栈监听的是 `-s` 中给出的端口，因此两块板必须约定同一个端口：

```
# 开发板 A，地址 192.168.1.20
sip> sip start --p2p -u 1001 -s 192.168.1.21:5060

# 开发板 B，地址 192.168.1.21
sip> sip start --p2p -u 1002 -s 192.168.1.20:5060
sip> sip auto on

# 然后在开发板 A 上
sip> sip call 1002
```

同一时刻只能运行一个会话。若要更换启动选项，请先执行 `sip stop`。

自动化测试（`pytest_sip_cli.py`）会启动 P2P 会话（唯一无需注册服务器的模式），检查各通话命令后再停止。

### 日志输出

启动阶段以 `[ 1 ]` 到 `[ 5 ]` 编号，并以 `CLI ready` 结束。会话期间，每个 SIP 事件都会按名称打印。

```text
I (1274) SIP_EXAMPLE: === SIP Example ===
I (1277) SIP_EXAMPLE: [ 1 ] Initialize NVS and the media adapter
I (1284) SIP_EXAMPLE: [ 2 ] Initialize board devices (audio / camera / LCD)
I (2801) SIP_EXAMPLE: [ 3 ] Register audio and video codecs
I (2810) SIP_EXAMPLE: [ 4 ] Connect to WiFi
I (2815) SIP_EXAMPLE: Connecting to Wi-Fi SSID:my-ap (attempt 1/3)
I (5120) SIP_EXAMPLE: Wi-Fi connected
I (5125) SIP_EXAMPLE: [ 5 ] Start CLI
I (5130) SIP_EXAMPLE: CLI ready
I (5147) SIP_EXAMPLE: Type 'help' to list all commands

sip> sip start
I (20130) SIP_SESSION: SIP session online as 1001@192.168.1.10:5060 over udp (audio: g711a, video: none, srtp: off)
I (20320) SIP_SESSION: SIP_REGISTERED: ready to place and receive calls

sip> sip call 1002
I (31502) SIP_SESSION: SIP_CALLING 1002
I (33110) SIP_SESSION: SIP_CALL_ANSWERED: talking to 1002
I (33125) SIP_SESSION: SIP_AUDIO_SESSION_BEGIN

sip> sip info
device ip   : 192.168.1.23
session     : online
account     : 1001@192.168.1.10:5060 (udp)
registered  : yes
call        : in call with 1002
auto answer : off
srtp        : inactive (off)
audio       : g711a 8000 Hz 1 ch
video       : disabled

sip> sip bye
I (60210) SIP_SESSION: SIP_HANGUP: BYE
```

## 常见问题

- `sip start` 后没有 `SIP_REGISTERED`：用 `sip info` 检查用户名、密码、服务器地址与端口，并确认 PBX 接受该分机。密码错误通常表现为带 reject reason 的 `SIP_ERROR`。
- `sip start failed: ESP_ERR_NOT_FOUND`：请求了视频但摄像头未就绪。检查排线与开发板选择，或去掉 `-v` 后重新启动。
- `sip call failed: ESP_ERR_INVALID_STATE`：要么没有在线会话，要么已有通话在振铃或通话中。执行 `sip info` 可以看出是哪一种。
- 通话建立但无声：通常是 NAT 导致的。请把两端放在同一子网，或在服务配置中打开 `use_public_addr` 与 `send_options`，让 RTP 通路保持打开。
- `sip start --srtp required` 被拒绝：对端未提供 crypto 行。改用 `prefer` 以允许回退到明文 RTP。
- 声音卡顿：`sdkconfig.defaults` 已加深 lwIP 接收队列；网络突发较多时，可继续加大 `SIP_RECV_AUDIO_CACHE`。
- 协商了视频但没有画面：开发板没有 LCD，启动时会打印相应警告。通话仍会传输音频。

## 技术支持

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈：[esp-adf issues](https://github.com/espressif/esp-adf/issues)

我们会尽快回复您。
