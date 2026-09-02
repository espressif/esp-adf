Multimedia Examples
=====================

:link_to_translation:`zh_CN:[中文]`

This page categorizes multimedia examples by function, covering playback, recording and capture, audio algorithms, display and rendering, Bluetooth audio, cloud agents, communication protocols, checks, and system services.

Playback Examples
------------------

Playback examples demonstrate a complete media playback flow: reading and decoding from an SD card, Flash, HTTP, HLS, or MIDI, then outputting to the speaker or display, with touchscreen or serial CLI control.

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - Example
     - Description
     - Component Link
   * - pipeline_play_sdcard_music
     - Plays music from an SD card, with decode and audio effects. Supports MP3/WAV/FLAC/AAC and other formats.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_play_sdcard_music>`__
   * - pipeline_play_multi_source_music
     - Switches between HTTP and SD card sources and can insert a Flash tone. Playback of the original track resumes after the tone finishes.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_play_multi_source_music>`__
   * - pipeline_loop_play_no_gap
     - Uses a GMF task policy to loop multiple SD card tracks with no noticeable gap when switching.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_loop_play_no_gap>`__
   * - pipeline_play_embed_music
     - Plays Flash audio embedded in firmware. It does not require an SD card or network and is suitable for boot tones.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_play_embed_music>`__
   * - pipeline_play_http_music
     - Connects to Wi-Fi, fetches an HTTP URL, then decodes and plays the stream. m3u8/HLS is not supported.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_play_http_music>`__
   * - audio_player
     - Implements full audio playback with ``esp_player``: demux, decode, and playback control. It can mix 2–4 streams to one DAC.
     - `esp_player <https://github.com/espressif/esp-gmf/tree/main/packages/esp_player/examples/audio_player>`__
   * - video_player (esp_player)
     - Plays local audio and video with ``esp_player``: demux, decode, and A/V sync. Video goes to the LCD and audio to the speaker.
     - `esp_player <https://github.com/espressif/esp-gmf/tree/main/packages/esp_player/examples/video_player>`__
   * - play_music_control
     - Controls multi-source playback from a serial CLI: SD card, HTTP/HTTPS, and Flash tones, with playlist support.
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/player/play_music_control>`__
   * - music_player
     - Scans MP3/AAC/WAV files on the SD card and shows a playlist and playback controls on a touchscreen. Supports single-track, list, and shuffle modes.
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/player/music_player>`__
   * - hls_live_stream
     - Parses an HLS (m3u8) live stream and decodes it for playback. Suitable for internet radio and live audio.
     - `esp_hls_stream <https://github.com/espressif/esp-adf-libs/tree/master/esp_hls_stream/examples/hls_live_stream>`__
   * - midi_play
     - Plays MIDI files from the SD card.
     - `esp_midi <https://github.com/espressif/esp-adf-libs/tree/master/esp_midi/examples/play>`__
   * - midi_keyboard
     - Receives MIDI messages in real time and synthesizes sound, implementing a simple electronic keyboard.
     - `esp_midi <https://github.com/espressif/esp-adf-libs/tree/master/esp_midi/examples/keyboard>`__
   * - play_embed_music
     - Board example: plays a WAV file embedded in firmware. Used to verify board audio output.
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/play_embed_music>`__
   * - play_sdcard_music
     - Board example: plays WAV from the SD card.
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/play_sdcard_music>`__

Recording and Capture Examples
--------------------------------

