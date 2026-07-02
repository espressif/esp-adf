# Services Hub Example

This example exercises `esp_service` with a production-style multi-service setup on real hardware. It wires together `wifi_service`, `esp_ota_service`, `esp_cli_service`, and `esp_button_service` through a shared `adf_event_hub` and an `esp_service_manager`, and optionally exposes service tools over MCP.

## Services Included

| Service | Type | Description |
|---------|------|-------------|
| `wifi_service` | Real | ESP-IDF Wi-Fi station with connect/disconnect events |
| `esp_ota_service` | Real | HTTP OTA with version check; wires `esp_ota_service_source_http_create()` + `esp_ota_service_target_app_create()` + `esp_ota_service_checker_app_create()` (`require_higher_version = true`) and skips body verification (`.verifier = NULL`) |
| `esp_cli_service` | Real | ESP console CLI; auto-discovers registered services and their commands |
| `esp_button_service` | Real | GPIO + ADC button input; long-press triggers provisioning mode |

## Optional MCP Integration

`CONFIG_ESP_MCP_ENABLE=y` is on by default, so `wifi_service_mcp` and `esp_ota_service_mcp` register their tools with the service manager. The shipped `sdkconfig.defaults` enables only the **STDIO** transport (`CONFIG_ESP_MCP_TRANSPORT_STDIO=y`); enable any of `CONFIG_ESP_MCP_TRANSPORT_{HTTP,SSE,WS,UART,SDIO}` in `menuconfig` if you need the tools exposed elsewhere.

## Build and Flash

