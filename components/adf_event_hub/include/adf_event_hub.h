/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "adf_event_hub_port.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * adf_event_hub — domain-scoped publish-subscribe event facility.
 *
 * Each hub handle represents one source domain (e.g. "wifi", "ota").
 * Publishers call adf_event_hub_publish(); subscribers receive events via
 * either a target_queue (queue mode, takes precedence when both are set) or
 * a handler callback invoked synchronously in the publisher's task.
 *
 * Lifecycle: create() -> subscribe / publish / unsubscribe -> destroy().
 * subscribe() may target a domain before its publisher calls create(); the
 * domain is auto-created without a publisher reference, and a later create()
 * returns the same handle. A domain is removed once it has no create()
 * references and no subscribers.
 *
 * Ownership: adf_event_t fields are shallow-copied. For callback-mode
 * delivery the caller's domain string and payload must stay valid until
 * publish() returns. For queue-mode delivery the hub copies the domain
 * string and owns the payload until the last delivery_done() (see
 * adf_event_payload_release_cb_t).
 *
 * Conventions:
 *   - Thread-safe (internal mutex); publish() never blocks on a queue or a
 *     subscriber; not ISR-safe.
 *   - Callbacks must not call adf_event_hub_publish() recursively.
 *   - unsubscribe() and destroy() wait until no other task is delivering to
 *     one of the removed subscriptions, so the caller must not hold a lock
 *     that a running handler may need. The wait polls once per tick and has
 *     no upper bound: it lasts as long as the handler being delivered to runs.
 *     Subscriptions already snapshotted by a running publish() but not yet
 *     delivered are skipped. A handler may therefore unsubscribe or destroy
 *     its own hub and free its handler contexts as soon as the call returns
 *     ESP_OK, even while other tasks publish to the same domain. A subscriber
 *     queue may still hold deliveries: drain it with delivery_done() before
 *     deleting it. The one exception is a cycle: when the handler being
 *     waited for is itself waiting for the caller's handler (two handlers on
 *     two tasks removing each other's subscriptions), the caller does not
 *     wait for it, logs a warning and returns ESP_ERR_NOT_FINISHED; the
 *     subscriptions are removed all the same, but that handler is still
 *     running, so its context must stay valid until it has returned.
 *   - All APIs except create() require at least one hub to exist, otherwise
 *     ESP_ERR_INVALID_STATE is returned.
 *   - Domain identity is case-sensitive: "WiFi" and "wifi" are different
 *     domains. Use the same spelling in adf_event_hub_create(),
 *     adf_event_hub_subscribe() (event_domain), adf_event_hub_unsubscribe(),
 *     and in adf_event_t.domain when subscribers compare that string.
 *
 * Minimal usage:
 *
 *   adf_event_hub_t wifi_hub;
 *   adf_event_hub_create("wifi", &wifi_hub);
 *
 *   adf_event_t ev = { .domain = "wifi", .event_id = 1, .payload = NULL };
 *   adf_event_hub_publish(wifi_hub, &ev, NULL, NULL);
 *
 *   adf_event_hub_t svc_hub;
 *   adf_event_hub_create("my_service", &svc_hub);
 *
 *   adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
 *   info.event_domain = "wifi";
 *   info.event_id     = 1;
 *   info.handler      = my_wifi_handler;
 *   adf_event_hub_subscribe(svc_hub, &info);
 */

/** Wildcard event ID: matches any event_id in subscribe / unsubscribe. */
#define ADF_EVENT_ANY_ID  (UINT16_MAX)

/**
 * Zero-initialise an adf_event_subscribe_info_t with event_id set to
 * ADF_EVENT_ANY_ID and all other fields NULL.
 *
 * Usage:
 *   adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
 *   info.event_domain = "wifi";
 *   info.handler      = my_handler;
 */
#define ADF_EVENT_SUBSCRIBE_INFO_DEFAULT()  {  \
    NULL, ADF_EVENT_ANY_ID, NULL, NULL, NULL   \
}

