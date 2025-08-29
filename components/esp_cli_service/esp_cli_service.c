/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include "adf_mem.h"
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "esp_console.h"
#include "esp_log.h"

#include "esp_cli_service_internal.h"

#define CLI_LOCK_TIMEOUT_MS  1000

/* After zero-init defaults are applied in esp_cli_service_create. */
#define ESP_CLI_SERVICE_MAX_CMDLINE_LENGTH_MIN  32U
#define ESP_CLI_SERVICE_MAX_CMDLINE_LENGTH_MAX  8192U
#define ESP_CLI_SERVICE_TASK_STACK_MIN          2048U
#define ESP_CLI_SERVICE_TASK_STACK_MAX          (512U * 1024U)
#define ESP_CLI_SERVICE_TASK_PRIO_MIN           1U

static const char *TAG = "esp_cli_service";

static esp_err_t cli_on_start(esp_service_t *base);
static esp_err_t cli_on_stop(esp_service_t *base);
static esp_err_t cli_on_deinit(esp_service_t *base);

static const esp_service_ops_t s_cli_ops = {
    .on_start  = cli_on_start,
    .on_stop   = cli_on_stop,
    .on_deinit = cli_on_deinit,
};

bool cli_token_is_empty(const char *s)
{
    return (s == NULL) || (s[0] == '\0');
}

