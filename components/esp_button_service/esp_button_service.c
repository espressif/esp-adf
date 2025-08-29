/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include "adf_mem.h"
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include "sys/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "adf_vec.h"
#include "esp_button_service.h"
#include "esp_board_device.h"
#include "esp_board_manager_defs.h"
#include "dev_button.h"
#include "iot_button.h"

/* Linked list head from board-generated code (esp_board_manager). */
extern const esp_board_device_desc_t g_esp_board_devices[];

/* Cursor sentinel: iteration finished (must not restart from head when the last match is the list tail). */
#define BMGR_DEVICE_ITER_DONE  ((void *)(uintptr_t)1)

static const char *TAG = "BUTTON_SERVICE";

struct esp_button_service {
    esp_service_t  base;  /* must be first member */
};

struct btn_ctx_node;

/**
 * All internal state.  Allocated in esp_button_service_create and stored in
 * base.user_data so it is accessible from every ops callback without exposing
 * any fields in the public header.
 *
 * Must be declared before btn_ctx_t because each ctx node holds a btn_priv_t *.
 */
typedef struct btn_priv {
    esp_service_t *service;     /* back-pointer for esp_service_publish_event */
    _Atomic bool   forwarding;  /* true when events should be forwarded */
    uint32_t       event_mask;

    /**
     * Device names that were successfully initialized.
     * Used in on_deinit to call esp_board_device_deinit in reverse order.
     */
    adf_vec_t  init_devs;

    STAILQ_HEAD(ctx_list, btn_ctx_node)  ctx_list;
} btn_priv_t;

/**
 * Per-button-handle context passed as user_data to iot_button_register_cb.
 * One instance per physical button handle.  Multiple handles can share the
 * same device (e.g. an ADC button group with N voltage levels).
 */
typedef struct btn_ctx_node {
    const char                 *label;   /* static board-manager device name — not owned */
    button_handle_t             handle;  /* iot_button handle for unregister */
    btn_priv_t                 *priv;    /* back-pointer to private state */
    STAILQ_ENTRY(btn_ctx_node)  entries;
} btn_ctx_t;

static btn_priv_t *button_priv_get(esp_service_t *service)
{
    void *ud = NULL;
    if (esp_service_get_user_data(service, &ud) != ESP_OK) {
        return NULL;
    }
    return (btn_priv_t *)ud;
}

static const struct {
    uint32_t        mask;
    button_event_t  iot_evt;
} s_evt_map[] = {
    {ESP_BUTTON_SERVICE_EVT_MASK_PRESS_DOWN, BUTTON_PRESS_DOWN},
    {ESP_BUTTON_SERVICE_EVT_MASK_PRESS_UP, BUTTON_PRESS_UP},
    {ESP_BUTTON_SERVICE_EVT_MASK_SINGLE_CLICK, BUTTON_SINGLE_CLICK},
    {ESP_BUTTON_SERVICE_EVT_MASK_DOUBLE_CLICK, BUTTON_DOUBLE_CLICK},
    {ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_START, BUTTON_LONG_PRESS_START},
    {ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_HOLD, BUTTON_LONG_PRESS_HOLD},
    {ESP_BUTTON_SERVICE_EVT_MASK_LONG_PRESS_UP, BUTTON_LONG_PRESS_UP},
    {ESP_BUTTON_SERVICE_EVT_MASK_PRESS_REPEAT, BUTTON_PRESS_REPEAT},
    {ESP_BUTTON_SERVICE_EVT_MASK_PRESS_REPEAT_DONE, BUTTON_PRESS_REPEAT_DONE},
    {ESP_BUTTON_SERVICE_EVT_MASK_PRESS_END, BUTTON_PRESS_END},
};

static void release_payload(const void *payload, void *ctx)
{
    (void)ctx;
    adf_free((void *)payload);
}

