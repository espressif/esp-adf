/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "adf_event_hub.h"
#include "adf_vec.h"

#define ADF_EVENT_HUB_INIT_CAP   2u  /* Initial vec capacity; grows automatically */
#define ADF_DOMAIN_KEY_WILDCARD  0u

/* Small-buffer threshold for the per-publish subscriber snapshot. Stack budget: 40 * 20B = 800B */
#define ADF_EVT_HUB_SNAP_STACK_MAX  40u

/* publish() counts subscribers in uint16 and adds one publisher reference on top */
#define ADF_EVT_HUB_MAX_SUBS_PER_DOMAIN  (UINT16_MAX - 1u)

/* _opaque stores the envelope index in 16 bits */
#define ADF_EVT_HUB_MAX_ENVELOPES  (1u << 16u)

/* Lazy-init state machine, used to serialise the one-time bootstrap when
 * adf_event_hub_create is invoked concurrently from multiple tasks. Values:
 *   0 = not initialised
 *   1 = one task is currently running hub_ensure_init()
 *   2 = initialisation completed, all static resources are ready
 * The transitions 0->1 and 1->{0,2} are exclusive, so only one winner runs
 * the allocations and no resources are leaked or double-assigned. */
#define ADF_EVT_HUB_INIT_NONE  0u
#define ADF_EVT_HUB_INIT_ING   1u
#define ADF_EVT_HUB_INIT_DONE  2u

/**
 * @brief  Delivery target for a single subscriber.
 *
 *         Holds either a FreeRTOS queue handle (queue mode) or a callback + context
 *         pair (callback mode). The active union member is determined by is_queue.
 */
typedef struct {
    uint8_t  is_queue;  /*!< 1 = queue-mode delivery, 0 = callback-mode */
    union {
        QueueHandle_t  queue;  /*!< Target queue for queue-mode delivery */
        struct {
            adf_event_handler_t  handler;  /*!< Callback invoked in the publisher's task context */
            void                *ctx;      /*!< Opaque argument forwarded to handler */
        } cb;                              /*!< Callback-mode target */
    } target;                              /*!< Delivery endpoint; active member determined by is_queue */
} adf_evt_hub_sub_target_t;

/**
 * @brief  Single subscriber entry stored in a domain's subscriber list.
 */
typedef struct adf_evt_hub_subscriber {
    uint32_t                  owner_key;  /*!< Key of the hub that created this subscription */
    uint16_t                  event_id;   /*!< Event ID filter; ADF_EVENT_ANY_ID matches all events */
    uint8_t                   cancelled;  /*!< Snapshot copies only: set by unsubscribe()/destroy(), entry is skipped */
    adf_evt_hub_sub_target_t  tgt;        /*!< Delivery target (queue or callback) */
} adf_evt_hub_subscriber_t;

/**
 * @brief  One entry per registered domain, stored in s_evt_hub_domains.
 *
 *         A domain exists while ref_count > 0 (publisher references from
 *         create()) or while it still has subscribers (auto-created by
 *         subscribe()). It is removed as soon as both conditions are false.
 */
typedef struct adf_evt_hub_domain {
    adf_event_hub_t  handle;      /*!< Opaque handle returned to callers; encodes domain_key */
    uint32_t         domain_key;  /*!< Non-zero djb2 hash of domain_str */
    char            *domain_str;  /*!< Heap-allocated copy of the domain name */
    uint16_t         ref_count;   /*!< create() references; auto-created domains start at 0 */
    adf_vec_t        sub_vec;     /*!< Per-domain adf_evt_hub_subscriber_t dynamic array */
} adf_evt_hub_domain_t;

/**
 * @brief  Shared ref-counted container for queue-mode payload delivery.
 *
 *         Slots are stored in s_evt_hub_envelopes (adf_vec_t). The vec only ever grows;
 *         slots are never removed, only recycled via in_use = false. This keeps
 *         every slot index permanently stable, so the _opaque encoding is safe.
 *
 *         event.domain is an owned copy of the publisher's domain string so a
 *         queued delivery stays valid after the publisher returns.
 *
 *         _opaque encoding in adf_event_delivery_t:
 *         bits 15:0  = vec index   (0 .. adf_vec_size(&s_evt_hub_envelopes) - 1)
 *         bits 31:16 = generation  (never 0; guards against stale delivery_done after slot reuse)
 */
typedef struct {
    adf_event_t                     event;        /*!< Shallow copy of the published event; domain is owned */
    adf_event_payload_release_cb_t  release_cb;   /*!< Called once when ref_count drops to zero */
    void                           *release_ctx;  /*!< Opaque context forwarded to release_cb */
    uint16_t                        ref_count;    /*!< Outstanding queue-mode delivery references */
    uint16_t                        generation;   /*!< Incremented on each slot reuse; upper 16 bits of _opaque */
    bool                            in_use;       /*!< True while ref_count > 0 */
    int                             next_free;    /*!< Free-list chain index; valid only when in_use == false */
} adf_evt_hub_envelope_t;

/**
 * @brief  Stack record of a publish() that is dispatching outside the mutex.
 *
 *         unsubscribe()/destroy() cancel the not-yet-dispatched snapshot entries
 *         of the removed subscriptions in every record, then wait until no other
 *         task is executing (cur) such an entry, so a removed handler or queue is
 *         never used after return. cur and cancelled are exchanged with seq_cst
 *         atomics: the dispatcher stores cur before loading cancelled, the remover
 *         stores cancelled before loading cur, so one of them always sees the other.
 */
typedef struct adf_evt_hub_inflight {
    TaskHandle_t                 task;        /*!< Publishing task; its own records are never waited for */
    uint32_t                     domain_key;  /*!< Domain being dispatched */
    adf_evt_hub_subscriber_t    *snap;        /*!< Snapshot this record is dispatching */
    uint16_t                     snap_count;  /*!< Number of snapshot entries */
    uint16_t                     cur;         /*!< Index of the entry being delivered; snap_count when done */
    struct adf_evt_hub_inflight *waits_on;    /*!< Dispatch this task waits for; cleared before unlink */
    struct adf_evt_hub_inflight *next;        /*!< Singly linked list */
} adf_evt_hub_inflight_t;

