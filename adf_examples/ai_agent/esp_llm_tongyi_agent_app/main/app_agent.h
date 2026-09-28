/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque Tongyi agent application context
 */
typedef struct app_agent app_agent_t;

/**
 * @brief  Create the Tongyi agent and its application-owned media environment
 *
 *         Media owns board audio devices, codec registrations, capture, and
 *         player. Vision mode also owns LCD preview and JPEG snapshots. This
 *         function passes media's borrowed capture handle through
 *         service.audio_ai_cfg.capture and creates the Tongyi LLM service.
 *
 * @note  On failure, @p out_agent is NULL if cleanup completed; otherwise it
 *        retains a handle that the caller must pass to app_agent_destroy().
 *
 * @param[out]  out_agent  Receives the created application context; must point to NULL
 *
 * @return
 *       - ESP_OK                  On success
 *       - ESP_ERR_INVALID_ARG     If @p out_agent is NULL or does not point to NULL
 *       - ESP_ERR_INVALID_STATE   If required Kconfig account values are empty
 *       - ESP_ERR_NO_MEM          If context allocation fails
 *       - Others                  Error returned by board, media, event, or LLM setup
 */
esp_err_t app_agent_create(app_agent_t **out_agent);

/**
 * @brief  Start the complete graph through the media environment
 *
 *         Media creates both links and starts player, LLM, then capture. If any
 *         operation fails, media performs reverse rollback.
 *
 * @param[in]  agent  Application context returned by app_agent_create()
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If @p agent is NULL
 *       - ESP_ERR_INVALID_STATE  If the graph has already started
 *       - Others                 Error returned by a service start or rollback
 */
esp_err_t app_agent_start(app_agent_t *agent);

/**
 * @brief  Stop the media graph without destroying the agent
 *
 *         Stops the CLI, then tongyi_app_media_stop(): capture, LLM, and player
 *         stop, then links are removed. Safe to call more than once.
 *
 * @param[in]  agent  Application context returned by app_agent_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p agent is NULL
 *       - Others               First error returned by media stop
 */
esp_err_t app_agent_stop(app_agent_t *agent);

/**
 * @brief  Stop, unlink, and destroy the complete Tongyi agent graph
 *
 *         Calls app_agent_stop(), then destroys the LLM and media environment.
 *
 * @note  If LLM destruction fails and retains its handle, the application
 *        context is retained for retry.
 *
 * @param[in,out]  agent  Address of an application context returned by app_agent_create()
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If @p agent or @p *agent is NULL
 *       - Others               First error returned during cleanup
 */
esp_err_t app_agent_destroy(app_agent_t **agent);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
