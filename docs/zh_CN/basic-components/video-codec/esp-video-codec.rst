ESP Video Codec
===============

:link_to_translation:`en:[English]`

ESP Video Codec 是乐鑫为 ESP 系列 SoC 提供的统一视频编解码框架，屏蔽底层硬件与软件实现差异，为各类视频编码器和解码器提供一致的接口，用于构建视频采集、传输、存储及播放应用。

组件支持硬件和软件编解码器的统一管理，并提供灵活的注册机制，支持新增或替换视频编码器、解码器。通过能力查询，开发者可获取编解码器支持的像素格式、缓冲区对齐要求及高级特性，实现自动能力协商和零拷贝等优化，提高系统性能并减少数据转换开销。

ESP Video Codec 采用模块化设计，可通过 menuconfig 按需裁剪编解码库，并支持旋转、缩放、裁剪等高级视频处理能力，以及 B 帧处理、统一刷新流程和跨平台调试等特性，在保证低资源占用的同时提供灵活、高效的视频编解码能力，适用于智能摄像机、视频会议、AI 视觉、IPC 及各类嵌入式视频产品。

相关链接：

- `组件管理器 <https://components.espressif.com/components/espressif/esp_video_codec>`__
- `GitHub 仓库 <https://github.com/espressif/esp-adf-libs/tree/master/esp_video_codec>`__
