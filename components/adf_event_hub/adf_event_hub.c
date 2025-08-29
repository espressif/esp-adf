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

static const char *TAG = "ADF_EVENT_HUB";

#define ADF_EVENT_HUB_INIT_CAP   2u  /* Initial vec capacity; grows automatically */
#define ADF_DOMAIN_KEY_WILDCARD  0u

/* Small-buffer threshold for the per-publish target snapshot. Stack budget: 40 * 12B = 480B */
#define ADF_EVT_HUB_SNAP_STACK_MAX  40u

/**
 * @brief  Lightweight key-to-string reverse index for domain lookup.
 *
 *         domain_key is always non-zero; zero is reserved as wildcard sentinel.
 */
typedef struct {
    uint32_t    domain_key;  /*!< Non-zero djb2 hash used as domain identity */
    const char *domain_str;  /*!< Points to the string copy owned by adf_evt_hub_domain_t */
} adf_evt_hub_domain_key_map_t;

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
    uint32_t                  domain_key;  /*!< Hash key of the subscribed domain */
    uint16_t                  event_id;    /*!< Event ID filter; ADF_EVENT_ANY_ID matches all events */
    adf_evt_hub_sub_target_t  tgt;         /*!< Delivery target (queue or callback) */
} adf_evt_hub_subscriber_t;

/**
 * @brief  One entry per registered domain, stored in s_evt_hub_domains.
 *
 *         Each adf_event_hub_create() appends one adf_evt_hub_domain_t to s_evt_hub_domains.
 *         Subscribers are stored in the per-domain sub_vec dynamic array.
 *         On publish, the matching domain is found by key, then sub_vec is
 *         iterated to dispatch matching subscribers via callback or xQueueSend.
 *         subscriber_head is refreshed after every sub_vec mutation and serves
 *         as a fast "has subscribers" guard on the publish hot path.
 */
typedef struct adf_evt_hub_domain {
    adf_event_hub_t           handle;           /*!< Opaque handle returned to callers; encodes domain_key */
    uint32_t                  domain_key;       /*!< Non-zero djb2 hash of domain_str */
    char                     *domain_str;       /*!< Heap-allocated copy of the domain name */
    uint16_t                  ref_count;        /*!< Reference count; domain is destroyed when it reaches 0 */
    adf_vec_t                 sub_vec;          /*!< Per-domain adf_evt_hub_subscriber_t dynamic array */
    adf_evt_hub_subscriber_t *subscriber_head;  /*!< adf_vec_front(&sub_vec); NULL when no subscribers */
} adf_evt_hub_domain_t;

/**
 * @brief  Shared ref-counted container for queue-mode payload delivery.
 *
 *         Slots are stored in s_evt_hub_envelopes (adf_vec_t). The vec only ever grows;
 *         slots are never removed, only recycled via in_use = false. This keeps
 *         every slot index permanently stable, so the _opaque encoding is safe.
 *
 *         _opaque encoding in adf_event_delivery_t:
 *         bits 15:0  = vec index   (0 .. adf_vec_size(&s_evt_hub_envelopes) - 1)
 *         bits 31:16 = generation  (guards against stale delivery_done after slot reuse)
 */
typedef struct {
    adf_event_t                     event;        /*!< Shallow copy of the published event */
    adf_event_payload_release_cb_t  release_cb;   /*!< Called once when ref_count drops to zero */
    void                           *release_ctx;  /*!< Opaque context forwarded to release_cb */
    uint16_t                        ref_count;    /*!< Outstanding queue-mode delivery references */
    uint16_t                        generation;   /*!< Incremented on each slot reuse; upper 16 bits of _opaque */
    bool                            in_use;       /*!< True while ref_count > 0 */
    int                             next_free;    /*!< Free-list chain index; valid only when in_use == false */
} adf_evt_hub_envelope_t;

static adf_vec_t s_evt_hub_domains;       /* elem: adf_evt_hub_domain_t */
static adf_vec_t s_evt_hub_domains_keys;  /* elem: adf_evt_hub_domain_key_map_t — key↔string index */

