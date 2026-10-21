// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_NODE_VOLC_RTC_H__
#define __CONV_AI_NODE_VOLC_RTC_H__
#if defined(ENABLE_RTC_MODE)
#include <stdbool.h>

#include "cJSON.h"

#include "VolcEngineRTCLite.h"

#include "base/volc_base.h"
#include "base/volc_device_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* volc_rtc_t;

volc_rtc_t volc_rtc_create(const char* appid, void* context, cJSON* p_config, volc_msg_cb message_callback, volc_data_cb data_callback);

void volc_rtc_destroy(volc_rtc_t rtc);

int volc_rtc_start(volc_rtc_t rtc, const char* bot_id, volc_iot_info_t* iot_info, const char* params);

int volc_rtc_stop(volc_rtc_t rtc);

int volc_rtc_send(volc_rtc_t rtc, const void* data, int size, volc_data_info_t* data_info);

int volc_rtc_interrupt(volc_rtc_t rtc);

int volc_rtc_send_jpg(volc_rtc_t rtc, void* data, int size);

int volc_rtc_send_text_to_agent(volc_rtc_t rtc, const char* text, volc_agent_type_e type, int interrupt_mode);

#endif

#ifdef __cplusplus
}
#endif
#endif /* __CONV_AI_NODE_VOLC_RTC_H__ */
