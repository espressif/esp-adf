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

#ifndef MAIN_CLIENT_BDS_CLIENT_COMMAND_H_
#define MAIN_CLIENT_BDS_CLIENT_COMMAND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "bds_client_params.h"

/**
 * @brief   bdsc cmd key type
 */
typedef enum {
    CMD_ASR_START = 100,
    CMD_ASR_CANCEL,
    CMD_ASR_EXTERN_DATA,
    CMD_ASR_CONFIG,
    CMD_WAKEUP_START = 200,
    CMD_WAKEUP_STOP,
    CMD_EVENTUPLOAD_START = 300,
    CMD_EVENTUPLOAD_CANCEL,
    CMD_EVENTUPLOAD_DATA,
    CMD_LINK_START = 500,
    CMD_LINK_STOP,
    CMD_RECORDER_START = 600,
    CMD_RECORDER_STOP,
    CMD_DYNAMIC_CONFIG = 700,
    CMD_TTS_HEADER = 701,
    CMD_TTS_START = 800,
    CMD_TTS_CANCEL
} bdsc_cmd_key_t;

/**
 * @brief   bdsc client cmd type
 */
typedef struct {
    bdsc_cmd_key_t key;
    void *content;
    uint16_t content_length;
} bds_client_command_t;

/**
 * @brief   bdsc cmd data type
 */
typedef struct {
    char sn[SN_LENGTH];
    int32_t flag;
    uint16_t buffer_length;
    uint8_t buffer[];
} bdsc_cmd_data_t;

/**
 * @brief      Create bdsc cmd data
 *
 * @param[in]  flag           Bdsc cmd flag
 * @param[in]  buffer_length  Bdsc cmd buffer length
 * @param[in]  buffer         Bdsc cmd buffer
 * @param[in]  sn             Bdsc cmd sn
 *
 * @return
 *  - bdsc_cmd_data_t handle
 *  - NULL if any errors
 */
bdsc_cmd_data_t *bdsc_cmd_data_create(int32_t flag, uint16_t buffer_length, uint8_t *buffer, char *sn);

/**
 * @brief      Destory bdsc cmd data
 *
 * @param[in]  data           bdsc_cmd_data_t handle
 *
 * @return
 */
void bdsc_cmd_data_destroy(bdsc_cmd_data_t *data);


/**
 * @brief      Deep cooy bds client command
 *
 * @param[in]  dst          Copy to
 * @param[in]  src          Copy from
 *
 * @return
 *  - TURE: on success
 *  - FALSE: on fail
 */
bool bdsc_deepcopy_command(bds_client_command_t *dst, bds_client_command_t *src);

/**
 * @brief      Destroy cooy bds client command
 *
 * @param[in]  command          bds client command handle
 *
 * @return
 */
void bdsc_deepdestroy_command(bds_client_command_t *command);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_CLIENT_BDS_CLIENT_COMMAND_H_ */
