/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ESP_CODEC_DEV_OS_H_
#define _ESP_CODEC_DEV_OS_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Codec device mutex handle
 */
typedef void *esp_codec_dev_mutex_handle_t;

/**
 * @brief  Create mutex for codec device
 *
 * @return
 *       - NULL    No available resource
 *       - Others  Mutex handle
 */
esp_codec_dev_mutex_handle_t esp_codec_dev_mutex_create(void);

/**
 * @brief  Lock mutex for codec device
 *
 * @param[in]  mutex       Mutex handle
 * @param[in]  timeout_ms  Wait timeout for mutex (unit ms)
 *
 * @return
 *       - 0       On success
 *       - Others  Failed to lock
 */
int esp_codec_dev_mutex_lock(esp_codec_dev_mutex_handle_t mutex, int timeout_ms);

/**
 * @brief  Unlock mutex for codec device
 *
 * @param[in]  mutex  Mutex handle
 *
 * @return
 *       - 0       On success
 *       - Others  Failed to unlock
 */
int esp_codec_dev_mutex_unlock(esp_codec_dev_mutex_handle_t mutex);

/**
 * @brief  Destroy mutex for codec device
 *
 * @param[in]  mutex  Mutex handle
 */
void esp_codec_dev_mutex_destroy(esp_codec_dev_mutex_handle_t mutex);

/**
 * @brief  Sleep in milliseconds
 *
 * @param[in]  ms  Sleep time (unit ms)
 */
void esp_codec_dev_sleep(int ms);

#ifdef __cplusplus
}
#endif

#endif
