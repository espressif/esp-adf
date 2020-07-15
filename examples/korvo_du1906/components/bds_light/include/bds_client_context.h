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

#ifndef MAIN_CLIENT_BDS_CLIENT_CONTEXT_H_
#define MAIN_CLIENT_BDS_CLIENT_CONTEXT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/**
 * @brief   Bdsc Engine error codes
 */
#define ERROR_BDSC_INVALID_RESOURCE             -1000
#define ERROR_BDSC_ASR_START_FAILED             -2000
#define ERROR_BDSC_ASR_CANCEL_FAILED            -2001
#define ERROR_BDSC_RECORDER_START_FAILED        -3000
#define ERROR_BDSC_RECORDER_READ_FAILED         -3001
#define ERROR_BDSC_EVENTUPLOAD_START_FAILED     -4000
#define ERROR_BDSC_EVENTUPLOAD_CANCEL_FAILED    -4001
#define ERROR_BDSC_EVENTUPLOAD_ENGINE_BUSY      -4002
#define ERROR_BDSC_WAKEUP_START_FAILED          -5000
#define ERROR_BDSC_WAKEUP_STOP_FAILED           -5001
#define ERROR_BDSC_LINK_START_INVALID           -6000
#define ERROR_BDSC_LINK_STOP_INVALID            -6001
#define ERROR_BDSC_LINK_CONNECT_FAILED          -6002
#define ERROR_BDSC_MIC_SIGNAL_AM_ALL_ZERO       -7000
#define ERROR_BDSC_MIC_SIGNAL_AM_CLIPPING       -7001
#define ERROR_BDSC_MIC_SIGNAL_UNIFORM           -7002
#define ERROR_BDSC_MIC_SIGNAL_INCONSISTENCY     -7003
#define ERROR_BDSC_DSP_RUNTIME_ERROR            -7004

#define SN_LENGTH                               37
#define KEY_LENGTH                              32
#define CUID_LENGTH                             37
#define HOST_LENGTH                             64
#define APP_LENGTH                              64

#define FLAG_DEFAULT                            0
#define FLAG_TAIL                               1

#define KEY_NQE_MODE                            "nqe_mode"
#define KEY_LC_HOST                             "lc_host"
#define KEY_ASR_MODE_STATE                      "asr_mode_state"
#define KEY_WS_HEAD_HOST                        "head_host"
#define KEY_SELF_WAKEUP_RESTRAIN                "self_wakeup_restrain"

#define KEY_HOST                                "host"
#define KEY_PORT                                "port"
#define KEY_CUID                                "cuid"
#define KEY_PID                                 "pid"
#define KEY_KEY                                 "key"
#define KEY_ENGINE_URI                          "engine_uri"
#define KEY_NET_LOG_LEVEL                       "qnet_log_level"

#define LENGTH_SN                               (SN_LENGTH)
#define LENGTH_HOST                             64
#define LENGTH_CUID                             64
#define LENGTH_KEY                              128
#define LENGTH_URI                              128

/**
 * @brief      Bdsc Audio data type
 */
typedef struct {
    char sn[LENGTH_SN];
    int32_t flag;
    uint16_t buffer_length;
    uint16_t real_length;
    uint8_t buffer[];
} bdsc_audio_t;

/**
 * @brief      Bdsc Error type
 */
typedef struct {
    int32_t code;
    uint16_t info_length;
    char info[];
} bdsc_error_t;

/**
 * @brief      Bdsc client context type
 */
typedef struct {

} bds_client_context_t;

/**
 * @brief      Create bdsc audio data type
 *
 * @param[in]  flag             bdsc audio flag
 * @param[in]  buffer_length    bdsc audio buffer length
 * @param[in]  buffer           bdsc audio buffer
 *
 * @return
 *     - `bdsc_audio_t`
 *     - NULL if any errors
 */
bdsc_audio_t* bdsc_audio_create(int32_t flag, uint16_t buffer_length, const uint8_t *buffer);

/**
 * @brief      Destroy bdsc audio data type
 *
 * @param[in]  audio             bdsc_audio_t handle
 *
 * @return
 */
void bdsc_audio_destroy(bdsc_audio_t *audio);

/**
 * @brief      Create bdsc error data type
 *
 * @param[in]  code           bdsc error code
 * @param[in]  info_length    bdsc error info length
 * @param[in]  info           bdsc error info
 *
 * @return
 *     - `bdsc_error_t`
 *     - NULL if any errors
 */
bdsc_error_t* bdsc_error_create(int32_t code, uint16_t info_length, char *info);

/**
 * @brief      Destroy bdsc error data type
 *
 * @param[in]  error           bdsc_error_t handle
 *
 * @return
 */
void bdsc_error_destroy(bdsc_error_t *error);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_CLIENT_BDS_CLIENT_CONTEXT_H_ */
