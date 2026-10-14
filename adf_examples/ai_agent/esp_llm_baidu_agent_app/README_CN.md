# ESP LLM 百度智能体应用

- [English](./README.md)
- 例程难度：⭐⭐⭐

## 例程简介

> **定位说明**：本例程是基于 `esp_llm_baidu` 的 **百度 RTC 智能体**示例，默认支持双工语音，并可按场景启用 **相机识图、云端文生图、本地 LCD 预览**。若只需纯语音，请在 menuconfig 中将 **Agent scene** 设为 **Voice chat**。板级无相机或 LCD 时，视觉相关能力会自动跳过，语音仍可运行。

- 通过百度 RTC 接入智能体平台，完成语音对讲；图像理解 / 文生图 / 多模态由 **Agent scene** 选择。
- 板级音频、显示与摄像头由 `esp_board_manager` 按设备名惰性初始化；创建媒体图前会用 `esp_board_manager_check_name()` 探测相机/LCD，缺失则跳过视觉路径并继续语音。
- 使用前请配置 App ID、User ID、Workflow、Licence Key、Server URL 等账号信息。

官方文档：[百度 RTC](https://cloud.baidu.com/doc/RTC/index.html)。

### 功能

下面每一项能力都在 `idf.py menuconfig` 中开启：

| 能力 | 作用 | 配置项 | 默认 |
| --- | --- | --- | --- |
| 语音对话 | 与智能体工作流进行双工语音交流 | `Agent scene` 选为语音对话 | 开 |
| 视觉理解 | 上传相机图像，让智能体就画面作答 | `Agent scene` 选为图像理解或多模态 | 关 |
| 图像生成 | 把智能体生成的图像绘制到 LCD 上 | `Agent scene` 选为图像生成或多模态 | 关 |
| 本地相机预览 | 相机场景运行时，在 LCD 一角显示相机画面 | `Local LCD camera preview` | 相机场景下开 |
| 唤醒词 | 播放提示音、取消当前回复，并通知智能体开始收音 | `Enable WakeNet wake-up` | 关 |
| 语音打断 | 说话即可打断正在回复的智能体 | `ESP LLM Baidu` 的服务端语音打断 | 开，等级 80 |
| 语音调音量 | 智能体通过 `adjust_volume` FunctionCall 修改播放音量 | 始终编译在内 | 开 |
| 音乐播放 | 播放智能体指定的 HTTP 音乐 URL | 始终编译在内 | 开 |

### 典型场景

- 由百度智能体工作流驱动的语音助手音箱
- 带屏设备，就相机看到的画面向智能体提问
- 带屏设备，显示智能体生成的图像
- 多模态设备，同时具备相机上行、生成图像和本地预览

### 媒体能力

| 项目 | 本例程的支持情况 |
| --- | --- |
| 音频上行 | OPUS，16 kHz，单声道；组件选择 `ESP_LLM_BAIDU_AGENT_AUDIO_CODEC_PCM` 时为 PCM |
| 音频下行 | 与上行采用相同的编码和采样率 |
| 视频上行 | 图像理解和多模态场景下启用；相机 JPEG 走 `VIDEO_UP` |
| 视频下行 | 图像生成和多模态场景下启用；智能体图像经 `VIDEO_DN` 送到 LCD 播放器 |
| 相机 | `BAIDU_CAMERA_WIDTH/HEIGHT/FPS`，默认 320x240 @ 10 fps，ESP32-S31 上为 640x480 @ 15 fps，与 LCD 尺寸无关 |
| LCD 预览 | 画中画显示在 LCD 右下角，距两边各 8 px；RGB565，分辨率为相机的一半，即默认 160x120，帧率 5 fps |
| 音频前端 | AEC 和 VAD 始终开启；NS 默认关闭（`BAIDU_AUDIO_NS`） |
| 唤醒词 | WakeNet，默认关闭，由 `BAIDU_AUDIO_WN` 开启，提示音 MP3 存放于 SPIFFS |
| 上行门控 | 无；即使开启 WakeNet，音频上行也保持打开 |
| 播放器流 | stream 0 承载智能体 TTS，stream 1 混合 HTTP 或文件 URL，stream 2 播放唤醒提示音 |

上面哪些视频项生效取决于场景，LCD 归谁所有也随场景变化。三条通路以及它们如何共用屏幕，详见[视频通路](#视频通路)。

### 智能体场景

`Agent scene` 决定固件启用哪些媒体流：

| 场景 | Kconfig | `VIDEO_UP` | `VIDEO_DN` | LCD 上显示什么 |
| --- | --- | --- | --- | --- |
| 语音对话 | `BAIDU_APP_SCENE_VOICE_CHAT` | 关 | 关 | 不显示，也不创建显示屏 |
| 图像理解 | `BAIDU_APP_SCENE_IMAGE_UNDERSTANDING` | 开 | 关 | 相机预览；关闭预览时则什么都不显示 |
| 图像生成 | `BAIDU_APP_SCENE_IMAGE_GENERATION` | 关 | 开 | 智能体生成的图像 |
| 多模态 | `BAIDU_APP_SCENE_MULTI_MODAL` | 开 | 开 | 智能体图像，上面叠加相机预览 |

语音对话是默认场景，不需要相机或显示屏。在两个相机场景下，`Vision uplink mode` 用于选择是在云端请求视觉时上传一张 JPEG（`BAIDU_VISION_MODE_IMAGE`），还是发送每一帧采集画面（`BAIDU_VISION_MODE_STREAM`）。

### 视频通路

本例程的视频行为是四个 AI 智能体例程中最丰富的：在一个场景里 LCD 归相机，在另一个场景里 LCD 归智能体。其中涉及三条相互独立的通路：

| 通路 | 方向 | 传输方式 | 生效场景 |
| --- | --- | --- | --- |
| 相机上行 | 相机到云端 | 采集流 0，按配置的相机尺寸输出 JPEG，连接到 `VIDEO_UP` | 图像理解、多模态 |
| 智能体图像 | 云端到 LCD | `VIDEO_DN`，由视频播放器绘制 | 图像生成、多模态 |
| 本地预览 | 相机到 LCD | 采集流 1，RGB565，直接绘制，不发往任何地方 | 开启预览的相机场景 |

只有当需要往屏幕上绘制内容时才会创建显示设备，也就是智能体图像或本地预览生效时。其余情况下例程只构建纯音频播放器，完全不碰 LCD；因此图像理解场景在关闭预览时虽然用到相机，却是无屏运行的。

多模态场景下两条通路同时绘制。智能体图像作为底层，相机预览以画中画形式叠加在其上，z-order 为 1。此时屏幕刷新率跟随智能体图像流，默认 3 fps，而不是预览的 5 fps。

两路下行共用一条 link。例程只把 `AUDIO_DN` 连到播放器，因为 LLM 服务为音频和视频下行导出的是同一个共享 provider，智能体图像经这同一条 link 到达，不需要再调用一次 link。

### 文件结构

```
esp_llm_baidu_agent_app/
├── main/
│   ├── app_main.c            平台初始化、SPIFFS、Wi-Fi 和应用生命周期
│   ├── app_agent.c           百度配置、场景媒体流、AFE 掩码和事件处理
│   ├── app_agent.h           智能体创建 / 启动 / 销毁接口
│   ├── app_agent_utils.c     adjust_volume FunctionCall 的 JSON 解析
│   ├── app_agent_utils.h     音量调节辅助函数声明
│   ├── Kconfig.projbuild     应用 menuconfig 选项
│   └── idf_component.yml
├── components/
│   └── baidu_app_media/      板级设备、采集、播放器和 LCD 预览
├── spiffs/
│   └── lai_lo.mp3            从 spiffs_data 分区播放的唤醒提示音
├── partitions.csv
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── sdkconfig.defaults.esp32s3
├── sdkconfig.defaults.esp32s31
├── README.md
└── README_CN.md
```

### 代码结构

`components/baidu_app_media/baidu_app_media.c` 负责把所选场景转换成一张媒体图。对外公开 API 仅提供 audio capture 与 player 的 getter；video 上行、生成图下行和本地预览在 `baidu_app_media_start()` 内完成 link，应用层无需持有 video 句柄。该文件顶部的编译期开关决定了整张图的形态：

| 开关 | 含义 |
| --- | --- |
| `BAIDU_APP_MEDIA_CAMERA` | 存在相机采集；由图像理解和多模态场景置位 |
| `BAIDU_APP_MEDIA_VIDEO_DN` | 需要接收智能体图像；由图像生成和多模态场景置位 |
| `BAIDU_APP_MEDIA_PREVIEW` | 开启本地预览；跟随 `BAIDU_VIDEO_PREVIEW` |
| `BAIDU_APP_MEDIA_LCD` | 上面两者任一需要屏幕，于是播放器按视频播放器构建 |

按执行顺序，建议优先阅读下面这些函数：

| 函数 | 职责 |
| --- | --- |
| `create_video_capture()` | 打开采集流 0 用于 JPEG 上行，采集流 1 用于 RGB565 预览 |
| `create_video_player()` | 构建挂有显示设备的播放器，`BAIDU_APP_MEDIA_LCD` 置位时使用 |
| `create_audio_player()` | 构建纯音频播放器，不需要屏幕时使用 |
| `create_preview_stream()` | 借用播放器的 render，打开预览流，并计算画中画矩形 |
| `preview_task()` | 从采集流 1 取帧，并直接写入预览 render 流 |
| `link_services()` | 把音频上行、相机上行和共享下行连接到播放器 |

`main/app_agent.c` 是另一半：它把场景映射到 `cfg.service.streams[]`，构建音频功能掩码，并处理智能体事件，包括唤醒流程和 `adjust_volume` FunctionCall。

## 环境配置

### 硬件要求

- 提供 `audio_adc` 和 `audio_dac` 设备的开发板
- 接到板级音频输出的扬声器或耳机
- 视觉场景下开发板还需提供 `camera` 和 `display_lcd` 设备
- 带 PSRAM，ESP32-S3 和 ESP32-S31 需 16 MB Flash，ESP32-P4 需 8 MB Flash

分区表使用 6 MB 的 `factory` 应用分区、512 KB 的 `model` 分区（存放 WakeNet 模型）和 128 KB 的 `spiffs_data` 分区（存放唤醒提示音）。

### 默认 IDF 分支

本例程支持 IDF release/v5.4 (>= v5.4.3) 与 release/v5.5 (>= v5.5.2) 分支。

### 软件要求

- 百度应用 ID、用户 ID、智能体平台 URL、工作流和实例 ID
- 账号要求时还需要许可证密钥
- 能访问所配置百度智能体平台 URL 的 Wi-Fi 网络

账号、实例 ID 和 Wi-Fi 在 `idf.py menuconfig` 中填写后再运行。CI 通过 `sdkconfig.ci` 从环境变量注入这些值。

## 编译和下载

### 编译准备

编译本例程前请先配置好 ESP-IDF。如果已经配置完成，直接进入工程目录即可；否则在 ESP-IDF 根目录下执行以下脚本。所用芯片的说明请参考 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)。

```
./install.sh
. ./export.sh
```

- 进入例程目录：

```
cd adf_examples/ai_agent/esp_llm_baidu_agent_app
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

在 `Baidu LLM agent application` 下设置：

- `Baidu application ID`、`Baidu user ID`、`Baidu license key`、`Baidu agent platform URL`、`Baidu workflow` 和 `Baidu instance ID`：设备接入的账号和工作流
- `Agent scene`：语音对话、图像理解、图像生成或多模态
- `Vision uplink mode`：按需抓拍或连续发帧；仅在相机场景下显示
- `Camera JPEG width` / `height` / `Camera capture FPS`：仅相机场景，尺寸必须匹配一个 esp-video JPEG 传感器 mode
- `Local LCD camera preview`：在 LCD 上绘制第二路 RGB565 流；这是本地预览，不是 `VIDEO_DN`
- `Audio front-end features`：只暴露 `Enable WakeNet wake-up`，因为 AEC、NS、VAD 始终开启
- `Audio output volume`：0 到 100，默认 80
- `Wi-Fi`：SSID、密码和连接超时（毫秒）
- `Run time before clean shutdown`：0 表示 `app_main` 启动后即返回并让智能体继续运行；非零值会走完整的停止、断开和销毁流程

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

1. 启动后设备先挂载 SPIFFS、连接 Wi-Fi，然后接入智能体平台
2. 入会成功后日志会提示百度智能体就绪
3. 语音对话场景下可随时说话，由智能体工作流判断一次提问何时结束
4. 开启 WakeNet 后，唤醒词会在播放器 stream 2 播放 `lai_lo.mp3`，取消当前回复，并发送 `[B]:[BEGIN]:0` 让智能体开始收音。麦克风不会被静音，因此唤醒词之前的语音同样会上云
5. 图像理解和多模态场景下，相机数据送入 `VIDEO_UP`；开启本地预览时 LCD 同时显示画面
6. 图像生成和多模态场景下，智能体生成的图像经 `VIDEO_DN` 到达并由视频播放器绘制
7. `adjust_volume` FunctionCall 会修改播放音量，再通过 `esp_llm_baidu_send_text_to_tts()` 语音确认
8. 后端 `AUDIO_PLAYER_START` 和 `AUDIO_PLAYER_STOP` 事件在播放器 stream 1 上播放和停止 HTTP 音乐 URL
9. 把 `Run time before clean shutdown` 设为非零值，可以观察完整的拆除流程

## 故障排除

### 应用在入会前就停止

查看日志定位失败的步骤。Wi-Fi SSID 为空会导致网络启动失败，百度账号字段不完整则在创建智能体引擎之前就被拒绝。

### 缺少板级设备名

编译或启动时报未知设备句柄，说明没有为所选芯片生成板级元数据。用与目标芯片匹配的开发板重新执行 `idf.py bmgr -b <board>`。

### 入会超时

检查网络连通性、智能体平台 URL、账号取值、工作流名称以及 Wi-Fi 连接超时。

### 出现 `V4L2_SRC: Fail to open device`

可能是所选芯片的相机默认项没有生效，也可能是板上实装的传感器与这些默认项描述的型号不一致。先用 `idf.py set-target` 重新生成 `sdkconfig` 以加载芯片文件。芯片文件只会选定一种传感器和一种 mode：ESP32-S3 为 DVP 接口的 OV3660、JPEG 320x240，ESP32-S31 为 DVP 接口的 SC101IOT、YUV422 YUYV 640x480，ESP32-P4 为 MIPI CSI 接口的 SC2336、RAW8 1024x600。如果板上是别的传感器，请在 menuconfig 中改为启用该传感器，并把 `Camera JPEG width` 和 `height` 设成它真实存在的 mode。

### 唤醒提示音不播放

`spiffs_data` 分区没有烧录，或者 `spiffs/` 下缺少 `lai_lo.mp3`。请重新烧录完整固件，确保 SPIFFS 分区被写入。

### 应用放不进 Flash

`factory` 分区为 6 MB。同时启用多个场景并打开 WakeNet 会让镜像变大；关闭本地预览或用不到的场景可以腾出空间。

## 技术支持

- 论坛：[esp32.com](https://esp32.com/viewforum.php?f=20)
- 问题反馈：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
