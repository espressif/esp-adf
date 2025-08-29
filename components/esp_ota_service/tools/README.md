# OTA Service Tools

Scripts and helper projects that back the `esp_ota_service` examples and the
end-to-end / resume tests. The goal of this directory is:

- Keep every OTA-upgradable firmware image in one versioned place
  (`firmware_samples/`).
- Give each scenario a single Python entry point, so a developer only has to
  remember **four** commands (build / SD / HTTP / BLE) to drive every flow.

## Directory layout

```
tools/
├── README.md
├── firmware_samples/            # versioned bins + manifest.json (see subdir README)
├── build_firmware.py            # (2) build example + publish ota_<x>_v<ver>.bin
├── ota_fs_flash_sdcard.py       # (3) build sdcard_writer + flash bins to SD
├── ota_http_serve.py            # (4) serve ota_http_v*.bin over HTTP
├── ota_ble.py               # (5) BLE OTA driver (upload / check-c6 / flash-c6)
├── ota_fs_test_resume.py        # HIL test: resume-from-offset (ota_fs)
├── sdcard_writer/               # ESP-IDF project used by ota_fs_flash_sdcard.py
├── esp_at_esp32c6/              # ESP-AT images for the C6 BLE bridge
├── gen_userdata_bin.py          # standalone userdata.bin generator (library)
├── _fw_common.py                # shared helpers (internal)
├── _ble_at.py                   # shared ESP-AT / BLE helpers (internal)
└── old/                         # legacy ota_ble artefacts, kept for reference only
    └── ota_ble_test/
```

Every user-facing script is a Python 3 CLI with `--help`. The two files that
start with `_` are internal helper modules imported by the other scripts.

## Typical workflows

> **Run commands from this `tools/` directory.** The examples below assume
> `cd components/esp_ota_service/tools` has already been done from the ESP-ADF
> repo root. Every script resolves its own paths via `__file__`, so the CWD
> does not matter for correctness — using `./script.py` just keeps the
> commands short and copy-pasteable.

### Publish a new firmware image

```bash
# ota_http  -> firmware_samples/ota_http_v1.0.1.bin + refreshed manifest.json
./build_firmware.py http -v 1.0.1

# ota_fs    -> ota_fs_v0.2.0.bin + userdata_v0.2.0.bin
./build_firmware.py fs -v 0.2.0

# ota_ble
./build_firmware.py ble -v 1.0.1
```

### ota_http — run the HTTP OTA server

```bash
# Start server (auto-picks newest ota_http_v*.bin, rewrites manifest url):
./ota_http_serve.py --update-manifest-url

# Example output:
#   URL     http://10.0.0.42:18070/firmware.bin
# Copy this URL into ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL in examples/ota_http/main/ota_http_example.c,
# rebuild + flash the target, and let it pull the firmware.
```

### ota_fs — stage firmware on the SD card

```bash
# 1) Publish a newer ota_fs image
./build_firmware.py fs -v 0.2.0

# 2) Build sdcard_writer, flash it, wait for "*** SUCCESS"
./ota_fs_flash_sdcard.py --port /dev/ttyUSB0
#   -> writes /sdcard/firmware.bin (+ /sdcard/userdata.bin) on the target

# 3) Reflash ota_fs at the older version and let it pick up the SD image:
cd ../examples/ota_fs
idf.py -p /dev/ttyUSB0 flash monitor
```

### ota_ble — BLE OTA

```bash
# One-time: flash ESP-AT onto the C6 (MTU=517 variant recommended)
./ota_ble.py flash-c6 --variant mtu517 --port /dev/ttyUSB1 --yes

# Anytime: probe the C6 bridge
./ota_ble.py check-c6 --port /dev/ttyUSB2

# Publish a new ota_ble firmware and push it over BLE
./build_firmware.py ble -v 1.0.1
./ota_ble.py upload --port /dev/ttyUSB2
```

### HIL resume test (ota_fs)

Rewrites a single command into the full "build new, stage on SD, reflash
older, reset mid-OTA, assert resume" cycle:

```bash
./ota_fs_test_resume.py --port /dev/ttyUSB0 --new-version 0.2.0
```

## Requirements

All scripts assume ESP-IDF is on `PATH` (`. $ADF_PATH/esp-idf/export.sh`) and
rely on:

```bash
pip install pyserial esptool
```

## Legacy

`old/ota_ble_test/` holds the managed-component `ota_ble` test shell scripts.
The native IoT flow (`ota_ble`) is the supported variant; the older
`ota_ble` example + its uploader are kept only for reference.
