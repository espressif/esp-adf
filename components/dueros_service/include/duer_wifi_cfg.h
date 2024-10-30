/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#ifndef __DUER_WIFI_CFG_H__
#define __DUER_WIFI_CFG_H__

#include "esp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Event id of the duer wifi configure process
 */
typedef enum {
    DUER_WIFI_CFG_SSID_GET,     /*!< Event Data: `duer_wifi_ssid_get_t` */
    DUER_WIFI_CFG_BLE_CONN,     /*!< Event Data: NULL */
    DUER_WIFI_CFG_BLE_DISC,     /*!< Event Data: NULL */
} duer_wifi_cfg_event_t;

/**
 * @brief Event data of `DUER_WIFI_CFG_SSID_GET`
 */
typedef struct {
    char *ssid;             /*!< SSID */
    char *pwd;              /*!< Password */
    char *bduss;            /*!< Got from server, should write back to profile */
    char *device_bduss;     /*!< Got from server, should write back to profile */
} duer_wifi_ssid_get_t;

/**
 * @brief The callback prototype of WiFi configure events.
 */
typedef void (*duer_wifi_cfg_user_cb_t)(duer_wifi_cfg_event_t event, void *data);

/**
 * @brief The callback prototype of WiFi configure events.
 */
typedef struct {
    duer_wifi_cfg_user_cb_t user_cb; /*!< User callback */
    const char *client_id;           /*!< Duer client id */
    const char *pub_key;             /*!< Duer public key */
} duer_wifi_cfg_t;

/**
 * @brief Initialize the duer WiFi configuration process
 *
 * @param[in] cfg  Provide initialization configuration
 *
 * @return
 *      - 0: Sucess
 *      - Others: Failed
 */
int duer_wifi_cfg_init(duer_wifi_cfg_t *cfg);

/**
 * @brief Deinitialize the duer WiFi configuration process
 *
 * @return
 *      - 0: Sucess
 *      - Others: Failed
 */
int duer_wifi_cfg_deinit(void);

#ifdef __cplusplus
}
#endif

#endif //__DUER_WIFI_CFG_H__
