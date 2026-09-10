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
| AV Record Live Display | [recorder/av_record_live_display](./recorder/av_record_live_display) | AV capture with MP4 recording to microSD and live LCD preview via `esp_capture`, encoders, and `mp4_muxer` on ESP32S3, ESP32S31 and ESP32P4. |
| Play Music Control | [player/play_music_control](./player/play_music_control) | CLI music player with SD card, HTTP/HTTPS, and embedded Flash sources; playlist control via `esp_cli_service` and `esp_audio_simple_player`. |
| Audio Power Save | [system/audio_power_save](./system/audio_power_save) | Idle low-power example with MQTT keepalive, automatic light sleep, and UART/MQTT/GPIO/timer wakeup with LittleFS sleep/wakeup prompt tones. |
| SD Card Music Player | [player/music_player](./player/music_player) | Local SD card music player based on `esp_audio_simple_player`, `esp_playlist`, and `esp_extractor`, with a dark LVGL touch UI, progress bar, and repeat modes. |
| RTSP Push | [protocols/rtsp_push](./protocols/rtsp_push) | Basic example that links camera and microphone capture to an `esp_rtsp_service` SINK and publishes to a configurable RTSP server. |
| RTSP CLI | [protocols/rtsp_cli](./protocols/rtsp_cli) | RTSP server, client push, and client play in one firmware, switched from the CLI; camera and microphone stream out over `esp_rtsp_service`, and a remote stream plays on the LCD and speaker. |
| RTMP CLI | [protocols/rtmp_cli](./protocols/rtmp_cli) | RTMP publish, play, and relay server in one firmware, switched from the CLI; `esp_rtmp_service` sends the camera to an ingest server or serves it locally, plays a remote stream on the LCD and speaker, and runs the whole chain on the board alone. |

## Usage

After activating the [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) environment, run the following command to download an example project locally (e.g., `ai_agent/coze_ws_app`):

```shell
idf.py create-project-from-example "espressif/adf_examples:coze_ws_app"
```
