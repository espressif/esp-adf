/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include "esp_err.h"
#include "adf_event_hub.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Service base module
 *
 *         This is the base class for all services. It provides:
 *         - Lifecycle operations via esp_service_ops_t (virtual methods)
 *         - State machine: UNINITIALIZED -> INITIALIZED -> RUNNING <-> PAUSED
 *         - All lifecycle API calls (start/stop/pause/resume) execute synchronously
 *           in the caller's task context, invoking the corresponding vtable op.
 *           Services that need a persistent background task create and manage it
 *           themselves inside on_init / on_start / on_stop.
 *         - Low-power hooks: optional on_lowpower_enter / on_lowpower_exit ops; invoked by
 *           esp_service_lowpower_enter() / esp_service_lowpower_exit(), which are simple
 *           direct calls — no state change, no SLEEPING state.
 *         - Event hub integration: optional adf_event_hub for publish-subscribe events
 *         - All functions return esp_err_t, results passed via output parameters
 *
 *         Subclass domain events (e.g. player track end, OTA progress) are not
 *         part of the base; each subclass defines its own event enum, payload,
 *         and set_event_cb; see examples/mock_services/components/ota_service
 *         for the pattern.
 */

/**
 * @brief  Event IDs published by the service base layer.
 *
 *         These are published automatically when a service has a bound event hub.
 *         Subscribers use these IDs in adf_event_subscribe_info_t::event_id.
 *
 *         Base-layer IDs occupy the top of the uint16_t range so that the low
 *         range [1 .. UINT16_MAX-2] is freely available for domain-specific events.
 *         ADF_EVENT_ANY_ID (UINT16_MAX) is the wildcard; derived services MUST NOT
 *         use UINT16_MAX or UINT16_MAX-1.
 */
#define ESP_SERVICE_EVENT_STATE_CHANGED  (UINT16_MAX - 1)  /*!< State transition; payload: esp_service_state_changed_payload_t */

/**
 * @brief  Get base service
 */
#define ESP_SERVICE_BASE(service)  ((esp_service_t *)(service))

/**
 * @brief  Default configuration macro
 */
#define ESP_SERVICE_CONFIG_DEFAULT()  {  \
    .name      = NULL,                   \
    .user_data = NULL,                   \
}

typedef struct esp_service esp_service_t;

/**
 * @brief  Service state enumeration
 */
typedef enum {
    ESP_SERVICE_STATE_UNINITIALIZED = 0,  /*!< Not initialized */
    ESP_SERVICE_STATE_INITIALIZED,        /*!< Initialized, ready to start */
    ESP_SERVICE_STATE_RUNNING,            /*!< Running normally */
    ESP_SERVICE_STATE_PAUSED,             /*!< Paused, can resume */
    ESP_SERVICE_STATE_STOPPING,           /*!< Closing or cleanup failed; only teardown retry is permitted */
    ESP_SERVICE_STATE_ERROR,              /*!< Error state */
    ESP_SERVICE_STATE_MAX                 /*!< Sentinel, do not use */
} esp_service_state_t;

/**
 * @brief  Payload for ESP_SERVICE_EVENT_STATE_CHANGED
 */
typedef struct {
    esp_service_state_t  old_state;  /*!< Previous state */
    esp_service_state_t  new_state;  /*!< New state */
} esp_service_state_changed_payload_t;

/**
 * @brief  Service base configuration
 */
typedef struct {
    const char *name;       /*!< Unique among live services; also used as event hub domain */
    void       *user_data;  /*!< User data passed to callbacks */
} esp_service_config_t;

/**
 * @brief  Lifecycle operations (virtual methods for derived services)
 *
 *         These are called by the base class at appropriate lifecycle points.
 *         Derived services implement these to add custom behavior.
 *         All methods are optional (NULL = no-op).
 *         Hooks and synchronous state notifications run outside the lifecycle
 *         lock. Overlapping lifecycle calls on the same instance return
 *         ESP_ERR_INVALID_STATE until the operation and its notification finish.
 *         Lifecycle calls from a synchronous Event Hub dispatch are rejected;
 *         use a notification to request the operation from the owner task.
 *         on_stop must stop/join asynchronous users before returning success.
 *         on_deinit releases private resources after event users drain; a
 *         failed on_deinit must leave the object safe for a cleanup retry.
 */
