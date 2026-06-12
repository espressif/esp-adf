多媒体例程
================

:link_to_translation:`en:[English]`

本页按功能分类列出多媒体例程，覆盖播放、录音与采集、音频算法、显示与渲染、蓝牙音频、云端智能体、传输协议、检测与系统服务。

播放例程
------------------------------------------------------------

播放例程演示完整的媒体播放流程：从 SD 卡、Flash、HTTP、HLS、MIDI 等音源读取并解码，再输出到扬声器或屏幕，并提供触摸屏或串口命令行控制。

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程名称
     - 简介
     - 例程组件链接
   * - pipeline_play_sdcard_music
     - 从 SD 卡播放音乐，经解码和音效处理后输出，支持 MP3/WAV/FLAC/AAC 等。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_play_sdcard_music>`__
   * - pipeline_play_multi_source_music
     - 支持在 HTTP 与 SD 卡音源之间切换，并可插入 Flash 提示音；提示音结束后恢复原曲播放。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_play_multi_source_music>`__
   * - pipeline_loop_play_no_gap
     - 基于 GMF 任务策略实现 SD 卡多首音乐无缝循环，切换时无明显停顿。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_loop_play_no_gap>`__
   * - pipeline_play_embed_music
     - 播放嵌入固件的 Flash 音频，不依赖 SD 卡或网络，适用于开机提示音。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_play_embed_music>`__
   * - pipeline_play_http_music
     - 连接 Wi-Fi 后按 HTTP URL 拉流并解码播放。不支持 m3u8/HLS。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_play_http_music>`__
   * - audio_player
     - 基于 ``esp_player`` 实现完整音频播放：解复用、解码、播放控制，并可配置 2–4 路混音到同一 DAC。
     - `esp_player <https://github.com/espressif/esp-gmf/tree/main/packages/esp_player/examples/audio_player>`__
   * - video_player (esp_player)
     - 基于 ``esp_player`` 播放本地音视频：解复用、解码、A/V 同步，画面输出至 LCD，音频输出至扬声器。
     - `esp_player <https://github.com/espressif/esp-gmf/tree/main/packages/esp_player/examples/video_player>`__
   * - play_music_control
     - 通过串口 CLI 控制多源播放：SD 卡、HTTP/HTTPS、Flash 提示音，并支持播放列表。
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/player/play_music_control>`__
   * - music_player
     - 扫描 SD 卡中的 MP3/AAC/WAV，在触摸屏上显示歌曲列表和播放控件，支持单曲/列表/随机。
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/player/music_player>`__
   * - hls_live_stream
     - 解析 HLS（m3u8）直播流并解码播放，适用于网络电台和在线直播音频。
     - `esp_hls_stream <https://github.com/espressif/esp-adf-libs/tree/master/esp_hls_stream/examples/hls_live_stream>`__
   * - midi_play
     - 从 SD 卡播放 MIDI 文件。
     - `esp_midi <https://github.com/espressif/esp-adf-libs/tree/master/esp_midi/examples/play>`__
   * - midi_keyboard
     - 实时接收 MIDI 消息并合成发声，实现简易电子琴。
     - `esp_midi <https://github.com/espressif/esp-adf-libs/tree/master/esp_midi/examples/keyboard>`__
   * - play_embed_music
     - 板卡例程：直接播放固件内的 WAV，用于验证板卡音频输出。
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/play_embed_music>`__
   * - play_sdcard_music
     - 板卡例程：从 SD 卡播放 WAV。
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/play_sdcard_music>`__

录音与采集例程
------------------------------------------------------------