Recording and capture examples demonstrate microphone and camera capture. Captured data can be encoded and written to an SD card, uploaded over HTTP, or previewed on a local display.

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - Example
     - Description
     - Component Link
   * - pipeline_record_sdcard
     - Records from the microphone to the SD card. Supports PCM/MP3/AAC and other formats, with optional low-power recording.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_record_sdcard>`__
   * - pipeline_record_http
     - Captures and encodes microphone audio, then uploads it to a server over HTTP. Suitable for cloud recording or a speech-recognition frontend.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_record_http>`__
   * - pipeline_record_audio_muxer
     - Encodes a recording and muxes it into an MP4/TS container before writing it to the SD card, instead of saving a raw audio stream only.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_record_audio_muxer>`__
   * - aec_rec
     - Plays an SD card MP3 on one path while the microphone path runs AEC, then encodes and saves the result for echo-cancellation comparison.
     - `gmf_ai_audio <https://github.com/espressif/esp-gmf/tree/main/elements/gmf_ai_audio/examples/aec_rec>`__
   * - audio_capture
     - Implements audio capture with ``esp_capture``, covering basic recording, AEC, file writing, and custom processing without building a pipeline manually.
     - `esp_capture <https://github.com/espressif/esp-gmf/tree/main/packages/esp_capture/examples/audio_capture>`__
   * - video_capture
     - Implements video capture with ``esp_capture``: single frame, recording, overlay, and dual-stream, with optional SD card storage.
     - `esp_capture <https://github.com/espressif/esp-gmf/tree/main/packages/esp_capture/examples/video_capture>`__
   * - av_record_live_display
     - Captures camera and microphone data, shows a live LCD preview, and records audio and video as MP4 on the SD card.
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/recorder/av_record_live_display>`__
   * - live_photo_capture
     - Captures a short video, picks a cover frame, and writes a Motion Photo style file to the SD card.
     - `esp_live_photo <https://github.com/espressif/esp-adf/tree/master/components/esp_live_photo/examples/live_photo_capture>`__
   * - video_muxer
     - Muxes camera and microphone streams with ESP Muxer and saves the result to the SD card.
     - `esp_muxer <https://github.com/espressif/esp-adf-libs/tree/master/esp_muxer/examples/video_muxer>`__
   * - record_to_sdcard
     - Board example: records from the microphone to the SD card.
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/record_to_sdcard>`__
   * - record_and_play
     - Board example: plays captured microphone audio on the speaker in real time to verify the audio path.
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/record_and_play>`__

Audio Algorithm Examples
------------------------

Audio algorithm examples demonstrate common audio processing features, including effects, howling suppression, wake-word and command recognition, sample-rate conversion, and spectrum analysis.

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - Example
     - Description
     - Component Link
   * - pipeline_audio_effects
     - Demonstrates resampling, Sonic time-stretch/pitch-shift, at least 4-way mixing, EQ, and DRC on multiple GMF pipelines.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_audio_effects>`__
   * - pipeline_howl
     - Karaoke setup with three pipelines: SD accompaniment and microphone are processed separately, then mixed after howling suppression and played from the DAC.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_howl>`__
   * - wwe (wake word and commands)
     - Implements wake-word, VAD, and command recognition with AFE and can run offline.
     - `gmf_ai_audio <https://github.com/espressif/esp-gmf/tree/main/elements/gmf_ai_audio/examples/wwe>`__
   * - asrc_demo
     - Converts sample rate, channel count, and bit depth with ASRC so record or playback formats match the target specification.
     - `esp_asrc <https://github.com/espressif/esp-gmf/tree/main/packages/esp_asrc/examples/asrc_demo>`__
   * - fft_spectrum_print
     - Generates a multi-tone cosine signal, runs a 512-point Q15 real FFT, prints a spectrum bar graph over UART, then checks round-trip error with IFFT.
     - `gmf_fft <https://github.com/espressif/esp-gmf/tree/main/packages/gmf_fft/examples/fft_spectrum_print>`__
   * - ae_howl
     - Applies ``esp_ae_howl`` howling suppression to the microphone, then mixes it with SD card background music.
     - `esp_audio_effects <https://github.com/espressif/esp-adf-libs/tree/master/esp_audio_effects/example/ae_howl>`__
   * - esp_audio_effects_demo
     - Demonstrates ALC, Fade, EQ, Sonic, DRC, MBC, Mixer, and basic format conversion one by one.
     - `esp_audio_effects <https://github.com/espressif/esp-adf-libs/tree/master/esp_audio_effects/example/esp_audio_effects_demo>`__

Display and Render Examples
---------------------------

