# ESP-ADF Examples

[English Version](./README.md)

本目录包含面向 ESP 平台的多媒体示例工程，覆盖音频与视频交互场景。这些示例提供了可参考的代码，便于在此基础上进行二次开发。

## 当前可用示例

| 示例 | 路径 | 说明 |
|---|---|---|
| 扣子 WebSocket 双向流式对话 | [ai_agent/coze_ws_app](./ai_agent/coze_ws_app) | 基于 ESP-GMF 的 Coze 语音对话示例。 |
| 百度 RTC 语音助手演示 | [ai_agent/baidu_rtc/solutions/voice_assistant_app](./ai_agent/baidu_rtc/solutions/voice_assistant_app) | 基于 ESP-GMF 的百度 RTC 实时音视频助手示例。 |
| ESP 音频分析工具示例 | [checks/esp_audio_analyzer_app](./checks/esp_audio_analyzer_app) | 基于 ESP 音频分析工具全面测试麦克风、扬声器及 AEC 功能的示例。 |
| 服务中枢综合示例 | [services_hub](./services_hub) | 通过 `adf_event_hub` 与 `esp_service_manager` 集成 Wi-Fi、OTA、CLI 与按键服务的多服务编排示例，可选支持 MCP。 |
| 音视频录制与实时显示 | [recorder/av_record_live_display](./recorder/av_record_live_display) | 基于 `esp_capture` 的音视频采集、MP4 录制与 LCD 实时显示示例，支持 ESP32-S3 与 ESP32-P4。 |
| CLI 多源音乐播放 | [player/play_music_control](./player/play_music_control) | 基于 `esp_audio_simple_player` 与 `esp_cli_service` 的 CLI 音乐播放器，支持 SD 卡、HTTP/HTTPS 与 Flash 内嵌音源及播放列表控制。 |
| 音频低功耗 | [system/audio_power_save](./system/audio_power_save) | 空闲低功耗示例：MQTT keepalive 保活、自动 light sleep，支持 UART/MQTT/GPIO/定时器唤醒，含 LittleFS 分区存储的休眠/唤醒提示音。 |
| SD 卡音乐播放器 | [player/music_player](./player/music_player) | 基于 `esp_audio_simple_player`、`esp_playlist` 与 LVGL music demo 的 SD 卡本地音乐播放器，支持触摸屏切歌与多种循环模式。 |

## 使用说明

激活 [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html) 环境后，执行以下指令可以下载某个示例工程到本地，以 `ai_agent/coze_ws_app` 为例：

```shell
idf.py create-project-from-example "espressif/adf_examples:coze_ws_app"
```