录音与采集例程演示麦克风与摄像头采集。采集数据可编码后写入 SD 卡、经 HTTP 上传，或在本地屏幕上预览。

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程名称
     - 简介
     - 例程组件链接
   * - pipeline_record_sdcard
     - 从麦克风录音并保存到 SD 卡，支持 PCM/MP3/AAC 等，可启用低功耗录音。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_record_sdcard>`__
   * - pipeline_record_http
     - 麦克风采集并编码后，通过 HTTP 上传到指定服务器，适用于云端录音或语音识别前端。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_record_http>`__
   * - pipeline_record_audio_muxer
     - 录音编码后经 muxer 封装为 MP4/TS 等容器再写入 SD，而非仅保存裸音频流。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_record_audio_muxer>`__
   * - aec_rec
     - 一路播放 SD 卡 MP3，另一路麦克风经 AEC 后编码保存，用于对比回声消除效果。
     - `gmf_ai_audio <https://github.com/espressif/esp-gmf/tree/main/elements/gmf_ai_audio/examples/aec_rec>`__
   * - audio_capture
     - 基于 ``esp_capture`` 实现音频采集，覆盖基础录音、AEC、写文件和自定义处理，无需自行搭建处理链。
     - `esp_capture <https://github.com/espressif/esp-gmf/tree/main/packages/esp_capture/examples/audio_capture>`__
   * - video_capture
     - 基于 ``esp_capture`` 实现视频采集：单帧、录像、叠加、双路，并可保存到 SD。
     - `esp_capture <https://github.com/espressif/esp-gmf/tree/main/packages/esp_capture/examples/video_capture>`__
   * - av_record_live_display
     - 采集摄像头和麦克风数据，在 LCD 上持续预览，同时将音视频录制为 MP4 写入 SD。
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/recorder/av_record_live_display>`__
   * - live_photo_capture
     - 采集短视频并自动提取封面帧，封装为 Motion Photo 风格文件后写入 SD。
     - `esp_live_photo <https://github.com/espressif/esp-adf/tree/master/components/esp_live_photo/examples/live_photo_capture>`__
   * - video_muxer
     - 基于 ESP Muxer 将摄像头与麦克风两路流混流后保存到 SD。
     - `esp_muxer <https://github.com/espressif/esp-adf-libs/tree/master/esp_muxer/examples/video_muxer>`__
   * - record_to_sdcard
     - 板卡例程：将麦克风录音保存到 SD 卡。
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/record_to_sdcard>`__
   * - record_and_play
     - 板卡例程：麦克风采集的音频实时从扬声器输出，用于验证音频通路。
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/record_and_play>`__

音频算法例程
------------------------------------------------------------

音频算法例程演示常见音频处理能力，包括音效、啸叫抑制、唤醒与命令词、采样率转换和频谱分析。

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程名称
     - 简介
     - 例程组件链接
   * - pipeline_audio_effects
     - 基于多条 GMF 处理链演示重采样、Sonic 变速变调、至少 4 路混音、EQ 和 DRC。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_audio_effects>`__
   * - pipeline_howl
     - 卡拉 OK 三路处理链：SD 伴奏与麦克风分别处理后，经啸叫抑制再混音，从 DAC 输出。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_howl>`__
   * - wwe (唤醒与命令词)
     - 基于 AFE 实现唤醒词、VAD 和命令词识别，可离线运行。
     - `gmf_ai_audio <https://github.com/espressif/esp-gmf/tree/main/elements/gmf_ai_audio/examples/wwe>`__
   * - asrc_demo
     - 基于 ASRC 进行采样率、声道数和位深转换，将录音或播放格式对齐到目标规格。
     - `esp_asrc <https://github.com/espressif/esp-gmf/tree/main/packages/esp_asrc/examples/asrc_demo>`__
   * - fft_spectrum_print
     - 生成多频余弦信号，执行 512 点 Q15 实数 FFT，经串口打印频谱柱状图，再通过 IFFT 校验往返误差。
     - `gmf_fft <https://github.com/espressif/esp-gmf/tree/main/packages/gmf_fft/examples/fft_spectrum_print>`__
   * - ae_howl
     - 麦克风经 ``esp_ae_howl`` 抑制啸叫后，与 SD 卡背景音乐混音播放。
     - `esp_audio_effects <https://github.com/espressif/esp-adf-libs/tree/master/esp_audio_effects/example/ae_howl>`__
   * - esp_audio_effects_demo
     - 逐项演示 ALC、Fade、EQ、Sonic、DRC、MBC、Mixer 和基本格式转换。
     - `esp_audio_effects <https://github.com/espressif/esp-adf-libs/tree/master/esp_audio_effects/example/esp_audio_effects_demo>`__

显示与渲染例程
------------------------------------------------------------