Display and render examples demonstrate how to send video frames and PCM audio to the LCD and DAC, covering dual-eye synchronized display, overlays, multi-stream mixing, and LVGL UI verification on the board.

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - Example
     - Description
     - Component Link
   * - dual_eyes
     - Renders left and right eye streams in sync. Supports side-by-side on one screen or dual-screen output.
     - `esp_video_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_video_render/examples/dual_eyes>`__
   * - video_player (esp_video_render)
     - Builds a lightweight local player with ``esp_video_render``: extract, decode, and render from the SD card, with an overlay UI.
     - `esp_video_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_video_render/examples/video_player>`__
   * - video_render
     - Reads MJPEG from the SD card and demonstrates single-stream/multi-stream render, scaling, and a progress-bar overlay.
     - `esp_video_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_video_render/examples/video_render>`__
   * - audio_render
     - Implements single-stream PCM playback and multi-stream mixing with ``esp_audio_render``. It can also play network-decoded audio.
     - `esp_audio_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_audio_render/examples/audio_render>`__
   * - simple_piano
     - Implements a polyphonic piano with ``esp_audio_render``: multiple tracks sound together and render to the DAC in real time.
     - `esp_audio_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_audio_render/examples/simple_piano>`__
   * - display_lvgl
     - Initializes the display with board manager and runs an LVGL test UI to verify display and touch.
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/display_lvgl>`__
   * - av_render_test
     - Captures camera and microphone data and outputs them to the LCD and I2S together to verify the A/V render path.
     - `av_render <https://github.com/espressif/esp-webrtc-solution/tree/main/components/av_render/examples/render_test>`__

Bluetooth Audio Examples
------------------------

Bluetooth audio examples demonstrate classic Bluetooth audio, covering A2DP playback, HFP calls, and AVRCP control, with optional LE Audio.

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - Example
     - Description
     - Component Link
   * - bt_audio
     - One project covers A2DP Source/Sink, HFP calls, and AVRCP control. LE Audio, BIS, and an LVGL touch UI are optional.
     - `esp_bt_audio <https://github.com/espressif/esp-gmf/tree/main/packages/esp_bt_audio/examples/bt_audio>`__

Cloud Agent Examples
--------------------

Cloud agent examples demonstrate connecting a device to a cloud voice agent, covering real-time voice interaction with Coze, Baidu RTC, and OpenAI Realtime.

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - Example
     - Description
     - Component Link
   * - coze_ws_app
     - Connects to the Coze WebSocket OpenAPI for two-way streaming voice, with direct talk, wake word, and button barge-in.
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/coze_ws_app>`__
   * - voice_assistant_app
     - Implements a Baidu RTC voice assistant: conversation, network music playback, and two-way audio mixing.
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/baidu_rtc/solutions/voice_assistant_app>`__
   * - openai_demo
     - Connects to OpenAI Realtime with ``esp_webrtc`` for live voice conversation and demonstrates function-call device control.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/openai_demo>`__

Protocol Examples
-----------------

Protocol examples demonstrate media transport over the network, including HTTP download and real-time streaming or calls over WebRTC, RTSP, and RTMP.

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - Example
     - Description
     - Component Link
   * - pipeline_http_download_to_sdcard
     - Downloads a file from a URL to the SD card and prints overall download and write speed when finished.
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_http_download_to_sdcard>`__
   * - peer_demo
     - Establishes a connection between two ESP boards with ``esp_peer`` and implements a simple chat over dummy audio and a data channel.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/peer_demo>`__
   * - esp_peer_demo
     - Component-level ``esp_peer`` demo: two-role real-time communication with an audio stream and data-channel messages.
     - `esp_peer <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_peer/examples/peer_demo>`__
   * - whip_demo
     - Pushes device audio and video to a WHIP server.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/whip_demo>`__
   * - kvs_master
     - Runs as an Amazon KVS MASTER: receives an SDP offer from a browser VIEWER and returns an answer.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/kvs_master>`__
   * - kms_demo
     - Publishes WebRTC audio and video to a Kurento Media Server so a browser can watch through KMS.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/kms_demo>`__
   * - janus_demo
     - Publishes media as a Janus VideoRoom publisher using Janus HTTP signaling.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/janus_demo>`__
   * - doorbell_demo
     - Smart doorbell over AppRTC WebSocket signaling: remote control, live video, and two-way audio.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/doorbell_demo>`__
   * - doorbell_local
     - Local doorbell with HTTPS signaling hosted on the device. Includes live video, two-way audio, and optional pedestrian detection.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/doorbell_local>`__
   * - videocall_demo
     - Video call between two devices over a modified AppRTC signaling path. Media uses an ``esp_webrtc`` data channel.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/videocall_demo>`__
   * - webrtc_usb_camera
     - The browser sends a WebRTC stream; the host enumerates the device as a USB UVC camera.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/webrtc_usb_camera>`__
   * - local_jpeg_stream
     - Hosts HTTPS signaling on the device and sends camera JPEG to a browser over a WebRTC data channel, with two-way audio.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/local_jpeg_stream>`__
   * - rtsp_demo
     - After Wi-Fi connects, starts an RTSP server or pusher on the device for LAN streaming.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/rtsp_demo>`__
   * - rtmp_demo
     - Captures device audio and video and pushes them to an RTMP server through ``esp_media_protocols``.
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/rtmp_demo>`__

