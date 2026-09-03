# RTSP Service Example

- [Chinese Version](./README_CN.md)

- Regular Example: ![alt text](../../../../docs/_static/level_regular.png "Regular Example") - demonstrates `esp_rtsp_service` pusher, puller, and local server

## Example Brief

- This example uses `esp_rtsp_service` as a **pusher (SINK)**, **puller (SRC)**, or **local server**. Copy-ready flows live in `main/simple_rtsp.c`. Wi-Fi credentials and default RTSP URLs are configured through menuconfig and mapped by `main/settings.h`.
- Dummy H264+AAC provides and consumes media, so no camera, microphone, display, or speaker is required. The `rtsp>` console starts each walkthrough asynchronously. Optional UART MCP controls the pre-created dummy source and RTSP server without carrying media frames.

### Prerequisites

- Familiarity with [`esp_rtsp_service`](../../README.md) and [`esp_media_service`](../../../esp_media_service/README.md)
- A Wi-Fi network the board can join
- For pusher/puller: a reachable RTSP endpoint (`rtsp://host:port/path`)
- For the local server: `ffplay` or VLC on a PC on the same LAN

### Folder Contents

```text
rtsp_service/
├── main/
│   ├── app_main.c          NVS, Wi-Fi, scheduler, MCP, and console
│   ├── simple_rtsp.c       Copy-ready pusher, puller, and server flows
│   ├── simple_rtsp.h
│   ├── rtsp_mcp.c          UART MCP registration (dummy source + RTSP server)
│   ├── rtsp_scheduler.c    Service thread scheduler
│   ├── settings.h          Wi-Fi and RTSP URLs from Kconfig
│   └── Kconfig.projbuild   Example Wi-Fi, URLs, duration, and UART pins
├── scripts/
│   └── test_rtsp_mcp_uart.py  PC-side MCP UART test client
└── pytest_esp_rtsp_service_example.py  Boot smoke test
```

## Environment Setup

### Hardware Required

- An ESP development board with Wi-Fi and PSRAM (recommended: ESP32-S3 or ESP32-P4)
- USB cable for the console; MCP testing also needs a second USB-UART adapter

### Additional Requirements

- Wi-Fi SSID and password
- A remote RTSP server/client, or a PC with `ffplay`/VLC for local-server testing
- For MCP UART: `pyserial` (`pip install pyserial`) and a UART separate from the console UART

## Build and Flash

This example supports ESP-IDF release/v5.5 and later branches. By default, it uses ADF's built-in `$ADF_PATH/esp-idf`.

Configure these entries under `RTSP service example`:

```text
WiFi SSID
WiFi password
Default run duration (ms)
Pusher RTSP URL
Puller RTSP URL
Local server URL
MCP UART pins
```

Then build and flash:

```bash
cd components/esp_rtsp_service/examples/rtsp_service
idf.py set-target esp32s3
idf.py menuconfig
idf.py build flash monitor
```

For complete ESP-IDF build instructions, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

## How to Use the Example

After boot, the serial prompt is `rtsp>`. The `simple` command starts a background task, leaving the console responsive.

| User scenario | Function | Console command |
| --- | --- | --- |
| Dummy H264 and/or AAC → remote RTSP server | `simple_rtsp_pusher()` | `simple pusher [duration_ms] [url] [a\|v\|av]` |
| Remote RTSP server → dummy sink | `simple_rtsp_puller()` | `simple puller [duration_ms] [url]` |
| Dummy H264 and/or AAC → local RTSP server | `simple_rtsp_server()` | `simple server [duration_ms] [url] [a\|v\|av]` |
| Reconnect Wi-Fi STA | `esp_wifi_service_request_connect()` | `wifi [ssid] [password]` |

Notes:

- `duration_ms` defaults to `CONFIG_RTSP_EXAMPLE_DURATION_MS` (10000).
- Omit `url` to use the menuconfig default. The URL may also be the only optional argument: `simple pusher rtsp://host:8554/live`.
- For pusher/server, `a` / `v` / `av` select audio, video, or both (default `av`). Arguments may appear in any order after the case name: `simple pusher v`, `simple server 20000 a`.
- Only one `simple` task can run at a time.
- `wifi` with no arguments uses menuconfig credentials. `wifi <ssid>` keeps the default password; `wifi <ssid> -` connects to an open AP.
- `assert` intentionally writes through address zero for panic/watchdog debugging; it reboots the target and is not part of normal operation.

### Local Server

Run `simple server`; the example prints the URL for a remote player:

```bash
ffplay -fflags nobuffer -flags low_delay rtsp://<device-ip>:554/live
```

The server currently supports one remote puller. Start the server before opening the player.

### MCP Operation Guide

MCP UART controls the pre-created dummy source (`media_dummy_src`) and RTSP server (`esp_rtsp_service`). Media frames remain in C through `esp_media_service_link()`.

Enable these options (already enabled by `sdkconfig.defaults`):

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP-RTSP Service → Enable RTSP service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media source
```

Default MCP UART: port 1, 115200 baud. TX / RX are GPIO 21 / 22 on ESP32 and ESP32-P4, and GPIO 17 / 18 on ESP32-S3 (S3 has no GPIO 22). Keep it separate from the console UART.

```bash
idf.py -p /dev/ttyACM0 flash monitor
python3 scripts/test_rtsp_mcp_uart.py /dev/ttyUSB1 115200
```

The script lists tools, then runs `setup` → `set_url` → `link` → start RTSP server → start dummy source → stop dummy source → stop server → `unlink`.

### Example Log

```text
I (...) ESP_SERVICE: [rtsp-cli] Started
I (...) RTSP_EX: RTSP service example is ready
I (...) RTSP_EX: RTSP_SERVICE_EXAMPLE_READY
rtsp> simple server 10000
simple server started in background
I (...) RTSP_EX: async simple server start duration_ms=10000 url=(default)
I (...) SIMPLE_RTSP: remote pull: ffplay rtsp://192.168.1.20:554/live
```

## Automated Smoke Test

`pytest_esp_rtsp_service_example.py` flashes the example and verifies both readiness log markers on ESP32-S3 and ESP32-P4:

```bash
pytest --target esp32s3 pytest_esp_rtsp_service_example.py
```

## Troubleshooting

- **Wi-Fi connection or remote push/pull fails**: verify menuconfig credentials and endpoint reachability.
- **Pusher stops after ANNOUNCE**: confirm the destination supports RTSP publishing (`ANNOUNCE`/`RECORD`), not only playback.
- **Player has no media**: start the RTSP server/sink before the dummy source and allow only one server client.
- **A/V drifts**: verify the rebuilt `esp_media_protocols` library includes AAC frame-duration and empty-frame handling fixes.
- **MCP response times out**: use a dedicated UART and stop the dummy source before stopping the RTSP server.

## Technical Support and Feedback

- For technical queries, use the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For feature requests or bug reports, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)
