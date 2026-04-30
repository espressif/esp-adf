# ESP32-C6 ESP-AT firmware images

Central location for every ESP32-C6 ESP-AT image consumed by the OTA BLE test
workflow (`../ota_ble_test/`, `../ota_ble_test/`). Only the binaries that
scripts actually flash are kept here; the rest of the upstream release package
(documentation, symbol files, step-by-step flash assets, factory variants) is
dropped to keep the repo small.

## Contents

```
esp_at_esp32c6/
├── README.md                           # this file
├── esp_at_c6_mtu517.bin                # custom ESP-AT build, ATT MTU cap = 517
└── factory/
    └── factory_ESP32C6-4MB.bin         # stock ESP-AT v4.1.1.0, 4 MB single image, flashed at 0x0
```

## factory/factory_ESP32C6-4MB.bin

- Chip: ESP32-C6, 4 MB flash
- Firmware: ESP-AT **v4.1.1.0** (official release)
- Source package:
  [ESP32-C6-4MB-AT-V4.1.1.0.zip](https://dl.espressif.com/esp-at/firmwares/esp32c6/ESP32-C6-4MB-AT-V4.1.1.0.zip)
- Release notes and documentation: see the [espressif/esp-at](https://github.com/espressif/esp-at) GitHub repo.
- Used by: `../ota_ble_test/04_flash_c6_esp_at.sh`

## esp_at_c6_mtu517.bin

- Chip: ESP32-C6, 4 MB flash
- Source: a local build of [espressif/esp-at](https://github.com/espressif/esp-at) based
  on v4.1.1.0 with the NimBLE ATT MTU cap raised from 203 to 517.
- Rationale: the stock build caps BLE payload at ~200 B per write; lifting the
  MTU to 517 lifts per-write payload to ~514 B and yields roughly 2.5x faster
  OTA throughput during `ota_ble` / `ota_ble` testing.
- Used by: `../ota_ble_test/04b_flash_c6_mtu517_at.sh`

## Upgrading to a newer ESP-AT release

1. Download the latest `ESP32-C6-4MB-AT-V*.zip` from dl.espressif.com.
2. Unzip and copy `factory/factory_ESP32C6-4MB.bin` in place of this file.
3. Update the version number and link in this README.
4. If you also rebuild the MTU=517 variant, drop the new `esp_at_c6_mtu517.bin`
   here alongside it.