static const char *TAG = "ADF_EVENT_HUB";

static adf_vec_t s_evt_hub_domains;                        /* elem: adf_evt_hub_domain_t */
static adf_evt_hub_inflight_t *s_evt_hub_inflight = NULL;  /* guarded by s_evt_hub_mutex */

static SemaphoreHandle_t s_evt_hub_mutex = NULL;

static adf_vec_t s_evt_hub_envelopes;  /* elem: adf_evt_hub_envelope_t; only grows */
static SemaphoreHandle_t s_evt_hub_env_mutex = NULL;
static int s_evt_hub_env_free_head = -1;  /* head of envelope free-list; -1 = empty */
static uint32_t s_evt_hub_queue_drops;    /* Saturating total, guarded by s_evt_hub_env_mutex */

static volatile uint32_t s_evt_hub_init_state = ADF_EVT_HUB_INIT_NONE;

static inline bool evt_hub_initialised(void)
{
    return __atomic_load_n(&s_evt_hub_init_state, __ATOMIC_ACQUIRE) == ADF_EVT_HUB_INIT_DONE;
}

static inline adf_evt_hub_domain_t *evt_hub_domain_find(uint32_t key)
{
    adf_evt_hub_domain_t *base = (adf_evt_hub_domain_t *)s_evt_hub_domains.data;
    size_t count = s_evt_hub_domains.size;
    for (size_t i = 0; i < count; i++) {
        if (base[i].domain_key == key) {
            return &base[i];
        }
    }
    return NULL;
}

static inline uint32_t evt_hub_make_opaque(int idx, uint16_t gen)
{
    return ((uint32_t)gen << 16u) | (uint32_t)(idx & 0xFFFF);
}

static inline int evt_hub_opaque_idx(uint32_t opaque)
{
    return (int)(opaque & 0xFFFFu);
}

static inline uint16_t evt_hub_opaque_gen(uint32_t opaque)
{
    return (uint16_t)(opaque >> 16u);
}

static inline uint32_t evt_hub_str_to_hash(const char *s)
{
    if (!s || s[0] == '\0') {
        return ADF_DOMAIN_KEY_WILDCARD;
    }
    uint32_t h = 5381u;
    int c;
    while ((c = (unsigned char)*s++) != '\0') {
        h = ((h << 5u) + h) ^ (uint32_t)c;
    }
    return h ? h : 1u;
}

static inline int evt_hub_envelope_alloc(const adf_event_t *event, char *domain_copy,
                                         adf_event_payload_release_cb_t release_cb, void *release_ctx, uint16_t ref_count, uint16_t *out_gen)
{
    xSemaphoreTake(s_evt_hub_env_mutex, portMAX_DELAY);
    int idx = 0;
    adf_evt_hub_envelope_t *env = NULL;
    if (s_evt_hub_env_free_head >= 0) {
        idx = s_evt_hub_env_free_head;
        env = (adf_evt_hub_envelope_t *)s_evt_hub_envelopes.data + idx;
        s_evt_hub_env_free_head = env->next_free;
    } else {
        adf_evt_hub_envelope_t blank;
        memset(&blank, 0, sizeof(blank));
        if (s_evt_hub_envelopes.size >= ADF_EVT_HUB_MAX_ENVELOPES ||
            adf_vec_push(&s_evt_hub_envelopes, &blank) != ADF_VEC_OK) {
            xSemaphoreGive(s_evt_hub_env_mutex);
            return -1;  /* pool exhausted or OOM */
        }
        idx = (int)s_evt_hub_envelopes.size - 1;
        env = (adf_evt_hub_envelope_t *)s_evt_hub_envelopes.data + idx;
        env->generation = 0;
    }

    env->in_use = true;
    env->event = *event;
    env->event.domain = domain_copy;
    env->release_cb = release_cb;
    env->release_ctx = release_ctx;
    env->ref_count = ref_count;
    if (++env->generation == 0) {
        env->generation = 1;
    }
    *out_gen = env->generation;

    xSemaphoreGive(s_evt_hub_env_mutex);
    return idx;
}

/* Returns false when idx is outside the pool; stale generation is silently ignored */
static inline bool evt_hub_envelope_release(
    int idx, uint16_t gen, adf_event_payload_release_cb_t *out_cb, void **out_ctx, const void **out_payload)
{
    *out_cb = NULL;
    *out_ctx = NULL;
    *out_payload = NULL;
    xSemaphoreTake(s_evt_hub_env_mutex, portMAX_DELAY);
    if (idx < 0 || (size_t)idx >= s_evt_hub_envelopes.size) {
        xSemaphoreGive(s_evt_hub_env_mutex);
        return false;
    }
    adf_evt_hub_envelope_t *env = (adf_evt_hub_envelope_t *)s_evt_hub_envelopes.data + idx;
    if (!env->in_use || env->generation != gen) {
        xSemaphoreGive(s_evt_hub_env_mutex);
        return true;
    }
    if (env->ref_count > 0) {
        env->ref_count--;
    }
    if (env->ref_count == 0) {
        env->in_use = false;
        env->next_free = s_evt_hub_env_free_head;
        s_evt_hub_env_free_head = idx;
        *out_cb = env->release_cb;
        *out_ctx = env->release_ctx;
        *out_payload = env->event.payload;
        free((void *)env->event.domain);
        env->event.domain = NULL;
    }
    xSemaphoreGive(s_evt_hub_env_mutex);
    return true;
}