显示与渲染例程演示如何把视频帧和 PCM 音频送到 LCD 与 DAC，覆盖双目同步显示、画面叠加、多路混音，以及板卡 LVGL 界面验证。

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程名称
     - 简介
     - 例程组件链接
   * - dual_eyes
     - 同步渲染左右眼画面，支持单屏并排或双屏输出。
     - `esp_video_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_video_render/examples/dual_eyes>`__
   * - video_player (esp_video_render)
     - 基于 ``esp_video_render`` 实现轻量本地播放器：从 SD 提取、解码、渲染，并提供 overlay UI。
     - `esp_video_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_video_render/examples/video_player>`__
   * - video_render
     - 读取 SD 卡 MJPEG，演示单流/多流渲染、缩放和进度条 overlay。
     - `esp_video_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_video_render/examples/video_render>`__
   * - audio_render
     - 基于 ``esp_audio_render`` 实现单路 PCM 播放和多路混音，也可播放网络解码后的音频。
     - `esp_audio_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_audio_render/examples/audio_render>`__
   * - simple_piano
     - 基于 ``esp_audio_render`` 实现复音钢琴：多轨同时发声并实时渲染到 DAC。
     - `esp_audio_render <https://github.com/espressif/esp-gmf/tree/main/packages/esp_audio_render/examples/simple_piano>`__
   * - display_lvgl
     - 基于 board manager 初始化屏幕，并运行 LVGL 测试界面，用于验证显示和触摸。
     - `esp_board_manager <https://github.com/espressif/esp-board-manager/tree/main/esp_board_manager/examples/display_lvgl>`__
   * - av_render_test
     - 采集摄像头和麦克风数据，经 LCD 与 I2S 同步输出，用于验证音视频渲染链路。
     - `av_render <https://github.com/espressif/esp-webrtc-solution/tree/main/components/av_render/examples/render_test>`__

蓝牙音频例程
------------------------------------------------------------

蓝牙音频例程演示经典蓝牙音频能力，覆盖 A2DP 播放、HFP 通话和 AVRCP 控制，并可按需启用 LE Audio。

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程名称
     - 简介
     - 例程组件链接
   * - bt_audio
     - 同一工程覆盖 A2DP Source/Sink、HFP 通话和 AVRCP 控制；可选启用 LE Audio、BIS 和 LVGL 触屏 UI。
     - `esp_bt_audio <https://github.com/espressif/esp-gmf/tree/main/packages/esp_bt_audio/examples/bt_audio>`__

云端智能体例程
------------------------------------------------------------

云端智能体例程演示设备与云端语音智能体的对接，覆盖扣子、百度 RTC 和 OpenAI Realtime 的实时语音交互。

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程名称
     - 简介
     - 例程组件链接
   * - coze_ws_app
     - 对接扣子 WebSocket OpenAPI，支持直接对话、唤醒和按键打断的双向流式语音交互。
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/coze_ws_app>`__
   * - voice_assistant_app
     - 基于百度 RTC 实现语音助手：支持对话、播放网络音乐，并将两路音频混音输出。
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/baidu_rtc/solutions/voice_assistant_app>`__
   * - openai_demo
     - 基于 ``esp_webrtc`` 连接 OpenAI Realtime，实现实时语音对话，并演示 function call 控制设备。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/openai_demo>`__

传输协议例程
------------------------------------------------------------

传输协议例程演示媒体在网络上的传输，包括 HTTP 下载，以及基于 WebRTC、RTSP、RTMP 的实时推流与通话。

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程名称
     - 简介
     - 例程组件链接
   * - pipeline_http_download_to_sdcard
     - 按 URL 将文件下载到 SD 卡，完成后打印下载和写入的整体速度。
     - `gmf_examples <https://github.com/espressif/esp-gmf/tree/main/gmf_examples/basic_examples/pipeline_http_download_to_sdcard>`__
   * - peer_demo
     - 两块 ESP 板基于 ``esp_peer`` 建立连接，通过模拟音频和数据通道实现简易聊天。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/peer_demo>`__
   * - esp_peer_demo
     - ``esp_peer`` 组件级演示：双角色实时通信，包含音频流和数据通道消息。
     - `esp_peer <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_peer/examples/peer_demo>`__
   * - whip_demo
     - 基于 WHIP 将设备音视频推流到 WHIP 服务器。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/whip_demo>`__
   * - kvs_master
     - ESP 作为 Amazon KVS MASTER，接收浏览器 VIEWER 的 SDP offer 并回传 answer。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/kvs_master>`__
   * - kms_demo
     - 向 Kurento Media Server 发布 WebRTC 音视频，浏览器经 KMS 观看。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/kms_demo>`__
   * - janus_demo
     - 作为 Janus VideoRoom publisher，经 Janus HTTP 信令发布媒体流。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/janus_demo>`__
   * - doorbell_demo
     - 基于 AppRTC WebSocket 信令的智能门铃：远程控制、实时视频、双向音频。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/doorbell_demo>`__
   * - doorbell_local
     - 设备自建 HTTPS 信令的本地门铃，包含实时视频、双向音频，可选行人检测。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/doorbell_local>`__
   * - videocall_demo
     - 两台设备经改造版 AppRTC 信令实现视频通话，媒体经 ``esp_webrtc`` 数据通道传输。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/videocall_demo>`__
   * - webrtc_usb_camera
     - 浏览器经 WebRTC 送流，主机侧将设备识别为 USB UVC 摄像头。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/webrtc_usb_camera>`__
   * - local_jpeg_stream
     - 设备自建 HTTPS 信令，将摄像头 JPEG 经 WebRTC 数据通道推送到浏览器，并支持双向音频。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/local_jpeg_stream>`__
   * - rtsp_demo
     - 连接 Wi-Fi 后在设备上启动 RTSP 服务端或推流端，供局域网拉流。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/rtsp_demo>`__
   * - rtmp_demo
     - 采集设备音视频，经 ``esp_media_protocols`` 推流到 RTMP 服务器。
     - `esp-webrtc-solution <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/rtmp_demo>`__

检测与压测例程
------------------------------------------------------------

检测与压测例程用于检查录音质量、验证媒体抽帧，以及对播放列表等组件做性能压测。

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程名称
     - 简介
     - 例程组件链接
   * - esp_audio_analyzer_app
     - 同时采集 MIC 和回采，保存到 SD 或经 Wi-Fi 上传，用于硬件录音质量和声学检查。
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/checks/esp_audio_analyzer_app>`__
   * - extractor_test
     - 基于 ``esp_extractor`` 从媒体文件中提取音视频帧，用于验证解复用和抽帧。
     - `esp_extractor <https://github.com/espressif/esp-adf-libs/tree/master/esp_extractor/examples/extractor_test>`__
   * - playlist_benchmark
     - 在 SD 卡上对 ``esp_playlist`` 各操作进行性能压测，经串口打印单次平均耗时。
     - `esp_playlist <https://github.com/espressif/esp-adf/tree/master/components/esp_playlist/examples/playlist_benchmark>`__

