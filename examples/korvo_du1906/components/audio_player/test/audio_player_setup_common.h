/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef AUDIO_PLAYER_SETUP_COMMON
#define AUDIO_PLAYER_SETUP_COMMON

void setup_wifi();
void teardown_wifi();
void setup_sdcard();
void teardown_sdcard();


audio_element_handle_t create_enc_filter(int source_rate,
        int source_channel,
        int dest_rate,
        int dest_channel);
audio_element_handle_t create_dec_filter(int source_rate,
        int source_channel,
        int dest_rate,
        int dest_channel);

esp_err_t init_audio_player(QueueHandle_t que, esp_audio_prefer_t type);
esp_err_t deinit_audio_player();
void clear_queue_events(QueueHandle_t que);
esp_audio_status_t audio_player_status_check(QueueHandle_t que, int ticks_ms);

void raw_read(const char *p);
void raw_read_save_file(const char *p);
void raw_write(const char *p);
void raw_stop(void);

void show_task_list();

#endif
