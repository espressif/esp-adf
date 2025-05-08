/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _ESP_CODEC_DEV_H_
#define _ESP_CODEC_DEV_H_

#include "audio_codec_if.h"
#include "audio_codec_data_if.h"
#include "esp_codec_dev_types.h"
#include "esp_codec_dev_vol.h"
#include "audio_codec_vol_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Codec device configuration
 */
typedef struct {
    esp_codec_dev_type_t         dev_type; /*!< Codec device type */
    const audio_codec_if_t      *codec_if; /*!< Codec interface */
    const audio_codec_data_if_t *data_if;  /*!< Codec data interface */
} esp_codec_dev_cfg_t;

/**
 * @brief Codec device handle
 */
typedef void *esp_codec_dev_handle_t;

/**
 * @brief         Get `esp_codec_dev` version string
 * @return        Version information
 */
const char *esp_codec_dev_get_version(void);

/**
 * @brief         New codec device
 * @param         codec_dev_cfg: Codec device configuration
 * @return        NULL: Fail to new codec device
 *                -Others: Codec device handle
 */
esp_codec_dev_handle_t esp_codec_dev_new(esp_codec_dev_cfg_t *codec_dev_cfg);

/**
 * @brief         Open codec device
 * @param         codec: Codec device handle
 * @param         fs: Audio sample information
 * @return        ESP_CODEC_DEV_OK: Open success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support or driver not ready yet
 */
int esp_codec_dev_open(esp_codec_dev_handle_t codec, esp_codec_dev_sample_info_t *fs);

/**
 * @brief         Read register value from codec
 * @param         codec: Codec device handle
 * @param         reg: Register address to be read
 * @param         val: Value to be read
 * @return        ESP_CODEC_DEV_OK: Read success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 */
int esp_codec_dev_read_reg(esp_codec_dev_handle_t codec, int reg, int *val);

/**
 * @brief         Write register value to codec
 * @param         codec: Codec device handle
 * @param         reg: Register address to be wrote
 * @param         val: Value to be wrote
 * @return        ESP_CODEC_DEV_OK: Read success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 */
int esp_codec_dev_write_reg(esp_codec_dev_handle_t codec, int reg, int val);

/**
 * @brief         Read data from codec
 * @param         codec: Codec device handle
 * @param         data: Data to be read
 * @param         len: Data length to be read
 * @return        ESP_CODEC_DEV_OK: Read success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_read(esp_codec_dev_handle_t codec, void *data, int len);

/**
 * @brief         Write data to codec
 *                Notes: when enable software volume, it will change input data level directly without copy
 *                Make sure that input data is writable
 * @param         codec: Codec device handle
 * @param         data: Data to be wrote
 * @param         len: Data length to be wrote
 * @return        ESP_CODEC_DEV_OK: Write success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_write(esp_codec_dev_handle_t codec, void *data, int len);

/**
 * @brief         Set codec hardware gain
 * @param         codec: Codec device handle
 * @param         volume: Volume setting
 * @return        ESP_CODEC_DEV_OK: Set output volume success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support output mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t codec, int volume);

/**
 * @brief         Set codec software volume handler
 *                Notes: it is not needed when codec support volume adjust in hardware
 *                If not provided, it will use internally software volume process handler instead
 * @param         codec: Codec device handle
 * @param         vol_handler: Software volume process interface
 * @return        ESP_CODEC_DEV_OK: Set volume handler success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support output mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_set_vol_handler(esp_codec_dev_handle_t codec, const audio_codec_vol_if_t* vol_handler);

/**
 * @brief         Set codec volume curve
 *                Notes: When volume curve not provided, it will use internally volume curve which is:
 *                    1 - "-49.5dB", 100 - "0dB"
 *                    Need to call this API if you want to customize volume curve
 * @param         codec: Codec device handle
 * @param         curve: Volume curve setting
 * @return        ESP_CODEC_DEV_OK: Set curve success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support output mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 *                ESP_CODEC_DEV_NO_MEM: Not enough memory to hold volume curve
 */
int esp_codec_dev_set_vol_curve(esp_codec_dev_handle_t codec, esp_codec_dev_vol_curve_t *curve);

/**
 * @brief         Get codec output volume
 * @param         codec: Codec device handle
 * @param[out]    volume: Volume to get
 * @return        ESP_CODEC_DEV_OK: Get volume success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support output mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_get_out_vol(esp_codec_dev_handle_t codec, int *volume);

/**
 * @brief         Set codec output mute
 * @param         codec: Codec device handle
 * @param         mute: Whether mute output or not
 * @return        ESP_CODEC_DEV_OK: Set output mute success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support output mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_set_out_mute(esp_codec_dev_handle_t codec, bool mute);

/**
 * @brief         Get codec output mute setting
 * @param         codec: Codec device handle
 * @param[out]    muted: Mute status to get
 * @return        ESP_CODEC_DEV_OK: Get output mute success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support output mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_get_out_mute(esp_codec_dev_handle_t codec, bool *muted);

/**
 * @brief         Set codec input gain
 * @param         codec: Codec device handle
 * @param         db_value: Input gain setting
 * @return        ESP_CODEC_DEV_OK: Set input gain success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support input mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_set_in_gain(esp_codec_dev_handle_t codec, float db_value);

/**
 * @brief         Set codec input gain by channel
 * @param         codec: Codec device handle
 * @param         channel_mask: Mask for channel to be set
 * @param         db_value: Input gain setting
 * @return        ESP_CODEC_DEV_OK: Set input gain success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support input mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_set_in_channel_gain(esp_codec_dev_handle_t codec, uint16_t channel_mask, float db_value);

/**
 * @brief         Get codec input gain
 * @param         codec: Codec device handle
 * @param         db_value: Input gain to get
 * @return        ESP_CODEC_DEV_OK: Get input gain success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support input mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_get_in_gain(esp_codec_dev_handle_t codec, float *db_value);

/**
 * @brief         Set codec input mute
 * @param         codec: Codec device handle
 * @param         mute: Whether mute code input or not
 * @return        ESP_CODEC_DEV_OK: Set input mute success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support input mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_set_in_mute(esp_codec_dev_handle_t codec, bool mute);

/**
 * @brief         Get codec input mute
 * @param         codec: Codec device handle
 * @param         muted: Mute value to get
 * @return        ESP_CODEC_DEV_OK: Set input mute success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 *                ESP_CODEC_DEV_NOT_SUPPORT: Codec not support input mode
 *                ESP_CODEC_DEV_WRONG_STATE: Driver not open yet
 */
int esp_codec_dev_get_in_mute(esp_codec_dev_handle_t codec, bool *muted);

/**
 * @brief         Whether disable codec when closed
 * @param         codec: Codec device handle
 * @param         disable: Disable when closed (default is true)
 * @return        ESP_CODEC_DEV_OK: Setting success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 */
int esp_codec_set_disable_when_closed(esp_codec_dev_handle_t codec, bool disable);

/**
 * @brief         Close codec device
 * @param         codec: Codec device handle
 * @return        ESP_CODEC_DEV_OK: Close success
 *                ESP_CODEC_DEV_INVALID_ARG: Invalid arguments
 */
int esp_codec_dev_close(esp_codec_dev_handle_t codec);

/**
 * @brief         Delete the specified codec device instance
 * @param         codec: Codec device handle
 */
void esp_codec_dev_delete(esp_codec_dev_handle_t codec);

#ifdef __cplusplus
}
#endif

#endif
