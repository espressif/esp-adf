/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2019 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#include "esp_log.h"
#include "board.h"
#include "audio_mem.h"

static const char *TAG = "AUDIO_BOARD";

static int audio_board_init_flag = 0;

audio_board_handle_t audio_board_init(void)
{
    if (audio_board_init_flag) {
        ESP_LOGE(TAG, "The board has already been initialized!");
        return NULL;
    }
    audio_hal_codec_config_t audio_codec_cfg = AUDIO_BOARD_DEFAULT_CONFIG();
    audio_board_handle_t audio_board = (audio_board_handle_t) audio_calloc(1, sizeof(audio_hal_handle_t));
    AUDIO_MEM_CHECK(TAG, audio_board, return NULL);

    audio_board->audio_hal = audio_hal_init(&audio_codec_cfg, &AUDIO_CODEC_DEFAULT_HANDLE);

    audio_board_init_flag = 1;
    return audio_board;
}

esp_err_t audio_borad_deinit(audio_board_handle_t audio_board)
{
    esp_err_t ret = ESP_OK;
    ret = audio_hal_deinit(audio_board->audio_hal);
    free(audio_board);
    audio_board_init_flag = 0;
    return ret;
}
