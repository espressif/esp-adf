# ADF Event Hub

[中文版](./README_CN.md)

**ADF Event Hub** is a lightweight, domain-scoped publish-subscribe facility for ADF / GMF components. Every hub handle represents one source domain (e.g. `"wifi"` or `"ota"`); publishers post `adf_event_t` items to that domain and subscribers receive matching events in either queue or callback mode. The module tolerates arbitrary startup ordering: a subscriber may register before the target domain's publisher is created.

## Key Features

- **Domain-scoped pub/sub** — one hub handle per source domain; subscribers filter by `(domain, event_id)` with a `ADF_EVENT_ANY_ID` wildcard
- **Two delivery modes** — queue mode (non-blocking `xQueueSend`) or synchronous callback mode; selected per-subscriber
- **Startup-order tolerant** — `adf_event_hub_subscribe()` auto-creates the target domain if the publisher has not yet called `adf_event_hub_create()`
- **Ref-counted envelope delivery** — optional `release_cb` is invoked exactly once per successful publish: immediately if there are no queue-mode subscribers, or after the last `adf_event_hub_delivery_done()` call; safe for heap-allocated payloads on all code paths
- **Best-effort queue delivery** — a full subscriber queue drops that subscriber's copy only; other subscribers still receive the event
- **Thread-safe** — a single internal mutex protects all public APIs; safe to call from any task (not ISR-safe)
- **Introspection** — `adf_event_hub_get_stats()` returns per-domain subscriber counts and envelope-pool usage; `adf_event_hub_dump()` logs the full state

## How It Works

```
Publisher task                   adf_event_hub                  Subscriber(s)
     │                                │                              │
     │── adf_event_hub_publish ──────▶│                              │
     │                                │── callback handler (sync) ──▶│  (callback mode)
     │                                │                              │
     │                                │── xQueueSend (non-blocking) ▶│  (queue mode)
     │                                │                              │── adf_event_hub_delivery_done ──▶
     │                                │                              │
     │◀───────────────── release_cb (after last delivery_done) ──────│
```

Events are shallow-copied into a ref-counted envelope slot. Callback-mode subscribers are invoked synchronously in the publisher's task; queue-mode subscribers get an `adf_event_delivery_t` item and must call `adf_event_hub_delivery_done()` exactly once per delivery to release the envelope reference.

## Configuration

`adf_event_hub` has no module-level configuration struct; state is per-hub and set at `create` / `subscribe` time.

### adf_event_subscribe_info_t

| Field          | Type                  | Description                                                       | Default                 |
|----------------|-----------------------|-------------------------------------------------------------------|-------------------------|
| `event_domain` | `const char *`        | Domain to subscribe to; `NULL` means the subscribing hub's domain | `NULL`                  |
| `event_id`     | `uint16_t`            | Event ID filter; `ADF_EVENT_ANY_ID` matches all                   | `ADF_EVENT_ANY_ID`      |
| `target_queue` | `QueueHandle_t`       | Queue-mode inbox; non-NULL selects queue mode                     | `NULL`                  |
| `handler`      | `adf_event_handler_t` | Callback-mode handler; used when `target_queue` is `NULL`         | `NULL`                  |
| `handler_ctx`  | `void *`              | Opaque context forwarded to `handler`                             | `NULL`                  |

Always initialize with `ADF_EVENT_SUBSCRIBE_INFO_DEFAULT()` and override only the fields you need. If both `target_queue` and `handler` are set, queue mode wins and `handler` is ignored.

## Quick Start

```c
#include "adf_event_hub.h"

#define WIFI_EVT_CONNECTED  1

static void on_wifi_event(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    printf("wifi event_id=%u payload_len=%u\n", event->event_id, event->payload_len);
}

void app_main(void)
{
    /* 1. A domain owner creates its hub. */
    adf_event_hub_t wifi_hub = NULL;
    ESP_ERROR_CHECK(adf_event_hub_create("wifi", &wifi_hub));

    /* 2. A subscriber creates its own hub (for identification) and subscribes
     *    to another domain.  Callback mode is chosen because target_queue is NULL. */
    adf_event_hub_t app_hub = NULL;
    ESP_ERROR_CHECK(adf_event_hub_create("app", &app_hub));

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "wifi";
    info.event_id     = WIFI_EVT_CONNECTED;
    info.handler      = on_wifi_event;
    ESP_ERROR_CHECK(adf_event_hub_subscribe(app_hub, &info));

    /* 3. Publisher posts an event. */
    adf_event_t ev = {
        .domain      = "wifi",
        .event_id    = WIFI_EVT_CONNECTED,
        .payload     = NULL,
        .payload_len = 0,
    };
    ESP_ERROR_CHECK(adf_event_hub_publish(wifi_hub, &ev, NULL, NULL));
}
```

### Queue-Mode Subscriber

```c
QueueHandle_t inbox = xQueueCreate(8, sizeof(adf_event_delivery_t));

adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
info.event_domain = "wifi";
info.target_queue = inbox;
adf_event_hub_subscribe(app_hub, &info);

adf_event_delivery_t delivery;
if (xQueueReceive(inbox, &delivery, portMAX_DELAY) == pdTRUE) {
    /* ... handle delivery.event ... */
    adf_event_hub_delivery_done(app_hub, &delivery);  /* release envelope reference */
}
```

### Heap-allocated Payload with Release Callback

```c
static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    free((void *)payload);  /* invoked exactly once after the last delivery_done */
}

char *msg = strdup("hello");
adf_event_t ev = { .domain = "wifi", .event_id = 7, .payload = msg, .payload_len = strlen(msg) + 1 };
adf_event_hub_publish(wifi_hub, &ev, release_payload, NULL);
```

## Typical Scenarios

- **Service decoupling** — each service (`wifi`, `ota`, `player`, …) publishes into its own domain; consumers subscribe without direct linkage
- **Late-binding subscribers** — UI / logging tasks subscribe at startup even when the publishing service has not finished init yet
- **Bulk / batched consumers** — use queue mode for event aggregators and drain the inbox from a dedicated worker task
- **Fine-grained callbacks** — use callback mode for latency-sensitive handlers that can run in the publisher's task

## Example Project

A multi-service simulation that exercises the hub end-to-end is available at:

```
components/adf_event_hub/examples/
```

The same service sources build on both platforms:

```bash
# PC host (FreeRTOS POSIX simulator)
cd components/adf_event_hub/examples/host
cmake -S . -B build -G Ninja
cmake --build build
./build/event_hub_example

# ESP-IDF target (flashes to a chip)
cd components/adf_event_hub/examples
idf.py set-target esp32s3
idf.py build flash monitor
```

## Testing

Unit tests live under [`test_apps/`](test_apps/) and share a single source-of-truth suite that compiles on both PC host and ESP target. See [`test_apps/README.md`](test_apps/README.md) for build, run, sanitizer, and coverage instructions.
