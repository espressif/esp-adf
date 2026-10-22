// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_NODE_VOLC_WS_H__
#define __CONV_AI_NODE_VOLC_WS_H__

#include <stdbool.h>

#include "cJSON.h"

#include "base/volc_base.h"
#include "base/volc_device_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* volc_ws_t;

volc_ws_t volc_ws_create(void* context, cJSON* p_config, volc_msg_cb message_callback, volc_data_cb data_callback);

void volc_ws_destroy(volc_ws_t ws);

int volc_ws_start(volc_ws_t ws, const char* bot_id, volc_iot_info_t* iot_info, const char* params);

int volc_ws_send(volc_ws_t ws, const void* data, int size, volc_data_info_t* data_info);

int volc_ws_stop(volc_ws_t ws);

int volc_ws_interrupt(volc_ws_t ws);

#ifdef __cplusplus
}
#endif
#endif /* __CONV_AI_NODE_VOLC_WS_H__ */
