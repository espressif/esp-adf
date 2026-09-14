# ESP Extractor Service Example

- [Chinese Version](./README_CN.md)

- Regular Example: ![alt text](../../../../docs/_static/level_regular.png "Regular Example") - demonstrates `esp_extractor_service` extracting audio and video frames from local files and network / HLS URLs

## Example Brief

- This example shows how to use `esp_extractor_service` as a high-level source: demux a container on SD card or over HTTP(S) / HLS, then either pull elementary frames yourself or **link** the extractor to a sink. Copy the create → set URL → start / link flow from `main/extractor_demo.c`.
- Technically, it demonstrates extractor registration, board-manager SD mount, Wi‑Fi connect, `esp_extractor_service_set_url()`, `esp_media_service_get_provider()` acquire / release, and `esp_media_service_link()` to `esp_media_dummy_service` (the same link API used with player sinks in a product).

### Prerequisites

- Familiarity with [`esp_extractor_service`](../../README.md)
- A board definition supported by `esp_board_manager` (SD card for file cases)
- Wi‑Fi credentials for HTTP / HLS cases

### Folder Contents

```text
extractor_service/
├── main/
│   ├── app_main.c          Board / Wi‑Fi init, CLI registration
│   ├── extractor_demo.c    Copy-ready provider and link-to-sink usage
│   ├── extractor_mcp.c     Optional UART MCP server
│   ├── settings.h          Named URLs, SD path, duration, pool size
│   └── Kconfig.projbuild   Wi‑Fi and MCP UART pins
├── scripts/
│   └── test_extractor_mcp_uart.py
├── pytest_esp_extractor_service_example.py
├── partitions.csv
├── sdkconfig.defaults
└── README.md
```

## Environment Setup

### Hardware Required

- An ESP development board with board-manager support
- Recommended: ESP32-P4 Function EV board (or another board with a matching board-manager definition and SDMMC / SD SPI)
- microSD card for local MP4 cases
- Network access for HLS / HTTP cases

### Additional Requirements

- Playable test file on the SD card at `/sdcard/video/test1.mp4` (see `EXTRACTOR_URL_SD_MP4` in `main/settings.h`)
- Flash size of at least 8 MB (`partitions.csv` / `sdkconfig.defaults`)

### Board manager

This example mounts the SD card with `esp_board_manager` (`ESP_BOARD_DEVICE_NAME_FS_SDCARD`). Generate the board package for **your** hardware before the first build:

```bash
idf.py set-target esp32p4          # or esp32s3 / your chip
idf.py gen-bmgr-config -l          # list board ids
idf.py gen-bmgr-config -b <your_board_name>
```

Replace `<your_board_name>` with the id that matches your board (for example the ESP32-P4 Function EV definition). Re-run `gen-bmgr-config` when you switch boards. File cases fail softly if the SD device is missing; HTTP / HLS still run if Wi‑Fi connects.

## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.5 and later branches.

### Configuration

After board-manager config, optionally tune:

```text
Extractor Service Example > WiFi SSID / password / connect wait
ESP-Extractor Service > file / HTTP / HLS support
Component config > FAT Filesystem support > Long filename support
```

Key defaults are already set in `sdkconfig.defaults` (SPIRAM, FatFS LFN, file + HTTP + HLS, dummy sink, MCP UART). Named URLs live in `main/settings.h`:

| Name | URL |
|------|-----|
| `hls_aac` | Qingting live AAC HLS |
| `sd_mp4` | `/sdcard/video/test1.mp4` |
| `hls_av` | JW Player oceans AES HLS |

Place a playable MP4 at `/sdcard/video/test1.mp4` before running `sd_mp4`.

### Build and Flash

```bash
idf.py build
idf.py -p YOUR_BOARD_PORT flash monitor
```

For full steps to configure and build an ESP-IDF project, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

## How to Use the Example

### Recommended Reading Order

1. Start with **`main/extractor_demo.c`**. `extractor_demo_run()` is the copy-ready path: create the extractor, set URL / mask / pool, then either pull frames or link a sink.
2. Run the matching `simple ...` console commands on hardware.
3. In a product, replace the dummy sink with [`esp_player_service`](../../../esp_player_service/README.md) / audio or video player services using the same `esp_media_service_link()` call.

