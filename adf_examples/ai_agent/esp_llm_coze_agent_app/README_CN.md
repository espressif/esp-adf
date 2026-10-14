# ESP LLM Coze 智能体

- [English](./README.md)
- 例程难度：⭐⭐

## 例程简介

本例程实现了扣子 **WebSocket 双向流式对话**，基于 **esp_llm_service**：会话与媒体经 `esp_llm_coze` 适配器接入，支持直接对话、语音唤醒与打断等交互方式。

- 通过 WebSocket 连接 Coze bot，双工语音：你说话，bot 回答，经板级扬声器播放。
- 本例程 **仅音频**，不使用相机或 LCD；Coze 适配器不启用视频流。
- 使用前请在 [Coze 文档](https://www.coze.cn/open/docs/developer_guides/streaming_chat_event) 申请 **Access token** 与 **BOT ID**，并在 menuconfig 中填写。

官方文档：[Coze API 概览](https://www.coze.cn/docs/developer_guides/coze_api_overview)。

### 功能

下面每一项能力都在 `idf.py menuconfig` 中开启：

| 能力 | 作用 | 配置项 | 默认 |
| --- | --- | --- | --- |
| 语音对话 | 与 Coze bot 进行双工语音交流 | 始终编译在内 | 开 |
| 持续拾音 | 由 Coze 服务端判断你的提问何时结束 | 保持 `Enable WakeNet wake-up` 关闭 | 开 |
| 唤醒词 | 唤醒词之前保持麦克风关闭，唤醒后取消当前回复 | `Enable WakeNet wake-up` | 关 |
| 回声消除与噪声抑制 | 上行 AEC、NS 默认开启 | 编译固定开启 | 开 |
| 语音活动检测 | 判断语音的起止 | `Enable voice activity detection` | 开 |
| 自定义 TTS 音色 | 指定 bot 回答所用的音色 | `Coze voice ID` | bot 默认音色 |
| 日志字幕 | 打印识别出的提问和朗读的回复 | 始终编译在内 | 开 |

### 典型场景

- 基于 Coze bot 的语音助手音箱
- 持续拾音的对话，由服务端判断一次提问何时结束
- 唤醒词门控的对话，WakeNet 触发前麦克风保持关闭

### 媒体能力

| 项目 | 本例程的支持情况 |
| --- | --- |
| 音频上行 | OPUS，16 kHz，单声道 |
| 音频下行 | OPUS，16 kHz，单声道 |
| 视频上行 | 不支持 |
| 视频下行 | 不支持 |
| 相机和 LCD | 不使用 |
| 音频前端 | AEC、NS 固定开启；VAD 由 `COZE_AUDIO_VAD` 控制，默认开 |
| 唤醒词 | WakeNet（`COZE_AUDIO_WN`）：开启后唤醒前关闭上行，唤醒后恢复并打断当前回复 |
| 上行门控 | 开启 WakeNet 后，唤醒词触发前音频上行保持关闭 |
| 播放器流 | stream 0 承载 Coze TTS；stream 1 空闲，可用于 URL 或提示音 |

本例程仅音频。Coze 适配器拒绝视频流，因此不会启用 `VIDEO_UP` 和 `VIDEO_DN`，也不需要相机或显示设备。

### 文件结构

```
esp_llm_coze_agent/
├── main/
│   ├── app_main.c            平台初始化、Wi-Fi 和应用生命周期
│   ├── app_agent.c           Coze 配置、AFE 掩码和 LLM 事件处理
│   ├── app_agent.h           智能体创建 / 启动 / 销毁接口
│   ├── Kconfig.projbuild     应用 menuconfig 选项
│   └── idf_component.yml
├── components/
│   └── coze_app_media/       板级设备、音频采集和音频播放器
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

- Coze 开放平台签发的 bot ID 和 access token
- 能访问 `ws.coze.cn` 和 `api.coze.cn` 的 Wi-Fi 网络

## 编译和下载

### 编译准备

编译本例程前请先配置好 ESP-IDF。如果已经配置完成，直接进入工程目录即可；否则在 ESP-IDF 根目录下执行以下脚本。所用芯片的说明请参考 [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)。

```
./install.sh
. ./export.sh
```

- 进入例程目录：

```
cd adf_examples/ai_agent/esp_llm_coze_agent
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

在 `Coze LLM agent application` 下设置：

- `Coze bot ID` 和 `Coze access token`：必填，任一为空时应用拒绝启动
- `Coze WebSocket URL` 和 `Coze conversation-create URL`：已预置为 Coze 公开端点
- `Coze user ID` 和 `Coze voice ID`：留空则沿用适配器默认值
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

1. 启动后应用先连接 Wi-Fi，然后创建并启动 Coze 会话
2. 日志出现 `Coze agent is ready` 表示设备已连接，可以开始对话
3. 关闭 WakeNet 时可随时说话，由 Coze 服务端 VAD 判断一次提问何时结束
4. 开启 WakeNet 时先说唤醒词。唤醒后麦克风打开，正在进行的回复被取消，可以开始新一轮对话
5. 回复以 TTS 形式到达播放器 stream 0，经板级音频输出播放
6. 字幕、VAD 状态跳变、连接变化和对话完成都会打印为 `Agent event:` 日志行
7. 把 `Run time before clean shutdown` 设为非零值，可以观察完整的拆除流程，而不是一直运行

## 故障排除

### 应用在连接前就停止

`Validate configuration failed: Coze bot ID or access token is empty` 表示账号字段留空。在 `Coze LLM agent application` 下把两项都填好并重新编译。`Start network failed: Wi-Fi SSID is empty` 表示 Wi-Fi SSID 尚未设置。

### 缺少板级设备名

编译或启动时报未知设备句柄，说明没有为所选芯片生成板级元数据。用与目标芯片匹配的开发板重新执行 `idf.py bmgr -b <board>`。

### WebSocket 连接失败

确认网络能访问 `ws.coze.cn`、access token 未过期、bot 已发布。TLS 还需要足够的堆内存：本例程将 `CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN` 设为 10240，并要求带 PSRAM。

### 播放声音失真或语速不对

板级 DAC 和 AEC 采集共用一条 I2S 总线，因此即使流是单声道，播放器也按双声道打开。改动音频配置时，请保持播放器和采集的采样率一致。

## 技术支持

- 论坛：[esp32.com](https://esp32.com/viewforum.php?f=20)
- 问题反馈：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