static SemaphoreHandle_t s_evt_hub_mutex = NULL;

static adf_vec_t s_evt_hub_envelopes;  /* elem: adf_evt_hub_envelope_t; only grows */
static SemaphoreHandle_t s_evt_hub_env_mutex = NULL;
static int s_evt_hub_env_free_head = -1;  /* head of envelope free-list; -1 = empty */

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
static volatile uint32_t s_evt_hub_init_state = ADF_EVT_HUB_INIT_NONE;

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

static inline int evt_hub_envelope_alloc(const adf_event_t *event,
                                         adf_event_payload_release_cb_t release_cb,
                                         void *release_ctx,
                                         uint16_t ref_count,
                                         uint16_t *out_gen)
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
        if (adf_vec_push(&s_evt_hub_envelopes, &blank) != ADF_VEC_OK) {
            xSemaphoreGive(s_evt_hub_env_mutex);
            return -1;  /* OOM */
        }
        idx = (int)s_evt_hub_envelopes.size - 1;
        env = (adf_evt_hub_envelope_t *)s_evt_hub_envelopes.data + idx;
        env->generation = 0;
    }

    env->in_use = true;
    env->event = *event;
    env->release_cb = release_cb;
    env->release_ctx = release_ctx;
    env->ref_count = ref_count;
    env->generation++;
    *out_gen = env->generation;

    xSemaphoreGive(s_evt_hub_env_mutex);
    return idx;
}

static inline void evt_hub_envelope_release(int idx, uint16_t gen,
                                            adf_event_payload_release_cb_t *out_cb,
                                            void **out_ctx,
                                            const void **out_payload)
{
    *out_cb = NULL;
    *out_ctx = NULL;
    *out_payload = NULL;
    xSemaphoreTake(s_evt_hub_env_mutex, portMAX_DELAY);
    adf_evt_hub_envelope_t *env = (adf_evt_hub_envelope_t *)s_evt_hub_envelopes.data + idx;
    if (!env->in_use || env->generation != gen) {
        xSemaphoreGive(s_evt_hub_env_mutex);
        return;
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
    }
    xSemaphoreGive(s_evt_hub_env_mutex);
}

