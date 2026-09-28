# baidu_rtc

Baidu RTC (BRTC) agent SDK for ESP-ADF. Provides prebuilt libraries and headers for Baidu real-time chat agents on Espressif chips.

## Features

- Chat agent APIs (`baidu_chat_agents_engine.h`)
- Optional video-call client APIs (`baidu_rtc_client.h`)
- Prebuilt `libbrtc.a` for `esp32s3`, `esp32s31`, and `esp32p4`

## Add Component to Your Project

Please use the component manager command `add-dependency` to add the `baidu_rtc` component to your project's `idf_component.yml` dependency file.

```bash
idf.py add-dependency "espressif/baidu_rtc^1.0.0"
```

Or edit `main/idf_component.yml` manually:

```yaml
dependencies:
  espressif/baidu_rtc: "^1.0.0"
```

Then add `PRIV_REQUIRES baidu_rtc` in your component `CMakeLists.txt`.

## Docs

- SDK notes from Baidu: [BRTC_SDK_NOTES.md](./BRTC_SDK_NOTES.md)
- License: [LICENSE](./LICENSE) (MIT)
