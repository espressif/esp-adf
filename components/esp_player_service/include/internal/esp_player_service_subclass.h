/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"
#include "esp_player_service.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Get the GMF pool used by this player service
 *
 *         Subclasses register extra elements (for example video scale / decode)
 *         into this pool. Do not destroy the returned handle.
 *
 * @param[in]  service  Player service handle
 *
 * @return
 *       - Non-NULL  Pool handle
 *       - NULL      service is NULL or no pool is installed
 */
void *esp_player_service_get_pool(esp_player_service_t *service);

/**
 * @brief  Install the video render this service hands to its players
 *
 *         Video subclasses call this after building a render. The handle is kept
 *         as-is and never destroyed here; the subclass stays the owner.
 *
 * @note  Deliberately separate from `esp_player_service_apply_setup()`: the render
 *        is device-scoped while players are session-scoped, and an audio-only
 *        apply_setup must not be able to drop the display. Only players created
 *        after this call see the new render.
 *
 * @param[in]  service  Player service handle
 * @param[in]  render   Render handle as void *; NULL disables the video path
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    service is NULL
 *       - ESP_ERR_INVALID_STATE  The service is not INITIALIZED
 */
esp_err_t esp_player_service_set_video_render(esp_player_service_t *service, void *render);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