typedef struct esp_service_ops {
    esp_err_t (*on_init)(esp_service_t *service, const esp_service_config_t *config);  /*!< Called during init */
    esp_err_t (*on_deinit)(esp_service_t *service);                                    /*!< Called during deinit */
    esp_err_t (*on_start)(esp_service_t *service);                                     /*!< Called when starting */
    esp_err_t (*on_stop)(esp_service_t *service);                                      /*!< Called when stopping */
    esp_err_t (*on_pause)(esp_service_t *service);                                     /*!< Called when pausing */
    esp_err_t (*on_resume)(esp_service_t *service);                                    /*!< Called when resuming */
    esp_err_t (*on_lowpower_enter)(esp_service_t *service);                            /*!< Low-power entry */
    esp_err_t (*on_lowpower_exit)(esp_service_t *service);                             /*!< Low-power exit */
    const char *(*event_to_name)(uint16_t event_id);                                   /*!< Event name */
} esp_service_ops_t;

/**
 * @brief  Service base structure
 *
 *         Derived services embed this as the first member:
 * @code{c}
 *         typedef struct {
 *             esp_service_t base;  // Must be first
 *             // ... derived fields
 *         } my_service_t;
 * @endcode
 */
struct esp_service {
    const char                   *name;       /*!< Service name (owned copy); also used as event hub domain */
    _Atomic  esp_service_state_t  state;      /*!< Atomic state; readable from any task */
    const esp_service_ops_t      *ops;        /*!< Lifecycle operations vtable */
    void                         *user_data;  /*!< User context (from config, for service logic) */

    /* ── Internal: managed by esp_service, do not access directly ─ */
    esp_err_t          last_err;          /*!< Last error recorded by the base layer */
    adf_event_hub_t    event_hub;         /*!< Bound before on_init; protected by lock */
    _Atomic unsigned   publish_refs;      /*!< In-flight publish, subscribe and unsubscribe calls */
    SemaphoreHandle_t  lock;              /*!< Lifecycle mutex; retained until final release */
    StaticSemaphore_t  lock_storage;      /*!< Backing storage for lock */
    bool               lifecycle_active;  /*!< Operation in progress; protected by lock */
};

/**
 * @brief  Initialize service base
 *
 * @note  Zeroes `*service` and initializes the instance once; start/stop/pause/
 *        resume cycles may repeat, whereas calling esp_service_init() more than
 *        once on the same instance (live, closing, or after esp_service_deinit())
 *        is not supported.
 *        Must not run concurrently with any other call on that instance, and must
 *        not be called from an Event Hub dispatch callback. ops->on_init runs
 *        outside the lifecycle lock.
 *        Names must be unique among live services: equal names alias the same
 *        hub owner and therefore share subscription ownership.
 *
 * @param[in]  service  Service instance (caller allocates)
 * @param[in]  config   Configuration
 * @param[in]  ops      Lifecycle operations (may be NULL)
 *
 * @return
 *       - ESP_OK                 On success.
 *       - ESP_ERR_INVALID_ARG    service or config is NULL.
 *       - ESP_ERR_INVALID_STATE  Called from an Event Hub dispatch callback.
 *       - ESP_ERR_NO_MEM         Insufficient memory.
 *       - ESP_FAIL               Failed to create the lifecycle mutex.
 *       - other                  Error from ops->on_init.
 */
esp_err_t esp_service_init(esp_service_t *service, const esp_service_config_t *config, const esp_service_ops_t *ops);

/**
 * @brief  Deinitialize service base
 *
 *         Stops the service if active or in ERROR, drains owned event callbacks
 *         and in-flight publishes, then calls on_deinit and releases base
 *         resources, not object memory. Calling on an UNINITIALIZED instance is
 *         a no-op returning ESP_OK.
 *
 * @note  Single-owner operation: stop external API/MCP callers, unlink borrowed
 *        media endpoints and return borrowed frames and deliveries first.
 *        Do not call from an event or same-instance lifecycle callback, ISR,
 *        or a worker that on_stop must join. Event-dispatch calls are rejected
 *        without side effects. Use esp_media_service_deinit() for media objects.
 *        If on_stop fails, the instance stays in ERROR with event admission
 *        open and may be stopped or deinitialized again. Once teardown starts,
 *        failure retains the object in STOPPING; retry resumes cleanup without
 *        reopening event admission or calling on_stop. Only success permits the
 *        owner to release the object and borrowed user_data. The base never
 *        frees the object or user_data. Not ISR-safe; waits are unbounded.
 *
 * @param[in]  service  Service instance
 *
 * @return
 *       - ESP_OK                 On success or already UNINITIALIZED
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Operation active or in Event Hub dispatch
 *       - other                  Stop or cleanup failure; retry deinit, do not free the object
 */
