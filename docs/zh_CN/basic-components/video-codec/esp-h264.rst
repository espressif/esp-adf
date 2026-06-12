ESP H264
========

:link_to_translation:`en:[English]`

ESP H.264 是乐鑫为 ESP 系列 SoC 提供的轻量级 H.264 编码与解码组件，集成硬件和软件两种实现，为嵌入式视频采集、传输和播放提供高效的视频编解码能力。

组件支持硬件编码器与软件编解码器统一接口。其中，硬件编码器针对 ESP32-P4 深度优化，可实现 1080P 30fps 以上的实时编码；软件编码器基于 OpenH264，软件解码器基于 TinyH264，并针对 ESP 平台进行了内存占用和 CPU 性能优化，在资源受限的设备上也能提供稳定、高效的编解码能力。

ESP H.264 采用轻量化、模块化设计，可根据目标平台灵活选择硬件或软件方案，适用于 IPC、视频对讲、智能门铃、实时视频传输、录像存储等各类嵌入式视频应用。

相关链接：

- `组件管理器 <https://components.espressif.com/components/espressif/esp_h264>`__
- `GitHub 仓库 <https://github.com/espressif/esp-h264-component/tree/master/esp_h264>`__
- `技术博客 <https://developer.espressif.com/blog/2025/07/esp-h264-use-tips>`__
