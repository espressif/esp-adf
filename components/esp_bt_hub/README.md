# ESP Bluetooth Hub

- [中文版](./README_CN.md)

`esp_bt_hub` provides centralized management for the ESP Bluetooth controller and one selected host stack, either Bluedroid or NimBLE. It manages hub lifecycle, Generic Access Profile (GAP) event distribution, pairing policy, discovery, scanning, advertising, and resource arbitration so that Bluetooth application modules do not initialize or control the same controller independently.

The application initializes this component once. Bluetooth audio, Personal Area Networking (PAN), and other application modules then open named clients and acquire only the resources they need.

## Features

- Separates native-stack initialization from runtime start and stop.
- Tracks the hub lifecycle through the `ESP_BT_HUB_STATE_*` states.
- Distributes the stack's single Classic and Bluetooth Low Energy (LE) GAP callbacks to multiple synchronous subscribers.
- Prevents hub deinitialization while generation-checked client handles remain open.
- Arbitrates Classic inquiry, LE scan, legacy advertising, and extended-advertising instances with typed leases.
- Merges per-client Classic visibility requests: connectable requests use OR; discoverability uses the highest vote.
- Supports automatic pairing replies or application-managed replies through GAP subscriptions.
- Normalizes Classic inquiry results and LE extended-advertising reports.
- Provides LE bond lookup, security replies, privacy setup, connection control, and disconnection.
- Supports extended scanning, extended advertising, periodic advertising, periodic sync, and Periodic Advertising Sync Transfer (PAST) when enabled by the selected host and ESP-IDF configuration.

## Lifecycle and Resource Ownership

The application owns the hub lifecycle:

1. Initialize non-volatile storage (NVS) and call `esp_bt_hub_init()`.
2. Each application module opens a client, subscribes to GAP events, and registers with the native stack.
3. Call `esp_bt_hub_start()` after all application modules are prepared.
4. Acquire and release discovery, scan, or advertising leases only while the hub is running.
5. Call `esp_bt_hub_stop()` after every lease is released.
6. Remove GAP subscriptions, close clients, and release native profile registrations.
7. Call `esp_bt_hub_deinit()`.

A client handle identifies one application module that uses the shared hub. A lease grants that client exclusive ownership of a controller resource. A released handle is stale and remains invalid if its internal slot is later reused.

GAP subscriptions receive native event IDs and parameter types from the selected host stack. This provides access to all stack events, but couples callback code to the selected backend and ESP-IDF version.

## Public Headers

| Header | Purpose |
| --- | --- |
| `esp_bt_hub.h` | Hub lifecycle, clients, GAP subscriptions, local identity, and pairing configuration |
| `esp_bt_hub_classic.h` | Classic visibility voting, inquiry leases, and inquiry-result parsing |
| `esp_bt_hub_le.h` | LE scanning, advertising, security, privacy, connections, bonds, and report parsing |

## Configuration

Enable Bluetooth and select either the Bluedroid or NimBLE host stack before calling the Hub APIs.

For Bluedroid Classic and LE:

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_CLASSIC_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
```

For NimBLE:

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
```

Extended LE operations additionally depend on the corresponding ESP-IDF BLE 5.0 or NimBLE extended-advertising options. Applications can use the capability macros `ESP_BT_HUB_CLASSIC_SUPPORTED`, `ESP_BT_HUB_LE_SUPPORTED`, `ESP_BT_HUB_LE_LEGACY_SUPPORTED`, `ESP_BT_HUB_LE_EXT_SUPPORTED`, and `ESP_BT_HUB_LE_PAST_SUPPORTED` for conditional compilation.

`ESP_BT_HUB_CONFIG_DEFAULT()` produces these defaults:

- Device name `ESP-BT`
- Bonding and secure connections enabled
- Man-in-the-middle (MITM) protection disabled
- Input/output (I/O) capability `ESP_BT_HUB_IO_CAP_NONE`
- Automatic pairing replies enabled
- Classic PIN `0000`

Automatic replies accept Classic PIN and numeric confirmation requests and LE security and numeric-comparison requests. Passkey-entry requests are rejected. Set `pairing_auto_reply` to `false` when the application will reply to pairing events itself.