static esp_err_t adf_vec_err_to_esp_err(int vec_err)
{
    switch (vec_err) {
        case ADF_VEC_OK:
            return ESP_OK;
        case ADF_VEC_ERR_NO_MEM:
            return ESP_ERR_NO_MEM;
        case ADF_VEC_ERR_ARG:
            return ESP_ERR_INVALID_ARG;
        default:
            return ESP_FAIL;
    }
}

static uint16_t iot_event_to_btn_evt(button_event_t evt)
{
    switch (evt) {
        case BUTTON_PRESS_DOWN:
            return ESP_BUTTON_SERVICE_EVT_PRESS_DOWN;
        case BUTTON_PRESS_UP:
            return ESP_BUTTON_SERVICE_EVT_PRESS_UP;
        case BUTTON_SINGLE_CLICK:
            return ESP_BUTTON_SERVICE_EVT_SINGLE_CLICK;
        case BUTTON_DOUBLE_CLICK:
            return ESP_BUTTON_SERVICE_EVT_DOUBLE_CLICK;
        case BUTTON_LONG_PRESS_START:
            return ESP_BUTTON_SERVICE_EVT_LONG_PRESS_START;
        case BUTTON_LONG_PRESS_HOLD:
            return ESP_BUTTON_SERVICE_EVT_LONG_PRESS_HOLD;
        case BUTTON_LONG_PRESS_UP:
            return ESP_BUTTON_SERVICE_EVT_LONG_PRESS_UP;
        case BUTTON_PRESS_REPEAT:
            return ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT;
        case BUTTON_PRESS_REPEAT_DONE:
            return ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT_DONE;
        case BUTTON_PRESS_END:
            return ESP_BUTTON_SERVICE_EVT_PRESS_END;
        default:
            return 0;
    }
}

static void btn_iot_cb(void *button_handle, void *user_data)
{
    btn_ctx_t *ctx = (btn_ctx_t *)user_data;
    btn_priv_t *priv = ctx->priv;

    if (!atomic_load_explicit(&priv->forwarding, memory_order_relaxed)) {
        return;
    }
    button_event_t iot_evt = iot_button_get_event((button_handle_t)button_handle);
    uint16_t event_id = iot_event_to_btn_evt(iot_evt);
    if (event_id == 0) {
        return;
    }
    esp_button_service_payload_t *pl = adf_malloc(sizeof(*pl));
    if (pl == NULL) {
        ESP_LOGW(TAG, "Publish '%s'/%u: no memory for payload, event dropped",
                 ctx->label, (unsigned)event_id);
        return;
    }
    pl->label = ctx->label;
    esp_err_t ret = esp_service_publish_event(priv->service,
                                              event_id,
                                              pl, sizeof(*pl),
                                              release_payload, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Publish '%s'/%u: esp_service_publish_event failed, %s",
                 ctx->label, (unsigned)event_id, esp_err_to_name(ret));
    }
}

static esp_err_t register_button_handle(btn_priv_t *priv,
                                        button_handle_t handle,
                                        const char *label)
{
    btn_ctx_t *ctx = adf_calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Register callbacks failed: no memory for context ('%s')", label);
        return ESP_ERR_NO_MEM;
    }
    ctx->label = label;
    ctx->handle = handle;
    ctx->priv = priv;

    int registered_cnt = 0;
    for (int i = 0; i < (int)(sizeof(s_evt_map) / sizeof(s_evt_map[0])); i++) {
        if (!(priv->event_mask & s_evt_map[i].mask)) {
            continue;
        }
        esp_err_t ret = iot_button_register_cb(handle, s_evt_map[i].iot_evt,
                                               NULL, btn_iot_cb, ctx);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Register callbacks failed: iot_button_register_cb event %d on '%s', %s",
                     (int)s_evt_map[i].iot_evt, label, esp_err_to_name(ret));
            for (int j = i - 1; j >= 0 && registered_cnt > 0; j--) {
                if (priv->event_mask & s_evt_map[j].mask) {
                    iot_button_unregister_cb(handle, s_evt_map[j].iot_evt, NULL);
                    registered_cnt--;
                }
            }
            adf_free(ctx);
            return ret;
        }
        registered_cnt++;
    }

    STAILQ_INSERT_TAIL(&priv->ctx_list, ctx, entries);
    return ESP_OK;
}

