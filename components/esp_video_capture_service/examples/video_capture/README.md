# Video Capture Example

- [Chinese Version](./README_CN.md)

- Regular Example: ![alt text](../../../../docs/_static/level_regular.png "Regular Example") - demonstrates `esp_video_capture_service` video-only, A/V streaming, storage, dual-stream, overlay, and AI audio capture

## Example Brief

- This example shows how to capture video (and optional audio) with `esp_video_capture_service` on a board-manager camera / ADC device. For common usage, copy from `main/simple_capture.c` (video-only / A/V stream / storage / link / AI / full-speed UVC). For dual-stream, overlay, and auto-record mixes, see `main/video_capture_cases.c`.
- Technically, it demonstrates create → optional AI feature setup → `apply_setup` → `esp_service_start` / stop, direct `acquire_frame` / `release_frame`, `esp_media_service_link()` to a test sink, muxer recording, overlay redraw, and optional full-speed decode on the source path.

### Prerequisites

- Familiarity with [`esp_video_capture_service`](../../README.md) and [`esp_capture_service`](../../../esp_capture_service/README.md)
- A board definition supported by `esp_board_manager` that provides `camera` (and `audio_adc` for A/V cases)
- Optional: SD card for storage cases; USB UVC camera when exercising full-speed re-encode cases

### Folder Contents

```text
video_capture/
├── main/
│   ├── app_main.c                Board init, CLI, background record task
│   ├── simple_capture.c          Copy-ready common usage (video / A/V / AI / UVC)
│   ├── video_capture_cases.c     Complex case matrix (dual / overlay / auto-record)
│   ├── video_capture_scheduler.c Capture thread scheduler bridge
│   └── settings.h                Resolution, codecs, storage paths
├── partitions.csv
├── sdkconfig.defaults
└── README.md
```

## Environment Setup

### Hardware Required

- An ESP development board with board-manager support for `camera`
- Recommended: ESP32-P4 function EV board with MIPI / onboard camera
- `audio_adc` for A/V and AI-audio cases
- microSD card for storage / auto-record cases
- Optional: USB UVC camera for `simple fullspeed_uvc`

### Additional Requirements

- Enable SPIRAM and the video encoder / muxer options used by `settings.h`
- Overlay cases need `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY` and a painter font
- Full-speed UVC cases need `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_DECODER`

## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.5 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Generate board-manager configuration for your board, then optionally tune:

```text
ESP Capture > Enable video overlay / video decoder
Component config > FAT Filesystem support > Long filename support
```

Target-specific defaults are provided in `sdkconfig.defaults.esp32p4` and related files.

### Build and Flash

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

For full steps to configure and build an ESP-IDF project, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

## How to Use the Example

### Recommended Reading Order

1. Start with **`main/simple_capture.c`**. These walkthroughs are intentionally self-contained and written as **copy-ready usage code** for the most common `esp_video_capture_service` flows.
2. Run the matching `simple ...` console commands to verify the same scenarios on hardware.
3. For dual-stream, overlay, auto-record mixes, and the fuller A/V matrix, open **`main/video_capture_cases.c`**.

### Common Usage (`simple_capture.c`)

| User scenario | Function | Console command |
| --- | --- | --- |
| Video-only direct pull | `simple_capture_video_only()` | `simple video_only [duration_ms]` |
| A/V direct pull (encoded video + AAC) | `simple_capture_av_stream()` | `simple av_stream [duration_ms]` |
| A/V storage-only MP4 recording | `simple_capture_av_storage()` | `simple av_storage [duration_ms]` |
| A/V linked to a test sink | `simple_capture_av_link()` | `simple av_link [duration_ms]` |
| A/V with AI audio (AEC + VAD) | `simple_capture_av_ai()` | `simple av_ai [duration_ms]` |
| Full-speed UVC decode + re-encode storage | `simple_capture_fullspeed_uvc()` | `simple fullspeed_uvc [duration_ms]` |

### Complex Cases (`video_capture_cases.c`)

Use the full case matrix when you need more combinations than the simple walkthroughs:

| User scenario | Case name |
| --- | --- |
| Single video stream with linked sink stats | `v_only_stream` |
| Manual A/V MP4 recording | `av_storage` |
| Stream while recording MP4 | `av_stream_storage` |
| Auto-record into SD-card directory | `av_auto_storage` |
| Dual video (encoded + RGB565) | `v_dual` |
| Dual A/V (encoded + RGB565 preview) | `av_dual` |
| Dual A/V with storage on encoded stream only | `av_dual_mixed` |
| Dual A/V with shared text overlay | `av_dual_overlay` |
| A/V with AI audio AEC + VAD | `av_ai_aec_vad` |