## Quick Start

Add the component to an ESP-IDF project:

```yaml
dependencies:
  espressif/esp_bt_hub: "^0.1"
```

Initialize NVS before the Bluetooth hub. The following example shows the required ownership and teardown order:

```c
#include "esp_bt_hub.h"
#include "nvs_flash.h"

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    esp_bt_hub_config_t config = ESP_BT_HUB_CONFIG_DEFAULT();
    esp_bt_hub_client_handle_t client = NULL;

    ESP_ERROR_CHECK(esp_bt_hub_init(&config));
    ESP_ERROR_CHECK(esp_bt_hub_client_open("app", &client));
    ESP_ERROR_CHECK(esp_bt_hub_start());

    /* Subscribe or acquire discovery, scan, and advertising leases here. */

    ESP_ERROR_CHECK(esp_bt_hub_stop());
    ESP_ERROR_CHECK(esp_bt_hub_client_close(client));
    ESP_ERROR_CHECK(esp_bt_hub_deinit());
}
```

The interactive console under `examples/bt_hub` demonstrates lifecycle, GAP fan-out, Classic inquiry, LE scanning, advertising, and resource-conflict handling.

## Usage Notes

- GAP subscribers run synchronously on the BTC task or NimBLE host task. They must not block, deinitialize the hub, or call an operation that waits for GAP completion.
- Native GAP parameters and advertising payloads returned by `esp_bt_hub_le_adv_report_parse()` remain valid only until the callback returns. Copy data that must be retained.
- `esp_bt_hub_stop()` returns `ESP_ERR_INVALID_STATE` while a runtime resource lease remains active.
- `esp_bt_hub_deinit()` returns `ESP_ERR_INVALID_STATE` until the runtime is stopped and every client and subscription is released.
- A stop or deinit failure changes the lifecycle to `ESP_BT_HUB_STATE_FAULT`; retry the operation that failed.
- `esp_bt_hub_le_ext_scan_stop()` stops scanning but retains the scan lease. Release an external lease explicitly, or call `esp_bt_hub_le_scan_stop()` for a lease created by the legacy scan API. Stop any extended scan started through `esp_bt_hub_le_ext_scan_start()` before releasing an external lease: the hub rejects the release while such a scan is active. Scans started through raw stack APIs are not tracked by the hub and remain the caller's responsibility.
- Extended advertising can end after a finite duration, an event limit, or a connection. Periodic advertising does not end automatically and must be stopped before its instance is released.
- On Bluedroid, `esp_bt_hub_le_privacy_enable()` loads bonded identities into the resolving list. On NimBLE, privacy follows the host configuration.
- On Bluedroid, the owner of a central connection attempt must call `esp_bt_hub_le_connect_complete()` for every
  terminal connect event, whether it reports success or failure.

## SoC and Host Compatibility

Bluetooth capabilities depend on both the target SoC and ESP-IDF configuration:

- Classic APIs are available only when Classic Bluetooth is enabled and the controller is not in BLE-only mode.
- NimBLE exposes LE APIs only; it does not provide Classic inquiry or Classic visibility voting.
- Legacy LE scan and advertising require NimBLE or Bluedroid BLE 4.2 support.
- Extended LE operations require NimBLE extended advertising or Bluedroid BLE 5.0 support.
- PAST requires extended LE support. Bluedroid also requires `CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER`.

## FAQ

**Why does `esp_bt_hub_client_close()` return `ESP_ERR_INVALID_STATE`?**

The client still owns a discovery, scan, or advertising lease. Stop or release that resource before closing the client.

**Why does `esp_bt_hub_deinit()` return `ESP_ERR_INVALID_STATE`?**

The runtime is still active, or at least one client or GAP subscription remains. Call `esp_bt_hub_stop()`, release native profile registrations, and then retry deinit.

**Why does a second discovery, scan, or legacy advertising request fail?**

These resources are exclusive. The current owner must stop or release its lease before another client can acquire it.

**Can multiple Bluetooth application modules share this component?**

Yes. Initialize the hub once and give each application module a separate named client. The clients share GAP events while resource leases prevent conflicting controller operations.