esp_err_t esp_service_deinit(esp_service_t *service);

/**
 * @brief  Start service (INITIALIZED -> RUNNING)
 *
 *         Calls ops->on_start() synchronously in the caller's task context.
 *         If the service needs background work it should create its own task
 *         inside on_start and return immediately.
 *
 * @param[in]  service  Service instance
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument
 *       - ESP_ERR_INVALID_STATE  Not in INITIALIZED state (e.g. already RUNNING)
 *       - other                  Error returned by ops->on_start
 */
esp_err_t esp_service_start(esp_service_t *service);

/**
 * @brief  Stop service (any -> INITIALIZED)
 *
 *         Calls ops->on_stop() synchronously. Stopping an UNINITIALIZED or
 *         INITIALIZED service is a no-op that returns ESP_OK. If on_stop fails
 *         the service enters ERROR and the error is returned.
 *
 * @param[in]  service  Service instance
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    Invalid argument
 *       - ESP_ERR_INVALID_STATE  Event dispatch or a lifecycle operation is active
 *       - other                  Error returned by ops->on_stop
 */
esp_err_t esp_service_stop(esp_service_t *service);

/**
 * @brief  Pause service (RUNNING -> PAUSED)
 *
 * @param[in]  service  Service instance
 *
 * @return
 *       - ESP_OK                 On success.
 *       - ESP_ERR_INVALID_ARG    service is NULL.
 *       - ESP_ERR_INVALID_STATE  Not in RUNNING state.
 *       - other                  Error from ops->on_pause.
 */
esp_err_t esp_service_pause(esp_service_t *service);

/**
 * @brief  Resume service (PAUSED -> RUNNING)
 *
 * @param[in]  service  Service instance
 *
 * @return
 *       - ESP_OK                 On success.
 *       - ESP_ERR_INVALID_ARG    service is NULL.
 *       - ESP_ERR_INVALID_STATE  Not in PAUSED state.
 *       - other                  Error from ops->on_resume.
 */
esp_err_t esp_service_resume(esp_service_t *service);

/**
 * @brief  Get current service state
 *
 * @param[in]   service    Service instance
 * @param[out]  out_state  Output: current state
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 */
esp_err_t esp_service_get_state(const esp_service_t *service, esp_service_state_t *out_state);

/**
 * @brief  Check if service is running
 *
 * @param[in]   service      Service instance
 * @param[out]  out_running  Output: true if state is RUNNING
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 */
esp_err_t esp_service_is_running(const esp_service_t *service, bool *out_running);

/**
 * @brief  Publish a domain event through the service-bound Event Hub
 *
 *         Publishing is permitted only after successful initialization, not from on_init.
 *
 * @note  Payload ownership contract (stricter than adf_event_hub_publish()):
 *         - If release_cb is non-NULL, it is invoked EXACTLY ONCE for every
 *           invocation of this function, regardless of the return code
 *           (ESP_OK, INVALID_ARG, INVALID_STATE, NOT_FOUND, NO_MEM).
 *         - The caller MUST NOT free or otherwise access payload after this
 *           function returns; doing so results in a double-free / UAF.
 *         - release_cb may fire synchronously inside this call (callback-mode
 *           delivery or early error path), so any fields the caller needs to
 *           log/use must be captured into local variables BEFORE the call.
 *         - If release_cb is NULL, the caller keeps payload ownership on all
 *           paths (typical for stack/static/long-lived payloads).
 *
 * @param[in]  service      Service instance
 * @param[in]  event_id     Domain-local event id (must be > 0)
 * @param[in]  payload      Event payload pointer (may be NULL)
 * @param[in]  payload_len  Payload length in bytes
 * @param[in]  release_cb   Optional payload release callback; when non-NULL,
 *                          ownership of payload is transferred on every path
 * @param[in]  release_ctx  Optional release callback context
 *
 * @return
 *       - ESP_OK                 Event published.
 *       - ESP_ERR_INVALID_ARG    service is NULL or event_id is zero.
 *       - ESP_ERR_INVALID_STATE  Initialization incomplete, teardown started, or no hub bound.
 *       - ESP_ERR_NOT_FOUND      Hub is not a registered domain.
 *       - ESP_ERR_NO_MEM         Internal envelope allocation failed.
 */