static inline esp_err_t hub_ensure_init(void)
{
    /* Fast path: the ACQUIRE load pairs with the RELEASE store at the end of
     * successful init, so once we observe DONE all static resources are
     * visible here without further synchronisation. */
    if (evt_hub_initialised()) {
        return ESP_OK;
    }
    /* Try to become the unique initialiser. Losers either spin until the
     * winner finishes (state becomes DONE) or retry if the winner fails
     * (state returns to NONE). */
    uint32_t expected = ADF_EVT_HUB_INIT_NONE;
    if (!__atomic_compare_exchange_n(&s_evt_hub_init_state, &expected,
                                     ADF_EVT_HUB_INIT_ING, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&s_evt_hub_init_state, __ATOMIC_ACQUIRE) == ADF_EVT_HUB_INIT_ING) {
            vTaskDelay(1);
        }
        return evt_hub_initialised() ? ESP_OK : ESP_ERR_NO_MEM;
    }
    /* We own the init; allocations run outside any lock so other tasks can
     * still be scheduled. On failure we revert to NONE so a retry is possible. */
    SemaphoreHandle_t mtx = xSemaphoreCreateMutex();
    if (!mtx) {
        ESP_LOGE(TAG, "Init failed: cannot create main mutex");
        goto fail;
    }
    SemaphoreHandle_t env_mtx = xSemaphoreCreateMutex();
    if (!env_mtx) {
        vSemaphoreDelete(mtx);
        ESP_LOGE(TAG, "Init failed: cannot create envelope mutex");
        goto fail;
    }
    if (adf_vec_init(&s_evt_hub_domains, sizeof(adf_evt_hub_domain_t), ADF_EVENT_HUB_INIT_CAP) != ADF_VEC_OK) {
        vSemaphoreDelete(env_mtx);
        vSemaphoreDelete(mtx);
        ESP_LOGE(TAG, "Init failed: cannot allocate domain table");
        goto fail;
    }
    if (adf_vec_init(&s_evt_hub_envelopes, sizeof(adf_evt_hub_envelope_t), ADF_EVENT_HUB_INIT_CAP) != ADF_VEC_OK) {
        adf_vec_destroy(&s_evt_hub_domains);
        vSemaphoreDelete(env_mtx);
        vSemaphoreDelete(mtx);
        ESP_LOGE(TAG, "Init failed: cannot allocate envelope pool");
        goto fail;
    }
    s_evt_hub_env_mutex = env_mtx;
    s_evt_hub_mutex = mtx;
    /* RELEASE store publishes all writes above to any ACQUIRE loader. */
    __atomic_store_n(&s_evt_hub_init_state, ADF_EVT_HUB_INIT_DONE, __ATOMIC_RELEASE);
    return ESP_OK;

fail:
    __atomic_store_n(&s_evt_hub_init_state, ADF_EVT_HUB_INIT_NONE, __ATOMIC_RELEASE);
    return ESP_ERR_NO_MEM;
}

/* Caller holds s_evt_hub_mutex */
static inline adf_evt_hub_domain_t *domain_register(const char *domain, uint32_t key)
{
    char *str_copy = strdup(domain);
    if (!str_copy) {
        ESP_LOGE(TAG, "Register '%s': out of memory copying domain name", domain);
        return NULL;
    }
    adf_evt_hub_domain_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    new_entry.handle = (adf_event_hub_t)(uintptr_t)key;
    new_entry.domain_key = key;
    new_entry.domain_str = str_copy;
    if (adf_vec_init(&new_entry.sub_vec, sizeof(adf_evt_hub_subscriber_t), 1) != ADF_VEC_OK) {
        free(str_copy);
        ESP_LOGE(TAG, "Register '%s': out of memory allocating subscriber list", domain);
        return NULL;
    }
    if (adf_vec_push(&s_evt_hub_domains, &new_entry) != ADF_VEC_OK) {
        adf_vec_destroy(&new_entry.sub_vec);
        free(str_copy);
        ESP_LOGE(TAG, "Register '%s': out of memory appending to domain table", domain);
        return NULL;
    }
    return evt_hub_domain_find(key);
}

/* Caller holds s_evt_hub_mutex. Removes domains that have neither references nor subscribers */
static void evt_hub_domain_cleanup_unused(void)
{
    adf_evt_hub_domain_t *base = (adf_evt_hub_domain_t *)s_evt_hub_domains.data;
    for (size_t i = 0; i < s_evt_hub_domains.size;) {
        if (base[i].ref_count == 0 && base[i].sub_vec.size == 0) {
            ESP_LOGI(TAG, "Domain '%s' unreferenced, removed", base[i].domain_str);
            adf_vec_destroy(&base[i].sub_vec);
            free(base[i].domain_str);
            adf_vec_remove_swap(&s_evt_hub_domains, i);
        } else {
            i++;
        }
    }
}

/* Caller holds s_evt_hub_mutex. Marks every not-yet-dispatched snapshot entry of the
 * removed subscriptions, in every in-flight dispatch, as cancelled so it is skipped.
 * Entries already dispatched (or being dispatched) are unaffected. */
static void evt_hub_cancel_pending(uint32_t domain_key, uint32_t owner_key, uint16_t event_id)
{
    for (adf_evt_hub_inflight_t *rec = s_evt_hub_inflight; rec; rec = rec->next) {
        if (domain_key != ADF_DOMAIN_KEY_WILDCARD && rec->domain_key != domain_key) {
            continue;
        }
        for (uint16_t i = 0; i < rec->snap_count; i++) {
            adf_evt_hub_subscriber_t *entry = &rec->snap[i];
            if (entry->owner_key != owner_key) {
                continue;
            }
            if (event_id != ADF_EVENT_ANY_ID && entry->event_id != event_id) {
                continue;
            }
            __atomic_store_n(&entry->cancelled, 1, __ATOMIC_SEQ_CST);
        }
    }
}

/* Caller holds s_evt_hub_mutex. True while rec is delivering to a removed subscription */
static bool evt_hub_inflight_executing(
    const adf_evt_hub_inflight_t *rec, uint32_t domain_key, uint32_t owner_key, uint16_t event_id)
{
    if (domain_key != ADF_DOMAIN_KEY_WILDCARD && rec->domain_key != domain_key) {
        return false;
    }
    uint16_t cur = __atomic_load_n(&rec->cur, __ATOMIC_SEQ_CST);
    if (cur >= rec->snap_count) {
        return false;
    }
    const adf_evt_hub_subscriber_t *entry = &rec->snap[cur];
    return entry->owner_key == owner_key && (event_id == ADF_EVENT_ANY_ID || entry->event_id == event_id);
}

