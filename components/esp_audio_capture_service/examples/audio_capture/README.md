# Audio Record Example

- [Chinese Version](./README_CN.md)

- Regular Example: ![alt text](../../../../docs/_static/level_regular.png "Regular Example") - demonstrates `esp_audio_capture_service` streaming, storage, dual-stream, and AI audio capture

## Example Brief

- This example shows how to record audio with `esp_audio_capture_service` on a board-manager audio ADC device. For common usage, copy from `main/simple_record.c` (stream / direct / AI / storage). For dual-stream, auto-record, full AI matrix, and verify playback, see `main/audio_record_cases.c`.
- Technically, it demonstrates create → optional AI feature setup → `apply_setup` → `esp_service_start` / stop, `esp_media_service_link()` to a test sink, direct `acquire_frame` / `release_frame` consumption, and muxer-based recording to an SD card.

### Prerequisites

- Familiarity with [`esp_audio_capture_service`](../../README.md) and [`esp_capture_service`](../../../esp_capture_service/README.md)
- A board definition supported by `esp_board_manager` that provides `audio_adc`
- Optional: SD card for storage cases; `audio_dac` for AEC reference playback and recorded-file verification

### Folder Contents

```text
audio_record/
├── assets/                 Embedded AAC used as AEC reference / verify playback source
├── main/
│   ├── app_main.c          Board init, CLI registration
│   ├── simple_record.c     Copy-ready common usage (stream / direct / AI / storage)
│   ├── audio_record_cases.c Complex case matrix (dual / auto-record / full AI)
│   ├── audio_record_player.c DAC reference / verify player
│   ├── audio_record_scheduler.c Capture thread scheduler bridge
│   ├── settings.h          Sample rate, codecs, storage paths
│   └── Kconfig.projbuild   AUDIO_RECORD_ENABLE_VERIFY
├── partitions.csv          8 MB layout for AI models when enabled
├── sdkconfig.defaults
└── README.md
```

## Environment Setup

### Hardware Required

- An ESP development board with board-manager support for `audio_adc`
- Recommended: ESP32-P4 function EV board (or another board with matching board-manager definition)
- microSD card for storage / auto-record / AI dump cases
- Optional speaker / headphone path through `audio_dac` for AEC reference and verify playback

### Additional Requirements

- Flash size of at least 8 MB when AI models are enabled (`partitions.csv`)
- For AEC cases, the board must route DAC playback into the ADC echo / reference channel

## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.5 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Generate board-manager configuration for your board, then optionally tune:

```text
Audio record example > Enable recorded-file playback verification
ESP Audio Capture Service > Enable AI audio source / feature supports
Component config > FAT Filesystem support > Long filename support
```

Key defaults are already set in `sdkconfig.defaults` (SPIRAM, FatFS LFN, AI source features, codecs used by `settings.h`).

### Build and Flash

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

For full steps to configure and build an ESP-IDF project, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

## How to Use the Example

### Recommended Reading Order

1. Start with **`main/simple_record.c`**. These walkthroughs are intentionally self-contained and written as **copy-ready usage code** for the most common `esp_audio_capture_service` flows.
2. Run the matching `simple ...` console commands to verify the same scenarios on hardware.
3. For dual-stream, auto-record mixes, full AI feature matrix, and verify playback, open **`main/audio_record_cases.c`**.

### Common Usage (`simple_record.c`)

| User scenario | Function | Console command |
| --- | --- | --- |
| Link capture to a sink and consume streamed frames | `simple_record_stream()` | `simple stream [duration_ms]` |
| Pull frames directly with `acquire_frame` / `release_frame` | `simple_record_direct()` | `simple direct [duration_ms]` |
| AEC + VAD processed PCM with direct pull | `simple_record_ai_direct()` | `simple ai_direct [duration_ms]` |
| Storage-only recording to MP4 (no frame pull) | `simple_record_storage()` | `simple storage [duration_ms]` |

### Complex Cases (`audio_record_cases.c`)

Use the full case matrix when you need more combinations than the simple walkthroughs:

| User scenario | Case name |
| --- | --- |
| Single AAC live stream | `normal_stream` |
| Manual MP4 recording | `normal_storage` |
| Auto-record into SD-card directory | `normal_auto_storage` |
| Stream while recording MP4 | `normal_stream_storage` |
| Dual G711A + AAC live streams | `normal_dual_stream` |
| Dual streams with storage on AAC only | `normal_dual_mixed` |
| AEC + NS PCM stream | `ai_afe_stream` |
| AEC + NS PCM recorded to WAV | `ai_afe_storage` |
| AEC-only / WakeNet-only / VAD-only / DOA-only | `ai_aec`, `ai_wn`, `ai_vad`, `ai_doa` |
| Same AI features with WAV storage | `ai_*_storage` |
| Full AI stack (AEC + NS + WN + VAD + DOA) | `ai_all` |
| AI source feeding AAC + G711A with storage | `ai_dual` |

