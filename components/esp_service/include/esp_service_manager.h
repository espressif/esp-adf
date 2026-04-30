/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_service.h"
#include "esp_service_mcp_server.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Service Manager - Dynamic service registry and lifecycle management
 *
 *         Features:
 *         - Dynamic service registration/unregistration
 *         - Automatic capability discovery from service JSON schemas
 *         - Service lifecycle management (init/start/stop/deinit)
 *         - Query services by name or category
 *         - Thread-safe operations
 */

/* Forward declaration */
typedef struct esp_service_manager esp_service_manager_t;

/**
 * @brief  Registration flags (per-service batch control)
 */
#define ESP_SERVICE_REG_FLAG_SKIP_BATCH_START  (1U << 0)  /*!< Skip this service in start_all */
#define ESP_SERVICE_REG_FLAG_SKIP_BATCH_STOP   (1U << 1)  /*!< Skip this service in stop_all */

/**
 * @brief  Tool invocation callback — translate a JSON-RPC tool call into a C API call
 *
 * @param[in]   service      Service instance (cast to concrete type inside)
 * @param[in]   tool         Tool metadata (name, description, input_schema)
 * @param[in]   args         JSON string with tool arguments (may be NULL or "{}")
 * @param[out]  result       Buffer to write JSON result string into
 * @param[in]   result_size  Size of result buffer
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_NOT_SUPPORTED  Unknown tool name
 *       - ESP_ERR_INVALID_ARG    Missing or invalid argument
 */
typedef esp_err_t (*esp_service_tool_invoke_fn_t)(esp_service_t *service,
                                                  const esp_service_tool_t *tool,
                                                  const char *args,
                                                  char *result,
                                                  size_t result_size);

/**
 * @brief  Service registration info
 *
 *         tool_desc and tool_invoke are both optional:
 *         - Both NULL  → service is registered for lifecycle management only;
 *                        no tool discovery or dispatch is set up.
 *         - Both set   → manager parses tool_desc and routes invoke_tool() calls
 *                        through tool_invoke; enables MCP tool support.
 */
typedef struct {
    esp_service_t                *service;      /*!< Service instance */
    const char                   *category;     /*!< Service category (e.g., "audio", "display") */
    uint32_t                      flags;        /*!< Combination of ESP_SERVICE_REG_FLAG_xxx (0 = default) */
    const char                   *tool_desc;    /*!< MCP tool description JSON array; NULL = no tool support */
    esp_service_tool_invoke_fn_t  tool_invoke;  /*!< Tool invocation handler; NULL = no tool support */
} esp_service_registration_t;

/**
 * @brief  Service manager configuration
 */
typedef struct {
    uint16_t  max_services;           /*!< Maximum number of services (default: 16) */
    uint16_t  max_tools_per_service;  /*!< Max tools per service (default: 32) */
    bool      auto_start_services;    /*!< Auto-start services on registration */
} esp_service_manager_config_t;

/**
 * @brief  Default configuration
 */
#define ESP_SERVICE_MANAGER_CONFIG_DEFAULT()  {  \
    .max_services          = 16,                 \
    .max_tools_per_service = 32,                 \
    .auto_start_services   = false,              \
}

/**
 * @brief  Create service manager
 *
 * @param[in]   config   Configuration (NULL for defaults)
 * @param[out]  out_mgr  Output: manager instance
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       Allocation failed
 *       - ESP_ERR_INVALID_ARG  out_mgr is NULL
 */
esp_err_t esp_service_manager_create(const esp_service_manager_config_t *config, esp_service_manager_t **out_mgr);

/**
 * @brief  Destroy service manager
 *
 * @param[in]  mgr  Manager instance
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  mgr is NULL.
 */
esp_err_t esp_service_manager_destroy(esp_service_manager_t *mgr);

/**
 * @brief  Register a service
 *
 *         Automatically parses tool description from service->tool_desc and
 *         registers all capabilities as invocable tools.
 *
 * @param[in]  mgr  Manager instance
 * @param[in]  reg  Registration info
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  Parameters invalid
 *       - ESP_ERR_NO_MEM       Registry full
 *       - ESP_FAIL             JSON parsing failed
 */
esp_err_t esp_service_manager_register(esp_service_manager_t *mgr, const esp_service_registration_t *reg);

/**
 * @brief  Unregister a service
 *
 * @param[in]  mgr      Manager instance
 * @param[in]  service  Service to unregister
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  mgr or service is NULL.
 *       - ESP_ERR_NOT_FOUND    Service not registered.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 */
esp_err_t esp_service_manager_unregister(esp_service_manager_t *mgr, esp_service_t *service);

/**
 * @brief  Find service by name
 *
 * @param[in]   mgr          Manager instance
 * @param[in]   name         Service name
 * @param[out]  out_service  Output: service instance
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  mgr, name, or out_service is NULL.
 *       - ESP_ERR_NOT_FOUND    Service not found.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 */
esp_err_t esp_service_manager_find_by_name(esp_service_manager_t *mgr, const char *name, esp_service_t **out_service);