static inline esp_err_t hub_ensure_init(void)
{
    /* Fast path: the ACQUIRE load pairs with the RELEASE store at the end of
     * successful init, so once we observe DONE all static resources are
     * visible here without further synchronisation. */
    if (__atomic_load_n(&s_evt_hub_init_state, __ATOMIC_ACQUIRE) == ADF_EVT_HUB_INIT_DONE) {
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
        return __atomic_load_n(&s_evt_hub_init_state, __ATOMIC_ACQUIRE) == ADF_EVT_HUB_INIT_DONE
                   ? ESP_OK
                   : ESP_ERR_NO_MEM;
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
    if (adf_vec_init(&s_evt_hub_domains_keys, sizeof(adf_evt_hub_domain_key_map_t),
                     ADF_EVENT_HUB_INIT_CAP) != ADF_VEC_OK) {
        adf_vec_destroy(&s_evt_hub_domains);
        vSemaphoreDelete(env_mtx);
        vSemaphoreDelete(mtx);
        ESP_LOGE(TAG, "Init failed: cannot allocate key-index table");
        goto fail;
    }
    if (adf_vec_init(&s_evt_hub_envelopes, sizeof(adf_evt_hub_envelope_t), ADF_EVENT_HUB_INIT_CAP) != ADF_VEC_OK) {
        adf_vec_destroy(&s_evt_hub_domains_keys);
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
    new_entry.ref_count = 1;
    new_entry.subscriber_head = NULL;
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
    adf_evt_hub_domain_key_map_t key_entry = {
        .domain_key = key,
        .domain_str = str_copy,
    };
    if (adf_vec_push(&s_evt_hub_domains_keys, &key_entry) != ADF_VEC_OK) {
        ESP_LOGW(TAG, "Register '%s': key-index append failed, domain still usable (non-fatal)", domain);
    }
    return evt_hub_domain_find(key);
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
    adf_evt_hub_domain_t *existing = evt_hub_domain_find(key);
    if (existing) {
        existing->ref_count++;
        *out_hub = existing->handle;
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGD(TAG, "Create '%s': already registered, refcount -> %u", domain, (unsigned)existing->ref_count);
        return ESP_OK;
    }
    adf_evt_hub_domain_t *d = domain_register(domain, key);
    if (!d) {
        xSemaphoreGive(s_evt_hub_mutex);
        return ESP_ERR_NO_MEM;  /* domain_register already logged */
    }
    xSemaphoreGive(s_evt_hub_mutex);
    *out_hub = (adf_event_hub_t)(uintptr_t)key;
    ESP_LOGI(TAG, "Create '%s': domain registered", domain);
    return ESP_OK;
}

esp_err_t adf_event_hub_destroy(adf_event_hub_t hub)
{
    if (!hub) {
        ESP_LOGE(TAG, "Destroy failed: hub is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_evt_hub_mutex) {
        ESP_LOGE(TAG, "Destroy failed: module not initialised");
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t key = (uint32_t)(uintptr_t)hub;
    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    adf_evt_hub_domain_t *base = (adf_evt_hub_domain_t *)s_evt_hub_domains.data;
    size_t n = s_evt_hub_domains.size;
    size_t domain_idx = n;
    for (size_t i = 0; i < n; i++) {
        if (base[i].domain_key == key) {
            domain_idx = i;
            break;
        }
    }
    if (domain_idx == n) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Destroy failed: no domain matches this hub handle (key=0x%08x)", (unsigned)key);
        return ESP_ERR_NOT_FOUND;
    }
    base[domain_idx].ref_count--;
    if (base[domain_idx].ref_count > 0) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGD(TAG, "Destroy '%s': refcount -> %u, deferred",
                 base[domain_idx].domain_str ? base[domain_idx].domain_str : "?",
                 (unsigned)base[domain_idx].ref_count);
        return ESP_OK;
    }
    char *str = base[domain_idx].domain_str;
    /* log before free so domain name is still accessible */
    ESP_LOGI(TAG, "Destroy '%s': refcount reached 0, domain removed", str ? str : "?");
    adf_vec_destroy(&base[domain_idx].sub_vec);
    adf_vec_remove_swap(&s_evt_hub_domains, domain_idx);
    free(str);
    adf_evt_hub_domain_key_map_t *kbase =
        (adf_evt_hub_domain_key_map_t *)s_evt_hub_domains_keys.data;
    for (size_t i = 0; i < s_evt_hub_domains_keys.size; i++) {
        if (kbase[i].domain_key == key) {
            adf_vec_remove_swap(&s_evt_hub_domains_keys, i);
            break;
        }
    }
    xSemaphoreGive(s_evt_hub_mutex);
    return ESP_OK;
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
    if (!s_evt_hub_mutex) {
        ESP_LOGE(TAG, "Subscribe failed: module not initialised, call adf_event_hub_create first");
        return ESP_ERR_INVALID_STATE;
    }
    /* NULL event_domain falls back to the hub's own domain */
    uint32_t target_key = info->event_domain
                              ? evt_hub_str_to_hash(info->event_domain)
                              : (uint32_t)(uintptr_t)hub;

    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    adf_evt_hub_domain_t *target = evt_hub_domain_find(target_key);
    if (!target) {
        if (!info->event_domain || target_key == ADF_DOMAIN_KEY_WILDCARD) {
            xSemaphoreGive(s_evt_hub_mutex);
            ESP_LOGE(TAG, "Subscribe failed: hub's own domain is not registered");
            return ESP_ERR_NOT_FOUND;
        }
        target = domain_register(info->event_domain, target_key);
        if (!target) {
            xSemaphoreGive(s_evt_hub_mutex);
            return ESP_ERR_NO_MEM;  /* domain_register already logged */
        }
        ESP_LOGI(TAG, "Subscribe: domain '%s' not found, auto-created", info->event_domain);
    }
    adf_evt_hub_subscriber_t sub;
    memset(&sub, 0, sizeof(sub));
    sub.domain_key = target_key;
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
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Subscribe failed: out of memory adding subscriber in domain '%s'",
                 info->event_domain ? info->event_domain : "own");
        return ESP_ERR_NO_MEM;
    }
    /* refresh subscriber_head; adf_vec_push may have reallocated sub_vec.data */
    target->subscriber_head = (adf_evt_hub_subscriber_t *)adf_vec_front(&target->sub_vec);
    xSemaphoreGive(s_evt_hub_mutex);
    return ESP_OK;
}