/**
 * Opaque hub handle. Do not perform pointer arithmetic or semantic
 * comparisons on this value.
 */
typedef void *adf_event_hub_t;

/**
 * @brief  Payload release callback.
 *
 * @note  Called exactly once per successful adf_event_hub_publish() invocation
 *        (i.e. publish() that returns anything other than ESP_ERR_INVALID_ARG
 *        due to a NULL hub or event):
 *          - immediately after publish() when there are no queue-mode
 *            subscribers (or queue-envelope allocation failed);
 *          - otherwise after the last delivery_done() call for queue-mode
 *            subscribers, or immediately on queue-full drops.
 *        Publish also invokes release_cb on its error paths (module not
 *        initialised, hub not registered, snapshot/envelope OOM) so callers
 *        never leak a heap-allocated payload when publish returns an error.
 *        The callee is responsible for freeing payload if it was heap-allocated.
 *
 * @param[in]  payload  Pointer passed as event.payload at publish time.
 * @param[in]  ctx      Opaque context passed to adf_event_hub_publish().
 */
typedef void (*adf_event_payload_release_cb_t)(const void *payload, void *ctx);

/**
 * @brief  Event descriptor passed to subscribers and to publish.
 *
 * @note  See file-level "Ownership" for shallow-copy and lifetime rules.
 *        Routing uses the publisher's hub (registered domain); event->domain
 *        is delivered to subscribers unchanged. Use the same spelling as at
 *        create() time if code compares event->domain (see file-level Conventions).
 */
typedef struct {
    const char *domain;       /*!< Source domain label (e.g. "wifi"); not normalized */
    uint16_t    event_id;     /*!< Application-defined event identifier */
    const void *payload;      /*!< Optional event data; may be NULL */
    uint16_t    payload_len;  /*!< Byte length of payload; 0 when payload is NULL */
} adf_event_t;

/**
 * @brief  Synchronous event callback for callback-mode subscribers.
 *
 * @note  Invoked in the publisher's task context. Must not block for a
 *        significant time. Not invoked from ISR context.
 *
 * @param[in]  event  Pointer to the delivered event descriptor.
 * @param[in]  ctx    Opaque context registered at subscribe time.
 */
typedef void (*adf_event_handler_t)(const adf_event_t *event, void *ctx);

/**
 * @brief  Item delivered to a queue-mode subscriber.
 *
 * @note  Caller must call adf_event_hub_delivery_done() exactly once after
 *        processing to release the shared payload reference.
 */
typedef struct {
    adf_event_t  event;    /*!< Shallow copy of the published event */
    uint32_t     _opaque;  /*!< Internal envelope index + generation; do not modify */
} adf_event_delivery_t;

/**
 * @brief  Subscription parameters for adf_event_hub_subscribe().
 *
 * @note  See file-level comment for queue/callback mode semantics.
 *        Use ADF_EVENT_SUBSCRIBE_INFO_DEFAULT() to zero-initialise, then set
 *        the fields you need.
 */
typedef struct {
    const char          *event_domain;  /*!< Domain to subscribe to; NULL = hub's own domain; case-sensitive */
    uint16_t             event_id;      /*!< Event ID filter; ADF_EVENT_ANY_ID matches all */
    QueueHandle_t        target_queue;  /*!< Queue-mode inbox; non-NULL enables queue mode */
    adf_event_handler_t  handler;       /*!< Callback-mode handler; used when target_queue is NULL */
    void                *handler_ctx;   /*!< Opaque context forwarded to handler */
} adf_event_subscribe_info_t;

/**
 * @brief  Per-domain statistics returned by adf_event_hub_get_stats().
 */
typedef struct {
    char   *domain;                  /*!< Caller-provided name buffer; owns its copied contents */
    size_t  domain_capacity;         /*!< Name buffer capacity including the terminator */
    size_t  cb_subscriber_count;     /*!< Callback-mode subscriber count */
    size_t  queue_subscriber_count;  /*!< Queue-mode subscriber count */
} adf_event_domain_stat_t;

