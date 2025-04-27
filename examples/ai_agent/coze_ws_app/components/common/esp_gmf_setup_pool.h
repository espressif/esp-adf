/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Register codec device io to gmf element pool
 *
 * @param[in]  pool        Handle of gmf pool
 * @param[in]  play_dev    Handle of play codec device
 * @param[in]  record_dev  Handle of record codec device
 */
void pool_register_codec_dev_io(esp_gmf_pool_handle_t pool, void *play_dev, void *record_dev);

/**
 * @brief  Register IO to gmf element pool
 *
 * @param[in]  pool  Handle of gmf pool
 */
void pool_register_io(esp_gmf_pool_handle_t pool);

/**
 * @brief  Register audio codec element to gmf element pool
 *
 * @param[in]  pool  Handle of gmf pool
 */
void pool_register_audio_codecs(esp_gmf_pool_handle_t pool);

/**
 * @brief  Unregister audio codec
 */
void pool_unregister_audio_codecs(void);

/**
 * @brief  Register audio effect element to gmf element pool
 *
 * @param[in]  pool  Handle of gmf pool
 */
void pool_register_audio_effects(esp_gmf_pool_handle_t pool);

/**
 * @brief  Register image element to gmf element pool
 *
 * @param[in]  pool  Handle of gmf pool
 */
void pool_register_image(esp_gmf_pool_handle_t pool);

/**
 * @brief  Register all the gmf element to gmf element pool
 *
 * @param[in]  pool        Handle of gmf pool
 * @param[in]  play_dev    Handle of play codec device
 * @param[in]  record_dev  Handle of record codec device
 */
void pool_register_all(esp_gmf_pool_handle_t pool, void *play_dev, void *codec_dev);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
