# Filesystem OTA Example

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

- This example performs offline multi-partition OTA upgrades from a microSD card (`/sdcard` via `esp_board_manager`) or a USB Mass Storage flash drive (`/usb`), updating the application slot together with two raw data partitions (`userdata` and `flash_tone`) in a single session.
- It demonstrates `esp_ota_service` batching up to three items with distinct checkers and targets: `esp_ota_service_checker_app_create()` + `esp_ota_service_target_app_create()`, `esp_ota_service_checker_data_version_create()` + `esp_ota_service_target_data_create()`, and `esp_ota_service_checker_tone_desc_create()` + `esp_ota_service_target_data_create()` over `esp_ota_service_source_fs_create()`.

### Typical Scenarios

Offline field servicing or factory provisioning where an operator inserts a microSD card or USB flash drive carrying a firmware bundle (`firmware.bin`, `userdata.bin`, `flash_tone.bin`) to update application code and companion assets in one pass.

### Run Flow

1. Boot, print the running version and active slot (`ota_0` or `ota_1`), and initialize NVS.
2. Mount the configured storage backend (`/sdcard` through `esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD)` by default, or `/usb` via USB Host MSC).
3. Probe `<mount>/firmware.bin`, `<mount>/userdata.bin`, and `<mount>/flash_tone.bin`; assemble an upgrade list with `firmware.bin` first (`resumable = true`, `skip_on_fail = false`) followed by any present data images (`resumable = false`, `skip_on_fail = true`).
4. Run the OTA session: each item's checker compares the incoming version against the installed partition version and skips items that are already up to date.
5. If at least one item was written (`EVT_OTA_SUCCESS`), destroy the service and call `esp_restart()`; otherwise unmount storage and print a hex preview of the `userdata` and `flash_tone` headers.

### File Structure

```text
├── main
│   ├── app_main.c
│   ├── CMakeLists.txt
│   ├── esp_ota_service_fs_example.c
│   ├── esp_ota_service_fs_example.h
│   ├── esp_ota_service_fs_example_usb.c
│   ├── idf_component.yml
│   └── Kconfig.projbuild
├── CMakeLists.txt
├── partitions.csv
├── pytest_esp_ota_service_ota_fs.py
├── README.md
├── README_CN.md
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
└── sdkconfig.defaults.esp32s3
```

## Environment Setup

### Hardware Required

- ESP32-S3-Korvo-2 V3 or ESP32-P4 Function EV Board with at least 8 MB flash (`partitions.csv` defines `ota_0`, `ota_1`, `userdata`, and `flash_tone`).
- A FAT32-formatted microSD card (or USB-OTG flash drive when USB MSC backend is selected).
- A USB cable for power supply, flashing, and serial monitoring.

### Default IDF Branch

This example supports IDF release/v5.4 (>= v5.4.3) and release/v5.5 (>= v5.5.2).

### Software Requirements

- `esp-bmgr-assist` in the ESP-IDF Python environment to generate board configuration files for `esp_board_manager`.
- Helper scripts in `../../tools/` (`build_firmware.py`, `ota_fs_flash_sdcard.py`, `gen_userdata_bin.py`, `ota_fs_test_resume.py`).
- Firmware version convention: `CMakeLists.txt` defines the baseline `project(ota_fs VERSION 0.1.0)` (tracked in `../../tools/_fw_common.py`) so an initial `idf.py build` produces a valid `0.1.0` image without `version.txt`. Running `../../tools/build_firmware.py fs -v 0.2.0` writes `version.txt` (git-ignored) and publishes `ota_fs_v0.2.0.bin` plus a matching 4-byte semver-headed `userdata_v0.2.0.bin`.

## Build and Flash

### Build Preparation

Before building this example, ensure the ESP-IDF environment is set up. If it is already set up, skip this paragraph and go to the project directory. If not, run the following in the ESP-IDF root directory to complete the environment setup. For full steps, see the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/index.html).

```bash
./install.sh
. ./export.sh
```

Short steps:

- Go to this example's project directory from the repository root:

```bash
cd components/esp_ota_service/examples/ota_fs
```

