# HTTP OTA Example

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

- This example downloads a firmware image from an HTTP/HTTPS server, verifies its version and SHA-256 digest via a JSON manifest, writes it to the inactive OTA app partition, reboots into the new slot, and confirms the update after Wi-Fi reconnects.
- It demonstrates `esp_ota_service` with `esp_ota_service_checker_manifest_create()`, `esp_ota_service_verifier_sha256_create()`, `esp_ota_service_source_http_create()`, `esp_ota_service_target_app_create()`, NVS breakpoint resume, and A/B rollback confirmation (`esp_ota_service_confirm_update()`).

### Typical Scenarios

Connected IoT products that check a remote manifest endpoint for newer firmware releases, verify image integrity on the fly before switching boot slots, and automatically roll back if network connectivity fails on the new firmware.

### Run Flow

1. Boot, print the running firmware version and partition (`ota_0` or `ota_1`), and initialize NVS.
2. Connect to the configured Wi-Fi AP; if the running image is in `ESP_OTA_IMG_PENDING_VERIFY` and Wi-Fi fails, call `esp_ota_service_rollback()`.
3. Once Wi-Fi connects on a `PENDING_VERIFY` image, call `esp_ota_service_confirm_update()` to mark the slot valid and cancel rollback.
4. Pre-fetch `manifest.json` using `esp_ota_service_checker_manifest_create()` to compare the server `x.y.z` version against the running firmware and extract the SHA-256 digest.
5. When a newer version is advertised, arm `esp_ota_service_verifier_sha256_create()`, submit the HTTP source + app target item via `esp_ota_service_set_upgrade_list()`, and start the session with `esp_service_start()`.
6. Upon `ESP_OTA_SERVICE_EVT_SESSION_END` with zero failures, destroy the service and call `esp_restart()` to boot into the updated partition.

### File Structure

```text
├── main
│   ├── app_main.c
│   ├── CMakeLists.txt
│   ├── esp_ota_service_http_example.c
│   ├── esp_ota_service_http_example.h
│   └── idf_component.yml
├── CMakeLists.txt
├── partitions.csv
├── pytest_esp_ota_service_ota_http.py
├── README.md
├── README_CN.md
├── sdkconfig.defaults
└── sdkconfig.defaults.esp32s3
```

## Environment Setup

### Hardware Required

- An ESP32-S3 (or other ESP-IDF supported Wi-Fi) development board with at least 8 MB flash (`partitions.csv` defines two 2 MB app slots).
- A 2.4 GHz Wi-Fi AP reachable by both the development board and the host PC running the HTTP server.
- A USB cable for power supply, flashing, and serial monitoring.

### Default IDF Branch

This example supports IDF release/v5.4 and later. The example uses IDF release/v5.5 by default.

### Software Requirements

- Python 3 on the host PC to build versioned firmware artefacts (`../../tools/build_firmware.py`) and serve `/manifest.json` + `/firmware.bin` (`../../tools/ota_http_serve.py`).
- Firmware version convention: `CMakeLists.txt` defines the baseline `project(ota_http VERSION 1.0.0)` so an initial `idf.py build` produces a valid `1.0.0` image without `version.txt`. Running `../../tools/build_firmware.py http -v 1.0.1` writes `version.txt` (git-ignored), which ESP-IDF prioritizes over `CMakeLists.txt` when building the upgrade payload; default baselines are tracked in `../../tools/_fw_common.py`.

## Build and Flash

### Build Preparation

Before building this example, ensure the ESP-IDF environment is set up. If it is already set up, skip this paragraph and go to the project directory. If not, run the following in the ESP-IDF root directory to complete the environment setup. For full steps, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/index.html).

```bash
./install.sh
. ./export.sh
```

Go to this example's project directory from the repository root:

```bash
cd components/esp_ota_service/examples/ota_http
idf.py set-target esp32s3
```

### Project Configuration

Edit `WIFI_SSID`, `WIFI_PASSWORD`, `ESP_OTA_SERVICE_EXAMPLE_MANIFEST_URL`, and `ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL` in `main/esp_ota_service_http_example.c` to match your Wi-Fi credentials and host PC LAN IP (port `18070` matches `ota_http_serve.py`).

To adjust OTA service Kconfig settings:

```bash
idf.py menuconfig
```

Key configuration items (already set in `sdkconfig.defaults`):

