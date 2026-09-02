ESP Video Codec
===============

:link_to_translation:`zh_CN:[中文]`

ESP Video Codec is a unified video codec framework provided by Espressif for ESP series SoCs. It abstracts away the differences between underlying hardware and software implementations, providing a consistent interface for various video encoders and decoders for capture, transmission, storage, and playback.

The component provides unified management of hardware and software codecs, along with a registration mechanism that lets you add or replace video encoders and decoders. Through capability query, developers can retrieve the pixel formats, buffer alignment requirements, and advanced features supported by a codec, enabling optimizations such as automatic capability negotiation and zero-copy to improve system performance and reduce data conversion overhead.

ESP Video Codec uses a modular design that lets you trim the codec library as needed through menuconfig, and it supports advanced video processing capabilities such as rotation, scaling, and cropping, as well as B-frame handling, a unified flush process, and cross-platform debugging. It provides flexible, efficient video codec capabilities while keeping resource usage low, and is suitable for smart cameras, video conferencing, AI vision, IPC, and other embedded video products.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_video_codec>`__
- `GitHub Repository <https://github.com/espressif/esp-adf-libs/tree/master/esp_video_codec>`__