esp_err_t esp_service_publish_event(esp_service_t *service,
                                    uint16_t event_id,
                                    const void *payload,
                                    uint16_t payload_len,
                                    adf_event_payload_release_cb_t release_cb,
                                    void *release_ctx);

/**
 * @brief  Subscribe to events on a domain through the service-bound Event Hub
 *
 * @note  Wraps adf_event_hub_subscribe(). At least one delivery target
 *         (target_queue or handler) must be set in info. Unlike publishing,
 *         subscription setup is permitted from on_init once the hub is bound.
 *
 * @param[in]  service  Service instance (must have a bound event hub)
 * @param[in]  info     Subscription parameters
 *
 * @return
 *       - ESP_OK                 Subscriber registered.
 *       - ESP_ERR_INVALID_ARG    service or info is NULL.
 *       - ESP_ERR_INVALID_STATE  No event hub bound.
 *       - ESP_ERR_NO_MEM         Subscriber storage allocation failed.
 */
esp_err_t esp_service_event_subscribe(esp_service_t *service, const adf_event_subscribe_info_t *info);

/**
 * @brief  Unsubscribe from events on a domain
 *
 * @note  Wraps adf_event_hub_unsubscribe(): returns once no other task is
 *         delivering to a removed subscription, except in the two-task
 *         handler cycle described in adf_event_hub.h, which is reported as
 *         ESP_ERR_NOT_FINISHED.
 *
 * @param[in]  service   Service instance (must have a bound event hub)
 * @param[in]  domain    Domain to unsubscribe from; NULL = service's own domain
 * @param[in]  event_id  Event ID filter; ADF_EVENT_ANY_ID removes all
 *
 * @return
 *       - ESP_OK                 At least one subscriber removed.
 *       - ESP_ERR_INVALID_ARG    service is NULL.
 *       - ESP_ERR_INVALID_STATE  No event hub bound.
 *       - ESP_ERR_NOT_FOUND      Domain not registered or no matching subscriber.
 *       - ESP_ERR_NOT_FINISHED   Subscriptions removed; a cyclically waiting handler still runs.
 *                                Keep its context valid until it returns.
 */
esp_err_t esp_service_event_unsubscribe(esp_service_t *service, const char *domain, uint16_t event_id);

/**
 * @brief  Release a queue-mode delivery reference
 *
 * @note  Must be called exactly once per received adf_event_delivery_t.
 *        Drain deliveries before replacing the hub or deinitializing the service.
 *
 * @param[in]  service   Service instance (must have a bound event hub)
 * @param[in]  delivery  Delivery item from the subscriber queue
 *
 * @return
 *       - ESP_OK                 Reference released.
 *       - ESP_ERR_INVALID_ARG    service or delivery is NULL.
 *       - ESP_ERR_INVALID_STATE  No event hub bound.
 */
esp_err_t esp_service_event_delivery_done(esp_service_t *service, adf_event_delivery_t *delivery);

/**
 * @brief  Get the event hub handle bound to a service
 *
 *         The returned handle is a snapshot, not a retained reference.
 *         Coordinate its use with the owner to avoid concurrent replacement or deinit.
 *
 * @param[in]   service  Service instance
 * @param[out]  out_hub  Output: event hub handle (NULL if not bound)
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  service or out_hub is NULL.
 */
esp_err_t esp_service_get_event_hub(const esp_service_t *service, adf_event_hub_t *out_hub);

/**
 * @brief  Get last error recorded by the base layer (e.g. on_start failed)
 *
 *         Returns the error code from the most recent failed lifecycle call.
 *         ESP_OK if no error has occurred.
 *
 * @param[in]   service  Service instance
 * @param[out]  out_err  Output: last error code (ESP_OK if none)
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 */
esp_err_t esp_service_get_last_error(const esp_service_t *service, esp_err_t *out_err);