This example uses [ESP Board Manager](https://github.com/espressif/esp-board-manager) to manage board-level resources. The [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) helper tool is recommended as the default entry point.

Install once in your activated ESP-IDF Python environment:

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # run this command when an update is requested
```

Enter the example directory:

```bash
cd adf_examples/services_hub
```

List the currently visible boards:

```bash
idf.py bmgr -l
```

Example output:

```text
ℹ️  Board Components:
  espressif/esp_boards:
    [1] esp32_c3_lyra
    [2] esp32_lyrat_4_3
    [3] esp32_lyrat_mini_1_1
    [4] esp32_p4_eye
    [5] esp32_p4_function_ev_board
    [6] esp32_s31_function_coreboard_1
    [7] esp32_s31_korvo_1
    [8] esp32_s3_box_3
    [9] esp32_s3_box_lite
    [10] esp32_s3_korvo_2_3
    [11] esp32_s3_lcd_ev_board
    [12] esp_vocat_1_0
    [13] esp_vocat_1_2
```

The example output above is based on the board list and ordering from `esp_boards` 0.5.2. Different `esp_boards` versions or custom board dependencies may change the list and indexes. Use the actual output of `idf.py bmgr -l` when selecting a board.

Select a board:

```bash
idf.py bmgr -b <board_index|board_name>
```

For example, to select `esp32_s3_korvo_2_3`:

```bash
idf.py bmgr -b 10
# or
idf.py bmgr -b esp32_s3_korvo_2_3
```

On first invocation of `idf.py bmgr`, the component is downloaded automatically based on the `espressif/esp_board_manager` dependency declared in `main/idf_component.yml`.

> [!NOTE]
> To use another board, run `idf.py bmgr -b <board_name|index>` with the same steps.
> For a custom board, see [Creating a Board Guide](https://docs.espressif.com/projects/esp-board-manager/en/latest/create-board/index.html).
> For more information about `esp_board_manager`, see the [ESP Board Manager Getting Started Guide](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md).
> Do not run `idf.py set-target` alone before bmgr, or the target chip may not match the board profile.

Then configure, build, flash, and monitor:

```bash
idf.py menuconfig                   # Wi-Fi SSID/password + OTA URL (see below)
idf.py build flash monitor
```

## HTTP OTA Server

The example reuses the shared tooling under
`components/esp_ota_service/tools/`, so no per-example server script is needed.
Two scripts drive the flow:

| Script | Purpose |
|--------|---------|
| `build_firmware.py` | Writes `version.txt`, runs `idf.py build`, and copies `build/services_hub.bin` to `firmware_samples/services_hub_v<ver>.bin` while refreshing `manifest.json`. |
| `ota_http_serve.py` | Stands up a tiny HTTP server on `0.0.0.0:18070` that returns the newest `services_hub_v*.bin` for `GET /firmware.bin` and the JSON metadata for `GET /manifest.json`. |

### 1. Flash the baseline (v1.0.0)

The URL is composed at boot from `CONFIG_PROD_OTA_HTTP_HOST` +
`CONFIG_PROD_OTA_HTTP_PORT` + `CONFIG_PROD_OTA_HTTP_PATH`, so set the host IP
(and optionally port/path) in `menuconfig` **before** building. The host must
be reachable from the Wi-Fi SSID you configured.

```bash
cd components/esp_ota_service/tools
./build_firmware.py services_hub -v 1.0.0      # build + publish baseline

cd ../../../adf_examples/services_hub
idf.py -p /dev/ttyUSB0 flash                   # flash baseline only; do not flash again
```

### 2. Publish a newer image (v1.0.1) without touching the device

```bash
cd components/esp_ota_service/tools
./build_firmware.py services_hub -v 1.0.1      # rebuild + publish services_hub_v1.0.1.bin
ls firmware_samples/services_hub_v1.0.*.bin
cat firmware_samples/manifest.json
```

> Do not run `idf.py flash` after this step, otherwise the device also runs
> v1.0.1 and has nothing to upgrade to.

### 3. Start the HTTP server and observe the upgrade

```bash
./ota_http_serve.py --example services_hub --update-manifest-url
#   Serving services_hub_v1.0.1.bin (1392.3 KiB)
#   URL      http://<host_ip>:18070/firmware.bin
#   Manifest http://<host_ip>:18070/manifest.json
```

Open a second terminal with `idf.py -p /dev/ttyUSB0 monitor`, then press the
device **RST** button. A successful run prints:

```
Wi-Fi connected: <ip>
OTA_SRC_HTTP: HTTP opened: http://<host_ip>:18070/firmware.bin  Content-Length: 1425696
OTA_VER_UTIL: Current firmware: 1.0.0  Incoming firmware: 1.0.1
OTA_CHK_APP: upgrade_available=true
OTA_TGT_APP: Writing to partition: ota_1  offset 0x00210000
OTA_SERVICE: [0] progress: ... / 1425696 bytes
ESP_GMF_HTTP: No more data, ret: 0
esp_image: segment 0..5 (verified twice)
OTA_TGT_APP: Boot partition set to: ota_1
```

After the automatic reboot the banner shows `App version: 1.0.1`, loaded from
`offset 0x210000` (ota_1). Subsequent checks print
`Current firmware: 1.0.1  Incoming firmware: 1.0.1` and skip the upgrade.

### Useful flags

- `./ota_http_serve.py --port 8080` — change listening port.
- `./ota_http_serve.py --bin services_hub_v1.0.0.bin` — serve a specific bin
  instead of the newest for the example.
- `./build_firmware.py services_hub --skip-build` — only re-publish the bin
  already in `build/`, without rebuilding.

### Caveats

- `CONFIG_PROD_OTA_HTTP_{HOST,PORT,PATH}` are composed into the URL at boot
  and baked into the running firmware. Changing any of them requires another
  `idf.py flash`; they are not pushed via OTA itself.
- `CONFIG_PROD_OTA_TIMEOUT_MS` defaults to 120 s. For slow networks or when
  the in-app CLI/MCP smoke loop competes for CPU, bump it (up to 600 000).
- On Linux hosts with `ufw` enabled, allow the port:
  `sudo ufw allow 18070/tcp`.

## Configuration (menuconfig)

Under **Services Hub Example** (all consumed by `main/app_main.c`):

| Option | Default | Description |
|--------|---------|-------------|
| `PROD_WIFI_SSID` | `""` | Wi-Fi SSID the example connects to on boot; leave empty to skip Wi-Fi start-up. Set per-developer in `menuconfig`; never commit concrete credentials |
| `PROD_WIFI_PASSWORD` | `""` | Password for `PROD_WIFI_SSID`; empty = open network. Same guidance as SSID |
| `PROD_OTA_HTTP_HOST` | `0.0.0.0` | Host portion of the firmware URL; override with the IP printed by `ota_http_serve.py` |
| `PROD_OTA_HTTP_PORT` | 18070 | TCP port `ota_http_serve.py` listens on; pass the same value via `--port` if you change it |
| `PROD_OTA_HTTP_PATH` | `/firmware.bin` | URL path served by the script; change only if a reverse proxy remaps it |
| `PROD_OTA_TIMEOUT_MS` | 120000 | Upper bound `app_main` waits for the OTA upgrade to finish; raise for slow networks |

The three HTTP fields are composed at boot into `http://<host>:<port><path>` and
printed in the startup banner (`OTA URL: ...`). Changing any of them still
requires a reflash, since the URL is built from compile-time Kconfig values.

Button GPIO/ADC pins come from the board-manager YAML (`idf.py bmgr -b <board>`), not from Kconfig.