### Common Usage (`extractor_demo.c`)

| User scenario | Console command |
| --- | --- |
| Pull frames with `get_provider` + acquire / release | `simple provider [name\|url] [duration_ms]` |
| Link extractor to a dummy sink (same API as a player sink) | `simple link [name\|url] [duration_ms]` |
| Repeat until N EOS events (or error / duration) | `repeat <provider\|link> [name\|url] [repeat_count] [duration_ms]` |

Named URLs (`hls_aac`, `sd_mp4`, `hls_av`) are listed in `main/settings.h`. Omit the name to use the first entry. Example: `simple provider sd_mp4 5000`.

Provider mode prints each frame (`type`, `pts`, `size`, first 8 payload bytes) and stops on wall-clock duration, all-track EOS, or error. Link mode lets the sink consume; logs dummy-sink audio / video frame and byte counts.

### Example Functionality

After boot, the serial console prompt is `extractor>`.

```text
simple provider
simple provider sd_mp4 5000
simple link hls_aac 10000
simple link https://example.com/live.m3u8 15000
repeat provider sd_mp4 3
repeat link hls_aac 2 60000
```

Notes:

- `duration_ms` defaults to `EXTRACTOR_TEST_DURATION_MS` in `settings.h`.
- `repeat_count` defaults to `EXTRACTOR_REPEAT_COUNT`. Repeat mode enables `set_auto_loop(true)`; the demo counts `ESP_EXTRACTOR_SERVICE_EVENT_EOS` and stops on error or when the count is reached.
- SD mount and Wi‑Fi connect are best-effort. File cases need `/sdcard`; HTTP / HLS cases need Wi‑Fi.
- Link statistics after each `simple link` come from `esp_media_dummy_service`.

### References

- Component README: [esp_extractor_service](../../README.md)
- Player sink (typical product link target): [esp_player_service](../../../esp_player_service/README.md)

## Troubleshooting

- **SD card unavailable / `sd_mp4` fails**: confirm `idf.py gen-bmgr-config -b <board>` was run, FatFS mounted, and `/sdcard/video/test1.mp4` exists.
- **HTTP / HLS fails immediately**: check Wi‑Fi SSID / password in menuconfig and that the URL is reachable.
- **HLS not compiled**: enable `CONFIG_ESP_EXTRACTOR_SERVICE_HLS_SUPPORT` (on in this example's `sdkconfig.defaults`).
- **No frames / small pool**: raise `EXTRACTOR_OUT_POOL_SIZE` / `esp_extractor_service_set_out_pool_size()` so the pool fits the largest access unit.

## MCP Operation Guide

This example can expose extractor setup/control, dummy-sink stats, and media link/unlink over UART MCP. Frames never go through MCP; they stay in C via `esp_media_service_link()`.

### 1. Enable component MCP options

In `menuconfig` (or rely on `sdkconfig.defaults`):

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP-Extractor Service → Enable extractor service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media sink service
```

Example UART pin options appear under `Extractor Service Example → MCP UART pins`. Defaults:

- UART port: `UART_NUM_1`
- TX GPIO: `21` (connect to USB-UART adapter RX)
- RX GPIO: `22` (connect to USB-UART adapter TX)
- Baud: `115200`

### 2. Build, flash, and keep the board running

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

On boot the example creates `esp_extractor_service` and `media_dummy_sink`, registers MCP tools, and starts the UART MCP server. The normal `extractor>` console remains available on the IDF console UART.

### 3. Run the PC UART script

Use a second USB-UART adapter wired to the MCP pins:

```bash
idf.py -p /dev/ttyACM0 flash monitor
python3 scripts/test_extractor_mcp_uart.py /dev/ttyUSB1 115200
```

Typical coverage:

1. `set_extract_mask`, `set_out_pool_size`, `set_url`
2. link, start sink then extractor
3. dummy stats
4. stop, unlink

Use a playable file at `/sdcard/video/test1.mp4` or pass `--url` for HTTP/HLS.

### Troubleshooting

- If `tools/list` times out, confirm MCP UART pins and that console logs are not sharing the same UART.
- If stats stay at zero, start the sink before the extractor and confirm the URL is readable.
- Non-JSON log lines on the MCP UART are ignored by the script.

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
