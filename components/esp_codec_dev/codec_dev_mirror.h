/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CODEC_DEV_MIRROR_H_
#define _CODEC_DEV_MIRROR_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

typedef struct codec_dev_mirror_t *codec_dev_mirror_handle_t;

/**
 * @brief  Initialize codec device input mirror
 *
 * @param[in]   size    Ringbuffer capacity in bytes
 * @param[out]  handle  Input mirror handle
 *
 * @return
 *       - ESP_CODEC_DEV_OK           On success
 *       - ESP_CODEC_DEV_INVALID_ARG  If handle is NULL or size is invalid
 *       - ESP_CODEC_DEV_NO_MEM       If allocation fails
 */
int codec_dev_mirror_init(int size, codec_dev_mirror_handle_t *handle);

/**
 * @brief  Deinitialize codec device input mirror
 *
 *         This function releases the ringbuffer and mirror handle resources.
 *         The caller is responsible for clearing its handle.
 *
 * @param[in]  handle  Input mirror handle
 *
 * @return
 *       - ESP_CODEC_DEV_OK           On success
 *       - ESP_CODEC_DEV_INVALID_ARG  If handle is NULL
 */
int codec_dev_mirror_deinit(codec_dev_mirror_handle_t handle);

/**
 * @brief  Write data into codec device input mirror
 *
 * @param[in]  handle  Input mirror handle
 * @param[in]  data    Data to mirror
 * @param[in]  len     Data length in bytes
 *
 * @return
 *       - ESP_CODEC_DEV_OK           On success
 *       - ESP_CODEC_DEV_INVALID_ARG  If arguments are invalid
 *       - ESP_CODEC_DEV_WRONG_STATE  If handle is not initialized
 */
int codec_dev_mirror_write(codec_dev_mirror_handle_t handle, const uint8_t *data, int len);

/**
 * @brief  Read data from codec device input mirror
 *
 * @param[in]   handle      Input mirror handle
 * @param[out]  buffer      Data buffer to fill
 * @param[in]   size        Data length to read
 * @param[in]   timeout_ms  Timeout in milliseconds, 0 no wait, negative wait forever
 * @param[out]  bytes_read  Actual bytes copied to buffer
 *
 * @return
 *       - ESP_CODEC_DEV_OK           On success
 *       - ESP_CODEC_DEV_INVALID_ARG  If arguments are invalid
 *       - ESP_CODEC_DEV_TIMEOUT      If timeout expires before size bytes are read
 *       - ESP_CODEC_DEV_WRONG_STATE  If mirror is disabled
 */
int codec_dev_mirror_read(codec_dev_mirror_handle_t handle,
                          uint8_t *buffer,
                          int size,
                          int timeout_ms,
                          int *bytes_read);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* _CODEC_DEV_MIRROR_H_ */