Check and Benchmark Examples
----------------------------

Check and benchmark examples verify recording quality, extract media frames, and benchmark components such as playlists.

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - Example
     - Description
     - Component Link
   * - esp_audio_analyzer_app
     - Captures MIC and loopback together, then saves to the SD card or uploads over Wi-Fi for hardware recording quality and acoustic checks.
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/checks/esp_audio_analyzer_app>`__
   * - extractor_test
     - Extracts audio and video frames from media files with ``esp_extractor`` to verify demux and frame extraction.
     - `esp_extractor <https://github.com/espressif/esp-adf-libs/tree/master/esp_extractor/examples/extractor_test>`__
   * - playlist_benchmark
     - Benchmarks ``esp_playlist`` operations on the SD card and prints average time per operation over UART.
     - `esp_playlist <https://github.com/espressif/esp-adf/tree/master/components/esp_playlist/examples/playlist_benchmark>`__

System Service Examples
-----------------------

System service examples demonstrate device-side system features, including low-power sleep, service orchestration, provisioning, configuration management, and OTA updates.

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - Example
     - Description
     - Component Link
   * - audio_power_save
     - Enters light sleep automatically after connecting, using MQTT keepalive. UART, MQTT, GPIO, or a timer can wake the device, and a sleep tone is played.
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/system/audio_power_save>`__
   * - services_hub
     - Orchestrates Wi-Fi, OTA, CLI, and button services through the event hub and service manager. MCP is optional.
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/services_hub>`__
   * - adf_event_hub_example
     - Connects five independent services through the event hub and prints the full event stream over UART. The same sources also build on PC.
     - `adf_event_hub <https://github.com/espressif/esp-adf/tree/master/components/adf_event_hub/examples>`__
   * - button_svc_example
     - Combines a single GPIO button and an ADC button group, and demonstrates full event subscription.
     - `esp_button_service <https://github.com/espressif/esp-adf/tree/master/components/esp_button_service/examples/button_svc_example>`__
   * - esp_cli_service_example
     - Controls the device from the serial console.
     - `esp_cli_service <https://github.com/espressif/esp-adf/tree/master/components/esp_cli_service/examples>`__
   * - mock_services
     - Six mock services demonstrate ``esp_service`` create, start, pause/resume, destroy, and event subscription.
     - `esp_service <https://github.com/espressif/esp-adf/tree/master/components/esp_service/examples/mock_services>`__
   * - wifi_service_example
     - Creates a Wi-Fi service, stores profiles encrypted, starts provisioning, and controls the connection from the CLI.
     - `esp_wifi_service <https://github.com/espressif/esp-adf/tree/master/components/esp_wifi_service/examples/wifi_service_example>`__
   * - config_manager_example
     - Saves multiple application config groups to persistent storage, demonstrating dual slots, default merge, and CRC checks.
     - `esp_config_manager <https://github.com/espressif/esp-adf/tree/master/components/esp_config_manager/examples/config_manager_example>`__
   * - ota_http
     - Pulls firmware over HTTP with manifest version checks, SHA-256 verification, and resumable download.
     - `esp_ota_service <https://github.com/espressif/esp-adf/tree/master/components/esp_ota_service/examples/ota_http>`__
   * - ota_fs
     - Performs offline OTA from an SD card or USB drive, including multi-partition batch upgrades.
     - `esp_ota_service <https://github.com/espressif/esp-adf/tree/master/components/esp_ota_service/examples/ota_fs>`__
   * - ota_ble
     - Pushes firmware over BLE GATT when Wi-Fi is unavailable, using the official ESP BLE OTA app.
     - `esp_ota_service <https://github.com/espressif/esp-adf/tree/master/components/esp_ota_service/examples/ota_ble>`__