/* Caller holds s_evt_hub_mutex. True if waiting for rec would close a cycle: the task
 * running rec is itself parked (directly or through other parked tasks) on a dispatch
 * of task self, whose handler is the caller. hops bounds the walk to the list length. */
static bool evt_hub_inflight_waits_on_task(const adf_evt_hub_inflight_t *rec, TaskHandle_t self, size_t hops)
{
    for (const adf_evt_hub_inflight_t *r = rec->waits_on; r && hops > 0; r = r->waits_on, hops--) {
        if (r->task == self) {
            return true;
        }
    }
    return false;
}

/* Caller holds s_evt_hub_mutex on entry and exit; the mutex is released while waiting.
 * Waits until no other task is delivering to a subscription matching the filter, by
 * polling once per tick for as long as such a handler runs (no upper bound).
 * Dispatches running on the calling task are skipped: the caller is one of their
 * handlers, and evt_hub_cancel_pending() already covers their remaining entries.
 * A dispatch whose task is in turn parked on this task (two handlers removing each
 * other's subscriptions) is skipped as well, with a warning, instead of deadlocking.
 * Returns true when every matching dispatch has finished, false when one was skipped
 * because of such a cycle and its handler may still be running. */
static bool evt_hub_wait_dispatch_idle(uint32_t domain_key, uint32_t owner_key, uint16_t event_id)
{
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    bool skipped = false;
    for (;;) {
        adf_evt_hub_inflight_t *busy = NULL;
        size_t hops = 0;
        for (adf_evt_hub_inflight_t *rec = s_evt_hub_inflight; rec; rec = rec->next) {
            hops++;
        }
        for (adf_evt_hub_inflight_t *rec = s_evt_hub_inflight; rec; rec = rec->next) {
            if (rec->task == self) {
                continue;
            }
            if (!evt_hub_inflight_executing(rec, domain_key, owner_key, event_id)) {
                continue;
            }
            if (evt_hub_inflight_waits_on_task(rec, self, hops)) {
                if (!skipped) {
                    skipped = true;
                    ESP_LOGW(TAG,
                             "Remove owner 0x%08x: handler on task %p is waiting for this task, "
                             "not waiting for it in return",
                             (unsigned)owner_key, (void *)rec->task);
                }
                continue;
            }
            busy = rec;
            break;
        }
        for (adf_evt_hub_inflight_t *rec = s_evt_hub_inflight; rec; rec = rec->next) {
            if (rec->task == self) {
                rec->waits_on = busy;
            }
        }
        if (!busy) {
            return !skipped;
        }
        xSemaphoreGive(s_evt_hub_mutex);
        vTaskDelay(1);
        xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    }
}

