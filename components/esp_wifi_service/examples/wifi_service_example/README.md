# Wi-Fi Service Example

- [中文版](./README_CN.md)
- Regular Example: ⭐⭐

## Example Brief

This example demonstrates the ADF `esp_wifi_service` component. It creates a Wi-Fi service, stores Wi-Fi profiles in encrypted NVS, starts the provisioning flow, listens for service events, and exposes CLI service commands for profile and provisioning control.

The example shows how to combine `esp_service`, `adf_event_hub`, `esp_config_manager`, HTTP SoftAP provisioning, optional BluFi provisioning, profile storage and network selection, and network quality monitoring in one ADF service-based application.

### Typical Scenarios

This example is suitable for IoT devices that need field provisioning, persistent Wi-Fi credential storage, automatic reconnection, and a single service interface for connection state and provisioning events.

### Run Flow

1. Initialize NVS and, when enabled, initialize the Bluetooth host for BluFi.
2. Create the HTTP provisioning and the BluFi provisioning when enabled.
3. Configure encrypted NVS storage for Wi-Fi profiles.
4. Configure selector triggers for RSSI, Internet access probe, latency, and throughput degradation.
5. Create and start `esp_wifi_service`.
6. Subscribe to service events and start an `esp_cli_service` UART REPL.
7. When `CONFIG_ESP_MCP_ENABLE` and `CONFIG_WIFI_SERVICE_MCP_ENABLE` are enabled, register the Wi-Fi MCP tools and start the UART MCP transport if selected.
8. If no available profile exists, start provisioning. If profiles exist, use these profiles for connection selection.

## Environment Setup

### Hardware Required

- An Espressif development board supported by ESP-IDF.
- A Wi-Fi AP for provisioning and connection testing.
- A phone for HTTP provisioning. A phone with the Espressif BluFi app is needed only when BluFi is enabled.

### Default IDF Branch

This example supports ESP-IDF release/v5.5 and later. The ADF repository uses ESP-IDF release/v5.5 by default.

### Software Requirements

- Browser for HTTP provisioning.
- Espressif BluFi mobile app when testing BluFi provisioning.

## Build and Flash

### Build Preparation

Before building this example, ensure the ESP-IDF environment is set up. If it is not set up, run the following commands in the ESP-IDF root directory:

```
./install.sh
. ./export.sh
```

Go to this example's project directory:

```
cd $ADF_PATH/components/esp_wifi_service/examples/wifi_service_example
```

### Project Configuration

Set the target before building. The example provides target-specific defaults for ESP32 and ESP32-S3:

```
idf.py set-target esp32
```

or:

```
idf.py set-target esp32s3
```

Important configuration items:

- `Wi-Fi Service` → `Provisioning agents` → `Enable HTTP provisioning agent`
- `Wi-Fi Service` → `Provisioning agents` → `Embed default HTTP provisioning Web UI`
- `Wi-Fi Service` → `Provisioning agents` → `Enable BLUFI provisioning agent`
- `WIFI_SERVICE_SELECTOR_PROBE_ENABLE`: enable the selector HTTP probe monitor.
- `WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE`: enable the selector throughput monitor.
- `ESP_MCP_ENABLE`: enable the shared MCP server and transports.
- `WIFI_SERVICE_MCP_ENABLE`: build Wi-Fi MCP tool support; depends on `ESP_MCP_ENABLE`.
- `ESP_MCP_TRANSPORT_UART`: expose MCP tools over the example's UART transport.

BluFi depends on ESP-IDF Bluetooth options. The example includes `sdkconfig.defaults.esp32` for Bluedroid BluFi and `sdkconfig.defaults.esp32s3` for NimBLE BluFi. If Bluetooth is disabled for the current target, HTTP provisioning can still be used independently.

The default HTTP agent uses the built-in Web UI and a provisioning SoftAP SSID in the form `ESP_SVC_XXXXXX`, where `XXXXXX` comes from the device SoftAP MAC address. To override the AP name or password, set `http_cfg.softap` in `main/app_main.c`, for example:

