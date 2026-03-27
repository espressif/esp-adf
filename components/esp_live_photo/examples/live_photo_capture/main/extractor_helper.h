/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_extractor.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Get extractor favorite type by url
 *
 * @param[in]  url  File URL to be checked
 *
 * @return
 *       - ESP_EXTRACTOR_TYPE_NONE  Not supported extension
 *       - Others                   Matched favorite extractor type
 */
esp_extractor_type_t esp_extractor_get_favor_type(const char *url);

/**
 * @brief  Get extractor format string representation
 *
 * @param[in]  format  Extractor format
 *
 * @return
 *       - "none"  Invalid format type
 *       - Others  Format string representation
 */
const char *esp_extractor_get_format_name(esp_extractor_format_t format);

/**
 * @brief  Allocate extractor configuration for local url file
 *
 * @note  This configuration need free after call `esp_extractor_close`
 *
 * @param[in]  file_url      File URI
 * @param[in]  extract_mask  Extract mask
 * @param[in]  output_size   Memory pool size to hold output frames
 *
 * @return
 *       - NULL    Not enough memory or file not exist
 *       - Others  Extract configuration
 */
esp_extractor_config_t *esp_extractor_alloc_file_config(const char *file_url, uint8_t extract_mask, uint32_t output_size);

/**
 * @brief  Allocate extractor configuration for buffer
 *
 * @note  This configuration need free after call `esp_extractor_close`
 *
 * @param[in]  buffer        Buffer pointer
 * @param[in]  buffer_size   Buffer size
 * @param[in]  extract_mask  Extract mask
 * @param[in]  output_size   Memory pool size to hold output frames
 *
 * @return
 *       - NULL    Not enough memory
 *       - Others  Extract configuration
 */
esp_extractor_config_t *esp_extractor_alloc_buffer_config(uint8_t *buffer, int buffer_size, uint8_t extract_mask,
                                                          uint32_t output_size);

/**
 * @brief  Free configuration
 *
 * @param[in]  config  Configuration
 */
void esp_extractor_free_config(esp_extractor_config_t *config);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
