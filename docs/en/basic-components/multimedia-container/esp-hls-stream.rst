ESP HLS Stream
==============

:link_to_translation:`zh_CN:[中文]`

ESP HLS Stream provides HTTP Live Streaming (HLS) on ESP devices. It parses m3u8 playlists, fetches media segments, and can handle AES-128 and SAMPLE-AES encryption. It can be used with the extractor component or a GMF IO, for internet radio, live audio, and segmented audio and video playback.

The component provides two integration paths. The extractor path registers HLS as an extractor, then opens the stream, parses it, and reads compressed audio and video access units. That path covers full demuxing and SAMPLE-AES; dependent extractors such as TS must be registered first. The GMF external-interface path attaches HLS as a reader on a processing chain and outputs a continuous byte stream. That path is for audio streaming, does not preserve frame boundaries, and does not support SAMPLE-AES.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_hls_stream>`__
- `GitHub Repository <https://github.com/espressif/esp-adf-libs/tree/master/esp_hls_stream>`__
- `hls_live_stream example <https://github.com/espressif/esp-adf-libs/tree/master/esp_hls_stream/examples/hls_live_stream>`__
