ESP HLS Stream
==============

:link_to_translation:`en:[English]`

ESP HLS Stream 在 ESP 平台上提供 HTTP Live Streaming（HLS）支持。组件解析 m3u8 播放列表、拉取媒体分段，并可处理 AES-128 与 SAMPLE-AES 加密。可与解封装组件或 GMF 外部接口一起使用，用于网络电台、直播音频和按段拉流的音视频播放。

组件提供两条接入路径。解封装路径将 HLS 注册为解封装器后，按打开、解析、读帧的顺序读取压缩音视频访问单元，覆盖完整解复用与 SAMPLE-AES；使用前需同时注册 TS 等依赖的解封装器。GMF 外部接口路径将 HLS 作为读端接入处理链，按连续字节流输出，用于音频拉流，不保证帧边界，也不支持 SAMPLE-AES。

相关链接：

- `组件管理器 <https://components.espressif.com/components/espressif/esp_hls_stream>`__
- `GitHub 仓库 <https://github.com/espressif/esp-adf-libs/tree/master/esp_hls_stream>`__
- `hls_live_stream 例程 <https://github.com/espressif/esp-adf-libs/tree/master/esp_hls_stream/examples/hls_live_stream>`__
