/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_console.h"
#include "esp_err.h"
#include "esp_service.h"
#include "esp_service_manager.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct esp_cli_service esp_cli_service_t;

/**
 * @brief  CLI service creation configuration
 *
 *         Set @c struct_size to @c sizeof(esp_cli_service_config_t) on every call. The @c reserved
 *         fields must be zero. Use @ref ESP_CLI_SERVICE_CONFIG_DEFAULT() unless you intentionally
 *         override fields.
 *
 *         String ownership: @c prompt is copied internally; the pointer in @a cfg only needs to
 *         remain valid for the duration of @ref esp_cli_service_create. @c base_cfg fields follow
 *         @c esp_service_init rules.
 */
typedef struct {
    uint32_t               struct_size;         /*!< Must equal @c sizeof(esp_cli_service_config_t) */
    uint32_t               reserved[2];         /*!< Reserved; must be zero */
    esp_service_config_t   base_cfg;            /*!< Core service registration configuration */
    const char            *prompt;              /*!< REPL prompt string (copied on create) */
    uint16_t               max_cmdline_length;  /*!< Maximum command line length for esp_console */
    uint32_t               task_stack;          /*!< REPL task stack size in bytes */
    uint32_t               task_prio;           /*!< REPL task priority */
    esp_service_manager_t *manager;             /*!< Optional manager for @c tool commands */
} esp_cli_service_config_t;

#define ESP_CLI_SERVICE_CONFIG_DEFAULT()  {                  \
    .struct_size        = sizeof(esp_cli_service_config_t),  \
    .reserved           = {0, 0},                            \
    .base_cfg           = ESP_SERVICE_CONFIG_DEFAULT(),      \
    .prompt             = "cli>",                            \
    .max_cmdline_length = 256,                               \
    .task_stack         = 4096,                              \
    .task_prio          = 2,                                 \
    .manager            = NULL,                              \
}

/**
 * @brief  Create a CLI service handle (UART REPL backed by esp_console)
 *
 * @note  Not ISR-safe. Not thread-safe for concurrent creation of the same logical instance;
 *        typical use is from one init task. May allocate heap memory.
 *
 * @note  Internal mutex acquisition may block briefly when contended.
 *
 * @param[in]   cfg  Configuration; @c struct_size must equal @c sizeof(esp_cli_service_config_t)
 * @param[out]  out  Receives new handle on success; set to NULL on error
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  cfg or out is NULL, or struct_size is wrong
 *       - ESP_ERR_NO_MEM       Allocation failed
 */
esp_err_t esp_cli_service_create(const esp_cli_service_config_t *cfg, esp_cli_service_t **out);

/**
 * @brief  Destroy a CLI service and free its resources
 *
 *         Stops the REPL if running, frees tracked/static command state, and invalidates @a svc.
 *
 * @note  After this returns, @a svc must not be used even if the return code is not ESP_OK
 *        (resources are still released).
 *
 * @note  Not ISR-safe. Not thread-safe against concurrent use of @a svc or its REPL task;
 *        call when the service is quiesced.
 *
 * @param[in]  svc  Handle from @ref esp_cli_service_create
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  svc is NULL.
 *       - Other                Error from underlying service teardown; all resources are
 *                              freed regardless.
 */
esp_err_t esp_cli_service_destroy(esp_cli_service_t *svc);

/**
 * @brief  Bind or replace the service manager used for dynamic @c svc / @c tool commands
 *
 * @note  Not ISR-safe. Thread-safe with other APIs on the same handle (mutex held briefly).
 *        May block briefly when acquiring an internal mutex.
 *
 * @param[in]  svc  Service handle
 * @param[in]  mgr  Manager pointer (may be NULL to clear)
 *
 * @return
 *       - ESP_OK               Manager bound.
 *       - ESP_ERR_INVALID_ARG  svc is NULL.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 */
esp_err_t esp_cli_service_bind_manager(esp_cli_service_t *svc, esp_service_manager_t *mgr);

/**
 * @brief  Track a service by name for @c svc list / lifecycle commands
 *
 *         If the name already exists, updates category and service pointer.
 *
 * @note  Not ISR-safe. Thread-safe with other APIs on the same handle.
 *
 * @param[in]  svc       Service handle
 * @param[in]  service   Service to track (name from @c esp_service_get_name)
 * @param[in]  category  Category string for display (may be NULL, stored as empty string)
 *
 * @return
 *       - ESP_OK               Tracked (created or updated).
 *       - ESP_ERR_INVALID_ARG  svc or service is NULL; service name is empty.
 *       - ESP_ERR_NO_MEM       Allocation failed.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 *       - Other                Error from esp_service_get_name.
 */
esp_err_t esp_cli_service_track_service(esp_cli_service_t *svc, esp_service_t *service, const char *category);

/**
 * @brief  Remove a tracked service entry by logical service name
 *
 * @note  Not ISR-safe. Thread-safe with other APIs on the same handle.
 *
 * @param[in]  svc   Service handle
 * @param[in]  name  Service name previously tracked
 *
 * @return
 *       - ESP_OK               Service untracked.
 *       - ESP_ERR_INVALID_ARG  svc is NULL or name is empty.
 *       - ESP_ERR_NOT_FOUND    name not in the tracked list.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 */
esp_err_t esp_cli_service_untrack_service(esp_cli_service_t *svc, const char *name);

/**
 * @brief  Register an extra static esp_console command before or after REPL start
 *
 *         Command name must be a single token (no spaces). Strings are copied; the @a cmd
 *         descriptor need only live for the call. If the REPL is already running, the command is
 *         registered with esp_console immediately.
 *
 * @note  Not ISR-safe. Thread-safe with other APIs on the same handle. Do not register from inside
 *        another command handler unless re-entrancy is safe for esp_console on your IDF version.
 *
 * @param[in]  svc  Service handle
 * @param[in]  cmd  Command descriptor (see @c esp_console_cmd_t)
 *
 * @return
 *       - ESP_OK                 Command registered.
 *       - ESP_ERR_INVALID_ARG    svc or cmd is NULL; invalid command descriptor.
 *       - ESP_ERR_INVALID_STATE  Command name already registered.
 *       - ESP_ERR_NO_MEM         Allocation or storage failed.
 *       - ESP_ERR_TIMEOUT        Mutex acquisition timed out.
 */
esp_err_t esp_cli_service_register_static_command(esp_cli_service_t *svc, const esp_console_cmd_t *cmd);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
