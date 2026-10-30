# Shared Bluetooth Hub Console

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example demonstrates how multiple application modules share one Bluetooth controller and host stack through `esp_bt_hub`. It can run on any ESP-IDF target with a supported Bluetooth controller and host backend. The commands registered by the interactive serial console follow the capabilities enabled for the selected chip: Bluetooth Classic, BLE, or dual mode.

The console creates independent clients, combines their Classic scan-mode requests, and arbitrates exclusive Classic inquiry, LE extended scan, and LE extended advertising resources when those features are available. It also registers two subscribers for each enabled GAP domain. One subscriber parses discovery reports, while the other observes and counts native GAP events, demonstrating callback fan-out without replacing the backend callback.

### Typical Scenarios

- Integrating several Bluetooth application modules that must share one host stack.
- Verifying ownership and release order before adding Bluetooth operations to an application.
- Exercising Classic-only, BLE-only, or dual-mode configurations on different Bluetooth-capable chips.
- Inspecting Classic discovery and LE extended-advertising reports from the console.

### Run Flow

1. The application initializes NVS and `esp_bt_hub`, then opens the `bt_hub>` console.
2. Each application module opens a named client.
3. A client requests a shared scan mode or acquires an exclusive operation lease.
4. GAP events are delivered to both registered subscribers.
5. The lease owner stops and releases the operation before closing its client.
6. After all clients are closed, the hub can be stopped and restarted.

## Environment Setup

### Hardware Required

- Any ESP development board whose chip and ESP-IDF version support Bluetooth
- A USB cable for flashing and serial-console interaction
- For Classic inquiry results, a nearby discoverable Bluetooth Classic device
- For LE scan results, a nearby device that sends extended advertising packets

### Default IDF Branch

The `esp_bt_hub` component requires ESP-IDF v5.5 or later. Use an ESP-IDF version that supports the selected chip and Bluetooth host backend.

This example does not use `esp_board_manager`.

## Build and Flash

### Build Preparation

Set up the ESP-IDF environment from the ESP-IDF root directory:

```bash
./install.sh
. ./export.sh
```

Then enter the example directory from the esp-adf repository root:

```bash
cd components/esp_bt_hub/examples/bt_hub
```

### Select a Target and Build

Set `TARGET` to a Bluetooth-capable chip supported by the active ESP-IDF version, for example `esp32`, `esp32s3`, or another supported target:

```bash
idf.py set-target TARGET
idf.py build
idf.py -p PORT flash monitor
```

For an ESP-IDF version where ESP32-S31 target selection is still a preview feature, use:

```bash
idf.py --preview set-target esp32s31
```

The target defaults select the Bluetooth capabilities used by each chip: ESP32 uses Bluedroid dual mode, ESP32-S3 uses Bluedroid BLE, and ESP32-S31 uses Bluedroid dual mode with BLE 5.0 extended scan and advertising.

Replace `PORT` with the board's serial port. Use `Ctrl-]` to exit the monitor.

### ESP32-S31 NimBLE LE-Only Build

The example includes a ready-to-use NimBLE defaults file for ESP32-S31. Use a separate build directory and sdkconfig file so the Bluedroid and NimBLE configurations do not overwrite each other:

```bash
idf.py --preview -B build-nimble \
  -D SDKCONFIG=sdkconfig.nimble \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32s31.nimble \
  set-target esp32s31
idf.py -B build-nimble build
idf.py -B build-nimble -p PORT flash monitor
```

This configuration enables the NimBLE central, peripheral, broadcaster, and observer roles together with extended scan and advertising. Bluetooth Classic and all `classic_*` console commands are disabled. Other NimBLE-capable targets can run the example after selecting NimBLE and the required roles in their target configuration.

### Project Configuration

The example has no project-specific menuconfig options. Available console functions are selected at compile time:

- Classic commands require `ESP_BT_HUB_CLASSIC_SUPPORTED`.
- The LE commands demonstrated by this example require `ESP_BT_HUB_LE_EXT_SUPPORTED`, because they use extended scan and advertising.
- Hub lifecycle and client commands are available with either Bluedroid or NimBLE.

Run `idf.py menuconfig` when changing the host backend or enabling Bluetooth features beyond the supplied defaults.

## How to Use the Example

### Start with the Console

After startup, the `bt_hub>` prompt is displayed. Run:

```text
help
hub_status
client_open audio
client_open pan
clients
```

The clients receive IDs `0` and `1`. Commands that acquire a resource use this ID to record and verify ownership.

### Verify Classic Resource Sharing