```c
esp_wifi_service_prov_http_config_t http_cfg = {
    .name = "http",
    .softap = {
        .ssid = "ESP_SVC_DEMO",
        .password = "12345678",
        .channel = 3,
        .max_connection = 4,
    },
};
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

After boot, the example starts `esp_wifi_service` and opens an `esp_cli_service` UART REPL with the prompt `wifi>`.

Useful CLI commands:

```
help
wifi profile list
wifi profile add <ssid> <password> [priority 0..20]
wifi profile del <ssid>
wifi profile clear
wifi connect <ssid> <password|- for open AP> [priority 0..20] [wait_sec]
wifi prov start
wifi prov stop
wifi reeval
reboot
```

To test HTTP provisioning:

1. Start the example and wait until the HTTP agent starts.
2. Connect a phone or PC to the provisioning SoftAP.
3. Open the captive portal page, or browse to the device gateway address if the portal is not opened automatically.
4. Select or enter the target AP SSID, enter the password, and submit.
5. The device attempts to connect, saves the profile after success, and reports service events in the serial log.

To directly demonstrate `esp_wifi_service_request_connect()`, use:

```
wifi connect <ssid> <password> [priority 0..20] [wait_sec]
```

For an open AP, pass `-` as the password. The command saves or updates the profile, requests selector-driven connection, and waits up to `wait_sec` seconds for STA got IP. The default priority is `10`, and the default wait time is `30` seconds. Set `wait_sec` to `0` to return immediately after the connect request is accepted.

You can also add a profile from the CLI with `wifi profile add <ssid> <password> [priority]`, then reboot or restart provisioning/selection logic as needed.

When MCP is enabled, the example registers these Wi-Fi tools:

```
esp_wifi_service_get_status
esp_wifi_service_list_profiles
esp_wifi_service_add_profile
esp_wifi_service_set_profile_enabled
esp_wifi_service_delete_profile
esp_wifi_service_clear_profiles
esp_wifi_service_prov_start
esp_wifi_service_prov_stop
esp_wifi_service_request_reeval
```

The MCP status and profile tools do not return saved Wi-Fi passwords. Enable `CONFIG_ESP_MCP_TRANSPORT_UART=y` to start the built-in UART MCP server in this example.

The example uses `UART_NUM_1` for MCP at 115200 baud:

- MCP TX: GPIO25, connect to the USB-UART adapter RX
- MCP RX: GPIO26, connect to the USB-UART adapter TX
- GND: connect board GND and adapter GND

Run the Wi-Fi MCP UART smoke test from this example directory:

```
python3 scripts/test_wifi_service_mcp_uart.py /dev/ttyUSB1 115200
```

### Log Output

The following log excerpt shows the service starting with the HTTP agent:

```
I (642) WIFI_SVC_EXAMPLE: Create Wi-Fi service skeleton with 1 agents
I (652) WIFI_SERVICE: Init wifi default: station mode started
I (672) WIFI_SERVICE: Init 'wifi_service': agent 'http' ready
I (682) WIFI_SERVICE: Init 'wifi_service': done with 1 agent(s)
I (692) WIFI_SVC_EXAMPLE: Start Wi-Fi service
I (742) WIFI_SERVICE_HTTP: HTTP server started: port=80 ctrl_port=32769
I (752) WIFI_SVC_EXAMPLE: Event received: domain=wifi_service id=4 name=PROV_STARTED
I (752) WIFI_SVC_EXAMPLE: Payload: name=http
I (762) WIFI_SERVICE_HTTP: HTTP agent started
I (772) WIFI_SERVICE: Start provisioning: agent 'http' started (index=0)
I (782) WIFI_SERVICE: Start 'wifi_service': all agents started
I (792) WIFI_SVC_CONSOLE: CLI service: type 'help', 'wifi ...' or 'reboot'
```

After a credential is submitted and the station gets an IP address, key logs look like this:

```
I (15432) WIFI_SERVICE_HTTP: HTTP station connect begin: ssid='Office', password_len=8, timeout=15000ms
I (17342) WIFI_SERVICE_HTTP: HTTP station connect success: ssid='Office'
I (17362) WIFI_SVC_EXAMPLE: Event received: domain=wifi_service id=8 name=PROV_CREDENTIAL_RECEIVED
I (17362) WIFI_SVC_EXAMPLE: Payload: name=http ssid='Office' priority=10 password_len=8
I (17402) WIFI_SVC_EXAMPLE: Event received: domain=wifi_service id=3 name=STA_GOT_IP
I (17402) WIFI_SVC_EXAMPLE: Payload: ip=192.168.1.123 netmask=255.255.255.0 gw=192.168.1.1 ip_changed=0
```

## Troubleshooting

### BluFi agent is not created

BluFi is compiled only when the required ESP-IDF Bluetooth BluFi option is enabled. Use `idf.py set-target esp32` or `idf.py set-target esp32s3` so the target-specific defaults are applied, then check the `Wi-Fi Service` provisioning menu and the ESP-IDF Bluetooth menu.

### Cannot find the provisioning SoftAP

The example leaves `http_cfg.softap` unset, so the agent uses the default `ESP_SVC_XXXXXX` SoftAP name. Configure `http_cfg.softap.ssid` and `http_cfg.softap.password` in `main/app_main.c` if you need a fixed AP name and password during testing.

### HTTP provisioning page does not open automatically

Some operating systems do not always open captive portal pages. After connecting to the provisioning SoftAP, open a browser and visit the device gateway address manually.

### Profile remains after reflashing

Profiles are saved in NVS and are not erased by normal application reflashing. Use either of the following methods to clear them:

```
wifi profile clear
```

or:

```
idf.py erase-flash
```

## Technical Support

For technical support, use the links below:

- Technical support: [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- Issue reports and feature requests: [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will reply as soon as possible.
