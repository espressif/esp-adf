/**
 * Copyright (2018) Baidu Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * File: lightduer_smart_config.h
 * Auth: Chen Xihao (chenxihao@baidu.com)
 * Desc: Duer smart config function file.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_SMART_CONFIG_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_SMART_CONFIG_H

#include "lightduer_wifi_config_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DUER_SC_STATUS_IDLE = 0,
    DUER_SC_STATUS_SEARCH_CHAN,
    DUER_SC_STATUS_LOCKED_CHAN,
    DUER_SC_STATUS_LEAD_CODE_COMPLETE,
    DUER_SC_STATUS_COMPLETE,
} duer_sc_status_t;

typedef void *duer_sc_context_t;

/**
 * DESC:
 * Initialize the context of smartconfig.
 *
 * PARAM: none
 *
 * @RETURN: duer_sc_context_t, the created context.
 */
duer_sc_context_t duer_smart_config_init(void);

/**
 * DESC:
 * When wifi on promiscuous mode, pass packets to this function to decode
 *
 * PARAM:
 * @param[in] context: the context
 * @param[in] packet: 802.11 frame, at least 24 bytes
 * @param[in] len: the length of packet
 *
 * @RETURN: duer_sc_status_t, the status of smartconfig
 */
duer_sc_status_t duer_smart_config_recv_packet(duer_sc_context_t context,
        uint8_t *packet, uint32_t len);

/**
 * DESC:
 * Notity smartconfig channel changed
 *
 * PARAM:
 * @param[in] context: the context
 * @param[in] channel: current channel
 *
 * @RETURN: 0 when success, negative when fail.
 */
int duer_smart_config_switch_channel(duer_sc_context_t context, int channel);

/**
 * DESC:
 * Get the status of smartconfig
 *
 * PARAM:
 * @param[in] context: the context
 *
 * @RETURN: duer_sc_status_t, the status of smartconfig
 */
duer_sc_status_t duer_smart_config_get_status(duer_sc_context_t context);

/**
 * DESC:
 * Get the result of smartconfig, can be called after smart config completed
 *
 * PARAM:
 * @param[in] context: the context
 * @param[out] result: the result of smartconfig
 *
 * @RETURN: 0 when success, negative when fail.
 */
int duer_smart_config_get_result(duer_sc_context_t context,
                                 duer_wifi_config_result_t *result);

/**
 * DESC:
 * Get the mac address of objective ap, can be called after the cannel locked
 *
 * PARAM:
 * @param[in] context: the context
 * @param[out] result: the result of smartconfig
 *
 * @RETURN: 0 when success, negative when fail.
 */
int duer_smart_config_get_ap_mac(duer_sc_context_t context, uint8_t *ap_mac);

/**
 * DESC:
 * Deinit the smartconfig
 *
 * PARAM:
 * @param[in] context: the context
 *
 * @RETURN: 0 when success, negative when fail.
 */
int duer_smart_config_deinit(duer_sc_context_t context);

#ifdef DUER_MODULE_DEVICE_STATUS_SUPPORT
/**
 * DESC:
 * Send the trace log about lock channel or lead completed
 * In this function maybe wait to mutex
 *
 * PARAM:
 * @param[in] context: the context
 *
 * @RETURN: none
 */
void duer_smart_config_send_log(duer_sc_context_t context);
#endif

/**
 * DESC:
 * Send ack to app, call this function after wifi connected
 *
 * PARAM:
 * @param[in] port: socket port to send ack
 * @param[in] random: send this random to app
 *
 * @RETURN: 0 when success, negative when fail.
 */
int duer_smart_config_ack(uint32_t port, uint8_t random);

/**
 * DESC:
 * Stop send ack
 *
 * PARAM:none
 *
 * @RETURN: none
 */
void duer_smart_config_stop_ack(void);

#ifdef __cplusplus
}
#endif
#endif /* BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_SMART_CONFIG_H */