This section applies when `help` lists the `classic_*` commands.

This workflow gives each client a Classic scan-mode request and lets client `0` own the inquiry:

```text
classic_mode 0 1 general
classic_mode 1 1 limited
classic_inquiry_start 0
classic_inquiry_start 1
client_close 0
classic_inquiry_stop 0
```

The second inquiry start fails because inquiry is exclusive. Closing client `0` also fails while its inquiry lease is active. After `classic_inquiry_stop 0`, the client can be closed.

### Verify LE Scan and Advertising

This section applies when the target and configuration support extended LE scan and advertising and `help` lists the `le_*` commands.

Run an extended scan with client `0`:

```text
le_scan_start 0
hub_status
le_scan_stop 0
```

Run one extended-advertising instance with client `1`:

```text
le_adv_start 1
hub_status
le_adv_stop 1
```

For received advertising packets, the parser prints both the backend event and a normalized `esp_bt_hub_le_adv_report_t` result. After an inquiry or scan receives events, `hub_status` shows non-zero parser and observer callback counts.

### Stop and Restart the Hub

Release all leases and close all clients before stopping the hub:

```text
client_close 0
client_close 1
hub_stop
hub_status
hub_start
```

### Command Reference

Common commands:

- `hub_status`: show hub state, active resources, owners, and GAP callback counts.
- `hub_start` / `hub_stop`: start or stop an idle hub.
- `client_open <name>` / `client_close <id>`: open or close a hub client.
- `clients`: list open clients.

Classic commands, registered only when Classic is supported and enabled:

- `classic_mode <id> <0|1> <none|limited|general>`
- `classic_inquiry_start <id>` / `classic_inquiry_stop <id>`

Extended LE commands, registered only when extended LE features are supported and enabled:

- `le_scan_start <id>` / `le_scan_stop <id>`
- `le_adv_start <id>` / `le_adv_stop <id>`

### Log Output

The following is a continuous excerpt from an ESP32-S31 Bluedroid run:

```text
2026-10-26 10:24:09 bt_hub> client_open audio
2026-10-26 10:24:09 Client opened: id=0 name=audio
2026-10-26 10:24:09 bt_hub> client_open pan
2026-10-26 10:24:10 Client opened: id=1 name=pan
2026-10-26 10:24:10 bt_hub> classic_mode 0 1 general
2026-10-26 10:24:10 Classic mode: success
2026-10-26 10:24:10 bt_hub> classic_mode 1 1 limited
2026-10-26 10:24:10 Classic mode: success
2026-10-26 10:24:10 bt_hub> classic_inquiry_start 0
2026-10-26 10:24:10 I (1972) BT_HUB_EXAMPLE: Classic inquiry state: 1
2026-10-26 10:24:10 Classic inquiry start: success
2026-10-26 10:24:10 bt_hub> classic_inquiry_start 1
2026-10-26 10:24:10 Classic inquiry start failed: ESP_ERR_INVALID_STATE
2026-10-26 10:24:10 Command returned non-zero error code: 0x1 (ERROR)
2026-10-26 10:24:10 bt_hub> client_close 0
2026-10-26 10:24:10 E (2072) ESP_BT_HUB: Close client failed: client still owns a lease
2026-10-26 10:24:10 Client close failed: ESP_ERR_INVALID_STATE
2026-10-26 10:24:10 Command returned non-zero error code: 0x1 (ERROR)
```

The failed commands confirm that only one client can own inquiry and that an owner cannot be closed before releasing its lease.

## Troubleshooting

### A command returns `ESP_ERR_INVALID_STATE`

Run `hub_status` and `clients` to find the active resource and its owner. Stop the resource with the same client ID, then close the client. All clients must be closed before `hub_stop`.

### Classic or LE commands are missing

Run `help` to see commands available in the current build. BLE-only targets do not register `classic_*` commands. A target with BLE but without extended scan and advertising enabled does not register this example's `le_scan_*` or `le_adv_*` commands.

### No discovery results are printed

A successful start only means the scan or inquiry is running. Place a discoverable Classic device or an LE advertiser nearby and confirm that it is actively discoverable or advertising.

### The application does not fit in the partition

Keep `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y` from the supplied defaults. The dual-mode Bluedroid image can exceed the default 1 MB factory partition.

### Flashing or the console fails

Check the serial port, USB cable, and port permissions. Close other programs using the port. This example enables the UART console by default.

## Technical Support

For technical support, use the following links:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20)
- Issue reports and feature requests: [esp-adf issues](https://github.com/espressif/esp-adf/issues)
