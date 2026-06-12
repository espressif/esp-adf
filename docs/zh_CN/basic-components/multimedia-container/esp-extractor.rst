ESP Extractor
=========================

:link_to_translation:`en:[English]`

ESP Extractor 是乐鑫为 ESP 系列 SoC 提供的轻量级多媒体流提取组件，用于解析各类媒体容器并提取音频、视频流，为后续解码、处理或重新封装提供统一的数据接口。

组件支持 MP4、TS、FLV、WAV 等主流容器格式，提供统一的解封装接口，并支持自动格式检测、时间定位、轨道选择、元数据解析等能力。开发者可以根据需求选择提取音频流、视频流或两者，并支持文件、内存缓冲区及自定义读写等多种输入方式，满足本地文件和流媒体场景的需求。

ESP Extractor 针对嵌入式平台进行了高性能优化，采用数据缓存、内存池管理等机制减少数据拷贝和读写开销，同时支持动态索引、播放恢复、自定义解封装器扩展等特性，在保持较低资源占用的同时提供高效、灵活的媒体解析能力，适用于媒体播放器、网络流媒体、音视频录制及转封装等多媒体应用。

相关链接：

- `组件管理器 <https://components.espressif.com/components/espressif/esp_extractor>`__
- `GitHub 仓库 <https://github.com/espressif/esp-adf-libs/tree/master/esp_extractor>`__
