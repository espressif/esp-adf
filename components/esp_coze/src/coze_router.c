/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_check.h"
#include "cJSON.h"

#include "coze_router.h"

static const char *TAG = "COZE_ROUTER";

#define COZE_ROUTER_MAX_TABLES  4

static bool should_log_rx_event(const char *event_type)
{
    return event_type != NULL && strcmp(event_type, "conversation.audio.delta") != 0;
}

typedef struct {
    const coze_event_entry_t *table;
    size_t                    n;
    void                     *ctx;
} router_table_t;

struct coze_router {
    router_table_t             tables[COZE_ROUTER_MAX_TABLES];
    size_t                     table_count;
    coze_event_fn_t            default_fn;
    void                      *default_ctx;
    coze_router_text_tap_fn_t  tap_fn;
    void                      *tap_ctx;
};

esp_err_t coze_router_create(coze_router_t **out)
{
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid out pointer");
    coze_router_t *r = (coze_router_t *)calloc(1, sizeof(coze_router_t));
    ESP_RETURN_ON_FALSE(r != NULL, ESP_ERR_NO_MEM, TAG, "No memory for router");
    *out = r;
    return ESP_OK;
}

void coze_router_destroy(coze_router_t *r)
{
    if (r == NULL) {
        return;
    }
    free(r);
}

esp_err_t coze_router_register(coze_router_t *r,
                               const coze_event_entry_t *table,
                               size_t n,
                               void *ctx)
{
    ESP_RETURN_ON_FALSE(r != NULL && table != NULL && n > 0, ESP_ERR_INVALID_ARG,
                        TAG, "Invalid register args");
    ESP_RETURN_ON_FALSE(r->table_count < COZE_ROUTER_MAX_TABLES, ESP_ERR_NO_MEM,
                        TAG, "Too many event tables (max %d)", COZE_ROUTER_MAX_TABLES);
    r->tables[r->table_count].table = table;
    r->tables[r->table_count].n = n;
    r->tables[r->table_count].ctx = ctx;
    r->table_count++;
    return ESP_OK;
}

void coze_router_set_default(coze_router_t *r, coze_event_fn_t fn, void *ctx)
{
    if (r == NULL) {
        return;
    }
    r->default_fn = fn;
    r->default_ctx = ctx;
}

void coze_router_set_text_tap(coze_router_t *r, coze_router_text_tap_fn_t fn, void *ctx)
{
    if (r == NULL) {
        return;
    }
    r->tap_fn = fn;
    r->tap_ctx = ctx;
}

esp_err_t coze_router_dispatch_text(coze_router_t *r, const char *raw)
{
    ESP_RETURN_ON_FALSE(r != NULL && raw != NULL, ESP_ERR_INVALID_ARG,
                        TAG, "Invalid dispatch args");

    if (r->tap_fn) {
        r->tap_fn(r->tap_ctx, raw);
    }

    cJSON *root = cJSON_Parse(raw);
    if (root == NULL) {
        ESP_LOGW(TAG, "Failed to parse incoming JSON");
        return ESP_FAIL;
    }

    cJSON *event_type = cJSON_GetObjectItem(root, "event_type");
    if (cJSON_IsString(event_type) && event_type->valuestring != NULL) {
        const char *type = event_type->valuestring;
        if (should_log_rx_event(type)) {
            ESP_LOGI(TAG, "WS RX event=%s: %s", type, raw);
        }
        for (size_t t = 0; t < r->table_count; t++) {
            const router_table_t *rt = &r->tables[t];
            for (size_t i = 0; i < rt->n; i++) {
                if (rt->table[i].event_type &&
                    strcmp(rt->table[i].event_type, type) == 0) {
                    if (rt->table[i].handler) {
                        rt->table[i].handler(rt->ctx, root, raw);
                    }
                    cJSON_Delete(root);
                    return ESP_OK;
                }
            }
        }
        if (r->default_fn) {
            r->default_fn(r->default_ctx, root, raw);
        }
    }
    cJSON_Delete(root);
    return ESP_OK;
}
