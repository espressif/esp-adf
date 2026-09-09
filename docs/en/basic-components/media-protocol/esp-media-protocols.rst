ESP Media Protocols
===================

:link_to_translation:`zh_CN:[中文]`

ESP Media Protocols is an official media protocol component provided by Espressif for ESP series SoCs. It provides a unified implementation of mainstream streaming media transport, real-time communication, and device interconnection protocols for building networked multimedia applications.

The component integrates common media protocols such as SIP, RTSP, RTMP, MRM, and UPnP, covering typical application scenarios such as VoIP communication, audio and video streaming media, device discovery, and multi-device audio synchronization. Each protocol uses a unified, componentized design, letting you select and combine protocols based on project requirements to reduce protocol integration and maintenance costs.

ESP Media Protocols is optimized for embedded devices, providing a complete protocol stack implementation and an easily extensible interface while maintaining stability and low resource usage. It is suitable for networked multimedia products such as smart speakers, IPC, streaming media players, smart home devices, and multi-room audio systems.

The stack implements SIP, RTSP, and RTMP. Applications use these protocols through ``esp_sip_service``, ``esp_rtsp_service``, and ``esp_rtmp_service``; see the media service examples below. ``rtsp_demo`` / ``rtmp_demo`` in ``esp-webrtc-solution`` call this component's API directly.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_media_protocols>`__
- `GitHub Repository <https://github.com/espressif/esp-adf-libs/tree/master/esp_media_protocols>`__
- `sip_cli <https://github.com/espressif/esp-adf/tree/master/adf_examples/protocols/sip_cli>`__
- `rtsp_cli <https://github.com/espressif/esp-adf/tree/master/adf_examples/protocols/rtsp_cli>`__
- `rtsp_push <https://github.com/espressif/esp-adf/tree/master/adf_examples/protocols/rtsp_push>`__
- `rtmp_cli <https://github.com/espressif/esp-adf/tree/master/adf_examples/protocols/rtmp_cli>`__