### Example Functionality

After boot, the serial console prompt is `audio-record>`.

`record` cases run in a background task so the CLI stays responsive. Use `i` to print memory / system status while a background record is running.

```text
cases
simple stream 10000
simple direct 10000
simple ai_direct 10000
simple storage 10000
record normal_stream 10000
record normal_storage 10000 verify
record ai_all 10000
run_all 5000
run_all 5000 with_trace
i
```

Notes:

- `duration_ms` defaults to 10000.
- Add `verify` or `1` to play recorded files after capture when `CONFIG_AUDIO_RECORD_ENABLE_VERIFY` is enabled.
- `i` prints GMF memory and FreeRTOS task CPU stats.
- Heap leak tracing is only for `run_all ... with_trace`: run once without `with_trace` to settle always-held allocations, then again with `with_trace` to dump residuals.
- Normal cases use the codec-device source. AI cases call `esp_capture_service_ai_audio_src_set_feature()` before `apply_setup()`, which selects the AI source.
- Cases that include AEC loop `assets/music.aac` through the board DAC as a stereo reference.
- Stream statistics after each case come from `esp_media_dummy_service` through `esp_media_service_link()`.
- Storage-only cases disable the provider track while leaving the muxer track active.
- AI storage cases can dump unprocessed source PCM to `/sdcard/audio_record/src.pcm` for comparison with the processed recording.

### Example Log

```text
I (xxx) AUDIO_RECORD: Audio record example is ready
I (xxx) AUDIO_RECORD: Type 'cases' to list examples, or 'record normal_stream 10000'
audio-record> record normal_stream 3000
I (xxx) RECORD_CASE: Running case 'normal_stream'
I (xxx) TEST_SINK: stream0 frames=... bytes=...
I (xxx) RECORD_CASE: Case 'normal_stream' finished
```

### References

- Component README: [esp_audio_capture_service](../../README.md)
- Core capture service: [esp_capture_service](../../../esp_capture_service/README.md)

## Troubleshooting

- **SD card unavailable / storage cases fail**: confirm FatFS mount and that `/sdcard/audio_record` can be created.
- **AEC effect weak or missing**: ensure DAC init succeeds and the board routes playback into the ADC reference channel.
- **AI feature returns `ESP_ERR_NOT_SUPPORTED`**: enable the matching `ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_*` Kconfig options and rebuild.
- **Verify playback skipped**: enable `AUDIO_RECORD_ENABLE_VERIFY` and ensure `audio_dac` is present.
- **Flash / model partition errors**: use the provided `partitions.csv` and an 8 MB (or larger) flash part.
- **Background record still running**: wait for the previous `record` job to finish, or check status with `i`.

## MCP Operation Guide

This example can expose audio capture, media link/unlink, and dummy-sink stats over UART MCP for PC-side verification. Data frames never go through MCP; they stay in C via `esp_media_service_link()`.

### 1. Enable component MCP options

In `menuconfig` (or rely on `sdkconfig.defaults`):

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP Audio Capture Service → Enable audio capture service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media sink service
```

Example UART pin options appear under `Audio record example → MCP UART pins` when audio MCP and UART transport are enabled. Defaults match the compiled values:

- UART port: `UART_NUM_1`
- TX GPIO: `17` (connect to USB-UART adapter RX)
- RX GPIO: `18` (connect to USB-UART adapter TX)
- Baud: `115200`

### 2. Build, flash, and keep the board running

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

On boot the example creates one real audio capture service (`audio-rec`) and one `media_dummy_sink`, registers MCP tools, and starts the UART MCP server. The normal `audio-record>` console remains available on the IDF console UART.

### 3. Run the PC UART script

Use a second USB-UART adapter wired to the MCP pins:

```bash
python3 scripts/test_audio_capture_mcp_uart.py /dev/ttyUSB1 115200
```

Typical coverage:

1. `tools/list`
2. `esp_audio_capture_service_get_status` / `apply_setup`
3. `esp_media_service_link`
4. start sink + capture, wait, stop
5. `esp_media_dummy_service_get_stats` (expects `audio_frame_count > 0`)
6. unlink, re-setup with muxer, then storage record + `enable_stream`

### Troubleshooting

- If `tools/list` times out, confirm MCP UART pins and that console logs are not sharing the same UART.
- If stats stay at zero, check board ADC init and that sink was started before capture.
- Storage / record tools require a second `apply_setup` with `muxer_type` (for example `MP4 `), then `set_storage_url` while stopped, then `start` → `start_record` → `stop_record`.
- `enable_stream` should be exercised while capture is running; after stop it may return `ESP_ERR_NOT_SUPPORTED`.
- Non-JSON log lines on the MCP UART are ignored by the script.

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