### Example Functionality

After boot, the serial console prompt is `video-capture>`.

`record` cases run in a background task so the CLI stays responsive. Use `i` to print memory / system status while a background capture is running.

```text
cases
simple video_only 10000
simple av_stream 10000
simple av_storage 10000
simple av_link 10000
simple av_ai 10000
record av_storage 10000
record av_dual_overlay 10000
run_all 5000
run_all 5000 with_trace
i
```

Notes:

- `duration_ms` defaults to 10000.
- Stream statistics come from `esp_media_dummy_service` through `esp_media_service_link()`.
- Overlay cases enable shared source overlay and may start periodic redraw for date-time text.
- AI cases call `esp_capture_service_ai_audio_src_set_feature()` before `apply_setup()` on the video capture handle.
- Recordings are written under `/sdcard/video_capture/` (see `settings.h`).

### Example Log

```text
I (xxx) VIDEO_CAPTURE: Video capture example is ready
I (xxx) VIDEO_CAPTURE: Type 'cases' to list examples, or 'simple video_only 10000'
video-capture> simple av_stream 3000
I (xxx) SIMPLE_CAPTURE: av_stream: video ... audio ...
video-capture> record av_storage 5000
I (xxx) VIDEO_CAPTURE: Started background record for 'av_storage' (5000 ms)
I (xxx) VIDEO_CAPTURE: Capture case 'av_storage' finished
```

### References

- Component README: [esp_video_capture_service](../../README.md)
- Core capture service: [esp_capture_service](../../../esp_capture_service/README.md)
- Audio AI APIs: [esp_audio_capture_service](../../../esp_audio_capture_service/README.md)

## Troubleshooting

- **Camera init / apply_setup fails**: confirm board-manager camera device init and that the target exposes a V4L2 camera path.
- **SD card unavailable / storage cases fail**: confirm FatFS mount and that `/sdcard/video_capture` can be created.
- **Overlay returns `ESP_ERR_NOT_SUPPORTED`**: enable `CONFIG_ESP_CAPTURE_ENABLE_VIDEO_OVERLAY` and a painter font, then rebuild.
- **Full-speed UVC case fails**: enable video decoder support, connect a UVC camera, and confirm UVC device Kconfig.
- **Background capture still running**: wait for the previous `record` job to finish, or check status with `i`.

## MCP Operation Guide

This example can expose video capture, media link/unlink, and dummy-sink stats over UART MCP for PC-side verification. Data frames never go through MCP; they stay in C via `esp_media_service_link()`.

### 1. Enable component MCP options

In `menuconfig` (or rely on `sdkconfig.defaults`):

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP Video Capture Service → Enable video capture service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media sink service
```

Example UART pin options appear under `Video capture example → MCP UART pins` when video MCP and UART transport are enabled. Defaults:

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

On boot the example creates one real video capture service (`video-rec`) and one `media_dummy_sink`, registers MCP tools, and starts the UART MCP server. The normal `video-capture>` console remains available on the IDF console UART.

### 3. Run the PC UART script

Use a second USB-UART adapter wired to the MCP pins:

```bash
python3 scripts/test_video_capture_mcp_uart.py /dev/ttyUSB1 115200
```

Typical coverage:

1. `tools/list`
2. `esp_video_capture_service_get_status` / `apply_setup`
3. `esp_media_service_link`
4. start sink + capture, wait, stop
5. `esp_media_dummy_service_get_stats` (expects video or audio frames > 0)
6. unlink, re-setup with muxer + overlay, then storage record / `enable_stream` / overlay redraw

### Troubleshooting

- If `tools/list` times out, confirm MCP UART pins and that console logs are not sharing the same UART.
- If stats stay at zero, confirm camera / ADC init and that sink was started before capture.
- Storage / record tools require a second `apply_setup` with `muxer_type` (for example `MP4 `), then `set_storage_url` while stopped, then `start` → `start_record` → `stop_record`. Calling `start_record` without muxer setup returns `ESP_ERR_INVALID_STATE`.
- `enable_stream` should be exercised while capture is running; after stop it may return `ESP_ERR_NOT_SUPPORTED`.
- Overlay redraw may return a tool error when overlay support or fonts are unavailable on the board; the script allows that case after enabling overlay in setup.

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
