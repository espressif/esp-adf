# Changelog

## v0.5.0

### Features

- Initial version of `adf_event_hub`
- Domain-scoped publish/subscribe event hub with queue and callback delivery modes
- Subscription filtering by event id set or ANY_ID wildcard
- Thread-safe module (internal mutex); callback delivery runs in the publisher task; queue mode uses caller-provided FreeRTOS queues with `adf_event_hub_delivery_done()` for shared payload lifetime
