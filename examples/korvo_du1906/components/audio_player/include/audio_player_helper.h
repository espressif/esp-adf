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

#ifndef AUDIO_PLAYER_HELPER_H
#define AUDIO_PLAYER_HELPER_H

#include "audio_player_type.h"
#include "audio_player.h"
#include "esp_audio_helper.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief To provide a function that is setup esp_audio handle by default
 *
 * @param cfg   A pointer to esp_audio_cfg_t
 *
 * @return
 *     esp_audio_handle_t : on succss
 *     NULL : other errors
 *
 */
esp_audio_handle_t setup_esp_audio_instance(esp_audio_cfg_t *cfg);

/*
 * @brief A default http player initialization function
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t default_http_player_init(void);

/*
 * @brief A default sdcard player initialization function
 *
 * @param scan_path   A pointer to path
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t default_sdcard_player_init(const char *scan_path);

/*
 * @brief A default flash music player initialization function
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t default_flash_music_player_init(void);

/*
 * @brief A default flash tone initialization function
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t default_flash_tone_player_init(void);

/*
 * @brief A default a2dp player initialization function
 *
 * @param periph   A pointer to a2dp peripheral handle
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t default_a2dp_player_init(void *periph);

/*
 * @brief A default raw player initialization function
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t default_raw_player_init(void);

/*
 * @brief Set raw element handle to audio player
 *
 * @param handle    A pointer to raw stream instance
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t ap_helper_raw_handle_set(void *handle);

/*
 * @brief Set a2dp stream handle to audio player
 *
 * @param el   A pointer to a2dp stream instance
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t ap_helper_a2dp_handle_set(void *el);

/*
 * @brief Feed data to RAW player
 *
 * @param buff              A pointer to data buffer
 * @param buffer_length     size of data buffer
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_raw_feed_data(void *buff, int buffer_length);

/*
 * @brief Set done to RAW player after feed finished
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_raw_feed_done();

/*
 * @brief Waiting for raw play finished
 *
 * @param at        A pointer to ap_ops_attr_t
 * @param para      A pointer to ap_ops_para_t
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_raw_waiting_finished(ap_ops_attr_t *at, ap_ops_para_t *para);

/*
 * @brief A default play interface for audio player
 *
 * @param at        A pointer to ap_ops_attr_t
 * @param para      A pointer to ap_ops_para_t
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_default_play(ap_ops_attr_t *at, ap_ops_para_t *para);

/*
 * @brief A default stop interface for audio player
 *
 * @param at        A pointer to ap_ops_attr_t
 * @param para      A pointer to ap_ops_para_t
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_default_stop(ap_ops_attr_t *at, ap_ops_para_t *para);

/*
 * @brief A default pause interface for audio player
 *
 * @param at        A pointer to ap_ops_attr_t
 * @param para      A pointer to ap_ops_para_t
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_default_pause(ap_ops_attr_t *at, ap_ops_para_t *para);

/*
 * @brief A default resume interface for audio player
 *
 * @param at        A pointer to ap_ops_attr_t
 * @param para      A pointer to ap_ops_para_t
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_default_resume(ap_ops_attr_t *at, ap_ops_para_t *para);

/*
 * @brief A default next interface for audio player
 *
 * @param at        A pointer to ap_ops_attr_t
 * @param para      A pointer to ap_ops_para_t
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_default_next(ap_ops_attr_t *at, ap_ops_para_t *para);

/*
 * @brief A default prev interface for audio player
 *
 * @param at        A pointer to ap_ops_attr_t
 * @param para      A pointer to ap_ops_para_t
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_default_prev(ap_ops_attr_t *at, ap_ops_para_t *para);

/*
 * @brief A default seek interface for audio player
 *
 * @param at        A pointer to ap_ops_attr_t
 * @param para      A pointer to ap_ops_para_t
 *
 * @return
 *     ESP_ERR_AUDIO_NO_ERROR : on succss
 *     ESP_FAIL : other errors
 *
 */
audio_err_t audio_player_helper_default_seek(ap_ops_attr_t *at, ap_ops_para_t *para);

#ifdef __cplusplus
}
#endif

#endif //
