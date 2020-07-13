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
#ifndef __MIXED_PLAY_TASK_H
#define __MIXED_PLAY_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief mixed player wait event type
 */
typedef enum {
    MIXED_PLAYER_TTS_DONE,
    MIXED_PLAYER_WAKEUP,
} mixed_wait_event_t;

/**
 * @brief mixed_url_t structure
 */
typedef struct {
    char *data;
    uint64_t pos;
} mixed_url_t;

typedef enum 
{
	HTTP_CLIENT_RST_STATE = 104,
} http_status_type_t;

/**
 * @brief mixed play task init
 */
void mixed_play_task_init();

#ifdef __cplusplus
}
#endif

#endif