esp_err_t cli_lock_take(esp_cli_service_t *svc)
{
    if (svc == NULL || svc->lock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xSemaphoreTake(svc->lock, pdMS_TO_TICKS(CLI_LOCK_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void cli_lock_give(esp_cli_service_t *svc)
{
    if (svc != NULL && svc->lock != NULL) {
        xSemaphoreGive(svc->lock);
    }
}

char *cli_dup_or_null(const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    return adf_strdup(s);
}

char *cli_dup_or_empty(const char *s)
{
    if (s == NULL) {
        return adf_strdup("");
    }
    return adf_strdup(s);
}

void cli_free_tracked_entry(tracked_service_entry_t *e)
{
    if (e == NULL) {
        return;
    }
    adf_free(e->name);
    adf_free(e->category);
    e->name = NULL;
    e->category = NULL;
    e->service = NULL;
}

void cli_free_static_cmd_entry(static_cmd_entry_t *e)
{
    if (e == NULL) {
        return;
    }
    adf_free(e->command);
    adf_free(e->help);
    adf_free(e->hint);
    e->command = NULL;
    e->help = NULL;
    e->hint = NULL;
    memset(&e->cmd, 0, sizeof(e->cmd));
}

esp_err_t cli_manager_get(esp_cli_service_t *svc, esp_service_manager_t **out_mgr)
{
    if (svc == NULL || out_mgr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_mgr = NULL;

    esp_err_t ret = cli_lock_take(svc);
    if (ret != ESP_OK) {
        return ret;
    }

    *out_mgr = svc->manager;
    cli_lock_give(svc);
    return ESP_OK;
}

esp_err_t cli_vec_err_to_esp_err(int vec_err)
{
    switch (vec_err) {
        case ADF_VEC_OK:
            return ESP_OK;
        case ADF_VEC_ERR_NO_MEM:
            return ESP_ERR_NO_MEM;
        case ADF_VEC_ERR_RANGE:
            return ESP_ERR_INVALID_SIZE;
        case ADF_VEC_ERR_ARG:
            return ESP_ERR_INVALID_ARG;
        default:
            return ESP_FAIL;
    }
}

static esp_err_t cli_set_repl_state(esp_cli_service_t *svc, esp_console_repl_t *repl, bool repl_starting)
{
    if (svc == NULL || svc->lock == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(svc->lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "REPL state update failed: mutex take");
        return ESP_ERR_INVALID_STATE;
    }

    svc->repl = repl;
    svc->repl_starting = repl_starting;
    xSemaphoreGive(svc->lock);
    return ESP_OK;
}

static void cli_mark_static_commands_registered(esp_cli_service_t *svc, bool registered)
{
    if (svc == NULL || svc->lock == NULL) {
        return;
    }

    if (xSemaphoreTake(svc->lock, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Static command flags update failed: mutex take");
        return;
    }

    size_t count = adf_vec_size(&svc->static_cmds);
    for (size_t i = 0; i < count; i++) {
        static_cmd_entry_t *entry = ADF_VEC_AT(static_cmd_entry_t, &svc->static_cmds, i);
        entry->registered = registered;
    }
    xSemaphoreGive(svc->lock);
}

static esp_err_t cli_on_start(esp_service_t *base)
{
    esp_cli_service_t *svc = (esp_cli_service_t *)base;
    esp_err_t ret = cli_lock_take(svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start failed: mutex timeout");
        return ret;
    }
    if (svc->repl != NULL || svc->repl_starting) {
        cli_lock_give(svc);
        ESP_LOGE(TAG, "Start failed: REPL already started");
        return ESP_ERR_INVALID_STATE;
    }
    svc->repl_starting = true;
    cli_lock_give(svc);

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = svc->prompt ? svc->prompt : "cli>";
    repl_cfg.max_cmdline_length = svc->max_cmdline_length;
    repl_cfg.task_stack_size = svc->task_stack;
    repl_cfg.task_priority = svc->task_prio;

#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
    esp_console_dev_uart_config_t uart_cfg = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ret = esp_console_new_repl_uart(&uart_cfg, &repl_cfg, &repl);
    if (ret != ESP_OK) {
        (void)cli_set_repl_state(svc, NULL, false);
        ESP_LOGE(TAG, "Start failed: %s", esp_err_to_name(ret));
        return ret;
    }
#else
    (void)cli_set_repl_state(svc, NULL, false);
    ESP_LOGE(TAG, "Start failed: UART REPL backend disabled in sdkconfig");
    return ESP_ERR_NOT_SUPPORTED;
#endif  /* CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM */

    ret = cli_register_core_commands(svc);
    if (ret != ESP_OK) {
        (void)esp_console_stop_repl(repl);
        (void)cli_set_repl_state(svc, NULL, false);
        cli_mark_static_commands_registered(svc, false);
        ESP_LOGE(TAG, "Start failed: register core commands, %s", esp_err_to_name(ret));
        return ret;
    }

    ret = cli_register_static_commands(svc);
    if (ret != ESP_OK) {
        (void)esp_console_stop_repl(repl);
        (void)cli_set_repl_state(svc, NULL, false);
        cli_mark_static_commands_registered(svc, false);
        ESP_LOGE(TAG, "Start failed: register static commands, %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_console_start_repl(repl);
    if (ret != ESP_OK) {
        (void)esp_console_stop_repl(repl);
        (void)cli_set_repl_state(svc, NULL, false);
        cli_mark_static_commands_registered(svc, false);
        ESP_LOGE(TAG, "Start failed: start REPL, %s", esp_err_to_name(ret));
        return ret;
    }

    /* Publish the running REPL pointer and clear "starting" flag first, then
     * do one more registration pass to pick up commands added during startup. */
    (void)cli_set_repl_state(svc, repl, false);
    ret = cli_register_static_commands(svc);
    if (ret != ESP_OK) {
        (void)esp_console_stop_repl(repl);
        (void)cli_set_repl_state(svc, NULL, false);
        cli_mark_static_commands_registered(svc, false);
        ESP_LOGE(TAG, "Start failed: register pending static commands, %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Start 'esp_cli_service': REPL running");
    return ESP_OK;
}

static esp_err_t cli_on_stop(esp_service_t *base)
{
    esp_cli_service_t *svc = (esp_cli_service_t *)base;
    if (svc->repl == NULL) {
        return ESP_OK;
    }

    esp_err_t ret = esp_console_stop_repl(svc->repl);
    (void)cli_set_repl_state(svc, NULL, false);
    cli_mark_static_commands_registered(svc, false);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Stop 'esp_cli_service': REPL stopped");
    } else {
        ESP_LOGW(TAG, "Stop 'esp_cli_service': esp_console_stop_repl returned %s; REPL state cleared", esp_err_to_name(ret));
    }
    return ESP_OK;
}

static esp_err_t cli_on_deinit(esp_service_t *base)
{
    esp_cli_service_t *svc = (esp_cli_service_t *)base;
    if (svc->repl != NULL) {
        (void)esp_console_stop_repl(svc->repl);
    }
    (void)cli_set_repl_state(svc, NULL, false);
    cli_mark_static_commands_registered(svc, false);
    return ESP_OK;
}

int cli_tracked_index_by_name_locked(const esp_cli_service_t *svc, const char *name)
{
    if (svc == NULL || name == NULL) {
        return -1;
    }
    size_t tracked_count = adf_vec_size(&svc->tracked);
    for (size_t i = 0; i < tracked_count; i++) {
        tracked_service_entry_t *entry = ADF_VEC_AT(tracked_service_entry_t, &svc->tracked, i);
        if (entry->name != NULL && strcmp(entry->name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

esp_err_t esp_cli_service_create(const esp_cli_service_config_t *cfg, esp_cli_service_t **out)
{
    if (cfg == NULL || out == NULL) {
        ESP_LOGE(TAG, "Create failed: cfg or out is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg->struct_size != sizeof(esp_cli_service_config_t)) {
        ESP_LOGE(TAG, "Create failed: struct_size mismatch (got %u, expected %u)",
                 (unsigned)cfg->struct_size, (unsigned)sizeof(esp_cli_service_config_t));
        return ESP_ERR_INVALID_ARG;
    }

    if (cfg->reserved[0] != 0 || cfg->reserved[1] != 0) {
        ESP_LOGE(TAG, "Create failed: reserved fields must be zero");
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t max_cmdline = cfg->max_cmdline_length ? cfg->max_cmdline_length : 256;
    uint32_t task_stack = cfg->task_stack ? cfg->task_stack : 4096;
    uint32_t task_prio = cfg->task_prio ? cfg->task_prio : 2;

    if (max_cmdline < ESP_CLI_SERVICE_MAX_CMDLINE_LENGTH_MIN ||
        max_cmdline > ESP_CLI_SERVICE_MAX_CMDLINE_LENGTH_MAX) {
        ESP_LOGE(TAG, "Create failed: max_cmdline_length out of range (%u)", (unsigned)max_cmdline);
        return ESP_ERR_INVALID_ARG;
    }
    if (task_stack < ESP_CLI_SERVICE_TASK_STACK_MIN || task_stack > ESP_CLI_SERVICE_TASK_STACK_MAX) {
        ESP_LOGE(TAG, "Create failed: task_stack out of range (%lu)", (unsigned long)task_stack);
        return ESP_ERR_INVALID_ARG;
    }
    if (task_prio < ESP_CLI_SERVICE_TASK_PRIO_MIN || task_prio >= (uint32_t)configMAX_PRIORITIES) {
        ESP_LOGE(TAG, "Create failed: task_prio out of range (%lu)", (unsigned long)task_prio);
        return ESP_ERR_INVALID_ARG;
    }

    *out = NULL;

    esp_cli_service_t *svc = adf_calloc(1, sizeof(*svc));
    if (svc == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for service object");
        return ESP_ERR_NO_MEM;
    }

    svc->lock = xSemaphoreCreateMutex();
    if (svc->lock == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for mutex");
        adf_free(svc);
        return ESP_ERR_NO_MEM;
    }

    svc->prompt = cli_dup_or_null(cli_token_is_empty(cfg->prompt) ? "cli>" : cfg->prompt);
    if (svc->prompt == NULL) {
        ESP_LOGE(TAG, "Create failed: no memory for prompt copy");
        vSemaphoreDelete(svc->lock);
        adf_free(svc);
        return ESP_ERR_NO_MEM;
    }

    svc->max_cmdline_length = max_cmdline;
    svc->task_stack = task_stack;
    svc->task_prio = task_prio;
    svc->manager = cfg->manager;

    int vec_ret = adf_vec_init(&svc->tracked, sizeof(tracked_service_entry_t), 0);
    if (vec_ret != ADF_VEC_OK) {
        esp_err_t err = cli_vec_err_to_esp_err(vec_ret);
        ESP_LOGE(TAG, "Create failed: init tracked list, %s", esp_err_to_name(err));
        adf_free(svc->prompt);
        vSemaphoreDelete(svc->lock);
        adf_free(svc);
        return err;
    }

    vec_ret = adf_vec_init(&svc->static_cmds, sizeof(static_cmd_entry_t), 0);
    if (vec_ret != ADF_VEC_OK) {
        esp_err_t err = cli_vec_err_to_esp_err(vec_ret);
        ESP_LOGE(TAG, "Create failed: init static command list, %s", esp_err_to_name(err));
        adf_vec_destroy(&svc->tracked);
        adf_free(svc->prompt);
        vSemaphoreDelete(svc->lock);
        adf_free(svc);
        return err;
    }

    esp_err_t ret = cli_register_default_static_commands(svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create failed: register default static commands, %s", esp_err_to_name(ret));
        /* Free any partially registered entries before destroying the vector. */
        size_t partial = adf_vec_size(&svc->static_cmds);
        for (size_t i = 0; i < partial; i++) {
            static_cmd_entry_t *entry = ADF_VEC_AT(static_cmd_entry_t, &svc->static_cmds, i);
            cli_free_static_cmd_entry(entry);
        }
        adf_vec_destroy(&svc->static_cmds);
        adf_vec_destroy(&svc->tracked);
        adf_free(svc->prompt);
        vSemaphoreDelete(svc->lock);
        adf_free(svc);
        return ret;
    }

    esp_service_config_t base_cfg = cfg->base_cfg;
    if (cli_token_is_empty(base_cfg.name)) {
        base_cfg.name = "esp_cli_service";
    }

    ret = esp_service_init(&svc->base, &base_cfg, &s_cli_ops);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Create failed: esp_service_init, %s", esp_err_to_name(ret));
        /* Free default static cmd entries registered earlier. */
        size_t partial = adf_vec_size(&svc->static_cmds);
        for (size_t i = 0; i < partial; i++) {
            static_cmd_entry_t *entry = ADF_VEC_AT(static_cmd_entry_t, &svc->static_cmds, i);
            cli_free_static_cmd_entry(entry);
        }
        adf_vec_destroy(&svc->static_cmds);
        adf_vec_destroy(&svc->tracked);
        adf_free(svc->prompt);
        vSemaphoreDelete(svc->lock);
        adf_free(svc);
        return ret;
    }

    *out = svc;
    return ESP_OK;
}

esp_err_t esp_cli_service_destroy(esp_cli_service_t *svc)
{
    if (svc == NULL) {
        ESP_LOGE(TAG, "Destroy failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_service_deinit(&svc->base);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Destroy 'esp_cli_service': esp_service_deinit returned %s", esp_err_to_name(ret));
    }

    size_t tracked_count = adf_vec_size(&svc->tracked);
    for (size_t i = 0; i < tracked_count; i++) {
        tracked_service_entry_t *entry = ADF_VEC_AT(tracked_service_entry_t, &svc->tracked, i);
        cli_free_tracked_entry(entry);
    }

    size_t static_count = adf_vec_size(&svc->static_cmds);
    for (size_t i = 0; i < static_count; i++) {
        static_cmd_entry_t *entry = ADF_VEC_AT(static_cmd_entry_t, &svc->static_cmds, i);
        cli_free_static_cmd_entry(entry);
    }

    adf_vec_destroy(&svc->tracked);
    adf_vec_destroy(&svc->static_cmds);

    if (svc->lock != NULL) {
        vSemaphoreDelete(svc->lock);
        svc->lock = NULL;
    }

    adf_free(svc->prompt);
    svc->prompt = NULL;

    adf_free(svc);
    return ret;
}

esp_err_t esp_cli_service_bind_manager(esp_cli_service_t *svc, esp_service_manager_t *mgr)
{
    if (svc == NULL) {
        ESP_LOGE(TAG, "Bind manager failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = cli_lock_take(svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bind manager failed: mutex timeout");
        return ret;
    }

    svc->manager = mgr;
    cli_lock_give(svc);
    return ESP_OK;
}

esp_err_t esp_cli_service_track_service(esp_cli_service_t *svc, esp_service_t *service, const char *category)
{
    if (svc == NULL || service == NULL) {
        ESP_LOGE(TAG, "Track service failed: svc or service is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    const char *name = NULL;
    esp_err_t ret = esp_service_get_name(service, &name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Track service failed: esp_service_get_name, %s", esp_err_to_name(ret));
        return ret;
    }
    if (cli_token_is_empty(name)) {
        ESP_LOGE(TAG, "Track service failed: service name is empty");
        return ESP_ERR_INVALID_ARG;
    }

    char *name_copy = cli_dup_or_null(name);
    char *category_copy = cli_dup_or_empty(category);
    if (name_copy == NULL || category_copy == NULL) {
        adf_free(name_copy);
        adf_free(category_copy);
        ESP_LOGE(TAG, "Track service failed: no memory for name or category copy");
        return ESP_ERR_NO_MEM;
    }

    ret = cli_lock_take(svc);
    if (ret != ESP_OK) {
        adf_free(name_copy);
        adf_free(category_copy);
        ESP_LOGE(TAG, "Track service failed: mutex timeout");
        return ret;
    }

    int idx = cli_tracked_index_by_name_locked(svc, name_copy);
    if (idx >= 0) {
        tracked_service_entry_t *entry = ADF_VEC_AT(tracked_service_entry_t, &svc->tracked, (size_t)idx);
        adf_free(entry->category);
        entry->category = category_copy;
        entry->service = service;
        adf_free(name_copy);
        cli_lock_give(svc);
        return ESP_OK;
    }

    tracked_service_entry_t entry = {
        .name = name_copy,
        .category = category_copy,
        .service = service,
    };
    int vec_ret = adf_vec_push(&svc->tracked, &entry);
    cli_lock_give(svc);

    if (vec_ret != ADF_VEC_OK) {
        adf_free(name_copy);
        adf_free(category_copy);
        ret = cli_vec_err_to_esp_err(vec_ret);
        ESP_LOGE(TAG, "Track service failed: push tracked entry, %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

esp_err_t esp_cli_service_untrack_service(esp_cli_service_t *svc, const char *name)
{
    if (svc == NULL || cli_token_is_empty(name)) {
        ESP_LOGE(TAG, "Untrack service failed: svc is NULL or name is empty");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = cli_lock_take(svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Untrack service failed: mutex timeout");
        return ret;
    }

    int idx = cli_tracked_index_by_name_locked(svc, name);
    if (idx < 0) {
        cli_lock_give(svc);
        ESP_LOGE(TAG, "Untrack service failed: '%s' not in tracked list", name);
        return ESP_ERR_NOT_FOUND;
    }

    tracked_service_entry_t *entry = ADF_VEC_AT(tracked_service_entry_t, &svc->tracked, (size_t)idx);
    cli_free_tracked_entry(entry);

    int vec_ret = adf_vec_remove(&svc->tracked, (size_t)idx);
    cli_lock_give(svc);

    if (vec_ret != ADF_VEC_OK) {
        ret = cli_vec_err_to_esp_err(vec_ret);
        ESP_LOGE(TAG, "Untrack service failed: remove from list, %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