/**
 * @brief  Statistics snapshot returned by adf_event_hub_get_stats().
 *
 * @note  Set domains and domains_capacity before calling; all other fields
 *        are populated by adf_event_hub_get_stats().
 */
typedef struct {
    size_t                   domain_count;        /*!< Total registered domains */
    size_t                   envelope_pool_size;  /*!< Total envelope slots (only grows) */
    size_t                   envelopes_in_use;    /*!< Currently active (referenced) envelopes */
    uint32_t                 queue_drop_count;    /*!< Queue-full deliveries since bootstrap; saturates at UINT32_MAX */
    adf_event_domain_stat_t *domains;             /*!< Caller-provided array; filled on return */
    size_t                   domains_capacity;    /*!< Capacity of the domains array */
} adf_event_hub_stats_t;

/**
 * @brief  Register a domain and return its hub handle.
 *
 * @note  Reference-counted. Each successful call increments the internal
 *        reference count of the domain — whether the domain was newly created
 *        or already existed. Every call to adf_event_hub_create() MUST be
 *        balanced by exactly one call to adf_event_hub_destroy(); the domain
 *        is physically removed only when the count reaches zero and no
 *        subscribers remain on it. The handle value is derived from the
 *        domain name, so a domain re-created after its last destroy()
 *        yields the same handle.
 *        This is a domain identity, not an allocation identity. After releasing
 *        a create reference, do not reuse that handle without a new create();
 *        stale-handle detection across destroy/re-create is not provided.
 *
 *        Use this balanced create/destroy pair as the idiomatic way to "retain"
 *        a handle that will outlive the immediate creator (e.g. when a shared
 *        hub is handed to a service that will destroy it on deinit).
 *
 *        The module copies the domain string; the caller need not keep it
 *        alive after this call.
 *
 * @param[in]   domain   Non-NULL, non-empty domain name (e.g. "wifi"). Matching is
 *                       case-sensitive (see file-level Conventions).
 * @param[out]  out_hub  Receives the hub handle on success. Store the result
 *                       as `adf_event_hub_t hub` and pass hub (not &hub) to
 *                       all subsequent APIs.
 *
 * @return
 *       - ESP_OK                 Domain registered (or retained); *out_hub is valid.
 *       - ESP_ERR_INVALID_ARG    domain or out_hub is NULL, or domain is empty.
 *       - ESP_ERR_INVALID_STATE  A different domain name already maps to the
 *                                same handle (hash collision); pick another name.
 *       - ESP_ERR_NO_MEM         Allocation failed, or the domain already holds
 *                                UINT16_MAX references.
 */
esp_err_t adf_event_hub_create(const char *domain, adf_event_hub_t *out_hub);

/**
 * @brief  Release one reference to a domain hub.
 *
 * @note  Reference-counted counterpart to adf_event_hub_create(). Each call
 *        decrements the domain's reference count. When the count reaches
 *        zero, every subscription created through this hub (on any domain)
 *        is removed and the call waits until no other task is still
 *        delivering to one of them (a dispatch running on the calling task
 *        skips them instead); subscriptions other hubs hold on this domain
 *        stay registered and the domain is removed once they are gone too.
 *
 *        In-flight deliveries already queued in subscriber inboxes are not
 *        invalidated when the domain is finally removed; drain subscriber
 *        queues before the last destroy call. The caller must set hub to NULL
 *        after the call returns.
 *
 * @param[in]  hub  Hub handle returned by adf_event_hub_create().
 *
 * @return
 *       - ESP_OK                Reference released. Domain removed if refcount
 *                               reached zero; otherwise removal is deferred.
 *       - ESP_ERR_NOT_FINISHED  Reference released and subscriptions removed, but
 *                               a handler of one of them is still running on a
 *                                task that is itself waiting for the caller (see
 *                                file-level Conventions); keep its context valid.
 *       - ESP_ERR_INVALID_ARG    hub is NULL.
 *       - ESP_ERR_INVALID_STATE  Module not initialised.
 *       - ESP_ERR_NOT_FOUND      hub does not hold a create() reference.
 */
