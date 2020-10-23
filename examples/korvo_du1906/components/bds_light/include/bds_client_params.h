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

#ifndef MAIN_CLIENT_BDS_CLIENT_PARAMS_H_
#define MAIN_CLIENT_BDS_CLIENT_PARAMS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "bds_client_context.h"

/**
 * @brief   bds client protocol type
 */
typedef enum PROTOCOL_TYPE {
    PROTOCOL_DEFAULT = 0x00,
    PROTOCOL_TLS = 0x01
} PROTOCOL_TYPE;

/**
 * @brief   bds client recorder type
 */
typedef enum RECORDER_TYPE {
    RECORDER_TYPE_PCM0 = 0x00,
    RECORDER_TYPE_PCM1 = 0x01
} RECORDER_TYPE;

/**
 * @brief   bds client asr force mode
 */
typedef enum ASR_FORCE_MODE {
    ASR_FORCE_AUTO = 0,
    ASR_FORCE_OPUS,
    ASR_FORCE_FEATURE
} ASR_FORCE_MODE;

/**
 * @brief   bds client launch mode
 */
typedef enum LAUNCH_MODE {
    LAUNCH_LOAD_DSP = 0x00,
    LAUNCH_CHECK_DSP = 0x01
} LAUNCH_MODE_T;

typedef enum WAKEUP_NUM {
    WP_NUM_DEFAULT = 0x00,
    WP_NUM_ONE = 0x01,
    WP_NUM_TWO = 0x02
} WAKEUP_NUM;

/**
 * @brief   bdsc engine params type
 */
typedef struct {
    char sn[SN_LENGTH];
    uint32_t pid;
    char host[HOST_LENGTH];
    int port;
    uint8_t protocol;
    char cuid[CUID_LENGTH];
    char app[APP_LENGTH];
    LAUNCH_MODE_T launch_mode;
    uint16_t pam_len;
    char pam[];
} bdsc_engine_params_t;

/**
 * @brief   bds client params type
 */
typedef struct {
    bdsc_engine_params_t *engine_params;
} bds_client_params_t;

/**
 * @brief   bdsc asr params type
 */
typedef struct {
    char sn[SN_LENGTH];
    uint32_t primary_pid;
    uint32_t assist_pid;
    char key[KEY_LENGTH];
    uint16_t audio_rate;
    char cuid[CUID_LENGTH];
    int wakeup_status;
    int voice_print;
    uint16_t pam_len;
    char pam[];
} bdsc_asr_params_t;

/**
 * @brief   bdsc eventupload params type
 */
typedef struct {
    char sn[SN_LENGTH];
    uint32_t pid;
    char key[KEY_LENGTH];
    char cuid[CUID_LENGTH];
    uint16_t pam_len;
    char pam[];
} bdsc_eventupload_params_t;

typedef struct {
    int wakeup_num;
} bdsc_wp_params_t;

/**
 * @brief      create bdsc asr params
 *
 * @param[in]  sn               sn
 * @param[in]  primary_pid      primary pid
 * @param[in]  assist_pid       assist pid
 * @param[in]  key              key
 * @param[in]  audio_rate       audio rate
 * @param[in]  cuid             cuid
 * @param[in]  backtrack_time   backtrack time
 * @param[in]  pam_len          pam len
 * @param[in]  pam              pam
 *
 * @return
 *     - `bdsc_asr_params_t`
 *     - NULL if any errors
 */
bdsc_asr_params_t *bdsc_asr_params_create(char *sn,
        uint32_t primary_pid, uint32_t assist_pid, char *key,
        uint16_t audio_rate, char *cuid, int wakeup_status,
        uint16_t pam_len, char *pam);

bdsc_asr_params_t *bdsc_asr_params_create_ext(char *sn,
        uint32_t primary_pid, uint32_t assist_pid, char *key,
        uint16_t audio_rate, char *cuid, int wakeup_status,
        uint16_t pam_len, char *pam, int voice_print);

/**
 * @brief      destroy bdsc asr params
 *
 * @param[in]  params               bdsc asr params handle
 *
 * @return
 */
void bdsc_asr_params_destroy(bdsc_asr_params_t *params);

/**
 * @brief      create bdsc event params
 *
 * @param[in]  sn               sn
 * @param[in]  pid              pid
 * @param[in]  key              key
 * @param[in]  cuid             cuid
 * @param[in]  pam_len          pam len
 * @param[in]  pam              pam
 *
 * @return
 *     - `bdsc_eventupload_params_t`
 *     - NULL if any errors
 */
bdsc_eventupload_params_t *bdsc_event_params_create(char *sn,
        uint32_t pid, char *key, char *cuid, uint16_t pam_len, char *pam);
/**
 * @brief      destroy bdsc event params
 *
 * @param[in]  params               bdsc event params
 *
 * @return
 *     - `bdsc_eventupload_params_t`
 *     - NULL if any errors
 */
void bdsc_event_params_destroy(bdsc_eventupload_params_t *params);

/**
 * @brief      create bdsc engine params
 *
 * @param[in]  sn               sn
 * @param[in]  pid              pid
 * @param[in]  host             host
 * @param[in]  port             port
 * @param[in]  protocol         protocol
 * @param[in]  cuid             cuid
 * @param[in]  app              app
 * @param[in]  pam_len          pam len
 * @param[in]  pam              pam
 *
 * @return
 *     - `bdsc_engine_params_t`
 *     - NULL if any errors
 */
bdsc_engine_params_t *bdsc_engine_params_create(char *sn, uint32_t pid, char *host, int port, uint8_t protocol,
        char *cuid, char *app, LAUNCH_MODE_T launch_mode, uint16_t pam_len, char *pam);

/**
 * @brief      destory bdsc engine params
 *
 * @param[in]  params               params
 *
 * @return
 */
void bdsc_engine_params_destroy(bdsc_engine_params_t *params);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_CLIENT_BDS_CLIENT_PARAMS_H_ */
