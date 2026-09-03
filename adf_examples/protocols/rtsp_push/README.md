# RTSP Audio/Video Push

- [中文版](./README_CN.md)
- Basic Example: ⭐

## Example Brief

This example uses `esp_service` to publish camera video and microphone audio to an RTSP server; the application configures the media parameters and destination, links `esp_video_capture_service` to `esp_rtsp_service`, and controls their lifecycle without handling capture, encoding, or RTSP transfer separately.
The service data path is:

```text
camera + microphone
        |
esp_video_capture_service
        |
esp_media_service_link()
        |
esp_rtsp_service (SINK)
        |
remote RTSP server
```

The stream uses MJPEG video at 640 × 480, 10 fps and AAC audio at 16 kHz mono, and the destination URL is configured in menuconfig.
The example also verifies that the same configured and linked service instances can complete two 15-second push sessions separated by stop/start.

For the multi-role server, push, and pull CLI, see the [`RTSP CLI`](../rtsp_cli/README.md) example.

## Environment Setup

### Hardware Required

- ESP32-P4 Function EV Board or ESP32-S3-Korvo-2 V3
- A camera and audio ADC provided by the selected board
- A PC on the same network running an RTSP server

### Software Requirements

This example supports ESP-IDF release/v5.4 (>= v5.4.3), release/v5.5 (>= v5.5.2), and IDF v6.1.

[MediaMTX](https://github.com/bluenviron/mediamtx) is recommended as the RTSP server.
Download it on the PC and run:

```bash
./mediamtx
```

MediaMTX accepts RTSP publishers on port `8554` without extra configuration.

## Build and Flash

Activate the ESP-IDF environment and enter the example:

```bash
. $IDF_PATH/export.sh
cd adf_examples/protocols/rtsp_push
```

Select the board:

```bash
idf.py bmgr -b esp32_p4_function_ev_board
# or
idf.py bmgr -b esp32_s3_korvo_2_3
```

Configure Wi-Fi and the destination:

```bash
idf.py menuconfig
```

Set these options under **RTSP Push Example Configuration**:

- **WiFi SSID**
- **WiFi Password**
- **RTSP push URL**, for example `rtsp://192.168.1.10:8554/live`

Use the PC's LAN address in the URL; `127.0.0.1` would refer to the ESP device itself.

Media defaults such as codecs, resolution, frame rate, sample rate, and bitrate are defined in `main/rtsp_push_settings.h`.

### Resource Optimization

The RTSP role switches are under **Component config → ESP-RTSP Service**, where this focused example keeps only `CONFIG_ESP_RTSP_SERVICE_SINK_SUPPORT`; its `sdkconfig.defaults` directly disables capture decoding, every audio and video decoder, and unused audio encoders. For a target-specific minimum build, use **Component config → Video Codec Configuration** to keep only one target-supported MJPEG `CONFIG_VIDEO_ENCODER_*_SUPPORT` implementation and disable H264 and redundant encoder implementations, then run `idf.py fullclean` before rebuilding.

Build, flash, and monitor:

```bash
idf.py build
idf.py -p PORT flash monitor
```

## How It Works

`app_main()` performs six steps:

1. Initialize NVS, the media adapter, the camera, the microphone, the encoders, and Wi-Fi.
2. Create and configure the capture and RTSP services, including the media parameters, destination URL, and local IP address.
3. Link the capture source to the RTSP sink once.
4. Start the RTSP sink before the capture source.
5. After 15 seconds, stop the capture source before the RTSP sink, restart the same instances without recreating, reconfiguring, or relinking them, push for another 15 seconds, and stop them again.
6. Unlink and deinitialize both services.

When startup succeeds, the device prints the configured push URL.
Play the stream from another terminal on the PC:

```bash
ffplay -rtsp_transport udp rtsp://127.0.0.1:8554/live
```

The first stop disconnects the player, so reconnect after the second push session starts.
For product use, replace the fixed delays with application-specific control; for continuous pushing, keep the first start and remove the timed stop, restart, second stop, and final cleanup sequence.

## Troubleshooting

- If Wi-Fi connection fails, verify the SSID and password in menuconfig.
- If RTSP startup fails, check that MediaMTX is running and that the push URL uses the PC's reachable LAN address.
- If no video is received, verify the camera connection and selected board.
- If the PC cannot play the stream, allow MediaMTX and UDP traffic through the host firewall.

## Technical Support

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issues: [esp-adf issues](https://github.com/espressif/esp-adf/issues)