esp_err_t adf_event_hub_destroy(adf_event_hub_t hub);

/**
 * @brief  Check whether the calling task is inside an event dispatch
 * @note  Includes synchronous publish payload-release callbacks, even when
 *        publication has no matching subscribers or exits early with an error.
 *        Deferred delivery_done payload release is not a publish dispatch.
 * @param[out]  out_dispatching  True during any dispatch on the calling task
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  out_dispatching is NULL
 */
esp_err_t adf_event_hub_is_dispatching(bool *out_dispatching);

/**
 * @brief  Remove all subscriptions owned by hub and wait for their callbacks
 *
 * @note  Idempotent, independent of create-reference count. Keeps the hub
 *        reference. Do not hold locks needed by callbacks. Not ISR-safe.
 *        Queue owners must still drain queued deliveries before deleting queues.
 * @param[in]  hub  Subscription owner
 * @return
 *       - ESP_OK                 All owned callbacks on other tasks have returned
 *       - ESP_ERR_INVALID_ARG    hub is NULL
 *       - ESP_ERR_INVALID_STATE  hub has no live create reference
 *       - ESP_ERR_NOT_FINISHED   A cyclic callback wait was skipped; keep contexts alive
 */
esp_err_t adf_event_hub_unsubscribe_all(adf_event_hub_t hub);

/**
 * @brief  Register a subscriber on a domain.
 *
 * @note  At least one delivery target (target_queue or handler) must be set.
 *        If info->event_domain names a domain not yet created, that domain is
 *        auto-created (see file-level "Lifecycle").
 *        event_domain matching is case-sensitive (see file-level Conventions).
 *
 * @param[in]  hub   Hub handle of the calling service's context.
 * @param[in]  info  Subscription parameters; event_domain NULL = hub's own domain.
 *
 * @return
 *       - ESP_OK                 Subscriber registered.
 *       - ESP_ERR_INVALID_ARG    hub or info is NULL; both target_queue and handler
 *                                are NULL; event_domain is an empty string.
 *       - ESP_ERR_INVALID_STATE  Module not initialised, or event_domain collides
 *                                with a different registered domain name.
 *       - ESP_ERR_NOT_FOUND      hub does not hold a create() reference.
 *       - ESP_ERR_NO_MEM         Domain auto-create or subscriber vector grow
 *                                failed, or the domain reached its subscriber limit.
 */
esp_err_t adf_event_hub_subscribe(adf_event_hub_t hub, const adf_event_subscribe_info_t *info);

/**
 * @brief  Remove subscriptions from a domain.
 *
 * @note  Removes the subscriptions created through hub whose event_id matches
 *        the filter. Subscriptions created through other hubs are untouched.
 *        Returns only after no other task is still delivering to one of the
 *        removed subscriptions, so the removed handler and its context can be
 *        released safely; a removed queue must still be drained with
 *        adf_event_hub_delivery_done() before it is deleted. When called from
 *        a handler of the same dispatch, the removed subscriptions that
 *        dispatch has not reached yet are skipped instead.
 *
 * @param[in]  hub       Hub handle of the calling service's context.
 * @param[in]  domain    Domain to unsubscribe from; NULL = hub's own domain.
 *                       Case-sensitive; must match the spelling used at subscribe/create.
 * @param[in]  event_id  Event ID filter; ADF_EVENT_ANY_ID removes all.
 *
 * @return
 *       - ESP_OK                At least one subscriber removed.
 *       - ESP_ERR_NOT_FINISHED  Subscribers removed, but a handler of one of them
 *                               is still running on a task that is itself waiting
 *                                for the caller (see file-level Conventions); keep
 *                                its context valid.
 *       - ESP_ERR_INVALID_ARG    hub is NULL.
 *       - ESP_ERR_INVALID_STATE  Module not initialised.
 *       - ESP_ERR_NOT_FOUND      Domain not registered, or no matching subscriber.
 */
