ESP H264
========

:link_to_translation:`zh_CN:[中文]`

ESP H.264 is a lightweight H.264 encoding and decoding component provided by Espressif for ESP series SoCs. It integrates both hardware and software implementations, providing efficient video codec capabilities for embedded video capture, transmission, and playback.

The component provides a unified interface for the hardware encoder and the software codec. The hardware encoder is deeply optimized for ESP32-P4 and can achieve real-time encoding above 1080p 30 fps; the software encoder is based on OpenH264 and the software decoder is based on TinyH264, both optimized for memory usage and CPU performance on ESP platforms, providing stable and efficient codec capabilities even on resource-constrained devices.

ESP H.264 uses a lightweight, modular design that lets you choose a hardware or software implementation depending on the target platform. It is suitable for IPC, video intercom, smart doorbells, real-time video transmission, video recording and storage, and other embedded video applications.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_h264>`__
- `GitHub Repository <https://github.com/espressif/esp-h264-component/tree/master/esp_h264>`__
- `Tech Blog <https://developer.espressif.com/blog/2025/07/esp-h264-use-tips>`__
