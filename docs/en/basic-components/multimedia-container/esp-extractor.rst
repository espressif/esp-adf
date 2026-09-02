ESP Extractor
=============

:link_to_translation:`zh_CN:[中文]`

ESP Extractor is a lightweight multimedia stream extraction component provided by Espressif for ESP series SoCs. It parses various media containers and extracts audio and video streams, providing a unified data interface for subsequent decoding, processing, or remuxing.

The component supports mainstream container formats such as MP4, TS, FLV, and WAV, provides a unified extractor interface, and supports automatic format detection, time-based seeking, track selection, and metadata parsing. Depending on requirements, developers can extract the audio stream, the video stream, or both, and can use a file, a memory buffer, or custom read and write callbacks as input, covering both local file and streaming media scenarios.

ESP Extractor is optimized for high performance on embedded platforms, using mechanisms such as data caching and memory pool management to reduce data copying and I/O overhead. It also supports features such as dynamic indexing, playback resumption, and custom extractor extensions, providing efficient, flexible media parsing capabilities while keeping resource usage low. It is suitable for multimedia applications such as media players, network streaming media, audio and video recording, and remuxing.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_extractor>`__
- `GitHub Repository <https://github.com/espressif/esp-adf-libs/tree/master/esp_extractor>`__
