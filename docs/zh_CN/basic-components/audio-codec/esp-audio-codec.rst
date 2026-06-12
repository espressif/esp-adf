ESP Audio Codec
===============

:link_to_translation:`en:[English]`

ESP Audio Codec 是乐鑫为 ESP 系列 SoC 提供的官方音频编解码组件，提供统一的音频编码、解码及音频流解析能力，用于集成各类音频格式处理。

组件采用统一的编码与解码接口，支持 AAC、MP3、OPUS、AMR、FLAC、ALAC、G711、SBC、LC3、G722 等主流音频格式，并支持同时创建多个编解码实例。开发者也可以通过注册机制扩展或替换默认编解码器，无需修改上层应用即可完成定制。

针对不同的数据输入方式，ESP Audio Codec 提供两种解码模式：ESP Audio Decoder 用于处理已按音频帧对齐的数据；ESP Audio Simple Decoder 内置音频格式解析器，可自动完成数据聚合、帧边界检测和帧提取，支持直接输入任意长度的数据流，进一步简化文件和网络流媒体的解码流程。

整个组件针对嵌入式平台进行了轻量化设计和性能优化，核心算法经过汇编优化，在保证低内存占用的同时提供较高的编解码性能，适用于音频播放、录音、语音通信、流媒体等各类音频应用场景。

相关链接：

- `组件管理器 <https://components.espressif.com/components/espressif/esp_audio_codec>`__
- `GitHub 仓库 <https://github.com/espressif/esp-adf-libs/tree/master/esp_audio_codec>`__
