# RTMP Service Example

- [Chinese Version](./README_CN.md)

- Regular Example: ![alt text](../../../../docs/_static/level_regular.png "Regular Example") - demonstrates `esp_rtmp_service` pusher, puller, and local server

## Example Brief

- This example shows how to use `esp_rtmp_service` as a **pusher (SINK)**, **puller (SRC)**, or **local server**. Copy-ready usage lives in `main/simple_rtmp.c`. Push/pull URLs and the server listen URL are in `main/settings.h` (menuconfig). Dummy H264+AAC is the pusher source, so no camera or microphone is required.
- Technically, it demonstrates `esp_rtmp_service_create` → `setup` / `set_url` → `esp_media_service_link()` → `esp_service_start` / stop. The pusher links a dummy source to the RTMP sink; the puller links the RTMP source to a dummy sink. The serial prompt `rtmp>` runs the same walkthroughs. Optional UART MCP controls the pre-created dummy source and RTMP pusher without sending frames over MCP.

### Prerequisites

- Familiarity with [`esp_rtmp_service`](../../README.md) and [`esp_media_service`](../../../esp_media_service/README.md)
- A Wi-Fi network the board can join
- For pusher/puller: a reachable RTMP URL (`rtmp://host:port/app/stream`)
- For the local server: `ffmpeg` / `ffplay` on a PC on the same LAN

### Folder Contents

```text
rtmp_service/
├── main/
│   ├── app_main.c          NVS, Wi-Fi, scheduler, MCP, console
│   ├── simple_rtmp.c       Copy-ready pusher / puller / server
│   ├── simple_rtmp.h
│   ├── rtmp_mcp.c          UART MCP registration (dummy src + RTMP sink)
│   ├── rtmp_scheduler.c    Service thread scheduler
│   ├── settings.h          Wi-Fi and RTMP URLs from Kconfig
│   └── Kconfig.projbuild   Example Wi-Fi, URLs, MCP UART pins
└── scripts/
    └── test_rtmp_mcp_uart.py  PC-side MCP UART test client
```

## Environment Setup

### Hardware Required

- An ESP development board with Wi-Fi and PSRAM (recommended: ESP32-S3 or ESP32-P4)
- USB cable for console (and a second USB-UART adapter if you use MCP UART)

### Additional Requirements

- Wi-Fi SSID and password
- Remote RTMP ingest/play URL, **or** a PC with `ffmpeg` / `ffplay` for the local server
- For MCP UART: `pyserial` (`pip install pyserial`) and a UART adapter that is **not** the console UART

## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.5 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Set Wi-Fi and RTMP URLs in menuconfig (mapped by `main/settings.h`):

```text
RTMP service example → WiFi SSID
RTMP service example → WiFi password
RTMP service example → Pusher RTMP URL
RTMP service example → Puller RTMP URL
RTMP service example → Local server listen URL
RTMP service example → Server stream name used in ffmpeg examples
```

MCP UART pins (when MCP UART transport is enabled):

```text
RTMP service example → MCP UART pins
```

`sdkconfig.defaults` already enables dummy SRC/SINK, RTMP SRC/SINK/SERVER, MCP UART, and larger LwIP TCP windows for live streams.

### Build and Flash

```bash
cd components/esp_rtmp_service/examples/rtmp_service
idf.py set-target esp32s3
idf.py menuconfig
idf.py build flash monitor
```

For full steps to configure and build an ESP-IDF project, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html).

## How to Use the Example

### Example Functionality

After boot, the serial prompt is `rtmp>`. Start with **`main/simple_rtmp.c`**, then run the matching console command.

| User scenario | Function | Console command |
| --- | --- | --- |
| Dummy H264+AAC → remote RTMP ingest | `simple_rtmp_pusher()` | `simple pusher [duration_ms] [url]` |
| Remote RTMP → dummy sink | `simple_rtmp_puller()` | `simple puller [duration_ms] [url]` |
| Local RTMP server (PC ffmpeg push / ffplay pull) | `simple_rtmp_server()` | `simple server [duration_ms] [url]` |

Notes:

- `duration_ms` defaults to `CONFIG_RTMP_EXAMPLE_DURATION_MS` (10000).
- Omit `url` to use the menuconfig / `sdkconfig` default. `url` can also be the only optional argument: `simple pusher rtmp://host/live/stream`.
- Link the sink/server before adding tracks on the dummy source when you need global interleaved cache.
- Start the RTMP sink (or dummy sink for puller) **before** the source.

#### Local server with ffmpeg / ffplay

`simple server` prints commands using the STA IP, for example:

```bash
ffmpeg -re -f lavfi -i testsrc=size=320x240:rate=15 -f lavfi -i sine \
  -c:v libx264 -preset ultrafast -tune zerolatency -c:a aac \
  -f flv rtmp://<device-ip>:1935/live/stream0

ffplay rtmp://<device-ip>:1935/live/stream0
```

Replace `stream0` with `CONFIG_RTMP_EXAMPLE_SERVER_STREAM`. URL shape for the server is `rtmp://host:port/app`; pusher/puller URLs include the stream name: `rtmp://host:port/app/stream`.

### MCP Operation Guide

MCP UART controls the pre-created dummy source (`media_dummy_src`) and RTMP pusher (`esp_rtmp_service`). Frames stay in C via `esp_media_service_link()`.

Enable (already in `sdkconfig.defaults`):

```text
Component config → ESP-Service: ESP Service Base → Enable MCP support
Component config → ESP-Service: ESP Service Base → MCP Transports → UART transport
ESP-RTMP Service → Enable RTMP service MCP tools
ESP Media Service → Enable media service MCP tools
ESP Media Service → ESP Media Dummy Service → Enable dummy media source
```

Default MCP UART (see `RTMP service example → MCP UART pins`):

- UART port: `1`
- TX GPIO: `21` (connect to USB-UART adapter RX)
- RX GPIO: `22` (connect to USB-UART adapter TX)
- Baud: `115200`

Keep MCP UART off the console UART. Flash and leave the board running, then from a PC:

```bash
idf.py -p /dev/ttyACM0 flash monitor
python3 scripts/test_rtmp_mcp_uart.py /dev/ttyUSB1 115200 --push-url rtmp://192.168.1.10/live/stream0
```

The script lists tools, then `setup` → `set_url` → `link` → start RTMP sink → start dummy source → stop sink → stop source → `unlink`. `esp_rtmp_service_query` is **server-only**; this MCP example registers a pusher, so the script allows that tool to return an error.

### Example Log

```text
I (8708) esp_cli_service: Start 'esp_cli_service': REPL running
I (8708) ESP_SERVICE: [rtmp-cli] Started
I (8708) RTMP_EX: RTMP service example is ready
I (8709) RTMP_EX: Type 'simple pusher', 'simple puller', or 'simple server'
rtmp> simple pusher 10000
I (16905) RTMP_CMD: Set tcUrl rtmp://192.168.1.10:1935/live
I (17140) RTMP: Got peer chunk size 4096
I (17160) RTMP: Publish 0x4801285c Started
I (17168) ESP_SERVICE: [esp_rtmp_service] Started
I (17212) ESP_SERVICE: [media_dummy_src] Started
I (27212) SIMPLE_RTMP: pusher url:rtmp://192.168.1.10/live/stream0 duration_ms:10000
```

### References

- Component README: [esp_rtmp_service](../../README.md)
- Media service: [esp_media_service](../../../esp_media_service/README.md)

## Troubleshooting

- **Wi-Fi connect failed / remote push-pull fails**: set SSID and password in menuconfig; confirm the RTMP host is reachable on the same LAN.
- **ffplay shows no video**: confirm the pusher linked **before** dummy tracks were added (global cache), and that the ingest server is receiving the stream.
- **Puller `receive fail` right after Play.Start**: check the stream codecs (this stack expects AAC / MP3 / PCM / G.711 and H264 / MJPEG) and that the dummy sink is started before the RTMP source.
- **MCP `Timed out waiting for a JSON-RPC response`**: use a dedicated UART (not the console port); start the RTMP sink before the dummy source; stop the sink before the source.
- **`esp_rtmp_service_query` error over MCP**: expected for the pusher role; query is SERVER-only.

## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
