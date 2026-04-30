/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "esp_cli_service_internal.h"

static const char *TAG = "esp_cli_service";

static int cmd_sys_heap(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("heap_free=%" PRIu32 "\n", (uint32_t)esp_get_free_heap_size());
    return 0;
}

static int cmd_sys_chip(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    esp_chip_info_t chip = {0};
    esp_chip_info(&chip);
    printf("chip_model=%d cores=%d revision=%d features=0x%lx\n",
           chip.model, chip.cores, chip.revision, (unsigned long)chip.features);
    return 0;
}

static int cmd_sys_uptime(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int64_t uptime_ms = esp_timer_get_time() / 1000;
    printf("uptime_ms=%" PRId64 "\n", uptime_ms);
    return 0;
}

static int cmd_sys_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("rebooting...\n");
    fflush(stdout);
    esp_restart();
    return 0;
}

static esp_err_t clone_static_cmd(const esp_console_cmd_t *cmd, static_cmd_entry_t *out)
{
    if (cmd == NULL || out == NULL || cli_token_is_empty(cmd->command)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strchr(cmd->command, ' ') != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((cmd->func == NULL && cmd->func_w_context == NULL) ||
        (cmd->func != NULL && cmd->func_w_context != NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->command = cli_dup_or_null(cmd->command);
    out->help = cli_dup_or_null(cmd->help);
    out->hint = cli_dup_or_null(cmd->hint);
    if (out->command == NULL ||
        (cmd->help != NULL && out->help == NULL) ||
        (cmd->hint != NULL && out->hint == NULL)) {
        cli_free_static_cmd_entry(out);
        return ESP_ERR_NO_MEM;
    }

    out->cmd = *cmd;
    out->cmd.command = out->command;
    out->cmd.help = out->help;
    out->cmd.hint = out->hint;
    out->registered = false;
    return ESP_OK;
}

static int static_cmd_index_by_name_locked(const esp_cli_service_t *svc, const char *name)
{
    size_t cmd_count = adf_vec_size(&svc->static_cmds);
    for (size_t i = 0; i < cmd_count; i++) {
        static_cmd_entry_t *entry = ADF_VEC_AT(static_cmd_entry_t, &svc->static_cmds, i);
        if (entry->command != NULL && strcmp(entry->command, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static esp_err_t add_static_command_internal(esp_cli_service_t *svc,
                                             const esp_console_cmd_t *cmd,
                                             bool log_error)
{
    if (svc == NULL || cmd == NULL) {
        if (log_error) {
            ESP_LOGE(TAG, "Register static command failed: invalid argument");
        }
        return ESP_ERR_INVALID_ARG;
    }

    static_cmd_entry_t entry = {0};
    esp_err_t ret = clone_static_cmd(cmd, &entry);
    if (ret != ESP_OK) {
        if (log_error) {
            ESP_LOGE(TAG, "Register static command '%s' failed: %s",
                     cmd->command ? cmd->command : "<null>", esp_err_to_name(ret));
        }
        return ret;
    }

    ret = cli_lock_take(svc);
    if (ret != ESP_OK) {
        cli_free_static_cmd_entry(&entry);
        if (log_error) {
            ESP_LOGE(TAG, "Register static command '%s' failed: mutex timeout",
                     cmd->command ? cmd->command : "<null>");
        }
        return ret;
    }

    if (static_cmd_index_by_name_locked(svc, entry.command) >= 0) {
        cli_lock_give(svc);
        cli_free_static_cmd_entry(&entry);
        if (log_error) {
            ESP_LOGE(TAG, "Register static command '%s' failed: duplicate name",
                     cmd->command ? cmd->command : "<null>");
        }
        return ESP_ERR_INVALID_STATE;
    }

    int vec_ret = adf_vec_push(&svc->static_cmds, &entry);
    if (vec_ret != ADF_VEC_OK) {
        cli_lock_give(svc);
        cli_free_static_cmd_entry(&entry);
        ret = cli_vec_err_to_esp_err(vec_ret);
        if (log_error) {
            ESP_LOGE(TAG, "Register static command '%s' failed: %s",
                     cmd->command ? cmd->command : "<null>", esp_err_to_name(ret));
        }
        return ret;
    }

    if (svc->repl != NULL && !svc->repl_starting) {
        size_t new_idx = adf_vec_size(&svc->static_cmds) - 1;
        static_cmd_entry_t *stored = ADF_VEC_AT(static_cmd_entry_t, &svc->static_cmds, new_idx);
        ret = esp_console_cmd_register(&stored->cmd);
        if (ret != ESP_OK) {
            cli_free_static_cmd_entry(stored);
            (void)adf_vec_pop(&svc->static_cmds);
            cli_lock_give(svc);
            if (log_error) {
                ESP_LOGE(TAG, "Register static command '%s' failed: %s",
                         cmd->command ? cmd->command : "<null>", esp_err_to_name(ret));
            }
            return ret;
        }
        stored->registered = true;
    }

    cli_lock_give(svc);
    return ESP_OK;
}

esp_err_t cli_register_default_static_commands(esp_cli_service_t *svc)
{
    if (svc == NULL) {
        ESP_LOGE(TAG, "Register default static commands failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    const esp_console_cmd_t default_cmds[] = {
        {
            .command = "sys_heap",
            .help = "Print current free heap",
            .func = cmd_sys_heap,
        },
        {
            .command = "sys_chip",
            .help = "Print chip info",
            .func = cmd_sys_chip,
        },
        {
            .command = "sys_uptime",
            .help = "Print uptime in ms",
            .func = cmd_sys_uptime,
        },
        {
            .command = "sys_reboot",
            .help = "Restart chip",
            .func = cmd_sys_reboot,
        },
    };

    for (size_t i = 0; i < (sizeof(default_cmds) / sizeof(default_cmds[0])); i++) {
        esp_err_t ret = add_static_command_internal(svc, &default_cmds[i], false);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

esp_err_t cli_register_static_commands(esp_cli_service_t *svc)
{
    if (svc == NULL) {
        ESP_LOGE(TAG, "Register static commands failed: svc is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = cli_lock_take(svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register static commands failed: mutex timeout");
        return ret;
    }

    size_t count = adf_vec_size(&svc->static_cmds);
    for (size_t i = 0; i < count; i++) {
        static_cmd_entry_t *entry = ADF_VEC_AT(static_cmd_entry_t, &svc->static_cmds, i);
        if (entry->registered) {
            continue;
        }
        ret = esp_console_cmd_register(&entry->cmd);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Register static commands failed: %s", esp_err_to_name(ret));
            cli_lock_give(svc);
            return ret;
        }
        entry->registered = true;
    }

    cli_lock_give(svc);
    return ESP_OK;
}

esp_err_t esp_cli_service_register_static_command(esp_cli_service_t *svc, const esp_console_cmd_t *cmd)
{
    return add_static_command_internal(svc, cmd, true);
}
