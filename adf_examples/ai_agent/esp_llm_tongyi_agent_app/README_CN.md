# ESP LLM 通义智能体

- [English](./README.md)
- 例程难度：⭐⭐⭐

## 例程简介

> **定位说明**：本例程是基于 `esp_llm_tongyi` 的 **DashScope 多模态 WebSocket 智能体**示例。**Dialog** 模式为双工语音；**Vision** 模式在板子具备相机与 LCD 时提供预览，并通过 `snap` 发送单张 JPEG（非连续视频上行）。板级缺少相机或 LCD 时，视觉路径自动跳过，语音仍可用。

- 通过 WebSocket 连接 DashScope 应用，完成双工语音对话。
- Vision：LCD 实时预览 + 控制台 `snap` / `snap <prompt>` 就图提问；`check_name` 探测不到相机或 LCD 时不建视觉路径，语音仍可用。
- 使用前配置 API Key、Workspace ID、Application ID（Vision 需 `mm_` 类应用）。

官方文档：[DashScope 多模态 WebSocket](https://help.aliyun.com/zh/model-studio/websocket-for-multimodal-dialogue)。

### 功能

下面每一项能力都在 `idf.py menuconfig` 中开启：

| 能力 | 作用 | 配置项 | 默认 |
| --- | --- | --- | --- |
| 语音对话 | 与 DashScope 应用进行双工语音交流 | 两种应用模式下都可用 | 开 |
| 相机预览 | 在 LCD 上显示实时相机画面 | `Application mode` 选为 vision | 仓库预置配置下开 |
| 就照片提问 | `snap <prompt>` 抓拍一张 JPEG，并就这张图向模型提问 | `Application mode` 选为 vision | vision 下开 |
| 静默更新图像 | 不带参数的 `snap` 只刷新会话图像，不请求回复 | `Default snapshot prompt` 留空 | vision 下开 |
| 唤醒词 | 唤醒词之前保持麦克风关闭，唤醒后取消当前回复 | `Dialog mode` 选为先唤醒词再对话 | 关 |
| 回声消除与噪声抑制 | 上行 AEC、NS 默认开启 | 编译固定开启 | 开 |
| 语音活动检测 | 判断语音起止 | `Enable voice activity detection` | 开 |

### 典型场景

- 由 DashScope 语音应用驱动的双工语音助手
- 带屏设备，显示实时相机预览，并就画面内容向模型提问
- 唤醒词门控的对话，WakeNet 触发前麦克风保持关闭

### 媒体能力

| 项目 | 本例程的支持情况 |
| --- | --- |
| 音频上行 | OPUS，16 kHz，单声道，24000 bit/s |
| 音频下行 | OPUS，16 kHz，单声道，32 kbit/s |
| 视频上行 | 没有连续视频轨道；每次 `snap` 命令只发送一张 JPEG |
| 视频下行 | 不支持 |
| 相机 | 仅 Vision 模式，MJPEG，分辨率由 `TONGYI_VISION_CAMERA_WIDTH/HEIGHT` 决定，默认 320x240，ESP32-S31 上为 640x480 |
| LCD 预览 | 仅 Vision 模式，全屏显示。按相机完整分辨率采集 RGB565 并缩放到屏幕，帧率取 `TONGYI_VISION_CAMERA_FPS`，默认 15 |
| 音频前端 | AEC、NS 固定开启；VAD 由 `TONGYI_AUDIO_VAD` 控制，默认开 |
| 唤醒词 | WakeNet，仅当 `Dialog mode` 选为 `Wake word then conversation` 时加入 |
| 上行门控 | 唤醒词对话模式下，唤醒词触发前音频上行保持关闭 |
| 播放器流 | stream 0 承载通义 TTS；stream 1 空闲，可用于 URL 或提示音 |

Vision 为 **单张 JPEG 快照**，不建立连续 `VIDEO_UP`。预览为 RGB565 写 LCD；性能与双路采集说明见 [FAQ](#faq)。

### 应用模式

`Application mode` 决定固件构建成什么形态：

| 模式 | Kconfig | 行为 |
| --- | --- | --- |
| Dialog | `TONGYI_APP_MODE_DIALOG` | 仅双工语音；不带相机和 LCD |
| Vision | `TONGYI_APP_MODE_VISION` | 增加相机采集、LCD 预览和 `snap` CLI 命令 |

Kconfig 的默认值是 Dialog，但仓库中的 `sdkconfig.defaults` 设置了 `CONFIG_TONGYI_APP_MODE_VISION=y`。因此新执行一次 `idf.py set-target` 得到的是 Vision 构建。要构建 Dialog 版本，请在 menuconfig 中修改 `Application mode`。

两种模式需要不同的 DashScope 应用。Dialog 需要纯语音应用 ID，而 Vision 预置的 `mm_` 应用是多模态应用，在从不发送相机帧的情况下会返回 `images must not be empty`。

### 文件结构

```
esp_llm_tongyi_agent/
├── main/
│   ├── app_main.c            平台初始化、Wi-Fi、SNTP 和应用生命周期
│   ├── app_agent.c           通义配置、AFE 掩码和事件处理
│   ├── app_agent.h           智能体创建 / 启动 / 销毁接口
│   ├── app_cli.c             UART CLI（`esp_cli_service`）；Vision 额外注册 `snap`
│   ├── app_cli.h             CLI 启动 / 停止接口
│   ├── libc_compat.c         兼容旧版 IDF 的 stderr 补丁
│   ├── Kconfig.projbuild     应用 menuconfig 选项
│   └── idf_component.yml
├── components/
│   ├── tongyi_app_media/     板级设备、采集、播放器和 LCD 预览
│   └── gen_bmgr_codes/       由 esp_board_manager 生成的板级元数据
├── partitions.csv
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── sdkconfig.defaults.esp32s3
├── sdkconfig.defaults.esp32s31
├── README.md
└── README_CN.md
```

## 环境配置

### 硬件要求

- 提供 `audio_adc` 和 `audio_dac` 设备的开发板
- 接到板级音频输出的扬声器或耳机
- Vision 模式下开发板还需提供 `camera` 和 `display_lcd` 设备
- 带 PSRAM，ESP32-S3 和 ESP32-S31 需 16 MB Flash，ESP32-P4 需 8 MB Flash

| 芯片 | 开发板 | 默认相机 |
| --- | --- | --- |
| `esp32s3` | `esp32_s3_korvo_2_3` | OV3660 DVP JPEG 320x240 @ 25 fps |
| `esp32s31` | `esp32_s31_korvo_1` | SC101IOT DVP YUV422 YUYV 640x480 @ 15 fps |
| `esp32p4` | `esp32_p4_function_ev_board` | SC2336 MIPI CSI RAW8 1024x600 @ 30 fps |

固件使用自定义分区表，应用分区 `factory` 为 6 MB。

### 默认 IDF 分支

本例程支持 IDF release/v5.4 (>= v5.4.3) 与 release/v5.5 (>= v5.5.2) 分支。

### 软件要求

- DashScope API key、workspace ID 和应用 ID
- 能访问 DashScope 端点和 NTP 服务器的 Wi-Fi 网络

`Kconfig.projbuild` 预置了内部实验室 API key 和 workspace ID，因此例程无需修改即可编译。在该实验室账号之外使用本例程前，请把两者替换为自己的 DashScope 凭据。

## 编译和下载

### 编译准备

编译本例程前请先配置好 ESP-IDF。如果已经配置完成，直接进入工程目录即可；否则在 ESP-IDF 根目录下执行以下脚本。所用芯片的说明请参考 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)。

```
./install.sh
. ./export.sh
```

- 进入例程目录：

```
cd adf_examples/ai_agent/esp_llm_tongyi_agent
```

- 先选芯片，再选开发板：

```bash
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
```

`idf.py set-target` 要在 `idf.py bmgr` 之前执行，因为它会同时加载 `sdkconfig.defaults` 和携带相机默认项的 `sdkconfig.defaults.<chip>` 文件。本例程使用 [ESP Board Manager](https://github.com/espressif/esp-board-manager)：`idf.py bmgr -l` 列出当前可见的开发板，`idf.py bmgr -b <board_index|board_name>` 选择其中一个。

### 项目配置

```bash
idf.py menuconfig
```

在 `Tongyi LLM agent application` 下设置：

- `DashScope API key`、`Tongyi workspace ID` 和 `Tongyi application ID`：要接入的 DashScope 账号和应用
- `Application mode`：Dialog 为纯音频，Vision 为相机预览加 `snap` 命令
- `Camera JPEG width` / `height` / `Camera / LCD preview FPS`：仅 Vision 模式，尺寸必须匹配一个 esp-video JPEG 传感器 mode
- `Default snapshot prompt`：仅 Vision 模式，`snap` 不带参数时使用
- `Dialog mode`：直接对话，或先唤醒词再对话
- `Audio front-end features`：AEC、NS 编译固定开启；VAD 默认开启
- `Audio output volume`：0 到 100，默认 80
- `Wi-Fi`：SSID、密码和连接超时（毫秒）
- `Run time before clean shutdown`：0 表示一直运行；非零值会走完整的停止、断开和销毁流程

凭据会被编译进固件，请勿提交真实值。Wi-Fi 配置档由 `esp_wifi_service_request_connect()` 持久化到 NVS。

### 编译与烧录

- 编译例程

```
idf.py build
```

- 烧录并打开监视器（将 PORT 替换为实际串口）：

```
idf.py -p PORT flash monitor
```

- 按 `Ctrl-]` 退出监视器

## 如何使用例程

### 功能和用法

1. 启动后应用先连接 Wi-Fi，然后等待 SNTP 同步完成
2. 随后日志会提示通义智能体就绪，表示设备可以开始对话
3. 直接对话模式下可随时说话，由服务端判断一次提问何时结束
4. 唤醒词对话模式下先说唤醒词。唤醒后麦克风打开，正在进行的回复被取消
5. 回复以 TTS 形式到达播放器 stream 0；朗读文本同时通过 `RESPONDING_CONTENT` 事件给出，对话状态跳变通过 `DIALOG_STATE` 给出
6. UART CLI 提示符是 `tongyi>`（`esp_cli_service`）。Dialog 提供 `help` / `sys_heap`。Vision 额外提供 `snap`

```text
snap
snap 镜头前面是什么
```

`snap` 不带参数时把缓存的 JPEG 作为 `UpdateInfo` 发送，只更新会话图像而不请求回复。`snap <prompt>` 发送 `RequestToRespond`，请求模型就这张图作答。设置了 `Default snapshot prompt` 后，不带参数的 `snap` 会使用它，因此同样会请求回复。

## 故障排除

### 应用在连接前就停止

查看日志定位失败的步骤。Wi-Fi SSID 为空会报 `Start network failed: Wi-Fi SSID is empty`，API key、workspace ID 或应用 ID 缺失则在创建会话之前就被拒绝。

### 缺少板级设备名

编译或启动时报未知设备句柄，说明没有为所选芯片生成板级元数据。用与目标芯片匹配的开发板重新执行 `idf.py bmgr -b <board>`。

### WebSocket 连接失败

确认网络能访问 DashScope 端点、API key 和 workspace ID 有效、时钟已同步。本例程把 `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` 提高到 16384，TLS 缓冲区需要 PSRAM。

### 模型返回 `images must not be empty`

在 Dialog 模式下用了多模态 `mm_` 应用 ID，而这种模式从不发送图像。请把 `Application mode` 切到 Vision，或者改用纯语音应用 ID。

### 出现 `V4L2_SRC: Fail to open device`

可能是所选芯片的相机默认项没有生效，也可能是板上实装的传感器与这些默认项描述的型号不一致。先用 `idf.py set-target` 重新生成 `sdkconfig` 以加载芯片文件。芯片文件只会选定一种传感器和一种 mode：ESP32-S3 为 DVP 接口的 OV3660、JPEG 320x240，ESP32-S31 为 DVP 接口的 SC101IOT、YUV422 YUYV 640x480，ESP32-P4 为 MIPI CSI 接口的 SC2336、RAW8 1024x600。如果板上是别的传感器，请在 menuconfig 中改为启用该传感器，并让 `TONGYI_VISION_CAMERA_WIDTH` 和 `TONGYI_VISION_CAMERA_HEIGHT` 对齐它真实存在的 mode。

## FAQ

### 屏要一直亮而 JPEG 只是偶尔上传，为什么预览这么费？

产品形态就是这样：LCD 常显，`snap` 偶发上传一张 JPEG。更省的做法是相机直接出屏幕尺寸的 RGB565 并直接刷屏，JPEG 只在 `tongyi_app_media_capture_jpeg()` 里编一帧。

Korvo-2 用的是 DVP 接口的 OV3660，在 320x240，也就是 ILI9341 屏的尺寸上，它的官方 mode 只有 JPEG；RGB565 只存在于 240x240 和 640x480，没有 320x240。Korvo-1 用的是 SC101IOT，它完全没有 JPEG mode，只输出 YUV422。两颗传感器都无法原生同时提供两种格式，而本例程不补自定义传感器表，因此 `vision_open()` 保留两路采集：

1. MJPEG，缓存给下一次 `snap`；在 OV3660 上是直出，在 SC101IOT 上由 YUV422 编码得到
2. RGB565，写到 LCD；在 OV3660 上由 JPEG 解码得到，在 SC101IOT 上由 YUV422 转换得到

ESP32-S3 没有硬件 JPEG 解码器，因此 Korvo-2 上每一帧预览都要软件解码，Korvo-1 上则是每帧做一次色彩转换。两种情况都是传感器 mode 的限制，不是播放器少注册了解码器。

### 如果相机能输出和 LCD 同尺寸的 RGB，要改什么？

只改 `tongyi_app_media.c`，不要动 `esp_cam_sensor`。

1. 在 `sdkconfig` 中打开传感器官方的 RGB565 或 RGB565_BE mode，并把 `TONGYI_VISION_CAMERA_WIDTH` 和 `TONGYI_VISION_CAMERA_HEIGHT` 设成这个 mode
2. `vision_open()` 只开一路即可：屏幕尺寸的 `ESP_CAPTURE_FMT_ID_RGB565` 或 `ESP_CAPTURE_FMT_ID_RGB565_BE`。去掉 MJPEG 流、`vision_preview_task()` 中的 JPEG drain 以及 JPEG 缓存
3. 继续用 `esp_video_render_stream_write()` 写 RGB。字节序与 LCD 保持一致（Korvo-2 是 `RGB565_BE`），render 就不用再做颜色转换
4. 在 `tongyi_app_media_capture_jpeg()` 中用 `esp_jpeg_enc` 以 `JPEG_PIXEL_FORMAT_RGB565_LE` 或 `RGB565_BE` 编码上一帧 RGB。`esp_video_enc` 的软件 MJPEG 编码器不接收 RGB565
5. `esp_video_dec_register_default()` 只在采集仍需把 JPEG 转成 RGB 时才需要，纯 RGB 传感器可以去掉

ESP32-P4 的 SC2336 输出 RAW 而非 packed RGB，该板仍需要 ISP 或转换通路，不在本问题范围内。

## 技术支持

- 论坛：[esp32.com](https://esp32.com/viewforum.php?f=20)
- 问题反馈：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
