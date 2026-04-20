# ESP Config Manager Simple Example

- [中文版](./README_CN.md)
- Basic Example: ⭐

## Example Brief

This example demonstrates how to use `esp_config_manager` to keep multiple application configuration groups in persistent storage. It loads defaults on first boot, saves modified values, reloads them, and prints the source of each loaded configuration.

The example shows dual-slot configuration storage, default-value merge, CRC-protected records, crypto hooks, and backend binding for NVS, SPIFFS, or raw flash partitions.

### Typical Scenarios

This example is suitable for products that need reliable local configuration storage, such as audio settings, boot counters, network-related flags, and schema-compatible configuration upgrades.

### Run Flow

1. Initialize NVS, and initialize SPIFFS when the filesystem backend is selected.
2. Create three independent configuration groups: audio, lifecycle, and net.
3. Bind each group to the selected storage backend.
4. Load each group with `esp_config_manager_load()`.
5. Update one field in each group, save it with `esp_config_manager_save()`, then load again to verify persistence.
6. Deinitialize the manager handles and print `Done`.

## Environment Setup

### Hardware Required

- An Espressif development board supported by ESP-IDF, such as ESP32 or ESP32-S3.

### Default IDF Branch

This example supports ESP-IDF release/v5.5 and later. The ADF repository uses ESP-IDF release/v5.5 by default.

## Build and Flash

### Build Preparation

Before building this example, ensure the ESP-IDF environment is set up. If it is not set up, run the following commands in the ESP-IDF root directory:

```
./install.sh
. ./export.sh
```

Go to this example's project directory:

```
cd $ADF_PATH/components/esp_config_manager/examples/simple
```

### Project Configuration

The default backend is NVS:

```c
#define EXAMPLE_CFG_STORAGE_BACKEND  EXAMPLE_STORAGE_NVS
```

To test other backends, edit `main/config_manager_example.c` and change `EXAMPLE_CFG_STORAGE_BACKEND` to one of:

- `EXAMPLE_STORAGE_NVS`: store each group in an isolated NVS namespace.
- `EXAMPLE_STORAGE_FS`: store primary and backup files under `/spiffs`; this uses the `storage` SPIFFS partition in `partitions.csv`.
- `EXAMPLE_STORAGE_FLASH`: store each primary/backup copy in raw data partitions defined in `partitions.csv`.

`sdkconfig.defaults` already enables the custom partition table needed by the SPIFFS and raw-flash backend demos.

To run the longer self-test instead of the short demo, set the following macro in `main/config_manager_example.c`:

```c
#define CFG_EX_RUN_SELFTEST  1
```

### Build and Flash Commands

- Build the example:

```
idf.py build
```

- Flash the firmware and run the serial monitor (replace `PORT` with your port name):

```
idf.py -p PORT flash monitor
```

- To exit the monitor, use `Ctrl-]`.

## How to Use the Example

### Functionality and Usage

After flashing, the example runs automatically. On the first boot, configuration groups are loaded from defaults and written to storage. On later boots, values are loaded from the primary storage slot and continue from the previous saved values.

For the default NVS backend, run `idf.py erase-flash` before flashing if you want to observe the first-boot default recovery again.

### Log Output

The following log excerpt shows the normal short demo with the default NVS backend:

```c
I (325) CFG_MANAGER_EXAMPLE: Storage backend: NVS (3 config groups)
I (345) CFG_MANAGER_EXAMPLE: [audio] from defaults repair=0
I (345) CFG_MANAGER_EXAMPLE:   vol=42 flags=0x00000001 profile=default bri=80 lvl=10 array=esp1
I (355) CFG_MANAGER_EXAMPLE: [audio] from primary repair=0
I (355) CFG_MANAGER_EXAMPLE:   vol=43 flags=0x00000001 profile=default bri=80 lvl=0 array=esp1
I (365) CFG_MANAGER_EXAMPLE: --- group audio done ---
I (375) CFG_MANAGER_EXAMPLE: [lifecycle] from defaults — boot_count=0 session_flags=0x0100
I (375) CFG_MANAGER_EXAMPLE:   array=esp2
I (385) CFG_MANAGER_EXAMPLE: [lifecycle] from primary — boot_count=1 session_flags=0x0100
I (395) CFG_MANAGER_EXAMPLE: --- group lifecycle done ---
I (405) CFG_MANAGER_EXAMPLE: [net] from defaults — route_tag=0 if=sta0 mtu=1500
I (415) CFG_MANAGER_EXAMPLE: [net] from primary — route_tag=305419896 if=sta0 mtu=1500
I (425) CFG_MANAGER_EXAMPLE: --- group net done ---
I (425) CFG_MANAGER_EXAMPLE: Done
```

When self-test mode is enabled, key lines look like this:

```c
I (325) CFG_MANAGER_EXAMPLE: ======== selftest start (backend=NVS, 3 groups) ========
I (345) CFG_MANAGER_EXAMPLE: [TEST] g0.packed_layout PASS
I (365) CFG_MANAGER_EXAMPLE: [TEST] g0.load_defaults_after_wipe PASS — ESP_OK
I (385) CFG_MANAGER_EXAMPLE: [TEST] g0.save_load_roundtrip PASS — ESP_OK
I (405) CFG_MANAGER_EXAMPLE: [TEST] g0.fallback_to_backup_crc PASS — ESP_OK
I (425) CFG_MANAGER_EXAMPLE: [TEST] g0.primary_repaired_after_load PASS — ESP_OK
I (445) CFG_MANAGER_EXAMPLE: [TEST] g0.merge_tail_new_fields PASS
I (465) CFG_MANAGER_EXAMPLE: ======== selftest end ========
```

## Troubleshooting

### SPIFFS or raw flash backend fails to load

Confirm that `sdkconfig.defaults` is used and the custom `partitions.csv` is selected. These backends require the `storage`, `cfg_pri`, `cfg_bak`, `cfg2_pri`, `cfg2_bak`, `cfg3_pri`, and `cfg3_bak` partitions.

### Values are not reset after reflashing

NVS data is preserved across normal application reflashing. Use the following command to clear saved configuration data:

```
idf.py erase-flash
```

### Self-test logs show `FAIL`

Use `idf.py erase-flash` and run the test again. If the flash backend is selected, also verify that the custom partition table is active and that every raw config partition in `partitions.csv` is present.

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