esp_err_t adf_event_hub_create(const char *domain, adf_event_hub_t *out_hub)
{
    if (!domain || !out_hub) {
        ESP_LOGE(TAG, "Create failed: domain or out_hub is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = hub_ensure_init();
    if (ret != ESP_OK) {
        return ret;  /* hub_ensure_init already logged */
    }
    uint32_t key = evt_hub_str_to_hash(domain);
    if (key == ADF_DOMAIN_KEY_WILDCARD) {
        ESP_LOGE(TAG, "Create failed: domain name must not be empty");
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    adf_evt_hub_domain_t *d = evt_hub_domain_find(key);
    if (d && strcmp(d->domain_str, domain) != 0) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Create '%s': key 0x%08x already taken by domain '%s'", domain, (unsigned)key, d->domain_str);
        return ESP_ERR_INVALID_STATE;
    }
    if (!d) {
        d = domain_register(domain, key);
        if (!d) {
            xSemaphoreGive(s_evt_hub_mutex);
            return ESP_ERR_NO_MEM;  /* domain_register already logged */
        }
        ESP_LOGI(TAG, "Create '%s': domain registered", domain);
    }
    if (d->ref_count == UINT16_MAX) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Create '%s': reference count exhausted", domain);
        return ESP_ERR_NO_MEM;
    }
    d->ref_count++;
    *out_hub = d->handle;
    ESP_LOGD(TAG, "Create '%s': refcount -> %u", domain, (unsigned)d->ref_count);
    xSemaphoreGive(s_evt_hub_mutex);
    return ESP_OK;
}

esp_err_t adf_event_hub_is_dispatching(bool *out_dispatching)
{
    if (out_dispatching == NULL) {
        ESP_LOGE(TAG, "Dispatch query failed: output is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    *out_dispatching = false;
    if (!evt_hub_initialised()) {
        return ESP_OK;
    }
    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    for (adf_evt_hub_inflight_t *rec = s_evt_hub_inflight; rec; rec = rec->next) {
        if (rec->task == self) {
            *out_dispatching = true;
            break;
        }
    }
    xSemaphoreGive(s_evt_hub_mutex);
    return ESP_OK;
}

esp_err_t adf_event_hub_unsubscribe_all(adf_event_hub_t hub)
{
    if (hub == NULL) {
        ESP_LOGE(TAG, "Unsubscribe all failed: hub is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!evt_hub_initialised()) {
        ESP_LOGE(TAG, "Unsubscribe all failed: module not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t key = (uint32_t)(uintptr_t)hub;
    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    adf_evt_hub_domain_t *domain = evt_hub_domain_find(key);
    if (domain == NULL || domain->ref_count == 0) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Unsubscribe all failed: owner has no create reference");
        return ESP_ERR_INVALID_STATE;
    }
    adf_evt_hub_domain_t *base = (adf_evt_hub_domain_t *)s_evt_hub_domains.data;
    for (size_t i = 0; i < s_evt_hub_domains.size; i++) {
        adf_evt_hub_subscriber_t *subs = (adf_evt_hub_subscriber_t *)base[i].sub_vec.data;
        for (size_t s = 0; s < base[i].sub_vec.size;) {
            if (subs[s].owner_key == key) {
                adf_vec_remove_swap(&base[i].sub_vec, s);
            } else {
                s++;
            }
        }
    }
    evt_hub_cancel_pending(ADF_DOMAIN_KEY_WILDCARD, key, ADF_EVENT_ANY_ID);
    bool idle = evt_hub_wait_dispatch_idle(ADF_DOMAIN_KEY_WILDCARD, key, ADF_EVENT_ANY_ID);
    evt_hub_domain_cleanup_unused();
    xSemaphoreGive(s_evt_hub_mutex);
    return idle ? ESP_OK : ESP_ERR_NOT_FINISHED;
}

esp_err_t adf_event_hub_destroy(adf_event_hub_t hub)
{
    if (!hub) {
        ESP_LOGE(TAG, "Destroy failed: hub is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!evt_hub_initialised()) {
        ESP_LOGE(TAG, "Destroy failed: module not initialised");
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t key = (uint32_t)(uintptr_t)hub;
    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    adf_evt_hub_domain_t *d = evt_hub_domain_find(key);
    if (!d || d->ref_count == 0) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Destroy failed: no domain matches this hub handle (key=0x%08x)", (unsigned)key);
        return ESP_ERR_NOT_FOUND;
    }
    d->ref_count--;
    if (d->ref_count > 0) {
        ESP_LOGD(TAG, "Destroy '%s': refcount -> %u, deferred", d->domain_str, (unsigned)d->ref_count);
        xSemaphoreGive(s_evt_hub_mutex);
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Destroy '%s': refcount reached 0", d->domain_str);
    /* drop every subscription this hub created, on any domain */
    adf_evt_hub_domain_t *base = (adf_evt_hub_domain_t *)s_evt_hub_domains.data;
    for (size_t i = 0; i < s_evt_hub_domains.size; i++) {
        adf_evt_hub_subscriber_t *subs = (adf_evt_hub_subscriber_t *)base[i].sub_vec.data;
        for (size_t s = 0; s < base[i].sub_vec.size;) {
            if (subs[s].owner_key == key) {
                adf_vec_remove_swap(&base[i].sub_vec, s);
            } else {
                s++;
            }
        }
    }
    evt_hub_cancel_pending(ADF_DOMAIN_KEY_WILDCARD, key, ADF_EVENT_ANY_ID);
    bool idle = evt_hub_wait_dispatch_idle(ADF_DOMAIN_KEY_WILDCARD, key, ADF_EVENT_ANY_ID);
    evt_hub_domain_cleanup_unused();
    xSemaphoreGive(s_evt_hub_mutex);
    return idle ? ESP_OK : ESP_ERR_NOT_FINISHED;
}

esp_err_t adf_event_hub_subscribe(adf_event_hub_t hub, const adf_event_subscribe_info_t *info)
{
    if (!hub || !info) {
        ESP_LOGE(TAG, "Subscribe failed: hub or info is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!info->target_queue && !info->handler) {
        ESP_LOGE(TAG, "Subscribe failed: no delivery mode set (target_queue and handler are both NULL)");
        return ESP_ERR_INVALID_ARG;
    }
    if (info->event_domain && info->event_domain[0] == '\0') {
        ESP_LOGE(TAG, "Subscribe failed: event_domain must not be empty");
        return ESP_ERR_INVALID_ARG;
    }
    if (!evt_hub_initialised()) {
        ESP_LOGE(TAG, "Subscribe failed: module not initialised, call adf_event_hub_create first");
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t hub_key = (uint32_t)(uintptr_t)hub;
    /* NULL event_domain falls back to the hub's own domain */
    uint32_t target_key = info->event_domain ? evt_hub_str_to_hash(info->event_domain) : hub_key;

    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    adf_evt_hub_domain_t *owner = evt_hub_domain_find(hub_key);
    if (!owner || owner->ref_count == 0) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Subscribe failed: hub handle is not registered (key=0x%08x)", (unsigned)hub_key);
        return ESP_ERR_NOT_FOUND;
    }
    adf_evt_hub_domain_t *target = (target_key == hub_key) ? owner : evt_hub_domain_find(target_key);
    if (info->event_domain && target && strcmp(target->domain_str, info->event_domain) != 0) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Subscribe '%s': key 0x%08x already taken by domain '%s'", info->event_domain,
                 (unsigned)target_key, target->domain_str);
        return ESP_ERR_INVALID_STATE;
    }
    if (!target) {
        target = domain_register(info->event_domain, target_key);
        if (!target) {
            xSemaphoreGive(s_evt_hub_mutex);
            return ESP_ERR_NO_MEM;  /* domain_register already logged */
        }
        ESP_LOGI(TAG, "Subscribe: domain '%s' not found, auto-created", info->event_domain);
    }
    if (target->sub_vec.size >= ADF_EVT_HUB_MAX_SUBS_PER_DOMAIN) {
        evt_hub_domain_cleanup_unused();
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Subscribe failed: subscriber limit reached in domain '%s'",
                 info->event_domain ? info->event_domain : "own");
        return ESP_ERR_NO_MEM;
    }
    adf_evt_hub_subscriber_t sub;
    memset(&sub, 0, sizeof(sub));
    sub.owner_key = hub_key;
    sub.event_id = info->event_id;
    if (info->target_queue) {
        sub.tgt.is_queue = true;
        sub.tgt.target.queue = info->target_queue;
    } else {
        sub.tgt.is_queue = false;
        sub.tgt.target.cb.handler = info->handler;
        sub.tgt.target.cb.ctx = info->handler_ctx;
    }

    if (adf_vec_push(&target->sub_vec, &sub) != ADF_VEC_OK) {
        evt_hub_domain_cleanup_unused();
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Subscribe failed: out of memory adding subscriber in domain '%s'",
                 info->event_domain ? info->event_domain : "own");
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(s_evt_hub_mutex);
    return ESP_OK;
}

esp_err_t adf_event_hub_unsubscribe(adf_event_hub_t hub, const char *domain, uint16_t event_id)
{
    if (!hub) {
        ESP_LOGE(TAG, "Unsubscribe failed: hub is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!evt_hub_initialised()) {
        ESP_LOGE(TAG, "Unsubscribe failed: module not initialised");
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t hub_key = (uint32_t)(uintptr_t)hub;
    uint32_t domain_key = domain ? evt_hub_str_to_hash(domain) : hub_key;

    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    adf_evt_hub_domain_t *target = evt_hub_domain_find(domain_key);
    if (!target || (domain && strcmp(target->domain_str, domain) != 0)) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGW(TAG, "Unsubscribe: domain '%s' not found", domain ? domain : "own");
        return ESP_ERR_NOT_FOUND;
    }
    bool removed = false;
    bool idle = true;
    adf_evt_hub_subscriber_t *sub_base = (adf_evt_hub_subscriber_t *)target->sub_vec.data;
    for (size_t i = 0; i < target->sub_vec.size;) {
        bool match =
            sub_base[i].owner_key == hub_key && (event_id == ADF_EVENT_ANY_ID || sub_base[i].event_id == event_id);
        if (match) {
            adf_vec_remove_swap(&target->sub_vec, i);
            removed = true;
        } else {
            i++;
        }
    }
    if (removed) {
        evt_hub_cancel_pending(domain_key, hub_key, event_id);
        idle = evt_hub_wait_dispatch_idle(domain_key, hub_key, event_id);
        evt_hub_domain_cleanup_unused();
    }
    xSemaphoreGive(s_evt_hub_mutex);
    if (!removed) {
        ESP_LOGW(TAG, "Unsubscribe: no matching subscriber in domain '%s' for event_id=%u",
                 domain ? domain : "own", (unsigned)event_id);
        return ESP_ERR_NOT_FOUND;
    }
    return idle ? ESP_OK : ESP_ERR_NOT_FINISHED;
}

esp_err_t adf_event_hub_publish(adf_event_hub_t hub,
                                const adf_event_t *event,
                                adf_event_payload_release_cb_t release_cb,
                                void *release_ctx)
{
    if (!hub || !event) {
        /* Caller bug: NULL hub/event means we cannot honor ownership transfer;
         * release_cb is intentionally NOT invoked here, so the caller still
         * owns event->payload and must free it. See adf_event_hub_publish() doc. */
        ESP_LOGE(TAG, "Publish failed: hub=%p or event=%p is NULL; "
                      "caller retains payload ownership, release_cb NOT invoked",
                 (void *)hub, (const void *)event);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;
    uint16_t snap_count = 0;
    uint16_t queue_count = 0;
    adf_evt_hub_subscriber_t snap_stack[ADF_EVT_HUB_SNAP_STACK_MAX];
    adf_evt_hub_subscriber_t *snap_heap = NULL;
    adf_evt_hub_subscriber_t *snap = snap_stack;
    adf_evt_hub_inflight_t inflight = {0};
    bool inflight_linked = false;
    char *domain_copy = NULL;
    int env_idx = -1;
    uint16_t env_gen = 0;
    bool envelope_owns_payload = false;

    if (!evt_hub_initialised()) {
        ESP_LOGE(TAG, "Publish failed: module not initialised");
        ret = ESP_ERR_INVALID_STATE;
        goto cleanup;
    }
    uint32_t hub_key = (uint32_t)(uintptr_t)hub;
    /* snapshot matching targets under mutex; deliver outside to avoid holding lock */
    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    inflight.task = xTaskGetCurrentTaskHandle();
    inflight.domain_key = hub_key;
    inflight.next = s_evt_hub_inflight;
    s_evt_hub_inflight = &inflight;
    inflight_linked = true;
    adf_evt_hub_domain_t *source = evt_hub_domain_find(hub_key);
    if (!source || source->ref_count == 0) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Publish failed: hub is not registered (key=0x%08x)", (unsigned)hub_key);
        ret = ESP_ERR_NOT_FOUND;
        goto cleanup;
    }
    if (source->sub_vec.size == 0) {
        xSemaphoreGive(s_evt_hub_mutex);
        goto cleanup;  /* no subscribers; release_cb fires in cleanup */
    }
    const adf_evt_hub_subscriber_t *sub_base = (const adf_evt_hub_subscriber_t *)source->sub_vec.data;
    size_t sub_count = source->sub_vec.size;
    if (sub_count > ADF_EVT_HUB_SNAP_STACK_MAX) {
        snap_heap = (adf_evt_hub_subscriber_t *)malloc(sub_count * sizeof(*snap_heap));
        if (!snap_heap) {
            xSemaphoreGive(s_evt_hub_mutex);
            ESP_LOGE(TAG, "Publish '%s'/%u: out of memory allocating snapshot for %u subscribers",
                     event->domain ? event->domain : "?", (unsigned)event->event_id,
                     (unsigned)sub_count);
            ret = ESP_ERR_NO_MEM;
            goto cleanup;
        }
        snap = snap_heap;
    }
    for (size_t i = 0; i < sub_count; i++) {
        bool id_match = (sub_base[i].event_id == ADF_EVENT_ANY_ID ||
                         sub_base[i].event_id == event->event_id);
        if (id_match) {
            if (sub_base[i].tgt.is_queue) {
                queue_count++;
            }
            snap[snap_count++] = sub_base[i];
        }
    }
    inflight.snap = snap;
    inflight.snap_count = snap_count;
    xSemaphoreGive(s_evt_hub_mutex);

    if (queue_count > 0) {
        domain_copy = event->domain ? strdup(event->domain) : NULL;
        /* +1: publisher's own ref keeps the payload alive until every
         * callback subscriber in this dispatch has run, even if all queue
         * refs are dropped or consumed first. */
        if (!event->domain || domain_copy) {
            env_idx = evt_hub_envelope_alloc(
                event, domain_copy, release_cb, release_ctx, (uint16_t)(queue_count + 1), &env_gen);
        }
        if (env_idx < 0) {
            free(domain_copy);
            ESP_LOGE(TAG, "Publish '%s'/%u: out of memory allocating event envelope",
                     event->domain ? event->domain : "?", (unsigned)event->event_id);
            ret = ESP_ERR_NO_MEM;
            /* Fall through: queue subscribers are skipped below, but callback
             * subscribers still get notified and release_cb fires in cleanup. */
        } else {
            envelope_owns_payload = true;
        }
    }
    for (uint16_t i = 0; i < snap_count; i++) {
        adf_evt_hub_sub_target_t *tgt = &snap[i].tgt;
        /* Publish cur before reading the cancel mark (see adf_evt_hub_inflight_t) */
        __atomic_store_n(&inflight.cur, i, __ATOMIC_SEQ_CST);
        bool cancelled = __atomic_load_n(&snap[i].cancelled, __ATOMIC_SEQ_CST) != 0;
        if (tgt->is_queue) {
            if (!envelope_owns_payload) {
                continue;  /* envelope alloc failed; queue delivery impossible */
            }
            if (cancelled) {
                /* Removed by unsubscribe()/destroy() after the snapshot was taken.
                 * The queue reference taken at envelope alloc will never be
                 * consumed: drop it here like a full-queue drop. */
                adf_event_payload_release_cb_t cancel_cb;
                void *cancel_ctx;
                const void *cancel_payload;
                evt_hub_envelope_release(env_idx, env_gen, &cancel_cb, &cancel_ctx, &cancel_payload);
                if (cancel_cb) {
                    cancel_cb(cancel_payload, cancel_ctx);
                }
                continue;
            }
            adf_event_delivery_t delivery = {
                .event = *event,
                ._opaque = evt_hub_make_opaque(env_idx, env_gen),
            };
            delivery.event.domain = domain_copy;
            if (xQueueSend(tgt->target.queue, &delivery, 0) != pdTRUE) {
                xSemaphoreTake(s_evt_hub_env_mutex, portMAX_DELAY);
                bool first_drop = s_evt_hub_queue_drops == 0;
                if (s_evt_hub_queue_drops < UINT32_MAX) {
                    s_evt_hub_queue_drops++;
                }
                xSemaphoreGive(s_evt_hub_env_mutex);
                if (first_drop) {
                    ESP_LOGW(TAG, "Publish: subscriber queue full; further drops are counted in get_stats");
                }
                /* subscriber will not call delivery_done; release this ref now */
                adf_event_payload_release_cb_t drop_cb;
                void *drop_ctx;
                const void *drop_payload;
                evt_hub_envelope_release(env_idx, env_gen, &drop_cb, &drop_ctx, &drop_payload);
                if (drop_cb) {
                    drop_cb(drop_payload, drop_ctx);
                }
            }
        } else if (!cancelled) {
            tgt->target.cb.handler(event, tgt->target.cb.ctx);
        }
    }
    __atomic_store_n(&inflight.cur, snap_count, __ATOMIC_SEQ_CST);
    if (envelope_owns_payload) {
        adf_event_payload_release_cb_t pub_cb;
        void *pub_ctx;
        const void *pub_payload;
        evt_hub_envelope_release(env_idx, env_gen, &pub_cb, &pub_ctx, &pub_payload);
        if (pub_cb) {
            pub_cb(pub_payload, pub_ctx);
        }
    }
cleanup:
    /* release_cb contract: invoked exactly once. When an envelope took
     * ownership, the final evt_hub_envelope_release() will invoke it. In
     * every other exit (no subscribers, module not initialised, domain not
     * found, snapshot OOM, envelope OOM) we invoke it here so heap-allocated
     * payloads are never leaked on any error path. */
    if (!envelope_owns_payload && release_cb) {
        release_cb(event->payload, release_ctx);
    }
    if (inflight_linked) {
        xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
        /* Keep dispatch context through payload finalization, including early
         * exits, then unlink before the snapshot or this frame is released. */
        for (adf_evt_hub_inflight_t *rec = s_evt_hub_inflight; rec; rec = rec->next) {
            if (rec->waits_on == &inflight) {
                rec->waits_on = NULL;
            }
        }
        for (adf_evt_hub_inflight_t **pp = &s_evt_hub_inflight; *pp; pp = &(*pp)->next) {
            if (*pp == &inflight) {
                *pp = inflight.next;
                break;
            }
        }
        xSemaphoreGive(s_evt_hub_mutex);
    }
    if (snap_heap) {
        free(snap_heap);
    }
    return ret;
}

esp_err_t adf_event_hub_delivery_done(adf_event_hub_t hub, adf_event_delivery_t *delivery)
{
    if (!hub || !delivery) {
        ESP_LOGE(TAG, "DeliveryDone failed: hub or delivery is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!evt_hub_initialised()) {
        ESP_LOGE(TAG, "DeliveryDone failed: module not initialised");
        return ESP_ERR_INVALID_STATE;
    }
    if (delivery->_opaque == 0) {
        return ESP_OK;  /* already released through this struct */
    }
    int idx = evt_hub_opaque_idx(delivery->_opaque);
    uint16_t gen = evt_hub_opaque_gen(delivery->_opaque);
    adf_event_payload_release_cb_t cb = NULL;
    void *cb_ctx = NULL;
    const void *payload = NULL;
    if (!evt_hub_envelope_release(idx, gen, &cb, &cb_ctx, &payload)) {
        ESP_LOGE(TAG, "DeliveryDone failed: invalid delivery token (idx=%d)", idx);
        return ESP_ERR_INVALID_ARG;
    }
    delivery->_opaque = 0;
    if (cb) {
        cb(payload, cb_ctx);
    }
    return ESP_OK;
}

void adf_event_hub_dump(void)
{
    if (!evt_hub_initialised()) {
        ESP_LOGE(TAG, "Dump failed: module not initialised");
        return;
    }

    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    size_t dom_count = adf_vec_size(&s_evt_hub_domains);
    ESP_LOGI(TAG, "======== Event Hub Dump ========");
    ESP_LOGI(TAG, "Registered domains: %u", (unsigned)dom_count);

    const adf_evt_hub_domain_t *base = (const adf_evt_hub_domain_t *)s_evt_hub_domains.data;
    for (size_t d = 0; d < dom_count; d++) {
        const adf_evt_hub_domain_t *dom = &base[d];
        size_t sub_n = adf_vec_size(&dom->sub_vec);
        ESP_LOGI(TAG, "  [%u] domain='%s'  key=0x%08x  refs=%u  subscribers=%u", (unsigned)d, dom->domain_str,
                 (unsigned)dom->domain_key, (unsigned)dom->ref_count, (unsigned)sub_n);

        const adf_evt_hub_subscriber_t *subs = (const adf_evt_hub_subscriber_t *)dom->sub_vec.data;
        for (size_t s = 0; s < sub_n; s++) {
            const adf_evt_hub_subscriber_t *sub = &subs[s];
            const char *mode = sub->tgt.is_queue ? "queue" : "callback";
            if (sub->event_id == ADF_EVENT_ANY_ID) {
                ESP_LOGI(
                    TAG, "    [%u] mode=%-8s  event_id=ANY  owner=0x%08x", (unsigned)s, mode, (unsigned)sub->owner_key);
            } else {
                ESP_LOGI(TAG, "    [%u] mode=%-8s  event_id=%u  owner=0x%08x", (unsigned)s, mode,
                         (unsigned)sub->event_id, (unsigned)sub->owner_key);
            }
        }
    }
    xSemaphoreGive(s_evt_hub_mutex);
    /* envelope pool — uses its own mutex */
    xSemaphoreTake(s_evt_hub_env_mutex, portMAX_DELAY);
    size_t pool_sz = adf_vec_size(&s_evt_hub_envelopes);
    size_t in_use = 0;
    const adf_evt_hub_envelope_t *env_base = (const adf_evt_hub_envelope_t *)s_evt_hub_envelopes.data;
    for (size_t i = 0; i < pool_sz; i++) {
        if (env_base[i].in_use) {
            in_use++;
        }
    }
    size_t free_count = 0;
    int walk = s_evt_hub_env_free_head;
    while (walk >= 0 && (size_t)walk < pool_sz) {
        free_count++;
        walk = env_base[walk].next_free;
    }
    ESP_LOGI(TAG, "Envelope pool: total=%u  in_use=%u  free_list=%u",
             (unsigned)pool_sz, (unsigned)in_use, (unsigned)free_count);
    ESP_LOGI(TAG, "Queue-full deliveries: %u", (unsigned)s_evt_hub_queue_drops);
    for (size_t i = 0; i < pool_sz; i++) {
        if (env_base[i].in_use) {
            ESP_LOGI(TAG, "  [%u] in_use  domain='%s'  event_id=%u  ref_count=%u  gen=%u",
                     (unsigned)i,
                     env_base[i].event.domain ? env_base[i].event.domain : "?",
                     (unsigned)env_base[i].event.event_id,
                     (unsigned)env_base[i].ref_count,
                     (unsigned)env_base[i].generation);
        }
    }
    xSemaphoreGive(s_evt_hub_env_mutex);
    ESP_LOGI(TAG, "======== End Dump ========");
}

esp_err_t adf_event_hub_get_stats(adf_event_hub_stats_t *stats)
{
    if (!stats) {
        ESP_LOGE(TAG, "GetStats failed: stats pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!evt_hub_initialised()) {
        ESP_LOGE(TAG, "GetStats failed: module not initialised");
        return ESP_ERR_INVALID_STATE;
    }
    adf_event_domain_stat_t *saved_domains = stats->domains;
    size_t saved_cap = stats->domains_capacity;
    memset(stats, 0, sizeof(*stats));
    stats->domains = saved_domains;
    stats->domains_capacity = saved_cap;

    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    size_t dom_count = adf_vec_size(&s_evt_hub_domains);
    stats->domain_count = dom_count;
    const adf_evt_hub_domain_t *base = (const adf_evt_hub_domain_t *)s_evt_hub_domains.data;
    size_t selected = stats->domains ? stats->domains_capacity : 0;
    if (selected > dom_count) {
        selected = dom_count;
    }
    for (size_t d = 0; d < selected; d++) {
        if (!stats->domains[d].domain || stats->domains[d].domain_capacity <= strlen(base[d].domain_str)) {
            xSemaphoreGive(s_evt_hub_mutex);
            ESP_LOGE(TAG, "GetStats failed: domain name buffer %u is too small", (unsigned)d);
            return ESP_ERR_INVALID_SIZE;
        }
    }
    size_t filled = 0;
    for (size_t d = 0; d < dom_count; d++) {
        const adf_evt_hub_domain_t *dom = &base[d];
        size_t sub_n = adf_vec_size(&dom->sub_vec);
        size_t cb_n = 0;
        size_t q_n = 0;
        const adf_evt_hub_subscriber_t *subs = (const adf_evt_hub_subscriber_t *)dom->sub_vec.data;
        for (size_t s = 0; s < sub_n; s++) {
            if (subs[s].tgt.is_queue) {
                q_n++;
            } else {
                cb_n++;
            }
        }
        if (stats->domains && filled < stats->domains_capacity) {
            adf_event_domain_stat_t *ds = &stats->domains[filled];
            memcpy(ds->domain, dom->domain_str, strlen(dom->domain_str) + 1);
            ds->cb_subscriber_count = cb_n;
            ds->queue_subscriber_count = q_n;
            filled++;
        }
    }

    xSemaphoreGive(s_evt_hub_mutex);
    xSemaphoreTake(s_evt_hub_env_mutex, portMAX_DELAY);
    size_t pool_sz = adf_vec_size(&s_evt_hub_envelopes);
    size_t in_use = 0;
    const adf_evt_hub_envelope_t *env_base = (const adf_evt_hub_envelope_t *)s_evt_hub_envelopes.data;
    for (size_t i = 0; i < pool_sz; i++) {
        if (env_base[i].in_use) {
            in_use++;
        }
    }
    stats->envelope_pool_size = pool_sz;
    stats->envelopes_in_use = in_use;
    stats->queue_drop_count = s_evt_hub_queue_drops;
    xSemaphoreGive(s_evt_hub_env_mutex);
    return ESP_OK;
}