/**
 * @brief  Notify service to enter low-power mode
 *
 *         Calls ops->on_lowpower_enter if provided, serialized with the other
 *         lifecycle operations by the lifecycle lock. It does not change the
 *         service lifecycle state. The hook is not run on an instance that is
 *         UNINITIALIZED or STOPPING, since its derived context is released.
 *         Typical use: suspend radio, turn off LED, reduce clock, etc.
 *
 * @param[in]  service  Service instance
 *
 * @return
 *       - ESP_OK                 On success or on_lowpower_enter is NULL
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is UNINITIALIZED or a deinit is in progress
 *       - other                  Error returned by on_lowpower_enter
 */
esp_err_t esp_service_lowpower_enter(esp_service_t *service);

/**
 * @brief  Notify service to exit low-power mode
 *
 *         Calls ops->on_lowpower_exit if provided, serialized with the other
 *         lifecycle operations by the lifecycle lock. It does not change the
 *         service lifecycle state. The hook is not run on an instance that is
 *         UNINITIALIZED or STOPPING, since its derived context is released.
 *
 * @param[in]  service  Service instance
 *
 * @return
 *       - ESP_OK                 On success or on_lowpower_exit is NULL
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service is UNINITIALIZED or a deinit is in progress
 *       - other                  Error returned by on_lowpower_exit
 */
esp_err_t esp_service_lowpower_exit(esp_service_t *service);

/**
 * @brief  Get service name
 *
 * @param[in]   service   Service instance
 * @param[out]  out_name  Output: service name pointer
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 */
esp_err_t esp_service_get_name(const esp_service_t *service, const char **out_name);

/**
 * @brief  Get user data
 *
 * @param[in]   service   Service instance
 * @param[out]  out_data  Output: user data pointer
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 */
esp_err_t esp_service_get_user_data(const esp_service_t *service, void **out_data);

/**
 * @brief  Replace the user-data pointer held by the base layer
 *
 *         Typical use: a subclass clears `user_data` in `on_deinit` after freeing
 *         its private context. Does not free the previous pointer.
 *
 * @param[in]  service    Service instance
 * @param[in]  user_data  New value (may be NULL)
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  service is NULL
 */
esp_err_t esp_service_set_user_data(esp_service_t *service, void *user_data);

/**
 * @brief  Replace the Event Hub handle bound to the service
 *
 *         Assigns the internal hub pointer only; the previous handle is not
 *         destroyed. The service owns one reference to whatever hub is bound:
 *         esp_service_deinit() calls adf_event_hub_destroy() on it. So the
 *         caller must (a) destroy or retain the previous handle (via
 *         `esp_service_get_event_hub`) before calling, and (b) pass a hub
 *         reference the service may release — e.g. obtain a shared hub with
 *         an extra adf_event_hub_create() of the same domain, which adds a
 *         reference that deinit will drop.
 *         The caller must quiesce event users before replacing the hub.
 *
 * @param[in]  service  Service instance
 * @param[in]  hub      Hub handle to store (may be NULL)
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  Service not ready, closing, or hub in use
 */
esp_err_t esp_service_set_event_hub(esp_service_t *service, adf_event_hub_t hub);

/**
 * @brief  Convert state to string
 *
 * @param[in]   state    Service state
 * @param[out]  out_str  Output: state name string
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Invalid argument or state
 */
esp_err_t esp_service_state_to_str(esp_service_state_t state, const char **out_str);

/**
 * @brief  Get human-readable name for a domain event ID
 *
 * @note  The base STATE_CHANGED event is handled by the core layer.
 *         Domain-specific events are delegated to ops->event_to_name if provided.
 *
 * @param[in]   service   Service instance
 * @param[in]   event_id  Event ID to look up
 * @param[out]  out_name  Output: event name string (may be NULL if unknown)
 *
 * @return
 *       - ESP_OK               Name found
 *       - ESP_ERR_INVALID_ARG  Invalid argument
 *       - ESP_ERR_NOT_FOUND    No name mapping for this event ID
 */
esp_err_t esp_service_get_event_name(const esp_service_t *service, uint16_t event_id, const char **out_name);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