- `ESP-Service: OTA Service` → `Verification and rollback` → `Require higher firmware version by default` (`CONFIG_OTA_FORCE_VERSION_CHECK=y`)
- `ESP-Service: OTA Service` → `Verification and rollback` → `Enable OTA rollback support` (`CONFIG_OTA_ENABLE_ROLLBACK=y`)
- `Bootloader config` → `Enable app rollback support` (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`)
- `ESP-Service: OTA Service` → `Resume (NVS checkpoint)` → `Enable OTA resume (breakpoint continuation)` (`CONFIG_OTA_ENABLE_RESUME=y`)

> Press `s` to save and `Esc` to exit after configuration.

### Build and Flash Commands

- Build the baseline `1.0.0` firmware (ensure `version.txt` is absent if you previously ran `build_firmware.py`):

```bash
rm -f version.txt
idf.py build
```

- Flash the baseline firmware and run the serial monitor (replace `PORT` with your serial port):

```bash
idf.py -p PORT flash monitor
```

- To exit the monitor, use `Ctrl-]`

## How to Use the Example

### Functionality and Usage

1. From `components/esp_ota_service/tools`, build and publish a newer `1.0.1` image and start the HTTP server:

```bash
cd components/esp_ota_service/tools
./build_firmware.py http -v 1.0.1
./ota_http_serve.py --update-manifest-url
```

2. Verify that `ESP_OTA_SERVICE_EXAMPLE_MANIFEST_URL` and `ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL` in `examples/ota_http/main/esp_ota_service_http_example.c` point to the URL printed by `ota_http_serve.py`, then flash the `1.0.0` baseline image (`rm -f version.txt && idf.py -p PORT build flash monitor` in `examples/ota_http`).
3. On boot, `1.0.0` connects to Wi-Fi, fetches `/manifest.json`, verifies that `1.0.1 > 1.0.0`, streams `/firmware.bin` while checking SHA-256, writes `ota_1`, and reboots.
4. After rebooting into `1.0.1` (`PENDING_VERIFY`), the device reconnects to Wi-Fi, calls `esp_ota_service_confirm_update()` to cancel rollback, re-checks `/manifest.json`, sees `1.0.1` is already running, and skips further downloads.

### Log Output

Representative continuous log across upgrade, reboot into `ota_1`, rollback confirmation, and post-upgrade version check:

```text
I (1470) OTA_HTTP_EXAMPLE: Run_ota_session: upgrade available '1.0.0' -> '1.0.1'
I (1471) OTA_HTTP_EXAMPLE: Run_ota_session: SHA-256 verifier armed from manifest
I (1472) OTA_HTTP_EXAMPLE: Run_ota_session: starting HTTP OTA service: http://10.0.2.1:18070/firmware.bin
I (1473) OTA_HTTP_EXAMPLE: [OTA] Session started
I (1510) OTA_HTTP_EXAMPLE: [OTA] Downloading from server ...
I (2054) OTA_VER_SHA256: SHA-256 verified successfully
I (2055) OTA_TGT_APP: Boot partition set to: ota_1
I (2055) esp_ota_svc_resume: Resume record cleared [idx=0]
I (2055) OTA_HTTP_EXAMPLE: [OTA] Download + write succeeded
I (2055) OTA_HTTP_EXAMPLE: Run_ota_session: OTA session finished OK; rebooting from the application
...
I (184) OTA_HTTP_APP: ========================================
I (184) OTA_HTTP_APP:   Project:  ota_http
I (184) OTA_HTTP_APP:   Version:  1.0.1
I (185) OTA_HTTP_APP:   Partition: ota_1 (0x00210000)
I (185) OTA_HTTP_APP: ========================================
I (191) OTA_HTTP_APP: Running partition OTA state: 1 (1=PENDING_VERIFY after OTA reboot)
I (191) OTA_HTTP_APP: Connecting to WiFi ...
I (1470) OTA_HTTP_EXAMPLE: WiFi connected — IP: 10.0.2.2
I (1470) OTA_HTTP_EXAMPLE: New image pending verify — WiFi OK, marking OTA valid ...
I (1470) OTA_SERVICE: OTA update confirmed - rollback cancelled
I (1470) OTA_HTTP_EXAMPLE: Confirm_if_pending_verify: upgrade OK, rollback cancelled
I (1473) OTA_CHK_MANIFEST: Manifest check: incoming=1.0.1 current=1.0.1 size=1065440 has_hash=true has_md5=false upgrade=false
W (1473) OTA_HTTP_EXAMPLE: Run_ota_session: server firmware '1.0.1' is not newer than running '1.0.1' — skipping
I (1474) OTA_HTTP_APP: Running version 1.0.1 — waiting for next upgrade ...
```

## Troubleshooting

### Manifest fetch fails with `ESP_ERR_HTTP_CONNECT`

```text
E (7495) OTA_HTTP_EXAMPLE: Run_ota_session failed: manifest check returned ESP_ERR_HTTP_CONNECT
```

Ensure `ota_http_serve.py` is running on the host PC, the host firewall permits TCP port `18070`, and `ESP_OTA_SERVICE_EXAMPLE_MANIFEST_URL` / `ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL` match the host's LAN IP on the same subnet as the board.

### Server firmware skipped immediately

```text
W (1473) OTA_HTTP_EXAMPLE: Run_ota_session: server firmware '1.0.0' is not newer than running '1.0.0' — skipping
```

The served `manifest.json` advertises a version less than or equal to the running image. Run `./build_firmware.py http -v <higher_x.y.z>` in `components/esp_ota_service/tools` and restart `ota_http_serve.py`.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