static esp_err_t init_device(btn_priv_t *priv, const char *dev_name)
{
    esp_err_t ret = esp_board_device_init(dev_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init device failed: '%s', %s", dev_name, esp_err_to_name(ret));
        return ret;
    }

    dev_button_handles_t *handles = NULL;
    ret = esp_board_device_get_handle(dev_name, (void **)&handles);
    if (ret != ESP_OK || handles == NULL) {
        ESP_LOGE(TAG, "Get device handle failed: '%s', %s", dev_name, esp_err_to_name(ret));
        esp_board_device_deinit(dev_name);
        return (ret == ESP_OK) ? ESP_FAIL : ret;
    }

    dev_button_config_t *cfg = NULL;
    (void)esp_board_device_get_config(dev_name, (void **)&cfg);

    int vec_ret = adf_vec_push(&priv->init_devs, &dev_name);
    if (vec_ret != ADF_VEC_OK) {
        ESP_LOGE(TAG, "Init device failed: no memory recording '%s'", dev_name);
        esp_board_device_deinit(dev_name);
        return adf_vec_err_to_esp_err(vec_ret);
    }

    for (int i = 0; i < handles->num_buttons; i++) {
        const char *button_label = dev_name;
        if (cfg
            && cfg->sub_type
            && (strcmp(cfg->sub_type, "adc_multi") == 0)
            && (i < cfg->sub_cfg.adc.multi.button_num)
            && cfg->sub_cfg.adc.multi.button_labels[i]) {
            button_label = cfg->sub_cfg.adc.multi.button_labels[i];
        }
        ret = register_button_handle(priv, handles->button_handles[i], button_label);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

static esp_err_t button_board_iterate_device_name_by_type(const char *type, void **cursor, const char **out_name)
{
    if (!type || !cursor || !out_name) {
        return ESP_ERR_INVALID_ARG;
    }
    if (*cursor == BMGR_DEVICE_ITER_DONE) {
        return ESP_BOARD_ERR_DEVICE_NOT_FOUND;
    }

    const esp_board_device_desc_t *desc;
    if (*cursor == NULL) {
        desc = g_esp_board_devices;
    } else {
        desc = (const esp_board_device_desc_t *)(*cursor);
    }

    while (desc != NULL) {
        if (desc->name && desc->type && strcmp(desc->type, type) == 0) {
            *out_name = desc->name;
            *cursor = (void *)desc->next;
            if (*cursor == NULL) {
                *cursor = BMGR_DEVICE_ITER_DONE;
            }
            return ESP_OK;
        }
        desc = desc->next;
    }
    *cursor = BMGR_DEVICE_ITER_DONE;
    return ESP_BOARD_ERR_DEVICE_NOT_FOUND;
}

static esp_err_t button_on_init(esp_service_t *service, const esp_service_config_t *config)
{
    (void)config;
    btn_priv_t *priv = button_priv_get(service);
    void *cursor = NULL;
    const char *dev_name = NULL;
    int vec_ret = adf_vec_init(&priv->init_devs, sizeof(const char *), 0);
    if (vec_ret != ADF_VEC_OK) {
        return adf_vec_err_to_esp_err(vec_ret);
    }
    while (1) {
        esp_err_t ret = button_board_iterate_device_name_by_type(ESP_BOARD_DEVICE_TYPE_BUTTON, &cursor, &dev_name);
        if (ret == ESP_BOARD_ERR_DEVICE_NOT_FOUND) {
            break;
        }
        if (ret != ESP_OK) {
            return ret;
        }
        ret = init_device(priv, dev_name);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    if (adf_vec_size(&priv->init_devs) == 0) {
        ESP_LOGE(TAG, "Init failed: no board-manager device of type '%s'", ESP_BOARD_DEVICE_TYPE_BUTTON);
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static esp_err_t button_on_deinit(esp_service_t *service)
{
    btn_priv_t *priv = button_priv_get(service);
    if (priv == NULL) {
        return ESP_OK;
    }
    atomic_store_explicit(&priv->forwarding, false, memory_order_relaxed);
    /* Explicitly unregister callbacks because board devices may stay alive
     * (ref-count not reaching zero) after service deinit. */
    btn_ctx_t *ctx = NULL;
    STAILQ_FOREACH (ctx, &priv->ctx_list, entries) {
        for (int i = 0; i < (int)(sizeof(s_evt_map) / sizeof(s_evt_map[0])); i++) {
            if (priv->event_mask & s_evt_map[i].mask) {
                iot_button_unregister_cb(ctx->handle, s_evt_map[i].iot_evt, NULL);
            }
        }
    }
    /* Deinit devices first: deletes iot_button handles so btn_iot_cb cannot
     * fire again before we adf_free the ctx memory below. */
    for (size_t i = adf_vec_size(&priv->init_devs); i > 0; i--) {
        const char *const *dev_name = ADF_VEC_AT(const char *, &priv->init_devs, i - 1);
        if (dev_name == NULL) {
            continue;
        }
        esp_err_t ret = esp_board_device_deinit(*dev_name);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Deinit device '%s': %s", *dev_name, esp_err_to_name(ret));
        }
    }
    adf_vec_destroy(&priv->init_devs);

    while ((ctx = STAILQ_FIRST(&priv->ctx_list)) != NULL) {
        STAILQ_REMOVE_HEAD(&priv->ctx_list, entries);
        adf_free(ctx);
    }

    adf_free(priv);
    (void)esp_service_set_user_data(service, NULL);
    return ESP_OK;
}

static esp_err_t button_on_start(esp_service_t *service)
{
    btn_priv_t *priv = button_priv_get(service);
    atomic_store_explicit(&priv->forwarding, true, memory_order_relaxed);
    ESP_LOGI(TAG, "Start 'esp_button_service': forwarding events");
    return ESP_OK;
}

static esp_err_t button_on_stop(esp_service_t *service)
{
    btn_priv_t *priv = button_priv_get(service);
    atomic_store_explicit(&priv->forwarding, false, memory_order_relaxed);
    ESP_LOGI(TAG, "Stop 'esp_button_service': forwarding disabled");
    return ESP_OK;
}

static esp_err_t button_on_pause(esp_service_t *service)
{
    btn_priv_t *priv = button_priv_get(service);
    atomic_store_explicit(&priv->forwarding, false, memory_order_relaxed);
    ESP_LOGI(TAG, "Pause 'esp_button_service': forwarding suspended");
    return ESP_OK;
}

static esp_err_t button_on_resume(esp_service_t *service)
{
    btn_priv_t *priv = button_priv_get(service);
    atomic_store_explicit(&priv->forwarding, true, memory_order_relaxed);
    ESP_LOGI(TAG, "Resume 'esp_button_service': forwarding events");
    return ESP_OK;
}

static esp_err_t button_on_lowpower_enter(esp_service_t *service)
{
    btn_priv_t *priv = button_priv_get(service);
    atomic_store_explicit(&priv->forwarding, false, memory_order_relaxed);
    ESP_LOGI(TAG, "Low-power enter 'esp_button_service': events suppressed");
    return ESP_OK;
}

static esp_err_t button_on_lowpower_exit(esp_service_t *service)
{
    btn_priv_t *priv = button_priv_get(service);
    atomic_store_explicit(&priv->forwarding, true, memory_order_relaxed);
    ESP_LOGI(TAG, "Low-power exit 'esp_button_service': events resumed");
    return ESP_OK;
}

static const char *button_event_to_name(uint16_t event_id)
{
    switch (event_id) {
        case ESP_BUTTON_SERVICE_EVT_PRESS_DOWN:
            return "PRESS_DOWN";
        case ESP_BUTTON_SERVICE_EVT_PRESS_UP:
            return "PRESS_UP";
        case ESP_BUTTON_SERVICE_EVT_SINGLE_CLICK:
            return "SINGLE_CLICK";
        case ESP_BUTTON_SERVICE_EVT_DOUBLE_CLICK:
            return "DOUBLE_CLICK";
        case ESP_BUTTON_SERVICE_EVT_LONG_PRESS_START:
            return "LONG_PRESS_START";
        case ESP_BUTTON_SERVICE_EVT_LONG_PRESS_HOLD:
            return "LONG_PRESS_HOLD";
        case ESP_BUTTON_SERVICE_EVT_LONG_PRESS_UP:
            return "LONG_PRESS_UP";
        case ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT:
            return "PRESS_REPEAT";
        case ESP_BUTTON_SERVICE_EVT_PRESS_REPEAT_DONE:
            return "PRESS_REPEAT_DONE";
        case ESP_BUTTON_SERVICE_EVT_PRESS_END:
            return "PRESS_END";
        default:
            return NULL;
    }
}

static const esp_service_ops_t s_btn_ops = {
    .on_init           = button_on_init,
    .on_deinit         = button_on_deinit,
    .on_start          = button_on_start,
    .on_stop           = button_on_stop,
    .on_pause          = button_on_pause,
    .on_resume         = button_on_resume,
    .on_lowpower_enter = button_on_lowpower_enter,
    .on_lowpower_exit  = button_on_lowpower_exit,
    .event_to_name     = button_event_to_name,
};

esp_err_t esp_button_service_create(const esp_button_service_cfg_t *cfg,
                                    esp_button_service_t **out_svc)
{
    if (cfg == NULL || out_svc == NULL) {
        ESP_LOGE(TAG, "Create failed: cfg or out_svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_button_service_t *btn = adf_calloc(1, sizeof(*btn));
    if (btn == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for service object");
        return ESP_ERR_NO_MEM;
    }

    btn_priv_t *priv = adf_calloc(1, sizeof(*priv));
    if (priv == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for private state");
        adf_free(btn);
        return ESP_ERR_NO_MEM;
    }
    STAILQ_INIT(&priv->ctx_list);
    priv->event_mask = (cfg->event_mask != 0) ? cfg->event_mask : ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT;
    priv->service = &btn->base;
    atomic_init(&priv->forwarding, false);

    esp_service_config_t base_cfg = ESP_SERVICE_CONFIG_DEFAULT();
    base_cfg.name = cfg->name ? cfg->name : "esp_button_service";
    base_cfg.user_data = priv;

    /* esp_service_init calls on_init synchronously; on failure it calls on_deinit,
     * freeing priv and any partially initialized devices — only btn needs freeing here. */
    esp_err_t ret = esp_service_init(&btn->base, &base_cfg, &s_btn_ops);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create failed: esp_service_init, %s", esp_err_to_name(ret));
        adf_free(btn);
        return ret;
    }

    ESP_LOGI(TAG, "Create '%s': %u button device(s)", base_cfg.name, (unsigned)adf_vec_size(&priv->init_devs));
    *out_svc = btn;
    return ESP_OK;
}

esp_err_t esp_button_service_destroy(esp_button_service_t *svc)
{
    if (svc == NULL) {
        ESP_LOGE(TAG, "Destroy failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    esp_service_deinit(&svc->base);
    adf_free(svc);
    ESP_LOGI(TAG, "Destroy 'esp_button_service': done");
    return ESP_OK;
}