系统服务例程
------------------------------------------------------------

系统服务例程演示设备侧系统能力，包括低功耗休眠、服务编排、配网、配置管理和 OTA 升级。

.. list-table::
   :header-rows: 1
   :widths: 28 52 20

   * - 例程名称
     - 简介
     - 例程组件链接
   * - audio_power_save
     - 联网后通过 MQTT keepalive 自动进入 light sleep，可由 UART/MQTT/GPIO/定时器唤醒，并播放休眠提示音。
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/system/audio_power_save>`__
   * - services_hub
     - 基于 event hub 和 service manager 编排 Wi-Fi、OTA、CLI 与按键服务，可选启用 MCP。
     - `adf_examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/services_hub>`__
   * - adf_event_hub_example
     - 五个独立服务经 event hub 互联，经串口打印完整事件流；同一份源码也可在 PC 上构建。
     - `adf_event_hub <https://github.com/espressif/esp-adf/tree/master/components/adf_event_hub/examples>`__
   * - button_svc_example
     - 将单 GPIO 按键和 ADC 多按键组一并接入，演示完整事件订阅。
     - `esp_button_service <https://github.com/espressif/esp-adf/tree/master/components/esp_button_service/examples/button_svc_example>`__
   * - esp_cli_service_example
     - 通过串口命令控制设备。
     - `esp_cli_service <https://github.com/espressif/esp-adf/tree/master/components/esp_cli_service/examples>`__
   * - mock_services
     - 六个 mock 服务演示 ``esp_service`` 的创建、启动、暂停/恢复、销毁和事件订阅。
     - `esp_service <https://github.com/espressif/esp-adf/tree/master/components/esp_service/examples/mock_services>`__
   * - wifi_service_example
     - 创建 Wi-Fi service，加密保存 profile，启动配网，并通过 CLI 控制连接。
     - `esp_wifi_service <https://github.com/espressif/esp-adf/tree/master/components/esp_wifi_service/examples/wifi_service_example>`__
   * - config_manager_example
     - 将多组应用配置保存到持久化介质，演示双槽、默认值合并和 CRC 校验。
     - `esp_config_manager <https://github.com/espressif/esp-adf/tree/master/components/esp_config_manager/examples/config_manager_example>`__
   * - ota_http
     - 通过 HTTP 拉取固件：清单版本检查、SHA-256 校验、断点续传。
     - `esp_ota_service <https://github.com/espressif/esp-adf/tree/master/components/esp_ota_service/examples/ota_http>`__
   * - ota_fs
     - 基于 SD 卡或 U 盘进行离线 OTA，支持多分区批量升级。
     - `esp_ota_service <https://github.com/espressif/esp-adf/tree/master/components/esp_ota_service/examples/ota_fs>`__
   * - ota_ble
     - 无 Wi-Fi 时通过 BLE GATT 推送固件，配合官方 ESP BLE OTA APP。
     - `esp_ota_service <https://github.com/espressif/esp-adf/tree/master/components/esp_ota_service/examples/ota_ble>`__
