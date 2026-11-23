# Changelog

## v1.0.0-beta1

### Features

- Added `adf_event_hub_unsubscribe_all()` to remove and drain an owner's subscriptions without releasing its hub reference
- Added `adf_event_hub_is_dispatching()`, including synchronous payload finalizers and early publish exits
- Added shared lifecycle regressions for ownership, cancellation, reference counting and concurrent dispatch
- Added a saturating queue-full delivery counter; repeated drops no longer flood the log

### Bugfix

- Kept payloads alive throughout mixed callback/queue dispatch, copied queued event domains and made `delivery_done()` idempotent
- Restricted subscription removal to its owner and cancelled removed entries in every in-flight snapshot
- Drained executing callbacks on other tasks; cyclic removal waits return `ESP_ERR_NOT_FINISHED` while retaining the caller's context-lifetime responsibility
- Cleared wait links before unlinking stack dispatch records and prevented envelope generation from colliding with the empty-delivery sentinel
- Rejected domain hash collisions, including subscriptions targeting the caller's own domain
- Made module initialization checks atomic
- Included the public header's boolean type definition for standalone C compilation
- Joined example worker tasks before releasing instances, unsubscribed before draining/deleting queues, and rolled back failed subscriptions
- Copied statistics domain names into caller-owned buffers and rejected insufficient capacity without partial writes
- Balanced test create references and checked active-domain/envelope baselines after each case

### Changes

- Domains remain registered while they have create references or subscribers and are collected when both reach zero
- Replaced the key-index table with a single hashed domain list
- Bounded subscriptions per domain and in-flight envelopes; exhaustion returns `ESP_ERR_NO_MEM`
- Removed unused PC critical-section compatibility macros and added component packaging metadata
- Made `heap` and `esp_system` private implementation dependencies

## v0.5.0~1

### Changes

- Added Component Registry badge to README.md and README_CN.md

## v0.5.0

### Features

- Initial version of `adf_event_hub`
- Domain-scoped publish/subscribe event hub with queue and callback delivery modes
- Subscription filtering by event id set or ANY_ID wildcard
- Thread-safe module (internal mutex); callback delivery runs in the publisher task; queue mode uses caller-provided FreeRTOS queues with `adf_event_hub_delivery_done()` for shared payload lifetime
