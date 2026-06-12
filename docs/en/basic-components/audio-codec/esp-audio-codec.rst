ESP Audio Codec
===============

:link_to_translation:`zh_CN:[中文]`

ESP Audio Codec is Espressif's official audio codec component for ESP series SoCs. It provides unified audio encoding, decoding, and audio stream parsing for integrating audio format processing.

The component uses a unified encoding and decoding interface and supports mainstream audio formats including AAC, MP3, OPUS, AMR, FLAC, ALAC, G711, SBC, LC3, and G722, and supports creating multiple codec instances at the same time. Developers can also extend or replace the default codecs through a registration mechanism, without modifying the upper-layer application.

For different data input methods, ESP Audio Codec provides two decoding modes: ESP Audio Decoder, used for data that is already aligned to audio frames; and ESP Audio Simple Decoder, which has a built-in audio format parser that automatically performs data aggregation, frame boundary detection, and frame extraction, and can accept a data stream of arbitrary length directly, further simplifying decoding for files and network streaming media.

The component is designed to be lightweight and optimized for embedded platforms; its core algorithms are optimized in assembly, delivering higher codec performance while keeping memory usage low, and it is suitable for audio playback, recording, voice communication, streaming media, and other audio application scenarios.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_audio_codec>`__
- `GitHub Repository <https://github.com/espressif/esp-adf-libs/tree/master/esp_audio_codec>`__