esp_err_t adf_event_hub_unsubscribe(adf_event_hub_t hub, const char *domain, uint16_t event_id)
{
    if (!hub) {
        ESP_LOGE(TAG, "Unsubscribe failed: hub is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_evt_hub_mutex) {
        ESP_LOGE(TAG, "Unsubscribe failed: module not initialised");
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t hub_key = (uint32_t)(uintptr_t)hub;
    uint32_t domain_key = domain ? evt_hub_str_to_hash(domain) : hub_key;

    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    adf_evt_hub_domain_t *target = evt_hub_domain_find(domain_key);
    if (!target) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGW(TAG, "Unsubscribe: domain '%s' not found", domain ? domain : "own");
        return ESP_ERR_NOT_FOUND;
    }
    bool removed = false;
    adf_evt_hub_subscriber_t *sub_base = (adf_evt_hub_subscriber_t *)target->sub_vec.data;
    for (size_t i = 0; i < target->sub_vec.size;) {
        bool id_match = (event_id == ADF_EVENT_ANY_ID ||
                         sub_base[i].event_id == event_id);
        if (id_match) {
            adf_vec_remove_swap(&target->sub_vec, i);
            removed = true;
        } else {
            i++;
        }
    }
    /* refresh subscriber_head after removal */
    target->subscriber_head = (adf_evt_hub_subscriber_t *)adf_vec_front(&target->sub_vec);
    xSemaphoreGive(s_evt_hub_mutex);
    if (!removed) {
        ESP_LOGW(TAG, "Unsubscribe: no matching subscriber in domain '%s' for event_id=%u",
                 domain ? domain : "own", (unsigned)event_id);
    }
    return removed ? ESP_OK : ESP_ERR_NOT_FOUND;
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
    adf_evt_hub_sub_target_t snap_stack[ADF_EVT_HUB_SNAP_STACK_MAX];
    adf_evt_hub_sub_target_t *snap_heap = NULL;
    adf_evt_hub_sub_target_t *snap = snap_stack;
    int env_idx = -1;
    uint16_t env_gen = 0;
    bool envelope_owns_payload = false;

    if (!s_evt_hub_mutex) {
        ESP_LOGE(TAG, "Publish failed: module not initialised");
        ret = ESP_ERR_INVALID_STATE;
        goto cleanup;
    }
    uint32_t hub_key = (uint32_t)(uintptr_t)hub;
    /* snapshot matching targets under mutex; deliver outside to avoid holding lock */
    xSemaphoreTake(s_evt_hub_mutex, portMAX_DELAY);
    adf_evt_hub_domain_t *source = evt_hub_domain_find(hub_key);
    if (!source) {
        xSemaphoreGive(s_evt_hub_mutex);
        ESP_LOGE(TAG, "Publish failed: hub is not registered (key=0x%08x)", (unsigned)hub_key);
        ret = ESP_ERR_NOT_FOUND;
        goto cleanup;
    }
    if (!source->subscriber_head) {
        xSemaphoreGive(s_evt_hub_mutex);
        goto cleanup;  /* no subscribers; release_cb fires in cleanup */
    }
    const adf_evt_hub_subscriber_t *sub_base = (const adf_evt_hub_subscriber_t *)source->sub_vec.data;
    size_t sub_count = source->sub_vec.size;
    if (sub_count > ADF_EVT_HUB_SNAP_STACK_MAX) {
        snap_heap = (adf_evt_hub_sub_target_t *)malloc(sub_count * sizeof(*snap_heap));
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
            snap[snap_count++] = sub_base[i].tgt;
        }
    }
    xSemaphoreGive(s_evt_hub_mutex);

    if (queue_count > 0) {
        env_idx = evt_hub_envelope_alloc(event, release_cb, release_ctx, queue_count, &env_gen);
        if (env_idx < 0) {
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
        adf_evt_hub_sub_target_t *tgt = &snap[i];
        if (tgt->is_queue) {
            if (!envelope_owns_payload) {
                continue;  /* envelope alloc failed; queue delivery impossible */
            }
            adf_event_delivery_t delivery = {
                .event = *event,
                ._opaque = evt_hub_make_opaque(env_idx, env_gen),
            };
            if (xQueueSend(tgt->target.queue, &delivery, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Publish '%s'/%u: subscriber queue full, event dropped",
                         event->domain ? event->domain : "?", (unsigned)event->event_id);
                /* subscriber will not call delivery_done; release this ref now */
                adf_event_payload_release_cb_t drop_cb;
                void *drop_ctx;
                const void *drop_payload;
                evt_hub_envelope_release(env_idx, env_gen, &drop_cb, &drop_ctx, &drop_payload);
                if (drop_cb) {
                    drop_cb(drop_payload, drop_ctx);
                }
            }
        } else {
            tgt->target.cb.handler(event, tgt->target.cb.ctx);
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
    if (!s_evt_hub_mutex) {
        ESP_LOGE(TAG, "DeliveryDone failed: module not initialised");
        return ESP_ERR_INVALID_STATE;
    }
    int idx = evt_hub_opaque_idx(delivery->_opaque);
    uint16_t gen = evt_hub_opaque_gen(delivery->_opaque);

    if (idx < 0 || (size_t)idx >= adf_vec_size(&s_evt_hub_envelopes)) {
        ESP_LOGE(TAG, "DeliveryDone failed: invalid delivery token (idx=%d, pool_size=%u)",
                 idx, (unsigned)adf_vec_size(&s_evt_hub_envelopes));
        return ESP_ERR_INVALID_ARG;
    }
    adf_event_payload_release_cb_t cb = NULL;
    void *cb_ctx = NULL;
    const void *payload = NULL;
    evt_hub_envelope_release(idx, gen, &cb, &cb_ctx, &payload);
    if (cb) {
        cb(payload, cb_ctx);
    }
    return ESP_OK;
}

void adf_event_hub_dump(void)
{
    if (!s_evt_hub_mutex) {
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
        ESP_LOGI(TAG, "  [%u] domain='%s'  key=0x%08x  subscribers=%u",
                 (unsigned)d, dom->domain_str ? dom->domain_str : "(null)",
                 (unsigned)dom->domain_key, (unsigned)sub_n);

        const adf_evt_hub_subscriber_t *subs = (const adf_evt_hub_subscriber_t *)dom->sub_vec.data;
        for (size_t s = 0; s < sub_n; s++) {
            const adf_evt_hub_subscriber_t *sub = &subs[s];
            const char *mode = sub->tgt.is_queue ? "queue" : "callback";
            if (sub->event_id == ADF_EVENT_ANY_ID) {
                ESP_LOGI(TAG, "    [%u] mode=%-8s  event_id=ANY",
                         (unsigned)s, mode);
            } else {
                ESP_LOGI(TAG, "    [%u] mode=%-8s  event_id=%u",
                         (unsigned)s, mode, (unsigned)sub->event_id);
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
    if (!s_evt_hub_mutex) {
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
            ds->domain = dom->domain_str;
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
    xSemaphoreGive(s_evt_hub_env_mutex);
    return ESP_OK;
}
