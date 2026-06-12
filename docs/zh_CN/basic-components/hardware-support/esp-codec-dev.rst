ESP Codec Device
================

:link_to_translation:`en:[English]`

ESP-Codec-Dev 是面向音频编解码器的设备驱动与抽象组件，为上层应用提供统一、可扩展的音频设备访问接口。组件内置多种常用编解码器驱动，并支持多实例管理、自定义编解码器接入以及跨平台适配，可屏蔽不同硬件之间的实现差异，降低应用与底层驱动的耦合。除播放、录音和设备控制外，组件还支持内部 ADC 麦克风采集、仅功放扬声器、软件音量调节及自定义音量曲线，覆盖从简单音频终端到多编解码器系统的需求。

相关链接：

- `组件管理器 <https://components.espressif.com/components/espressif/esp_codec_dev>`__
- `GitHub 仓库 <https://github.com/espressif/esp-audio-dev>`__
