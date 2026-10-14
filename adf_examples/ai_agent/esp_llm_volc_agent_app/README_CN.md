# ESP LLM Volc 智能体

- [English](./README.md)
- 例程难度：⭐⭐

## 例程简介

本例程把开发板变成一台火山引擎语音设备。它通过火山 IoT 完成鉴权、加入 RTC 房间，进行双工语音对话：你说话，智能体回答，回复经板级扬声器播放出来。本例程只涉及音频，不使用相机或显示屏。

官方文档：[火山引擎 RTC](https://www.volcengine.com/docs/6348)。

### 功能

下面每一项能力都在 `idf.py menuconfig` 中开启：

| 能力 | 作用 | 配置项 | 默认 |
| --- | --- | --- | --- |
| 语音对话 | 通过 RTC 与火山对话智能体进行双工语音交流 | 始终编译在内 | 开 |
| 持续拾音 | 由火山服务端判断你的提问何时结束 | 保持 `Enable WakeNet wake-up` 关闭 | 开 |
| 唤醒词 | 唤醒词之前保持麦克风关闭，唤醒后取消当前回复 | `Enable WakeNet wake-up` | 关 |
| 回声消除 | 从麦克风信号中去掉扬声器声音，避免智能体听到自己 | `Enable acoustic echo cancellation` | 开 |
| 噪声抑制 | 净化上行信号 | `Enable noise suppression` | 开 |
| 语音活动检测 | 判断语音的起止 | `Enable voice activity detection` | 开 |
| 低码率语音链路 | 用 G.711A 把 RTC 音频控制在 64 kbit/s | `ESP LLM Volc` 的 RTC 音频编码 | G.711A |
| 直接文本转语音 | 通过 `esp_llm_volc_send_text_to_tts()` 朗读一段文本，不经过模型 | 始终编译在内 | 开 |

### 典型场景

- 通过 RTC 房间而非 WebSocket 接入模型的语音助手音箱
- 低码率语音链路，G.711A 将上行控制在 64 kbit/s
- 唤醒词门控的对话，WakeNet 触发前麦克风保持关闭

### 媒体能力

| 项目 | 本例程的支持情况 |
| --- | --- |
| 音频上行 | G.711A，8 kHz，单声道 |
| 音频下行 | G.711A，8 kHz，单声道 |
| 视频上行 | 被应用关闭 |
| 视频下行 | 被应用关闭 |
| 相机和 LCD | 不使用 |
| 音频前端 | AEC、NS 固定开启；VAD 由 `VOLC_AUDIO_VAD` 控制，默认开 |
| 唤醒词 | WakeNet，默认关闭，由 `VOLC_AUDIO_WN` 开启 |
| 上行门控 | 开启 WakeNet 后，唤醒词触发前音频上行保持关闭 |
| 播放器流 | stream 0 承载 Volc TTS；stream 1 空闲，可用于 URL 或提示音 |

RTC 流是 8 kHz 的 G.711A，由 `sdkconfig.defaults` 中的 `CONFIG_ESP_LLM_VOLC_AUDIO_CODEC_G711A` 选定。AFE 和共用的 I2S 总线仍按 16 kHz PCM 运行，适配器把上行编码降到 8 kHz。播放器按 16 kHz 双声道打开，以便与 AEC 参考信号保持同一 I2S 时钟。

本例程仅音频。即使组件 Kconfig 会发布视频流，这些流也保持关闭，因此不需要相机或显示设备。

### 文件结构

```
esp_llm_volc_agent/
├── main/
│   ├── app_main.c            平台初始化、Wi-Fi、SNTP 和应用生命周期
│   ├── app_agent.c           Volc 配置、AFE 掩码和 LLM 事件处理
│   ├── app_agent.h           智能体创建 / 启动 / 销毁接口
│   ├── Kconfig.projbuild     应用 menuconfig 选项
│   └── idf_component.yml
├── components/
│   └── volc_app_media/       板级设备、音频采集和音频播放器
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
- 带 PSRAM，ESP32-S3 和 ESP32-S31 需 16 MB Flash，ESP32-P4 需 8 MB Flash

| 芯片 | 开发板 |
| --- | --- |
| `esp32s3` | `esp32_s3_korvo_2_3` |
| `esp32s31` | `esp32_s31_korvo_1` |
| `esp32p4` | `esp32_p4_function_ev_board` |

固件使用自定义分区表，应用分区 `factory` 为 6 MB。

### 默认 IDF 分支

本例程支持 IDF release/v5.4 (>= v5.4.3) 与 release/v5.5 (>= v5.5.2) 分支。

### 软件要求

- 火山 IoT 凭据：instance ID、product key、product secret 和 device name
- 一个火山引擎对话 bot ID
- 能访问火山 RTC 端点和 NTP 服务器的 Wi-Fi 网络

`Kconfig.projbuild` 和 `sdkconfig.defaults` 预置了内部实验室账号和实验室 Wi-Fi SSID，因此例程无需修改即可编译。在该实验室网络之外使用本例程前，请把所有凭据替换为自己的。

## 编译和下载

### 编译准备

编译本例程前请先配置好 ESP-IDF。如果已经配置完成，直接进入工程目录即可；否则在 ESP-IDF 根目录下执行以下脚本。所用芯片的说明请参考 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)。

```
./install.sh
. ./export.sh
```

- 进入例程目录：

```
cd adf_examples/ai_agent/esp_llm_volc_agent
```

- 先选芯片，再选开发板：

```bash
idf.py set-target esp32s3
idf.py bmgr -b esp32_s3_korvo_2_3
```

`idf.py set-target` 要在 `idf.py bmgr` 之前执行，因为它会同时加载 `sdkconfig.defaults` 和对应的 `sdkconfig.defaults.<chip>` 文件。本例程使用 [ESP Board Manager](https://github.com/espressif/esp-board-manager)：`idf.py bmgr -l` 列出当前可见的开发板，`idf.py bmgr -b <board_index|board_name>` 选择其中一个。

### 项目配置

```bash
idf.py menuconfig
```

在 `Volc LLM agent application` 下设置：

- `Volc IoT instance ID`、`Volc IoT product key`、`Volc IoT product secret` 和 `Volc IoT device name`：用于给 RTC 配置请求签名的设备身份
- `Volc bot ID`：设备对话的目标 bot
- `Audio front-end features`：AEC、NS 编译固定开启；VAD 默认开启；`Enable WakeNet wake-up` 默认关闭
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
2. 日志出现 `Volc agent is ready` 表示设备已加入房间，可以开始对话
3. 关闭 WakeNet 时可随时说话，由火山服务端判断一次提问何时结束
4. 开启 WakeNet 时先说唤醒词。唤醒后麦克风打开，正在进行的回复被取消，可以开始新一轮对话
5. 回复以 TTS 形式到达播放器 stream 0，经板级音频输出播放
6. 对话状态变化以后端事件 `CONV_STATUS` 发布；连接变化、VAD 状态跳变和错误打印为 `Agent event:` 日志行
7. `esp_llm_service_send_text()` 向模型提问，`esp_llm_volc_send_text_to_tts()` 则直接朗读一段文本，不经过模型
8. 把 `Run time before clean shutdown` 设为非零值，可以观察完整的拆除流程，而不是一直运行

## 故障排除

### 应用在加入房间前就停止

查看日志定位失败的步骤。Wi-Fi SSID 为空会报 `Start network failed: Wi-Fi SSID is empty`，账号字段缺失则在创建 RTC 会话之前就被拒绝。

### 签名被拒或 `GetRTCConfig` 失败

日志出现 `SNTP sync timed out; Volc GetRTCConfig signature will be rejected` 表示系统时钟没有设置成功。时钟不准会导致 RTC 请求签名失败，请确认网络能访问 `pool.ntp.org` 或 `cn.pool.ntp.org`。

### 缺少板级设备名

编译或启动时报未知设备句柄，说明没有为所选芯片生成板级元数据。用与目标芯片匹配的开发板重新执行 `idf.py bmgr -b <board>`。

### RTC 会话始终连不上

对照火山控制台核对 IoT instance ID、product key、product secret 和 device name，并确认该实例下的 bot ID 有效。

### 播放声音失真或语速不对

板级 DAC 和 AEC 采集共用一条 I2S 总线，因此即使 G.711A 流是 8 kHz 单声道，播放器也按 16 kHz 双声道打开。改动音频配置时请保持这一关系。

## 技术支持

- 论坛：[esp32.com](https://esp32.com/viewforum.php?f=20)
- 问题反馈：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
