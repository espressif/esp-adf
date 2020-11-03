/*
 * Copyright (c) 2020 Baidu.com, Inc. All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

#ifndef MAIN_CLIENT_BDS_CLIENT_EVENT_H_
#define MAIN_CLIENT_BDS_CLIENT_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "bds_client_context.h"

/**
 * @brief   Bdsc Event key type
 */
typedef enum {
    EVENT_ASR_BEGIN = 1000,
    EVENT_ASR_RESULT,
    EVENT_ASR_EXTERN_DATA,
    EVENT_ASR_TTS_DATA,
    EVENT_ASR_END,
    EVENT_ASR_CANCEL,
    EVENT_ASR_ERROR,
    EVENT_WAKEUP_TRIGGER = 2000,
    EVENT_WAKEUP_ERROR,
    EVENT_EVENTUPLOAD_BEGIN = 3000,
    EVENT_EVENTUPLOAD_END,
    EVENT_EVENTUPLOAD_DATA,
    EVENT_EVENTUPLOAD_TTS,
    EVENT_EVENTUPLOAD_CANCEL,
    EVENT_EVENTUPLOAD_ERROR,
    EVENT_PUSH_DATA = 4000,
    EVENT_PUSH_ERROR,
    EVENT_LINK_CONNECTED = 5000,
    EVENT_LINK_DISCONNECTED,
    EVENT_LINK_ERROR,
    EVENT_RECORDER_DATA = 6000,
    EVENT_RECORDER_ERROR,
    EVENT_SDK_START_COMPLETED = 7000,
    EVENT_RECV_MQTT_PUSH_URL,
    EVENT_RECV_A2DP_START_PLAY,
    EVENT_DSP_FATAL_ERROR = 8000,
    EVENT_DSP_LOAD_FAILED = 8001,
    EVENT_TTS_BEGIN = 9000,
    EVENT_TTS_END,
    EVENT_TTS_RESULT,
    EVENT_TTS_CANCEL,
    EVENT_TTS_ERROR,

} bdsc_event_key_t;

/**
 * @brief      Bdsc event type

 * @note       content may be basetype or struct, can custom
 *
 */
typedef struct {
    bdsc_event_key_t key;
    void *content;
    uint16_t content_length;
} bds_client_event_t;

/**
 * @brief      Bdsc event error type
 */
typedef struct {
    char sn[SN_LENGTH];
    int32_t code;
    uint16_t info_length;
    char info[];
} bdsc_event_error_t;

/**
 * @brief      Bdsc event data type
 */
typedef struct {
    char sn[SN_LENGTH];
    int16_t idx;
    uint16_t buffer_length;
    uint8_t buffer[];
} bdsc_event_data_t;

/**
 * @brief      Bdsc event process type
 */
typedef struct {
    char sn[SN_LENGTH];
} bdsc_event_process_t;

/**
 * @brief      Bdsc event wakeup type
 */
typedef struct {
    int status;
    uint16_t dci_length;
    uint8_t dci_buffer[];
} bdsc_event_wakeup_t;

/**
 * @brief      Create bdsc event error handle
 *
 * @param[in]  sn           bdsc event error sn
 * @param[in]  code         bdsc event error code
 * @param[in]  info_length  bdsc event error info length
 * @param[in]  info         bdsc event error info
 *
 * @return
 *     - `bdsc_event_error_t`
 *     - NULL if any errors
 */
bdsc_event_error_t *bdsc_event_error_create(char *sn,
        int32_t code, uint16_t info_length, char *info);

/**
 * @brief      Destroy bdsc event error handle
 *
 * @param[in]  error        bdsc event error handle
 *
 * @return
 */
void bdsc_event_error_destroy(bdsc_event_error_t *error);

/**
 * @brief      Create bdsc event data handle
 *
 * @param[in]  sn             bdsc event data sn
 * @param[in]  idx            bdsc event data idx
 * @param[in]  buffer_length  bdsc event data buffer length
 * @param[in]  buffer         bdsc event data buffer
 *
 * @return
 *     - `bdsc_event_data_t`
 *     - NULL if any errors
 */
bdsc_event_data_t *bdsc_event_data_create(char *sn,
        int16_t idx, uint16_t buffer_length, uint8_t *buffer);

/**
 * @brief      Destroy bdsc event data handle
 *
 * @param[in]  data        bdsc event data handle
 *
 * @return
 */
void bdsc_event_data_destroy(bdsc_event_data_t *data);

bdsc_event_wakeup_t *bdsc_wakeup_data_create(int status, uint16_t buffer_length, uint8_t *buffer);
void bdsc_wakeup_data_destroy(bdsc_event_wakeup_t *data);


/**
 * @brief      Deep copy bdsc event handle
 *
 * @param[in]  dst        copy to
 * @param[in]  src        copy from
 *
 * @return
 *  - TURE: on success
 *  - FALSE: on fail
 */
bool bdsc_deepcopy_event(bds_client_event_t *dst, bds_client_event_t *src);

/**
 * @brief      Deep Destroy bdsc event handle
 *
 * @param[in]  event        bdsc event handle
 *
 * @return
 */
void bdsc_deepdestroy_event(bds_client_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_CLIENT_BDS_CLIENT_EVENT_H_ */