esp_err_t adf_event_hub_unsubscribe(adf_event_hub_t hub, const char *domain, uint16_t event_id);

/**
 * @brief  Deliver an event to all matching subscribers.
 *
 * @note  Queue-mode delivery is non-blocking and best-effort; a full queue
 *        causes that subscriber's delivery to be dropped.
 *        release_cb semantics: see adf_event_payload_release_cb_t.
 *
 * @param[in]  hub          Hub handle; must be a registered domain.
 * @param[in]  event        Event descriptor (shallow-copied; see adf_event_t).
 * @param[in]  release_cb   Payload release callback; may be NULL.
 * @param[in]  release_ctx  Opaque context forwarded to release_cb.
 *
 * @return
 *       - ESP_OK               Event published.
 *       - ESP_ERR_INVALID_ARG  hub or event is NULL. Caller retains ownership
 *                              of event->payload; release_cb is NOT invoked.
 *                                All other return codes transfer payload
 *                                ownership to the hub and guarantee release_cb
 *                                is invoked exactly once.
 *       - ESP_ERR_INVALID_STATE  Module not initialised.
 *       - ESP_ERR_NOT_FOUND      hub is not a registered domain.
 *       - ESP_ERR_NO_MEM         Subscriber snapshot or event envelope could not
 *                                be allocated. On envelope failure the callback
 *                                subscribers are still invoked and only the queue
 *                                subscribers are skipped.
 */
esp_err_t adf_event_hub_publish(adf_event_hub_t hub,
                                const adf_event_t *event,
                                adf_event_payload_release_cb_t release_cb,
                                void *release_ctx);

/**
 * @brief  Release a queue-mode delivery reference.
 *
 * @note  Must be called exactly once per received adf_event_delivery_t.
 *        Skipping leaks the envelope slot. Calling it again on the same
 *        struct is safe and ignored; a copy of the struct is not protected.
 *        After this call delivery->event.domain and payload must not be
 *        accessed. Not ISR-safe.
 *
 * @param[in]  hub       Any non-NULL hub handle; it is not validated.
 * @param[in]  delivery  Delivery item from the subscriber queue;
 *                       delivery->_opaque must not be modified by the caller.
 *
 * @return
 *       - ESP_OK                 Reference released; payload may have been freed.
 *       - ESP_ERR_INVALID_ARG    hub or delivery is NULL; _opaque index out of range.
 *       - ESP_ERR_INVALID_STATE  Module not initialised.
 */
esp_err_t adf_event_hub_delivery_done(adf_event_hub_t hub, adf_event_delivery_t *delivery);

/**
 * @brief  Dump all internal state to the log (ESP_LOGI).
 *
 * @note  Intended for debugging only; not for production hot-paths.
 */
void adf_event_hub_dump(void);

/**
 * @brief  Populate a statistics snapshot.
 *
 * @note  If stats->domains is non-NULL and stats->domains_capacity > 0,
 *        per-domain details are written up to min(domain_count, domains_capacity).
 *        Initialize each selected entry's domain and domain_capacity with a
 *        caller-owned buffer. Names are copied under the hub lock and remain
 *        valid after domains are removed. No allocation or ownership transfer
 *        occurs. Insufficient name capacity returns ESP_ERR_INVALID_SIZE
 *        without modifying any per-domain entries; retry with larger buffers.
 *        The domain and envelope totals are sampled under separate locks.
 *
 * @param[in,out]  stats  Caller-owned structure; zero-initialise before calling
 *                        if per-domain detail is not needed.
 *
 * @return
 *       - ESP_OK                 Stats populated.
 *       - ESP_ERR_INVALID_ARG    stats is NULL.
 *       - ESP_ERR_INVALID_SIZE   A selected domain name buffer is absent or too small.
 *       - ESP_ERR_INVALID_STATE  No hub has been created yet.
 */
esp_err_t adf_event_hub_get_stats(adf_event_hub_stats_t *stats);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
