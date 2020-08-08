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
#ifndef _PLAY_TASK_H
#define _PLAY_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief raw play task cmd type
 */
typedef enum _play_task_cmd {
    CMD_RAW_PLAY_START = '0',
    CMD_RAW_PLAY_FEED_DATA = '1',
    CMD_RAW_PLAY_FEED_FINISH = '2',
    CMD_RAW_PLAY_STOP = '3',

    CMD_HTTP_PLAY_START = '4',
    CMD_A2DP_PLAY_START = '5',
    CMD_HTTP_PLAY_PAUSE = '6',
    CMD_HTTP_PLAY_RESUME = '7',
} play_task_cmd_t;


void handle_play_cmd(int cmd, uint8_t *buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _PLAY_TASK_H
