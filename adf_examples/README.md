# ESP-ADF Examples

[中文版](./README_CN.md)

This directory contains multimedia example projects for ESP platforms, including audio and video interaction scenarios. These examples provide reference code and can be used as a starting point for your own projects.

## Available Examples

| Example | Path | Notes |
|---|---|---|
| COZE WebSocket Bidirectional Streaming Conversation | [ai_agent/coze_ws_app](./ai_agent/coze_ws_app) | ESP-GMF based voice dialog example (Coze WebSocket). |
| Baidu RTC Voice Assistant Demo | [ai_agent/baidu_rtc/solutions/voice_assistant_app](./ai_agent/baidu_rtc/solutions/voice_assistant_app) | ESP-GMF based Baidu RTC real-time audio/video assistant demo. |
| ESP Audio Analyzer APP | [checks/esp_audio_analyzer_app](./checks/esp_audio_analyzer_app) | Example to comprehensively test microphone, speaker, and AEC functionality using ESP Audio Analyzer. |
| Services Hub | [services_hub](./services_hub) | Multi-service orchestration example integrating Wi-Fi, OTA, CLI, and button services via `adf_event_hub` and `esp_service_manager`, with optional MCP integration. |
| AV Record Live Display | [recorder/av_record_live_display](./recorder/av_record_live_display) | AV capture with MP4 recording to microSD and live LCD preview via `esp_capture`, encoders, and `mp4_muxer` on ESP32-S3 and ESP32-P4. |
| Play Music Control | [player/play_music_control](./player/play_music_control) | CLI music player with SD card, HTTP/HTTPS, and embedded Flash sources; playlist control via `esp_cli_service` and `esp_audio_simple_player`. |
| Audio Power Save | [system/audio_power_save](./system/audio_power_save) | Idle low-power example with MQTT keepalive, automatic light sleep, and UART/MQTT/GPIO/timer wakeup with LittleFS sleep/wakeup prompt tones. |
| SD Card Music Player | [player/music_player](./player/music_player) | Local SD card music player based on `esp_audio_simple_player`, `esp_playlist`, and the LVGL music demo, with touch controls and repeat modes. |

## Usage

After activating the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) environment, run the following command to download an example project locally (e.g., `ai_agent/coze_ws_app`):

```shell
idf.py create-project-from-example "espressif/adf_examples:coze_ws_app"
```