This example uses [ESP Board Manager](https://github.com/espressif/esp-board-manager) to manage board-level resources. Use the [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) helper tool as the default entry point.

- Install once in your activated ESP-IDF Python environment:

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # run this command when an update is requested
```

- List supported boards:

```bash
idf.py bmgr -l
```

- Select a board (for example `esp32_s3_korvo_2_3`):

```bash
idf.py -D IDF_TARGET=esp32s3 gen-bmgr-config -b esp32_s3_korvo_2_3
```

On first invocation, the component is downloaded automatically based on the `espressif/esp_board_manager` dependency declared in `main/idf_component.yml`.

> [!NOTE]
> To switch to a different board supported by `esp_board_manager`, repeat the command with the target board name.
> For a custom board, see [Creating a Board Guide](https://docs.espressif.com/projects/esp-board-manager/en/latest/create-board/index.html).

### Project Configuration

```bash
idf.py menuconfig
```

Configure the storage backend and OTA options if needed:

- `OTA FS Example` → `Storage backend for OTA bin files` (`SD card (via esp_board_manager FS_SDCARD)` or `USB flash drive (USB Host MSC)`)
- `ESP-Service: OTA Service` → `Verification and rollback` → `Require higher firmware version by default` (`CONFIG_OTA_FORCE_VERSION_CHECK=y`)
- `ESP-Service: OTA Service` → `Resume (NVS checkpoint)` → `Enable OTA resume (breakpoint continuation)` (`CONFIG_OTA_ENABLE_RESUME=y`)

> Press `s` to save and `Esc` to exit after configuration.

### Build and Flash Commands

- Build the baseline `0.1.0` firmware:

```bash
rm -f version.txt
idf.py build
```

- Flash the baseline firmware and run the serial monitor (replace `PORT` with your port name):

```bash
idf.py -p PORT flash monitor
```

- To exit the monitor, use `Ctrl-]`

## How to Use the Example

### Functionality and Usage

1. Build and publish a `0.2.0` upgrade bundle (`ota_fs_v0.2.0.bin` + `userdata_v0.2.0.bin`) from `components/esp_ota_service/tools`:

```bash
cd components/esp_ota_service/tools
./build_firmware.py fs -v 0.2.0
```

2. Copy `firmware_samples/ota_fs_v0.2.0.bin` as `firmware.bin`, `firmware_samples/userdata_v0.2.0.bin` as `userdata.bin`, and `firmware_samples/flash_tone_v0.5.0.bin` as `flash_tone.bin` onto the root of the microSD card (or stage them directly over serial with `./ota_fs_flash_sdcard.py --port PORT`).
3. Reflash the `0.1.0` baseline image in `examples/ota_fs` (`rm -f version.txt && idf.py -p PORT build flash monitor`) with the microSD card inserted.
4. The device upgrades `ota_1`, `userdata`, and `flash_tone`, then reboots into `0.2.0`. On the second boot all three checkers report up-to-date versions and skip rewriting.

### Log Output

Representative continuous log after upgrading to `1.0.1` + `userdata` `1.0.1` + `flash_tone` `0.5.0` and rebooting into `ota_1`:

```text
I (159) OTA_FS_APP: ========================================
I (159) OTA_FS_APP:   Project:  ota_fs
I (159) OTA_FS_APP:   Version:  1.0.1
I (160) OTA_FS_APP:   Partition: ota_1 (0x00210000)
I (160) OTA_FS_APP:   Storage:   SD card (mount /sdcard)
I (160) OTA_FS_APP: ========================================
I (182) DEV_FS_FAT: Filesystem mounted, base path: /sdcard
I (182) OTA_FS: Create 'fs_sdcard': mount_point=/sdcard
I (182) OTA_FS: Run_ota: found /sdcard/firmware.bin (707536 bytes)
I (182) OTA_FS: Run_ota: found /sdcard/userdata.bin (1024 bytes) → partition "userdata" (4-byte semver checker)
I (182) OTA_FS: Run_ota: found /sdcard/flash_tone.bin (83082 bytes) → partition "flash_tone" (tone-desc checker)
I (183) OTA_FS: Run_ota: installed tone version: 0.5.0
I (183) OTA_FS: Run_ota: starting OTA (3 item(s)) ...
I (184) OTA_CHK_APP: Check: incoming version='1.0.1' project='ota_fs' size=707536 upgrade_available=false
W (184) OTA_FS: [OTA] Item 0 skipped: SD image not newer
I (184) OTA_CHK_DATA: Data version check: incoming=1.0.1 current=1.0.1 upgrade_available=false
W (184) OTA_FS: [OTA] Item 1 skipped: SD image not newer
I (185) OTA_CHK_TONE: Tone-desc check: incoming version='0.5.0' project='ESP_TONE_BIN' size=83082 upgrade_available=false
W (185) OTA_FS: [OTA] Item 2 skipped: SD image not newer
I (185) OTA_FS: [OTA] Session finished: all 3 item(s) skipped, nothing to update
I (185) OTA_FS: Run_ota: all items up to date, no firmware applied
I (186) OTA_FS: Userdata header: 4-byte packed semver (LE u32=0x00010001): 1.0.1
I (186) OTA_FS: Userdata version matches running app semver '1.0.1'
I (187) OTA_FS: Flash_tone header: tone bin (magic 0xf55f9876), project='ESP_TONE_BIN' version='0.5.0'
I (188) OTA_FS_APP: Running version 1.0.1 — idle on ota_1
```

## Troubleshooting

### `firmware.bin` not found on mounted storage

```text
I (920) OTA_FS: Run_ota: no firmware.bin on /sdcard — nothing to upgrade
```

Ensure the upgrade image is named `firmware.bin` at the root of the FAT32 volume (`/sdcard/firmware.bin` or `/usb/firmware.bin`).

### SD card mount failure

```text
E (910) OTA_FS: Init_sdcard failed: esp_board_manager_init_device_by_name returned ESP_FAIL
```

Verify that a FAT32-formatted microSD card is inserted and that `gen-bmgr-config` was generated for your exact development board.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
