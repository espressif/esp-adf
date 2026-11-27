# BLE OTA Example

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

- This example receives a firmware image over Bluetooth Low Energy (BLE) using the official **ESP BLE OTA** mobile application protocol (GATT service `0x8018`, firmware characteristic `0x8020`, command characteristic `0x8022` with 4 KB sector CRC16-CCITT acknowledgements), validates the incoming app descriptor, writes the inactive OTA slot, and reboots into the new firmware.
- It demonstrates `esp_ota_service` with the native NimBLE `esp_ota_service_source_ble_create()` source, `esp_ota_service_checker_app_create()`, `esp_ota_service_target_app_create()`, and boot-time rollback confirmation (`esp_ota_service_confirm_update()`), with zero external BLE OTA managed components.

### Typical Scenarios

Wi-Fi-less or offline devices upgraded directly from the official [ESP BLE OTA](https://github.com/EspressifApps/esp-ble-ota-android/releases) mobile app or an automated BLE central bridge (`../../tools/ota_ble.py`).

### Run Flow

1. Boot, print project and partition metadata, initialize NVS, and call `esp_ota_service_ble_example_confirm_running_image()` to confirm the running slot when it is in `PENDING_VERIFY`.
2. Create `esp_ota_service`, subscribe to OTA events, and instantiate `esp_ota_service_source_ble_create()`, `esp_ota_service_target_app_create()`, and `esp_ota_service_checker_app_create(NULL)`.
3. Register the single `ble://` upgrade item (`resumable = false`) via `esp_ota_service_set_upgrade_list()` and call `esp_service_start()`.
4. Advertise as `OTA_BLE` over NimBLE, wait for a central to connect and send `CMD_START`, check the incoming `esp_app_desc_t` header (requiring a higher `x.y.z` semver and matching `project_name`), and stream 4 KB sectors into the target partition.
5. On `ESP_OTA_SERVICE_EVT_SESSION_END` with zero failures, destroy the service and call `esp_restart()`.

### File Structure

```text
├── main
│   ├── app_main.c
│   ├── CMakeLists.txt
│   ├── esp_ota_service_ble_example.c
│   ├── esp_ota_service_ble_example.h
│   ├── esp_ota_service_ble_example_err.h
│   └── idf_component.yml
├── CMakeLists.txt
├── partitions.csv
├── pytest_esp_ota_service_ota_ble.py
├── README.md
├── README_CN.md
└── sdkconfig.defaults
```

## Environment Setup

### Hardware Required

- An ESP32-S3 (or other BLE-capable Espressif SoC) development board with at least 4 MB flash (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`).
- Either an Android/iOS phone with the **ESP BLE OTA** app installed, or a second ESP32-C6 board flashed with ESP-AT for automated PC-driven testing via `../../tools/ota_ble.py`.
- A USB cable for power supply, flashing, and serial monitoring.

### Default IDF Branch

This example supports IDF release/v5.4 and later. The example uses IDF release/v5.5 by default.

### Software Requirements

- Mobile app: [ESP BLE OTA Android releases](https://github.com/EspressifApps/esp-ble-ota-android/releases), or PC script `../../tools/ota_ble.py` (`pip install pyserial esptool`).
- Firmware version convention: `CMakeLists.txt` defines the baseline `project(ota_ble VERSION 1.0.0)` (tracked in `../../tools/_fw_common.py`) so an initial `idf.py build` embeds `1.0.0` without `version.txt`. Running `../../tools/build_firmware.py ble -v 1.0.1` writes `version.txt` (git-ignored) and publishes `ota_ble_v1.0.1.bin` with version `1.0.1` so `esp_ota_service_checker_app_create(NULL)` accepts the upgrade.

## Build and Flash

### Build Preparation

Before building this example, ensure the ESP-IDF environment is set up. If it is already set up, skip this paragraph and go to the project directory. If not, run the following in the ESP-IDF root directory to complete the environment setup. For full steps, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/index.html).

```bash
./install.sh
. ./export.sh
```

Go to this example's project directory from the repository root:

```bash
cd components/esp_ota_service/examples/ota_ble
idf.py set-target esp32s3
```

### Project Configuration

```bash
idf.py menuconfig
```

Key configuration items (enabled in `sdkconfig.defaults`):

- `ESP-Service: OTA Service` → `Download sources` → `Enable BLE OTA source (official ESP BLE OTA APP protocol)` (`CONFIG_OTA_ENABLE_BLE_SOURCE=y`)
- `ESP-Service: OTA Service` → `Verification and rollback` → `Require higher firmware version by default` (`CONFIG_OTA_FORCE_VERSION_CHECK=y`)
- `ESP-Service: OTA Service` → `Verification and rollback` → `Enable OTA rollback support` (`CONFIG_OTA_ENABLE_ROLLBACK=y`)

> Press `s` to save and `Esc` to exit after configuration.

### Build and Flash Commands

- Build the baseline `1.0.0` firmware:

```bash
rm -f version.txt
idf.py build
```

- Flash the firmware and run the serial monitor (replace `PORT` with your port name):

```bash
idf.py -p PORT flash monitor
```

- To exit the monitor, use `Ctrl-]`

## How to Use the Example

### Functionality and Usage

#### Method 1: Official ESP BLE OTA Mobile App

1. Build a `1.0.1` upgrade payload (`cd components/esp_ota_service/tools && ./build_firmware.py ble -v 1.0.1`) and copy `tools/firmware_samples/ota_ble_v1.0.1.bin` to your phone.
2. Flash the `1.0.0` baseline image onto the board (`rm -f version.txt && idf.py -p PORT build flash monitor` in `examples/ota_ble`).
3. Open **ESP BLE OTA**, connect to `OTA_BLE`, select `ota_ble_v1.0.1.bin`, and start transmission. The board validates the incoming header, writes the image, and reboots into `1.0.1`.

#### Method 2: PC Automation via ESP32-C6 ESP-AT Bridge

```bash
cd components/esp_ota_service/tools
./build_firmware.py ble -v 1.0.1
./ota_ble.py upload --port /dev/ttyUSB2
```

See `../../tools/README.md` for `check-c6` and `flash-c6` bridge setup commands.

### Log Output

Representative startup log when `ota_ble` initializes NimBLE GATT and waits for a BLE central connection:

```text
I (269) app_init: Project name:     ota_ble
I (381) esp_ota_svc_ble_ex:   BLE OTA (esp_ota_service + APP protocol)
I (387) esp_ota_svc_ble_ex:   Project:  ota_ble
I (441) OTA_BLE: Confirm_running_image: image is not pending verify, nothing to do
I (441) ADF_EVENT_HUB: Create 'esp_ota_service': domain registered
I (443) ESP_SERVICE: [esp_ota_service] Initialized
I (448) OTA_BLE: Run_ota: BLE OTA (APP protocol) ready. Use ESP BLE OTA APP to connect and upgrade.
I (457) OTA_BLE: [OTA] Session started
I (545) ESP_SERVICE: [esp_ota_service] Started
```

## Troubleshooting

### Start timeout waiting for BLE central

```text
E (180550) OTA_BLE: [OTA] Failed: ESP_ERR_TIMEOUT (reason=4)
```

No BLE central connected and sent `CMD_START` within `wait_start_timeout_ms` (180 s in this example). Reset the board and initiate the transfer from the mobile app or `ota_ble.py` within 3 minutes.

### Incoming firmware skipped due to version check

```text
W (4210) OTA_BLE: [OTA] Firmware is not newer - skipping
```

`esp_ota_service_checker_app_create(NULL)` rejected an image whose `x.y.z` version is not strictly higher than the running version (or whose `project_name` is not `ota_ble`). Build a higher version with `./build_firmware.py ble -v 1.0.1`.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
