/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * Shared service-registration / MCP-HTTP helpers used by the test_apps/pc
 * executables (coverage / dispatch / integration tests and the long-running
 * mcp_http_server).  The simulation runner does not need these — it talks to
 * the services through their direct C API.
 *
 * All functions are static inline so that each consumer can include this
 * header directly without requiring a new compiled object or CMakeLists
 * change.  Only the helpers that are actually called will generate code.
 */

#pragma once

#include "esp_err.h"
#include "esp_service_manager.h"
#include "esp_service_mcp_server.h"
#include "esp_service_mcp_trans.h"
#include "player_service.h"
#include "player_service_mcp.h"
#include "led_service.h"
#include "led_service_mcp.h"
#include "ota_service.h"
#include "ota_service_mcp.h"
#include "wifi_service.h"
#include "wifi_service_mcp.h"
#include "host_http_transport.h"

/* ── Service registration helpers ─────────────────────────────────────────
 *
 * Wrap the esp_service_registration_t boilerplate so callers only state
 * which service and which category they intend.
 */

static inline esp_err_t svc_register_player(esp_service_manager_t *mgr,
                                            player_service_t *player,
                                            const char *category)
{
    esp_service_registration_t reg = {
        .service = (esp_service_t *)player,
        .category = category,
        .tool_desc = player_service_mcp_schema(),
        .tool_invoke = player_service_tool_invoke,
    };
    return esp_service_manager_register(mgr, &reg);
}

static inline esp_err_t svc_register_led(esp_service_manager_t *mgr,
                                         led_service_t *led,
                                         const char *category)
{
    esp_service_registration_t reg = {
        .service = (esp_service_t *)led,
        .category = category,
        .tool_desc = led_service_mcp_schema(),
        .tool_invoke = led_service_tool_invoke,
    };
    return esp_service_manager_register(mgr, &reg);
}

static inline esp_err_t svc_register_ota(esp_service_manager_t *mgr,
                                         ota_service_t *ota,
                                         const char *category)
{
    const char *schema = NULL;
    esp_err_t err = ota_service_mcp_schema_get(&schema);
    if (err != ESP_OK) {
        return err;
    }
    esp_service_registration_t reg = {
        .service = (esp_service_t *)ota,
        .category = category,
        .tool_desc = schema,
        .tool_invoke = ota_service_tool_invoke,
    };
    return esp_service_manager_register(mgr, &reg);
}

static inline esp_err_t svc_register_wifi(esp_service_manager_t *mgr,
                                          wifi_service_t *wifi,
                                          const char *category)
{
    esp_service_registration_t reg = {
        .service = (esp_service_t *)wifi,
        .category = category,
        .tool_desc = wifi_service_mcp_schema(),
        .tool_invoke = wifi_service_tool_invoke,
    };
    return esp_service_manager_register(mgr, &reg);
}

/* ── MCP HTTP server helpers ──────────────────────────────────────────────
 *
 * svc_mcp_http_start: create transport → create server → start server.
 * On any step failing, already-created resources are released and out-params
 * are left NULL.
 *
 * svc_mcp_http_stop: stop → destroy server, then destroy transport.
 * Safe to call with NULL arguments.
 */

static inline esp_err_t svc_mcp_http_start(esp_service_manager_t *mgr,
                                           uint16_t port,
                                           esp_service_mcp_trans_t **out_transport,
                                           esp_service_mcp_server_t **out_server)
{
    host_http_transport_config_t http_cfg = HOST_HTTP_TRANSPORT_CONFIG_DEFAULT();
    http_cfg.port = port;
    http_cfg.uri_path = "/mcp";

    esp_err_t ret = host_http_transport_create(&http_cfg, out_transport);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_service_mcp_server_config_t mcp_cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &mcp_cfg.tool_provider);
    mcp_cfg.transport = *out_transport;
    ret = esp_service_mcp_server_create(&mcp_cfg, out_server);
    if (ret != ESP_OK) {
        esp_service_mcp_trans_destroy(*out_transport);
        *out_transport = NULL;
        return ret;
    }

    ret = esp_service_mcp_server_start(*out_server);
    if (ret != ESP_OK) {
        esp_service_mcp_server_destroy(*out_server);
        *out_server = NULL;
        esp_service_mcp_trans_destroy(*out_transport);
        *out_transport = NULL;
    }
    return ret;
}

static inline void svc_mcp_http_stop(esp_service_mcp_server_t *server,
                                     esp_service_mcp_trans_t *transport)
{
    if (server) {
        esp_service_mcp_server_stop(server);
        esp_service_mcp_server_destroy(server);
    }
    if (transport) {
        esp_service_mcp_trans_destroy(transport);
    }
}