/**
 * @brief  Find service by category
 *
 * @param[in]   mgr          Manager instance
 * @param[in]   category     Category string
 * @param[in]   index        Index in category (0-based)
 * @param[out]  out_service  Output: service instance
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  mgr, category, or out_service is NULL.
 *       - ESP_ERR_NOT_FOUND    Service not found.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 */
esp_err_t esp_service_manager_find_by_category(esp_service_manager_t *mgr,
                                               const char *category,
                                               uint16_t index,
                                               esp_service_t **out_service);

/**
 * @brief  Get number of registered services
 *
 * @param[in]   mgr        Manager instance
 * @param[out]  out_count  Output: service count
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  mgr or out_count is NULL.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 */
esp_err_t esp_service_manager_get_count(esp_service_manager_t *mgr, uint16_t *out_count);

/**
 * @brief  Start all registered services
 *
 * @param[in]  mgr  Manager instance
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  mgr is NULL.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 */
esp_err_t esp_service_manager_start_all(esp_service_manager_t *mgr);

/**
 * @brief  Stop all registered services
 *
 * @param[in]  mgr  Manager instance
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  mgr is NULL.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 */
esp_err_t esp_service_manager_stop_all(esp_service_manager_t *mgr);

/**
 * @brief  Get all available tools
 *
 * @param[in]   mgr        Manager instance
 * @param[out]  out_tools  Output: array of tool pointers
 * @param[in]   max_tools  Size of out_tools array
 * @param[out]  out_count  Output: actual tool count
 *
 * @return
 *       - ESP_OK               On success.
 *       - ESP_ERR_INVALID_ARG  mgr, out_tools, or out_count is NULL.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 */
esp_err_t esp_service_manager_get_tools(esp_service_manager_t *mgr,
                                        const esp_service_tool_t **out_tools,
                                        uint16_t max_tools,
                                        uint16_t *out_count);

/**
 * @brief  Get a deep-copied snapshot of all registered tools
 *
 *         Unlike esp_service_manager_get_tools(), which returns raw pointers
 *         into manager-owned storage that are invalidated on unregister, this
 *         function allocates a caller-owned array whose name/description/
 *         input_schema strings are duplicated under the manager mutex, so the
 *         snapshot stays valid across concurrent service unregistration.
 *         Free the result with esp_service_manager_free_cloned_tools().
 *
 * @param[in]   mgr        Manager instance
 * @param[out]  out_tools  Receives a newly allocated array (NULL when count==0)
 * @param[out]  out_count  Receives the number of entries in *out_tools
 *
 * @return
 *       - ESP_OK               On success (including the empty-registry case).
 *       - ESP_ERR_INVALID_ARG  mgr, out_tools, or out_count is NULL.
 *       - ESP_ERR_TIMEOUT      Mutex acquisition timed out.
 *       - ESP_ERR_NO_MEM       Allocation failed.
 */
esp_err_t esp_service_manager_clone_tools(esp_service_manager_t *mgr,
                                          esp_service_tool_t **out_tools,
                                          uint16_t *out_count);

/**
 * @brief  Free an array returned by esp_service_manager_clone_tools()
 *
 * @param[in]  tools  Array to free (may be NULL)
 * @param[in]  count  Number of entries in the array
 */
void esp_service_manager_free_cloned_tools(esp_service_tool_t *tools, uint16_t count);

/**
 * @brief  Invoke a tool
 *
 *         The manager mutex is released before the handler runs, so
 *         long-running handlers do not block other manager APIs.
 *
 * @note  The caller must not unregister the owning service while a tool
 *         on it is being invoked; the handler receives a shallow snapshot
 *         whose string fields are owned by the service entry.
 *
 * @param[in]   mgr          Manager instance
 * @param[in]   tool_name    Tool name
 * @param[in]   args_json    Arguments (JSON string)
 * @param[out]  result       Buffer for result (JSON string)
 * @param[in]   result_size  Size of result buffer
 *
 * @return
 *       - ESP_OK                 On success.
 *       - ESP_ERR_INVALID_ARG    mgr, tool_name, or result is NULL.
 *       - ESP_ERR_NOT_FOUND      Tool not found.
 *       - ESP_ERR_NOT_SUPPORTED  Tool has no invocation handler.
 *       - ESP_ERR_TIMEOUT        Mutex acquisition timed out.
 *       - Other                  Error from tool invocation handler.
 */
esp_err_t esp_service_manager_invoke_tool(esp_service_manager_t *mgr,
                                          const char *tool_name,
                                          const char *args_json,
                                          char *result,
                                          size_t result_size);

/**
 * @brief  Wrap a service manager instance as an esp_service_mcp_tool_provider_t
 *
 *         Populates out_provider with callbacks that delegate to the given
 *         manager.  No allocation is performed; out_provider may live on the
 *         stack or inside a config struct.
 *
 * @param[in]   mgr           Service manager instance
 * @param[out]  out_provider  Provider struct to populate
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  mgr or out_provider is NULL
 */
esp_err_t esp_service_manager_as_tool_provider(esp_service_manager_t *mgr, esp_service_mcp_tool_provider_t *out_provider);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
