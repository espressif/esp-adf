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

## 使用说明

激活 [ESP-IDF](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/index.html) 环境后，执行以下指令可以下载某个示例工程到本地，以 `ai_agent/coze_ws_app` 为例：

```shell
idf.py create-project-from-example "espressif/adf_examples:coze_ws_app"
```
