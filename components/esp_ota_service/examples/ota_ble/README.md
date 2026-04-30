# BLE OTA (esp_ota_service + official APP protocol)

This example uses `esp_ota_service` together with the BLE source
(`esp_ota_service_source_ble_create()`), so the device can be upgraded with the official
**ESP BLE OTA** Android/iOS APP (same protocol as
[esp-iot-solution BLE OTA](https://github.com/espressif/esp-iot-solution/blob/master/examples/bluetooth/ble_ota)).

The GATT server is implemented natively on top of NimBLE inside
`esp_ota_service`, so this example pulls in **no managed components**.

## Why this example

`esp_ota_service` used to ship a separate legacy custom-protocol BLE source with
a 128-bit GATT service and plain Control/Data characteristics. That
implementation has been retired. The current
`esp_ota_service_source_ble` implementation used by this example is the **supported** BLE OTA flow: a
native NimBLE GATT server implementing the official ESP BLE OTA APP protocol,
driven through `esp_ota_service`, with no managed component required.

## Build and flash

From the repo root (with ADF and IDF environment set):

```bash
cd components/esp_ota_service/examples/ota_ble
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

The bundled `sdkconfig.defaults` assumes **4 MB flash** (`CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`),
which matches what the ESP BLE OTA APP expects by default.

## Verify with official APP

1. Install [ESP BLE OTA Android](https://github.com/EspressifApps/esp-ble-ota-android/releases) (or iOS if available).
2. Flash this example to the device.
3. Open the APP → scan → connect to the advertised device (default name `OTA_BLE`).
4. Select a firmware `.bin` and start OTA; the device will reboot into the new image when done.

## PC test via C6 AT bridge

`tools/ota_ble.py` simulates the Android APP through an ESP32-C6 running
ESP-AT (USB-UART central → BLE peripheral on this device):

```bash
cd components/esp_ota_service/tools

# Publish a versioned bin via the shared build script
./build_firmware.py ble -v 1.0.1

# Upload it over BLE (auto-picks newest ota_ble_v*.bin)
./ota_ble.py upload --port /dev/ttyUSB2
```

It scans for the OTA service `0x8018`, enables indications on `0x8020` /
`0x8022`, sends Start / per-sector data / Stop frames per the official
protocol and verifies every CRC16-CCITT ACK. See
`components/esp_ota_service/tools/README.md` for the full tool set
(`check-c6`, `flash-c6`, etc.).

## Configuration

- `CONFIG_OTA_ENABLE_BLE_SOURCE=y` enables the BLE source in `esp_ota_service`.
- `CONFIG_OTA_FORCE_VERSION_CHECK=y` + `esp_ota_service_checker_app_create(NULL)` require the
  incoming firmware to have a higher semver than the running image **and** the
  same `esp_app_desc_t::project_name`.
- `CONFIG_OTA_ENABLE_ROLLBACK=y` keeps the new image in `PENDING_VERIFY` until
  `esp_ota_service_confirm_update()` succeeds (the example calls it on boot).

## References

- [ESP BLE OTA Android APP](https://github.com/EspressifApps/esp-ble-ota-android/releases)
- [esp-iot-solution BLE OTA README](https://github.com/espressif/esp-iot-solution/blob/master/examples/bluetooth/ble_ota/README.md)
