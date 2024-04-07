/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef __AUDIO_MIXER_PIPELINE_H__
#define __AUDIO_MIXER_PIPELINE_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define MIXER_PIPELINE_TASK_STACK_SIZE (5 * 1024)
#define MIXER_PIPELINE_TASK_CORE       (0)
#define MIXER_PIPELINE_TASK_PRIO       (5)

#define AUDIO_MIXER_PIPELINE_CFG_DEFAULT() { \
    .mixer = NULL,                           \
    .url = NULL,                             \
    .slot = 0,                               \
    .resample_rate = 44100,                  \
    .resample_channel = 2,                   \
}

/**
 * @brief  Type definition for a handle to a mixer pipeline
 */
typedef void *mixer_pipepine_handle_t;

/**
 * @brief  Enumeration of mixer pipeline events
 */
typedef enum {
    MIXER_PIP_EVENT_INIT = 0,  /*!< Mixer pipeline initialization event */
    MIXER_PIP_EVENT_RUN,       /*!< Mixer pipeline run event */
    MIXER_PIP_EVENT_FINISH,    /*!< Mixer pipeline finish event */
    MIXER_PIP_EVENT_DESTROY,   /*!< Mixer pipeline destruction event */
} mixer_pip_event_t;

/**
 * @brief  Callback function type for mixer pipeline events
 */
typedef void (*mixer_pip_callback_t)(mixer_pip_event_t event, mixer_pipepine_handle_t mix_pip_handle, int slot, void *user_data);

/**
 * @brief  Structure representing the configuration for an audio mixer pipeline
 */
typedef struct {
    const char *url;               /*!< URI of the audio element to be added to the mixer pipeline */
    void       *mixer;             /*!< Mixer pipeline initialization event */
    int         slot;              /*!< Slot index for the mixer pipeline */
    int         resample_rate;     /*!< Resample rate for the audio */
    int         resample_channel;  /*!< Resample channel for the audio */
} audio_mixer_pip_cfg_t;

/**
 * @brief  Create a mixer pipeline
 *
 * @param[in]   config  Mixer pipeline create configuration parameters
 * @param[out]  handle  Pointer to the handle that will be assigned for the created mixer pipeline
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NO_MEM       Create mixer pipeline failed because out of memory
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t mixer_pipeline_create(audio_mixer_pip_cfg_t *config, mixer_pipepine_handle_t *handle);

/**
 * @brief  Registers an event callback for the mixer pipeline handle
 *
 * @param[in]  handle     Handle of the mixer pipeline
 * @param[in]  event_cb   Callback function to be registered
 * @param[in]  user_data  User data to be passed to the callback function
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t mixer_pipeline_regiser_event_callback(mixer_pipepine_handle_t *handle, mixer_pip_callback_t event_cb, void *user_data);

/**
 * @brief  Destroys the mixer pipeline
 *
 * @param[in]  handle  Handle of the mixer pipeline to be destroyed
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t mixer_pipeline_destroy(mixer_pipepine_handle_t handle);

/**
 * @brief  Run the mixer pipeline
 *
 * @param[in]  handle  Handle of the mixer pipeline to be run
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t mixer_pipepine_run(mixer_pipepine_handle_t handle);

/**
 * @brief  Stop the mixer pipeline
 *
 * @param[in]  handle  Handle of the mixer pipeline to be stopped
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t mixer_pipepine_stop(mixer_pipepine_handle_t handle);

/**
 * @brief  Restart the mixer pipeline with a new URI
 * 
 * @note  Restarts the mixer pipeline with a new URI after it has been stopped or finished.
 *         When playing audio in a slot for the second time, `mixer_pipepine_restart_with_uri` will start faster than `mixer_pipepine_run`.
 *
 * @param[in]  handle  Handle of the mixer pipeline to be restarted
 * @param[in]  uri     New URI for the audio element in the mixer pipeline
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_NOT_FOUND    Not found valid In-stream handle
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t mixer_pipepine_restart_with_uri(mixer_pipepine_handle_t handle, const char *uri);

/**
 * @brief  Get the raw audio element from the mixer pipeline
 *
 * @param[in]  handle  Handle to the mixer pipeline
 * @param[in]  el      Pointer to a variable where the raw audio element handle will be stored
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_Fail             Get In-stream handle fialed
 *       - ESP_ERR_INVALID_ARG  NULL pointer or invalid configuration
 */
esp_err_t mixer_pipepine_get_raw_el(mixer_pipepine_handle_t handle, audio_element_handle_t *el);

#ifdef __cplusplus
}
#endif

#endif
