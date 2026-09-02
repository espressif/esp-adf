ESP Muxer
=========

:link_to_translation:`zh_CN:[中文]`

ESP Muxer is a multimedia muxing component provided by Espressif for ESP series SoCs. It packages audio and video data into standard media containers, or outputs streaming media data directly, and is suitable for recording, storage, network transmission, and similar scenarios.

The component supports synchronized muxing of audio and video data, is compatible with mainstream audio and video encoding formats, and supports multiple audio tracks, multiple video tracks, file segmentation, streaming output, and a custom write interface. Depending on requirements, developers can save data to a file system, or output it to the network, flash, or other custom storage media, adapting to different application scenarios.

ESP Muxer uses a modular design that balances high performance with low resource usage. It can serve as the data muxing module for applications such as audio and video capture, recording, live streaming, and media segmentation, and is suitable for multimedia products such as dashcams, network cameras, HLS segmentation services, and HTTP-FLV streaming.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_muxer>`__
- `GitHub Repository <https://github.com/espressif/esp-adf-libs/tree/master/esp_muxer>`__
