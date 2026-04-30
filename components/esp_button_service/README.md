# Button Service

[中文版](./README_CN.md)

**Button Service** is a lightweight `esp_service_t` subclass that integrates `esp_board_manager` button devices into the ADF event system. It automatically discovers every board-manager device of type `button`, registers `iot_button` callbacks for the selected event set, and forwards button actions as typed service events — no manual device-name list required.

> **Version Requirements:** Requires `espressif/esp_board_manager *`.

## Key Features

- **Zero-configuration discovery** — iterates all `ESP_BOARD_DEVICE_TYPE_BUTTON` devices registered in `esp_board_manager`; GPIO and ADC multi-button groups are both supported
- **Selective event registration** — choose exactly which `iot_button` callbacks to install via a bitmask (`event_mask`), minimizing CPU overhead
- **Lifecycle-aware forwarding** — events are published only while the service is running; forwarding is automatically suppressed on stop/pause/low-power-enter and resumed on start/resume/low-power-exit
- **Typed payloads** — every event carries a `esp_button_service_payload_t` with the board-manager device label, making multi-button fan-out trivial
- **Standard service API** — inherits `esp_service_start`, `esp_service_stop`, `esp_service_event_subscribe`, etc. from `esp_service`

## How It Works

```
esp_board_manager                esp_button_service                 Application
      │                               │                              │
      │── ESP_BOARD_DEVICE_TYPE_BUTTON devices ──▶ discover & init  │
      │                               │                              │
  iot_button ──── callback ──────────▶│                              │
                                      │── esp_service_publish_event ▶│
                                      │   (ESP_BUTTON_SERVICE_EVT_*, label)         │
```

`esp_button_service_create()` scans the board-manager registry, initializes each button device, and registers the selected `iot_button` callbacks.
After `esp_service_start()`, every button action is published through the service event hub so any number of subscribers can react to it.

## Configuration

### esp_button_service_cfg_t

| Field        | Type       | Description                                                    | Default             |
|--------------|------------|----------------------------------------------------------------|---------------------|
| `name`       | `const char *` | Service name used as the event hub domain; `NULL` → `"esp_button_service"` | `"esp_button_service"` |
| `event_mask` | `uint32_t` | OR of `ESP_BUTTON_SERVICE_EVT_MASK_xxx` constants; selects registered callbacks | `ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT` |

### Event Mask Constants

| Mask Constant                  | Corresponding Event ID          |
|--------------------------------|---------------------------------|
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_DOWN`      | `ESP_BUTTON_SERVICE_EVT_PRESS_DOWN`            |
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_UP`        | `ESP_BUTTON_SERVICE_EVT_PRESS_UP`              |
| `ESP_BUTTON_SERVICE_EVT_MASK_SINGLE_CLICK`    | `ESP_BUTTON_SERVICE_EVT_SINGLE_CLICK`          |
| `ESP_BUTTON_SERVICE_EVT_MASK_DOUBLE_CLICK`    | `ESP_BUTTON_SERVICE_EVT_DOUBLE_CLICK`          |
| `ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_START`| `ESP_BUTTON_SERVICE_EVT_LONG_PRESS_START`      |
| `ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_HOLD` | `ESP_BUTTON_SERVICE_EVT_LONG_PRESS_HOLD`       |
| `ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_UP`   | `ESP_BUTTON_SERVICE_EVT_LONG_PRESS_UP`         |
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_REPEAT`    | `ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT`          |
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_REPEAT_DONE`| `ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT_DONE`    |
| `ESP_BUTTON_SERVICE_EVT_MASK_PRESS_END`       | `ESP_BUTTON_SERVICE_EVT_PRESS_END`             |

`ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT` enables `PRESS_DOWN`, `PRESS_UP`, `SINGLE_CLICK`, `DOUBLE_CLICK`, `LONG_PRESS_START`, and `LONG_PRESS_UP`.

## Quick Start

```c
#include "esp_board_manager.h"
#include "esp_service.h"
#include "adf_event_hub.h"
#include "esp_button_service.h"

static void on_button_event(const adf_event_t *event, void *ctx)
{
    const esp_button_service_payload_t *pl = (const esp_button_service_payload_t *)event->payload;
    printf("button=%s event_id=%u\n", pl ? pl->label : "?", event->event_id);
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_board_manager_init());

    esp_button_service_cfg_t cfg = {
        .name       = "esp_button_service",
        .event_mask = ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT,
    };
    esp_button_service_t *svc = NULL;
    ESP_ERROR_CHECK(esp_button_service_create(&cfg, &svc));

    esp_service_t *base = (esp_service_t *)svc;

    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_id = ADF_EVENT_ANY_ID;
    sub.handler  = on_button_event;
    ESP_ERROR_CHECK(esp_service_event_subscribe(base, &sub));

    ESP_ERROR_CHECK(esp_service_start(base));
}
```

## Event Payload

Every `ESP_BUTTON_SERVICE_EVT_*` event carries a heap-allocated `esp_button_service_payload_t` that is freed automatically after all subscribers have received it.

```c
typedef struct {
    const char *label;  /* board-manager device name; valid for the process lifetime */
} esp_button_service_payload_t;
```

For ADC multi-button groups the label is the per-button label from the board-manager device configuration (`button_labels[]` in `dev_button_config_t`), not the device name itself.

## Typical Scenarios

- **Single GPIO button** — press/release detection for a simple user-input key
- **ADC multi-button group** — one ADC channel with multiple voltage-level buttons (e.g., volume up/down, play/pause on a development board)
- **Low-power wake-up** — the service resumes event forwarding automatically when the system exits low-power mode, so no application-level glue is needed

## Example Project

A complete multi-button example (GPIO + ADC group, full event subscription flow) is available at:

```
components/esp_button_service/examples/esp_button_service_example
```
