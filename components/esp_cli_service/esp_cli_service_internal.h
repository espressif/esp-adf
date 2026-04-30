/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_console.h"

#include "adf_vec.h"

#include "esp_cli_service.h"

/**
 * @brief  Tracked service entry for @c svc list and lifecycle commands
 */
typedef struct {
    char          *name;      /*!< Logical service name (heap copy) */
    char          *category;  /*!< Display category (heap copy; may be empty string) */
    esp_service_t *service;   /*!< Live service pointer (not owned by CLI) */
} tracked_service_entry_t;

/**
 * @brief  Stored copy of a static esp_console command and registration state
 */
typedef struct {
    esp_console_cmd_t  cmd;         /*!< Command descriptor (pointers target heap copies below) */
    char              *command;     /*!< Heap copy of command name */
    char              *help;        /*!< Heap copy of help text (may be NULL) */
    char              *hint;        /*!< Heap copy of hint text (may be NULL) */
    bool               registered;  /*!< True if registered with esp_console */
} static_cmd_entry_t;

/**
 * @brief  CLI service runtime state (internal layout; not part of public API)
 */
struct esp_cli_service {
    esp_service_t          base;           /*!< Embedded esp_service base object */
    esp_service_manager_t *manager;        /*!< Bound manager for @c tool commands */
    esp_console_repl_t    *repl;           /*!< UART REPL instance when started */
    bool                   repl_starting;  /*!< True while REPL creation is in progress */
    SemaphoreHandle_t      lock;           /*!< Protects manager, repl, vectors */

    char     *prompt;              /*!< Heap copy of configured prompt */
    uint16_t  max_cmdline_length;  /*!< Passed to esp_console REPL config */
    uint32_t  task_stack;          /*!< REPL task stack size */
    uint32_t  task_prio;           /*!< REPL task priority */

    adf_vec_t  tracked;      /*!< tracked_service_entry_t vector */
    adf_vec_t  static_cmds;  /*!< static_cmd_entry_t vector */
};

bool cli_token_is_empty(const char *s);
esp_err_t cli_lock_take(esp_cli_service_t *svc);
void cli_lock_give(esp_cli_service_t *svc);
char *cli_dup_or_null(const char *s);
char *cli_dup_or_empty(const char *s);
void cli_free_tracked_entry(tracked_service_entry_t *e);
void cli_free_static_cmd_entry(static_cmd_entry_t *e);
esp_err_t cli_manager_get(esp_cli_service_t *svc, esp_service_manager_t **out_mgr);
esp_err_t cli_vec_err_to_esp_err(int vec_err);

int cli_tracked_index_by_name_locked(const esp_cli_service_t *svc, const char *name);

esp_err_t cli_register_core_commands(esp_cli_service_t *svc);
esp_err_t cli_register_default_static_commands(esp_cli_service_t *svc);
esp_err_t cli_register_static_commands(esp_cli_service_t *svc);

esp_err_t cli_parse_i64(const char *s, long long *out);
esp_err_t cli_parse_double(const char *s, double *out);
esp_err_t cli_parse_bool(const char *s, bool *out);
