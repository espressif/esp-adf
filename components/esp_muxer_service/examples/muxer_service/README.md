# Muxer Service Example

- [Chinese Version](./README_CN.md)

- Regular Example: ![alt text](../../../../docs/_static/level_regular.png "Regular Example") - demonstrates `esp_muxer_service` muxing dummy AAC or dummy H264+AAC into a TS container

## Example Brief

- This example shows the convenience of `esp_muxer_service` as a product sink: link a source, pick storage / streaming / both, and get a TS file and/or muxed bytes without a custom muxer loop. Copy the create → setup → link → start muxer then source flow from `main/simple_muxer.c`. In a real app, swap the dummy source for capture or extractor using the same `esp_media_service_link()`.
- Technically, it demonstrates muxer registration, board-manager SD mount, `esp_muxer_service_setup()`, dummy-source linking, and optional streaming reads. Unused containers can be turned off in menuconfig to shrink the binary (see Configuration).

### Prerequisites

- Familiarity with [`esp_muxer_service`](../../README.md)
- A board definition supported by `esp_board_manager` (SD card for storage cases)
- No camera or microphone is required; sources are dummy encoded patterns

### Folder Contents

```text
muxer_service/
├── main/
│   ├── app_main.c           Board / CLI / MCP start
│   ├── simple_muxer.c       Copy-ready audio / AV linkage
│   ├── muxer_scheduler.c    Muxer / dummy thread scheduler
│   ├── muxer_mcp.c          UART MCP registration
│   ├── settings.h           Duration, TS type, SD path, RAM cache
│   └── Kconfig.projbuild    MCP UART pins
├── scripts/
│   └── test_muxer_mcp_uart.py
├── pytest_esp_muxer_service_example.py
├── sdkconfig.defaults
└── README.md
```

## Environment Setup

### Hardware Required

- An ESP development board with board-manager support
- Recommended: ESP32-P4 Function EV board (or another board with a matching board-manager definition)
- microSD card for `storage` / `both` cases (path `/sdcard/muxed`)
- Streaming-only cases do not require an SD card

### Additional Requirements

- Register the TS muxer (already enabled in `sdkconfig.defaults`)
- Storage directory is created by the service (maximum depth 2)

### Board manager

This example mounts the SD card with `esp_board_manager` (`ESP_BOARD_DEVICE_NAME_FS_SDCARD`). Generate the board package for **your** hardware before the first build:

```bash
idf.py set-target esp32p4          # or esp32s3 / your chip
idf.py gen-bmgr-config -l          # list board ids
idf.py gen-bmgr-config -b <your_board_name>
```

Replace `<your_board_name>` with the id that matches your board. Re-run `gen-bmgr-config` when you switch boards. SD mount is best-effort: missing SD does not block boot; `storage` / `both` then fail until the card is available.

## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.5 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

After board-manager config, optionally tune:

```text
Component config → ESP_Muxer Configuration    (enable only the containers you need)
Component config → FAT Filesystem support → Long filename support
Muxer service example → MCP UART pins
```

This example only muxes **TS**. To save flash, keep `Support TS Muxer` (`CONFIG_ESP_MUXER_TS_SUPPORT`) and **unselect** unused types (MP4 / OGG / WAV / FLV / CAF / AVI). `esp_muxer_register_default()` then links only the enabled muxers. See the component [Optimization](../../README.md#optimization) section.

Key defaults are already set in `sdkconfig.defaults` (dummy source, TS muxer, MCP UART). Storage path and duration live in `main/settings.h` (`MUXER_EXAMPLE_FAKE_STORAGE_DIR` = `/sdcard/muxed`).

### Build and Flash

```bash
idf.py build
idf.py -p YOUR_BOARD_PORT flash monitor
```

For full steps to configure and build an ESP-IDF project, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

## How to Use the Example

### Recommended Reading Order

1. Start with **`main/simple_muxer.c`**. These walkthroughs are copy-ready: dummy SRC → muxer SINK via `esp_media_service_link()`.
2. Run the matching `simple ...` console commands on hardware.
3. In a product, replace the dummy source with [`esp_audio_capture_service`](../../../esp_audio_capture_service/README.md), [`esp_video_capture_service`](../../../esp_video_capture_service/README.md), or [`esp_extractor_service`](../../../esp_extractor_service/README.md) using the same link call.

### User scenario

| User scenario | Function | Console command |
| --- | --- | --- |
| Dummy AAC → TS muxer | `simple_muxer_audio()` | `simple audio [duration_ms] [streaming\|storage\|both]` |
| Dummy H264+AAC → TS muxer | `simple_muxer_av()` | `simple av [duration_ms] [streaming\|storage\|both]` |

`duration_ms` defaults to 5000. Mode defaults to **streaming**. Storage and both write under `/sdcard/muxed`. Streaming drain is skipped in storage-only mode.

### Usage

After boot, the serial console prompt is `muxer>`.

```text
simple audio
simple audio 5000 streaming
simple av 5000 both
simple av 8000 storage
```

Notes:

- Start order is muxer first, then dummy source.
- Streaming mode prints packet / byte counts from `esp_muxer_service_read_streaming_data()`.
- Storage mode relies on FatFS at `/sdcard`.

### References

- Component README: [esp_muxer_service](../../README.md)
- Typical product sources: [esp_extractor_service](../../../esp_extractor_service/README.md), [esp_audio_capture_service](../../../esp_audio_capture_service/README.md)

## Troubleshooting

- **SD card unavailable / `storage` fails**: confirm `idf.py gen-bmgr-config -b <board>` was run, FatFS mounted, and `/sdcard` is writable.
- **Streaming reads stay empty**: start the muxer before the source; use a streaming-capable container (TS in this example).
- **`setup` / `set_storage_url` returns `ESP_ERR_INVALID_STATE`**: call them while the muxer is stopped.

## MCP Operation Guide

This example can expose muxer setup/control, dummy-src start/stop, and media link/unlink over UART MCP. Muxed bytes never go through MCP; they stay in C via `esp_media_service_link()`.

### 1. Enable component MCP options

In `menuconfig` (or rely on `sdkconfig.defaults`):

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP-Muxer Service → Enable muxer service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media source service
```

Example UART pin options appear under `Muxer service example → MCP UART pins`. Defaults:

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

On boot the example creates one dummy source (`media_dummy_src`) and one muxer (`esp_muxer_service`), registers MCP tools, and starts the UART MCP server. The `muxer>` console remains on the IDF console UART.

### 3. Run the PC UART script

Use a second USB-UART adapter wired to the MCP pins:

```bash
python3 scripts/test_muxer_mcp_uart.py /dev/ttyUSB1 115200
```

Typical coverage:

1. `tools/list`
2. `esp_muxer_service_setup`
3. `esp_media_service_link`
4. start muxer then dummy src, wait, stop
5. unlink and `esp_muxer_service_set_storage_url`

### Troubleshooting

- If `tools/list` times out, confirm MCP UART pins and that console logs are not sharing the same UART
- If streaming reads stay empty, confirm dummy src was started after the muxer
- `set_storage_url` / `setup` must be called while the muxer is stopped
- Non-JSON log lines on the MCP UART are ignored by the script

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
