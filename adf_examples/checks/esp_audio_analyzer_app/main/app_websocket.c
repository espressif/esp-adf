/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_https_server.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "esp_gmf_data_bus.h"
#include "esp_fourcc.h"
#include "app_websocket.h"
#include "app_audio.h"

static const char *TAG = "APP_WS";

#define WS_EVENT_SHUTDOWN_COMPLETE          BIT0
#define WS_EVENT_RECORD_TASK_STOP           BIT1
#define WS_EVENT_RECORD_TASK_EXIT           BIT2
#define WS_EVENT_PLAYBACK_TASK_STOP         BIT3
#define WS_EVENT_PLAYBACK_TASK_EXIT         BIT4
#define WS_EVENT_PLAYBACK_BUFFER_TASK_STOP  BIT5
#define WS_EVENT_PLAYBACK_BUFFER_TASK_EXIT  BIT6

/* Generic resource and protocol settings */
#define WS_AUDIO_BUFFER_SIZE      (1024 * 1024)  /*!< Buffer for audio data transfer */
#define WS_MIN_AUDIO_BUFFER_SIZE  (16 * 1024)    /*!< Minimum buffer size for basic functionality */
#define WS_MAX_PACKET_SIZE        (16 * 1024)    /*!< Maximum packet size for WebSocket frames */
#define WS_IPV4_STR_LEN           (16)           /*!< Size of IPv4 string */
#define WS_CLIENT_FD_LIST_SIZE    (16)           /*!< Size of client file descriptor list */
#define WS_CHANNEL_TYPE_STR_LEN   (16)           /*!< Size of channel type string */
#define WS_CFG_VALUE_NOT_SET      (-1000)        /*!< Configuration value not set */

/* Task-related settings */
#define WS_PRIORITY_HTTPD             (6)     /*!< Priority for HTTP server task */
#define WS_PRIORITY_RECORD_SEND       (7)     /*!< Priority for record send task */
#define WS_PRIORITY_PLAYBACK_BUFFER   (8)     /*!< Priority for playback buffer task */
#define WS_PRIORITY_PLAYBACK_CONTROL  (4)     /*!< Priority for playback control task */
#define WS_TASK_WAIT_TIMEOUT_MS       (2000)  /*!< Timeout for task wait */
#define WS_TASK_STACK_SIZE            (4096)  /*!< Size of task stack */
#define WS_POLL_DELAY_MS              (10)    /*!< Delay for polling loop */
#define WS_BUFFER_MONITOR_WAIT_MS     (50)    /*!< Timeout for monitor wait */
#define WS_BUFFER_MONITOR_IDLE_MS     (100)   /*!< Delay for monitor idle */

/* Data bus timeout settings */
#define WS_DB_READ_TIMEOUT_MS     (100)  /*!< Timeout for DB read */
#define WS_DB_RELEASE_TIMEOUT_MS  (10)   /*!< Timeout for DB release */

/* Playback buffer threshold settings */
#define WS_PLAYBACK_READY_THRESHOLD  (75)  /*!< Threshold for playback ready */
#define WS_PLAYBACK_PAUSE_THRESHOLD  (50)  /*!< Threshold for playback pause */

typedef enum {
    WS_STATE_IDLE = 0,
    WS_STATE_RECORDING,
    WS_STATE_PLAYING,
    WS_STATE_DUPLEX,
} ws_audio_state_t;

typedef enum {
    BUF_STATUS_UNKNOWN = 0,
    BUF_STATUS_READY,
    BUF_STATUS_PAUSE,
} ws_buf_status_t;

typedef struct {
    httpd_handle_t          server_handle;
    app_websocket_config_t  config;
    ws_audio_state_t        state;
    TaskHandle_t            record_task;                                /*!< Task to send recording data */
    TaskHandle_t            playback_task;                              /*!< Task to control playback */
    TaskHandle_t            playback_buffer_task;                       /*!< Task to monitor playback buffer */
    EventGroupHandle_t      event_group;                                /*!< Event group for task lifecycle */
    app_audio_format_t      audio_format;                               /*!< Current audio format */
    bool                    is_play_local_file;                         /*!< true if playing from local file, false if playing from WS data packets */
    bool                    running;                                    /*!< true if the WebSocket server is running, false if not */
    int                     mic_gain;                                   /*!< Microphone gain (0-100) */
    int                     volume;                                     /*!< Playback volume (0-100) */
    char                    channel_type[WS_CHANNEL_TYPE_STR_LEN + 1];  /*!< Channel type string (e.g., "MMNR" for Mic, Mic, Not used, Reference) */
#if CONFIG_IDF_TARGET_ESP32
    uint8_t  *send_buf;  /*!< Internal RAM buffer for WiFi TX on ESP32 */
#endif  /* CONFIG_IDF_TARGET_ESP32 */
} websocket_context_t;

static websocket_context_t g_ws_ctx = {0};

extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
extern const unsigned char servercert_end[] asm("_binary_servercert_pem_end");
extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
extern const unsigned char prvtkey_pem_end[] asm("_binary_prvtkey_pem_end");

static esp_err_t handle_reset_cmd(void);

static size_t calculate_audio_buffer_size(void)
{
    size_t free_size = 0;

#ifdef CONFIG_SPIRAM_BOOT_INIT
    free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGD(TAG, "PSRAM enabled, free size: %zu bytes", free_size);
#else
    free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGD(TAG, "Using internal RAM, free size: %zu bytes", free_size);
#endif  /* CONFIG_SPIRAM_BOOT_INIT */

    size_t buffer_size;

    if (free_size >= (WS_AUDIO_BUFFER_SIZE << 1)) {
        buffer_size = WS_AUDIO_BUFFER_SIZE;
        ESP_LOGD(TAG, "Sufficient memory, using max buffer size: %zu bytes", buffer_size);
    } else {
        buffer_size = free_size / 3;

        if (buffer_size < WS_MIN_AUDIO_BUFFER_SIZE) {
            buffer_size = WS_MIN_AUDIO_BUFFER_SIZE;
            ESP_LOGD(TAG, "Very low memory, using minimum buffer size: %zu bytes", buffer_size);
        } else {
            ESP_LOGD(TAG, "Limited memory, using 1/3 of free size: %zu bytes", buffer_size);
        }
    }

    return buffer_size;
}

static void send_status_to_clients(const char *status, const char *message)
{
    if (!g_ws_ctx.server_handle) {
        ESP_LOGW(TAG, "Server handle not available, cannot send status");
        return;
    }

    if (!status) {
        ESP_LOGE(TAG, "Status string is NULL");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        ESP_LOGE(TAG, "Failed to create JSON object (out of memory?)");
        return;
    }

    if (!cJSON_AddStringToObject(root, "status", status)) {
        ESP_LOGE(TAG, "Failed to add status to JSON");
        cJSON_Delete(root);
        return;
    }

    if (message && !cJSON_AddStringToObject(root, "message", message)) {
        ESP_LOGE(TAG, "Failed to add message to JSON");
        cJSON_Delete(root);
        return;
    }

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        httpd_ws_frame_t ws_pkt = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t *)json_str,
            .len = strlen(json_str),
        };

        size_t fds = g_ws_ctx.config.max_clients;
        int *client_fds = calloc(fds, sizeof(int));
        if (client_fds) {
            esp_err_t ret = httpd_get_client_list(g_ws_ctx.server_handle, &fds, client_fds);
            if (ret == ESP_OK) {
                for (size_t i = 0; i < fds; i++) {
                    if (httpd_ws_get_fd_info(g_ws_ctx.server_handle, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                        esp_err_t send_ret = httpd_ws_send_frame_async(g_ws_ctx.server_handle, client_fds[i], &ws_pkt);
                        if (send_ret != ESP_OK) {
                            ESP_LOGW(TAG, "Failed to send to client fd %d: %s",
                                     client_fds[i], esp_err_to_name(send_ret));
                        }
                    }
                }
            } else {
                ESP_LOGE(TAG, "Failed to get client list: %s", esp_err_to_name(ret));
            }
            free(client_fds);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for client FDs");
        }
        free(json_str);
    } else {
        ESP_LOGE(TAG, "Failed to print JSON string");
    }

    cJSON_Delete(root);
}

static esp_err_t send_audio_to_clients(const uint8_t *data, size_t len)
{
    if (!g_ws_ctx.server_handle || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_IDF_TARGET_ESP32
    if (!g_ws_ctx.send_buf || len > WS_MAX_PACKET_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(g_ws_ctx.send_buf, data, len);
    uint8_t *payload = g_ws_ctx.send_buf;
#else
    uint8_t *payload = (uint8_t *)data;
#endif  /* CONFIG_IDF_TARGET_ESP32 */

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = payload,
        .len = len,
    };

    int client_fds[WS_CLIENT_FD_LIST_SIZE];
    size_t fds = g_ws_ctx.config.max_clients;

    esp_err_t ret = ESP_OK;
    if (httpd_get_client_list(g_ws_ctx.server_handle, &fds, client_fds) == ESP_OK) {
        for (size_t i = 0; i < fds; i++) {
            if (httpd_ws_get_fd_info(g_ws_ctx.server_handle, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                esp_err_t send_ret = httpd_ws_send_frame_async(g_ws_ctx.server_handle, client_fds[i], &ws_pkt);
                if (send_ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to send audio to client fd %d", client_fds[i]);
                    ret = ESP_FAIL;
                }
            }
        }
    }

    return ret;
}

static void stop_task_and_wait(TaskHandle_t *task_handle,
                               EventBits_t stop_bit, EventBits_t exit_bit,
                               const char *task_name)
{
    if (!*task_handle) {
        return;
    }
    ESP_LOGI(TAG, "Signaling %s to stop...", task_name);
    xEventGroupSetBits(g_ws_ctx.event_group, stop_bit);

    EventBits_t bits = xEventGroupWaitBits(g_ws_ctx.event_group, exit_bit,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(WS_TASK_WAIT_TIMEOUT_MS));
    if (!(bits & exit_bit)) {
        ESP_LOGW(TAG, "%s did not exit cleanly", task_name);
    }
}

static void stop_playback_tasks(void)
{
    stop_task_and_wait(&g_ws_ctx.playback_buffer_task,
                       WS_EVENT_PLAYBACK_BUFFER_TASK_STOP,
                       WS_EVENT_PLAYBACK_BUFFER_TASK_EXIT,
                       "playback buffer task");
    stop_task_and_wait(&g_ws_ctx.playback_task,
                       WS_EVENT_PLAYBACK_TASK_STOP,
                       WS_EVENT_PLAYBACK_TASK_EXIT,
                       "playback control task");
}

static void audio_event_callback(app_audio_event_t event, void *user_ctx)
{
    switch (event) {
        case APP_AUDIO_EVENT_RECORD_START:
            ESP_LOGI(TAG, "Recording started");
            break;
        case APP_AUDIO_EVENT_RECORD_STOP:
            ESP_LOGI(TAG, "Recording stopped");
            break;
        case APP_AUDIO_EVENT_PLAY_START:
            ESP_LOGI(TAG, "Playback started");
            break;
        case APP_AUDIO_EVENT_PLAY_STOP:
            ESP_LOGI(TAG, "Playback stopped");
            break;
        case APP_AUDIO_EVENT_PLAY_FINISHED:
            ESP_LOGI(TAG, "Playback finished");
            xEventGroupSetBits(g_ws_ctx.event_group, WS_EVENT_PLAYBACK_TASK_STOP);
            break;
        case APP_AUDIO_EVENT_ERROR:
            ESP_LOGE(TAG, "Audio error");
            send_status_to_clients("error", "Audio processing error");
            break;
        default:
            break;
    }
}

static void record_send_task(void *arg)
{
    ESP_LOGI(TAG, "Record send task started");
    uint8_t *buffer = heap_caps_malloc(WS_MAX_PACKET_SIZE, MALLOC_CAP_INTERNAL);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate record buffer (size: %d bytes)", WS_MAX_PACKET_SIZE);
        send_status_to_clients("error", "Out of memory");
        xEventGroupSetBits(g_ws_ctx.event_group, WS_EVENT_RECORD_TASK_EXIT);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Record buffer allocated: %d bytes", WS_MAX_PACKET_SIZE);

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(g_ws_ctx.event_group,
                                               WS_EVENT_RECORD_TASK_STOP,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(WS_POLL_DELAY_MS));

        if (bits & WS_EVENT_RECORD_TASK_STOP) {
            ESP_LOGI(TAG, "Record task received stop signal");
            break;
        }

        if (g_ws_ctx.state != WS_STATE_RECORDING && g_ws_ctx.state != WS_STATE_DUPLEX) {
            vTaskDelay(pdMS_TO_TICKS(WS_POLL_DELAY_MS));
            continue;
        }

        esp_gmf_db_handle_t record_buffer = app_audio_get_record_buffer();
        if (!record_buffer) {
            vTaskDelay(pdMS_TO_TICKS(WS_POLL_DELAY_MS));
            continue;
        }

        esp_gmf_data_bus_block_t blk = {0};
        blk.buf = buffer;
        blk.buf_length = WS_MAX_PACKET_SIZE;

        int ret = esp_gmf_db_acquire_read(record_buffer, &blk, WS_MAX_PACKET_SIZE, pdMS_TO_TICKS(WS_DB_READ_TIMEOUT_MS));
        if (ret == 0 && blk.valid_size > 0) {
            esp_err_t send_ret = send_audio_to_clients(blk.buf, blk.valid_size);
            if (send_ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send audio data to clients");
            }
            esp_gmf_db_release_read(record_buffer, &blk, 0);
        } else if (ret != ESP_GMF_IO_TIMEOUT) {
            ESP_LOGW(TAG, "Failed to read from record buffer: 0x%x", ret);
        }
    }

    free(buffer);
    ESP_LOGI(TAG, "Record send task exiting");
    g_ws_ctx.record_task = NULL;
    xEventGroupSetBits(g_ws_ctx.event_group, WS_EVENT_RECORD_TASK_EXIT);
    vTaskDelete(NULL);
}

static void playback_control_task(void *arg)
{
    ESP_LOGI(TAG, "Playback control task started");

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(g_ws_ctx.event_group,
                                               WS_EVENT_PLAYBACK_TASK_STOP,
                                               pdTRUE,
                                               pdFALSE,
                                               portMAX_DELAY);

        if (bits & WS_EVENT_PLAYBACK_TASK_STOP) {
            ESP_LOGI(TAG, "Playback control task received stop signal");

            if (g_ws_ctx.is_play_local_file) {
                app_audio_playback_stop();
            }

            if (g_ws_ctx.state == WS_STATE_PLAYING) {
                g_ws_ctx.state = WS_STATE_IDLE;
                send_status_to_clients("playing_stopped", NULL);
            } else if (g_ws_ctx.state == WS_STATE_DUPLEX) {
                g_ws_ctx.state = WS_STATE_RECORDING;
                send_status_to_clients("playing_stopped", NULL);
                ESP_LOGI(TAG, "Duplex playback stopped, recording continues");
            }
            break;
        }
    }

    ESP_LOGI(TAG, "Playback control task exiting");
    g_ws_ctx.playback_task = NULL;
    xEventGroupSetBits(g_ws_ctx.event_group, WS_EVENT_PLAYBACK_TASK_EXIT);
    vTaskDelete(NULL);
}

static void playback_buffer_task(void *arg)
{
    ESP_LOGI(TAG, "Playback buffer task started");

    ws_buf_status_t last_status = BUF_STATUS_UNKNOWN;

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(g_ws_ctx.event_group,
                                               WS_EVENT_PLAYBACK_BUFFER_TASK_STOP,
                                               pdTRUE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(WS_BUFFER_MONITOR_WAIT_MS));

        if (bits & WS_EVENT_PLAYBACK_BUFFER_TASK_STOP) {
            ESP_LOGI(TAG, "Playback buffer task received stop signal");
            break;
        }

        if (g_ws_ctx.state != WS_STATE_PLAYING && g_ws_ctx.state != WS_STATE_DUPLEX) {
            vTaskDelay(pdMS_TO_TICKS(WS_BUFFER_MONITOR_IDLE_MS));
            continue;
        }

        if (g_ws_ctx.is_play_local_file) {
            vTaskDelay(pdMS_TO_TICKS(WS_BUFFER_MONITOR_IDLE_MS));
            continue;
        }

        uint8_t free_percent = 0;
        app_audio_get_playback_buffer_free_percent(&free_percent);

        if (free_percent > WS_PLAYBACK_READY_THRESHOLD) {
            if (last_status != BUF_STATUS_READY) {
                send_status_to_clients("receive_ready", NULL);
                ESP_LOGI(TAG, "Playback buffer is high: %d%%, sending receive_ready", free_percent);
                last_status = BUF_STATUS_READY;
            }
        } else if (free_percent < WS_PLAYBACK_PAUSE_THRESHOLD) {
            if (last_status != BUF_STATUS_PAUSE) {
                send_status_to_clients("receive_pause", NULL);
                ESP_LOGI(TAG, "Playback buffer is low: %d%%, sending receive_pause", free_percent);
                last_status = BUF_STATUS_PAUSE;
            }
        }
    }

    ESP_LOGI(TAG, "Playback buffer task exiting");
    g_ws_ctx.playback_buffer_task = NULL;
    xEventGroupSetBits(g_ws_ctx.event_group, WS_EVENT_PLAYBACK_BUFFER_TASK_EXIT);
    vTaskDelete(NULL);
}

static esp_err_t handle_configure_cmd(cJSON *params, int sockfd)
{
    if (g_ws_ctx.state != WS_STATE_IDLE) {
        send_status_to_clients("error", "Cannot configure while audio is active");
        return ESP_FAIL;
    }

    if (!params) {
        send_status_to_clients("error", "Missing 'params' in configure command");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *rate = cJSON_GetObjectItem(params, "sample_rate");
    cJSON *bits = cJSON_GetObjectItem(params, "bits_per_sample");
    cJSON *ch = cJSON_GetObjectItem(params, "channels");
    if (!cJSON_IsNumber(rate) || !cJSON_IsNumber(bits) || !cJSON_IsNumber(ch)) {
        send_status_to_clients("error", "Invalid configuration parameters");
        return ESP_ERR_INVALID_ARG;
    }

    int sample_rate = rate->valueint;
    int bits_per_sample = bits->valueint;
    int channels = ch->valueint;
    if (sample_rate < 8000 || sample_rate > 48000) {
        ESP_LOGE(TAG, "Invalid sample rate: %d (valid: 8000-48000)", sample_rate);
        send_status_to_clients("error", "Invalid sample rate");
        return ESP_ERR_INVALID_ARG;
    }

    if (bits_per_sample != 16 && bits_per_sample != 24 && bits_per_sample != 32) {
        ESP_LOGE(TAG, "Invalid bits per sample: %d (valid: 16, 24, 32)", bits_per_sample);
        send_status_to_clients("error", "Invalid bits per sample");
        return ESP_ERR_INVALID_ARG;
    }

    if (channels < 1 || channels > 4) {
        ESP_LOGE(TAG, "Invalid channels: %d (valid: 1-4)", channels);
        send_status_to_clients("error", "Invalid channels");
        return ESP_ERR_INVALID_ARG;
    }

    g_ws_ctx.audio_format.sample_rate = sample_rate;
    g_ws_ctx.audio_format.bits_per_sample = bits_per_sample;
    g_ws_ctx.audio_format.channels = channels;

    cJSON *mic_gain_obj = cJSON_GetObjectItem(params, "mic_gain");
    if (cJSON_IsNumber(mic_gain_obj)) {
        int mic_gain = mic_gain_obj->valueint;
        if (mic_gain >= -120 && mic_gain <= 40) {
            esp_err_t ret = app_audio_set_mic_gain(mic_gain);
            if (ret == ESP_OK) {
                g_ws_ctx.mic_gain = mic_gain;
                ESP_LOGI(TAG, "Mic gain set to %d dB", mic_gain);
            } else {
                ESP_LOGW(TAG, "Failed to set mic gain to %d dB: %s", mic_gain, esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "Invalid mic_gain: %d (valid: -120 to 40 dB), ignoring", mic_gain);
        }
    }

    cJSON *volume_obj = cJSON_GetObjectItem(params, "volume");
    if (cJSON_IsNumber(volume_obj)) {
        int volume = volume_obj->valueint;
        if (volume >= 0 && volume <= 100) {
            esp_err_t ret = app_audio_set_volume(volume);
            if (ret == ESP_OK) {
                g_ws_ctx.volume = volume;
                ESP_LOGI(TAG, "Volume set to %d", volume);
            } else {
                ESP_LOGW(TAG, "Failed to set volume to %d: %s", volume, esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "Invalid volume: %d (valid: 0-100), ignoring", volume);
        }
    }

    cJSON *channel_type_obj = cJSON_GetObjectItem(params, "channel_type");
    if (cJSON_IsString(channel_type_obj)) {
        const char *channel_type = channel_type_obj->valuestring;
        if (channel_type && strlen(channel_type) < sizeof(g_ws_ctx.channel_type)) {
            strncpy(g_ws_ctx.channel_type, channel_type, sizeof(g_ws_ctx.channel_type) - 1);
            g_ws_ctx.channel_type[sizeof(g_ws_ctx.channel_type) - 1] = '\0';
            ESP_LOGI(TAG, "Channel type set to: %s", g_ws_ctx.channel_type);
        } else {
            ESP_LOGW(TAG, "Invalid channel_type string (too long or NULL), ignoring");
        }
    }

    ESP_LOGI(TAG, "Configured: %uHz, %dbit, %dch, mic_gain=%d, volume=%d, channel_type=%s",
             (unsigned)g_ws_ctx.audio_format.sample_rate,
             g_ws_ctx.audio_format.bits_per_sample,
             g_ws_ctx.audio_format.channels,
             g_ws_ctx.mic_gain,
             g_ws_ctx.volume,
             g_ws_ctx.channel_type[0] ? g_ws_ctx.channel_type : "default");

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "status", "configured");
    cJSON *res_params = cJSON_CreateObject();
    cJSON_AddNumberToObject(res_params, "sample_rate", g_ws_ctx.audio_format.sample_rate);
    cJSON_AddNumberToObject(res_params, "bits_per_sample", g_ws_ctx.audio_format.bits_per_sample);
    cJSON_AddNumberToObject(res_params, "channels", g_ws_ctx.audio_format.channels);
    if (g_ws_ctx.mic_gain != WS_CFG_VALUE_NOT_SET) {
        cJSON_AddNumberToObject(res_params, "mic_gain", g_ws_ctx.mic_gain);
    }
    if (g_ws_ctx.volume != WS_CFG_VALUE_NOT_SET) {
        cJSON_AddNumberToObject(res_params, "volume", g_ws_ctx.volume);
    }
    if (g_ws_ctx.channel_type[0]) {
        cJSON_AddStringToObject(res_params, "channel_type", g_ws_ctx.channel_type);
    }
    cJSON_AddItemToObject(res, "params", res_params);

    char *json_str = cJSON_PrintUnformatted(res);
    if (json_str) {
        httpd_ws_frame_t ws_pkt = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t *)json_str,
            .len = strlen(json_str),
        };
        httpd_ws_send_frame_async(g_ws_ctx.server_handle, sockfd, &ws_pkt);
        free(json_str);
    }
    cJSON_Delete(res);

    return ESP_OK;
}

static esp_err_t handle_start_recording_cmd(cJSON *params)
{
    if (g_ws_ctx.state != WS_STATE_IDLE) {
        send_status_to_clients("error", "Recording already active or playing");
        return ESP_FAIL;
    }

    app_audio_record_config_t rec_cfg = APP_AUDIO_RECORD_CONFIG_DEFAULT();
    rec_cfg.format = g_ws_ctx.audio_format;
    rec_cfg.event_callback = audio_event_callback;
    rec_cfg.user_ctx = NULL;

    cJSON *file_path = NULL;
    if (params) {
        file_path = cJSON_GetObjectItem(params, "file_path");
    }
    if (cJSON_IsString(file_path)) {
        rec_cfg.file_path = file_path->valuestring;
        rec_cfg.buffer_size = 0;
        ESP_LOGI(TAG, "Recording to file: %s", rec_cfg.file_path);
    } else {
        rec_cfg.file_path = NULL;
        rec_cfg.buffer_size = calculate_audio_buffer_size();
        ESP_LOGI(TAG, "Recording to ringbuffer for WebSocket, buffer size: %lu bytes", (unsigned long)rec_cfg.buffer_size);
    }

    esp_err_t ret = app_audio_record_start(&rec_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start recording: %d", ret);
        send_status_to_clients("error", "Failed to start recording");
        return ret;
    }

    if (rec_cfg.file_path == NULL) {
        xEventGroupClearBits(g_ws_ctx.event_group,
                             WS_EVENT_RECORD_TASK_STOP | WS_EVENT_RECORD_TASK_EXIT);
        ESP_LOGI(TAG, "Cleared record event bits before creating new task");

        BaseType_t task_ret = xTaskCreate(
            record_send_task,
            "ws_rec_send",
            WS_TASK_STACK_SIZE,
            NULL,
            WS_PRIORITY_RECORD_SEND,
            &g_ws_ctx.record_task);

        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create record send task");
            app_audio_record_stop();
            send_status_to_clients("error", "Failed to create record send task");
            return ESP_FAIL;
        }
    }

    g_ws_ctx.state = WS_STATE_RECORDING;
    send_status_to_clients("recording_started", NULL);
    ESP_LOGI(TAG, "Recording started");

    return ESP_OK;
}

static esp_err_t handle_stop_recording_cmd(void)
{
    if (g_ws_ctx.state != WS_STATE_RECORDING && g_ws_ctx.state != WS_STATE_DUPLEX) {
        send_status_to_clients("error", "Not recording");
        return ESP_FAIL;
    }

    stop_task_and_wait(&g_ws_ctx.record_task,
                       WS_EVENT_RECORD_TASK_STOP,
                       WS_EVENT_RECORD_TASK_EXIT,
                       "record task");
    app_audio_record_stop();

    if (g_ws_ctx.state == WS_STATE_RECORDING) {
        g_ws_ctx.state = WS_STATE_IDLE;
    } else if (g_ws_ctx.state == WS_STATE_DUPLEX) {
        g_ws_ctx.state = WS_STATE_PLAYING;
    }

    send_status_to_clients("recording_stopped", NULL);
    ESP_LOGI(TAG, "Recording stopped");

    return ESP_OK;
}

static esp_err_t handle_start_playback_cmd(cJSON *params)
{
    if (g_ws_ctx.state != WS_STATE_IDLE) {
        send_status_to_clients("error", "Playback already active or recording");
        return ESP_FAIL;
    }

    app_audio_playback_config_t play_cfg = APP_AUDIO_PLAYBACK_CONFIG_DEFAULT();
    play_cfg.format = g_ws_ctx.audio_format;

    cJSON *file_path = NULL;
    if (params) {
        file_path = cJSON_GetObjectItem(params, "file_path");
    }

    if (cJSON_IsString(file_path)) {
        play_cfg.file_path = file_path->valuestring;
        play_cfg.buffer_size = 0;
        g_ws_ctx.is_play_local_file = true;
        ESP_LOGI(TAG, "Playing from file: %s", play_cfg.file_path);
    } else {
        play_cfg.file_path = NULL;
        play_cfg.buffer_size = calculate_audio_buffer_size();
        play_cfg.decode_type = ESP_FOURCC_WAV;
        g_ws_ctx.is_play_local_file = false;
        ESP_LOGI(TAG, "Playing from WebSocket ringbuffer, buffer size: %lu bytes", (unsigned long)play_cfg.buffer_size);
    }

    play_cfg.event_callback = audio_event_callback;
    play_cfg.user_ctx = NULL;
    play_cfg.loop = true;

    esp_err_t ret = app_audio_playback_start(&play_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start playback: %d", ret);
        send_status_to_clients("error", "Failed to start playback");
        return ret;
    }

    if (g_ws_ctx.is_play_local_file) {
        xEventGroupClearBits(g_ws_ctx.event_group,
                             WS_EVENT_PLAYBACK_TASK_STOP | WS_EVENT_PLAYBACK_TASK_EXIT);
        ESP_LOGI(TAG, "Cleared playback event bits before creating new task");

        BaseType_t task_ret = xTaskCreate(
            playback_control_task,
            "ws_play_ctrl",
            WS_TASK_STACK_SIZE,
            NULL,
            WS_PRIORITY_PLAYBACK_CONTROL,
            &g_ws_ctx.playback_task);

        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create playback control task");
            app_audio_playback_stop();
            send_status_to_clients("error", "Failed to create playback control task");
            return ESP_FAIL;
        }
    }

    if (!g_ws_ctx.is_play_local_file) {
        xEventGroupClearBits(g_ws_ctx.event_group,
                             WS_EVENT_PLAYBACK_BUFFER_TASK_STOP | WS_EVENT_PLAYBACK_BUFFER_TASK_EXIT);
        ESP_LOGI(TAG, "Cleared playback buffer event bits before creating new task");

        BaseType_t task_ret = xTaskCreate(
            playback_buffer_task,
            "ws_play_back_buffer",
            WS_TASK_STACK_SIZE,
            NULL,
            WS_PRIORITY_PLAYBACK_BUFFER,
            &g_ws_ctx.playback_buffer_task);

        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create playback buffer task");
            app_audio_playback_stop();
            send_status_to_clients("error", "Failed to create playback buffer task");
            return ESP_FAIL;
        }
    }

    g_ws_ctx.state = WS_STATE_PLAYING;
    send_status_to_clients("playing_started", NULL);
    ESP_LOGI(TAG, "Playback started");

    return ESP_OK;
}

static esp_err_t handle_stop_playback_cmd(void)
{
    if (g_ws_ctx.state != WS_STATE_PLAYING && g_ws_ctx.state != WS_STATE_DUPLEX) {
        send_status_to_clients("error", "Not playing");
        return ESP_FAIL;
    }

    app_audio_playback_stop();
    stop_playback_tasks();

    if (g_ws_ctx.state == WS_STATE_PLAYING) {
        g_ws_ctx.state = WS_STATE_IDLE;
    } else if (g_ws_ctx.state == WS_STATE_DUPLEX) {
        g_ws_ctx.state = WS_STATE_RECORDING;
    }
    send_status_to_clients("playing_stopped", NULL);

    ESP_LOGI(TAG, "Playback stopped");
    return ESP_OK;
}

static esp_err_t handle_start_duplex_cmd(cJSON *params)
{
    if (g_ws_ctx.state != WS_STATE_IDLE) {
        send_status_to_clients("error", "Cannot start duplex: already active");
        return ESP_FAIL;
    }

    cJSON *record_file_path = NULL;
    cJSON *play_file_path = NULL;
    cJSON *enable_afe = NULL;
    if (params) {
        record_file_path = cJSON_GetObjectItem(params, "record_file_path");
        play_file_path = cJSON_GetObjectItem(params, "play_file_path");
        enable_afe = cJSON_GetObjectItem(params, "enable_afe");
    }

    app_audio_record_config_t rec_cfg = APP_AUDIO_RECORD_CONFIG_DEFAULT();
    rec_cfg.format = g_ws_ctx.audio_format;
    rec_cfg.event_callback = audio_event_callback;
    rec_cfg.user_ctx = NULL;
    rec_cfg.enable_afe = cJSON_IsBool(enable_afe) ? cJSON_IsTrue(enable_afe) : false;

    if (rec_cfg.enable_afe) {
#ifndef CONFIG_GMF_AI_AUDIO_INIT_AFE
        ESP_LOGE(TAG, "enable_afe requested but firmware was built without GMF AFE (GMF_AI_AUDIO_INIT_AFE)");
        send_status_to_clients("error", "AFE not available in this firmware");
        return ESP_ERR_NOT_SUPPORTED;
#else
        const char *ch_allocation = CONFIG_GMF_AI_AUDIO_AFE_CH_ALLOCATION;
        if (ch_allocation == NULL || strlen(ch_allocation) == 0) {
            ESP_LOGE(TAG, "AFE is enabled but CONFIG_GMF_AI_AUDIO_AFE_CH_ALLOCATION is not configured");
            send_status_to_clients("error", "AFE enabled but channel allocation not configured");
            return ESP_ERR_INVALID_ARG;
        }

        size_t ch_allocation_len = strlen(ch_allocation);
        if (ch_allocation_len != (size_t)g_ws_ctx.audio_format.channels) {
            ESP_LOGE(TAG, "Channel allocation (%s) length (%zu) does not match audio format channels (%d)",
                     ch_allocation, ch_allocation_len, g_ws_ctx.audio_format.channels);
            send_status_to_clients("error", "Channel allocation mismatch");
            return ESP_ERR_INVALID_ARG;
        }

        ESP_LOGI(TAG, "AFE channel allocation check passed: %s (channels: %d)",
                 ch_allocation, g_ws_ctx.audio_format.channels);
#endif  /* CONFIG_GMF_AI_AUDIO_INIT_AFE */
    }

    if (cJSON_IsString(record_file_path)) {
        rec_cfg.file_path = record_file_path->valuestring;
        rec_cfg.buffer_size = 0;
        ESP_LOGI(TAG, "Duplex: recording to file: %s, AFE: %s",
                 rec_cfg.file_path, rec_cfg.enable_afe ? "enabled" : "disabled");
    } else {
        rec_cfg.file_path = NULL;
        rec_cfg.buffer_size = calculate_audio_buffer_size();
        ESP_LOGI(TAG, "Duplex: recording to ringbuffer (buffer: %lu bytes), AFE: %s",
                 (unsigned long)rec_cfg.buffer_size, rec_cfg.enable_afe ? "enabled" : "disabled");
    }

    send_status_to_clients("duplex_started", rec_cfg.enable_afe ? "afe_enabled" : "afe_disabled");

    esp_err_t ret = app_audio_record_start(&rec_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start recording in duplex mode: %d", ret);
        send_status_to_clients("error", "Failed to start recording");
        return ret;
    }

    if (rec_cfg.file_path == NULL) {
        xEventGroupClearBits(g_ws_ctx.event_group,
                             WS_EVENT_RECORD_TASK_STOP | WS_EVENT_RECORD_TASK_EXIT);
        ESP_LOGI(TAG, "Cleared record event bits before creating new task");

        BaseType_t task_ret = xTaskCreate(
            record_send_task,
            "ws_rec_send",
            WS_TASK_STACK_SIZE,
            NULL,
            WS_PRIORITY_RECORD_SEND,
            &g_ws_ctx.record_task);

        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create record send task");
            app_audio_record_stop();
            send_status_to_clients("error", "Failed to create record send task");
            return ESP_FAIL;
        }
    }

    app_audio_playback_config_t play_cfg = APP_AUDIO_PLAYBACK_CONFIG_DEFAULT();
    play_cfg.format = g_ws_ctx.audio_format;
    play_cfg.event_callback = audio_event_callback;
    play_cfg.user_ctx = NULL;
    play_cfg.loop = true;

    if (cJSON_IsString(play_file_path)) {
        play_cfg.file_path = play_file_path->valuestring;
        play_cfg.buffer_size = 0;
        g_ws_ctx.is_play_local_file = true;
        ESP_LOGI(TAG, "Duplex: playing from file: %s", play_cfg.file_path);
    } else {
        play_cfg.file_path = NULL;
        play_cfg.buffer_size = calculate_audio_buffer_size();
        play_cfg.decode_type = ESP_FOURCC_WAV;
        g_ws_ctx.is_play_local_file = false;
        ESP_LOGI(TAG, "Duplex: playing from WebSocket ringbuffer (buffer: %lu bytes)", (unsigned long)play_cfg.buffer_size);
    }

    ret = app_audio_playback_start(&play_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start playback: %d", ret);
        handle_stop_recording_cmd();
        send_status_to_clients("error", "Failed to start duplex mode");
        return ret;
    }

    if (g_ws_ctx.is_play_local_file) {
        xEventGroupClearBits(g_ws_ctx.event_group,
                             WS_EVENT_PLAYBACK_TASK_STOP | WS_EVENT_PLAYBACK_TASK_EXIT);
        ESP_LOGI(TAG, "Cleared playback event bits before creating new task");

        BaseType_t task_ret = xTaskCreate(
            playback_control_task,
            "ws_play_ctrl",
            WS_TASK_STACK_SIZE,
            NULL,
            WS_PRIORITY_PLAYBACK_CONTROL,
            &g_ws_ctx.playback_task);

        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create playback control task");
            app_audio_playback_stop();
            handle_stop_recording_cmd();
            send_status_to_clients("error", "Failed to create playback control task");
            return ESP_FAIL;
        }
    }

    if (!g_ws_ctx.is_play_local_file) {
        xEventGroupClearBits(g_ws_ctx.event_group,
                             WS_EVENT_PLAYBACK_BUFFER_TASK_STOP | WS_EVENT_PLAYBACK_BUFFER_TASK_EXIT);
        ESP_LOGI(TAG, "Cleared playback buffer event bits before creating new task");

        BaseType_t task_ret = xTaskCreate(
            playback_buffer_task,
            "ws_play_back_buffer",
            WS_TASK_STACK_SIZE,
            NULL,
            WS_PRIORITY_PLAYBACK_BUFFER,
            &g_ws_ctx.playback_buffer_task);

        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create playback buffer task");
            app_audio_playback_stop();
            handle_stop_recording_cmd();
            send_status_to_clients("error", "Failed to create playback buffer task");
            return ESP_FAIL;
        }
    }

    g_ws_ctx.state = WS_STATE_DUPLEX;
    ESP_LOGI(TAG, "Duplex mode started, AFE: %s", rec_cfg.enable_afe ? "enabled" : "disabled");

    return ESP_OK;
}

static esp_err_t handle_stop_duplex_cmd(void)
{
    if (g_ws_ctx.state != WS_STATE_DUPLEX && g_ws_ctx.state != WS_STATE_RECORDING) {
        send_status_to_clients("error", "Not in duplex mode");
        return ESP_FAIL;
    }

    app_audio_playback_stop();
    stop_playback_tasks();
    stop_task_and_wait(&g_ws_ctx.record_task,
                       WS_EVENT_RECORD_TASK_STOP,
                       WS_EVENT_RECORD_TASK_EXIT,
                       "record task");
    app_audio_record_stop();

    g_ws_ctx.state = WS_STATE_IDLE;
    send_status_to_clients("duplex_stopped", NULL);
    ESP_LOGI(TAG, "Duplex mode stopped");

    return ESP_OK;
}

static esp_err_t handle_reset_cmd(void)
{
    if (g_ws_ctx.state == WS_STATE_DUPLEX) {
        handle_stop_duplex_cmd();
    } else if (g_ws_ctx.state == WS_STATE_RECORDING) {
        handle_stop_recording_cmd();
    } else if (g_ws_ctx.state == WS_STATE_PLAYING) {
        handle_stop_playback_cmd();
    }
    g_ws_ctx.state = WS_STATE_IDLE;
    g_ws_ctx.is_play_local_file = false;

    return ESP_OK;
}

static esp_err_t websocket_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "New WebSocket connection on fd %d", httpd_req_to_sockfd(req));
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "status", "ready");
        cJSON_AddStringToObject(root, "device_id", CONFIG_AUDIO_ANALYZER_HARDWARE_NAME);
        cJSON *afe_mode = cJSON_CreateObject();
#ifdef CONFIG_GMF_AI_AUDIO_INIT_AFE
        const char *ch_allocation = CONFIG_GMF_AI_AUDIO_AFE_CH_ALLOCATION;
        cJSON_AddNumberToObject(afe_mode, "sample_rate", 16000);
        cJSON_AddNumberToObject(afe_mode, "bits_per_sample", 16);
        cJSON_AddNumberToObject(afe_mode, "channels", strlen(ch_allocation));
        cJSON_AddStringToObject(afe_mode, "channel_type", ch_allocation);
#else
        cJSON_AddNumberToObject(afe_mode, "sample_rate", 16000);
        cJSON_AddNumberToObject(afe_mode, "bits_per_sample", 16);
        cJSON_AddNumberToObject(afe_mode, "channels", 1);
        cJSON_AddStringToObject(afe_mode, "channel_type", "M");
#endif  /* CONFIG_GMF_AI_AUDIO_INIT_AFE */
        cJSON_AddItemToObject(root, "afe_mode", afe_mode);

        char *json_str = cJSON_PrintUnformatted(root);
        if (json_str) {
            httpd_ws_frame_t ws_pkt = {
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)json_str,
                .len = strlen(json_str),
            };
            httpd_ws_send_frame_async(g_ws_ctx.server_handle, httpd_req_to_sockfd(req), &ws_pkt);
            free(json_str);
        }
        cJSON_Delete(root);

        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.len == 0) {
        ESP_LOGD(TAG, "Received empty frame");
        return ESP_OK;
    }

    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate buffer for WebSocket frame (size: %zu)", ws_pkt.len);
        return ESP_ERR_NO_MEM;
    }

    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
        free(buf);
        return ret;
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
        buf[ws_pkt.len] = '\0';
        cJSON *root = cJSON_ParseWithLength((const char *)ws_pkt.payload, ws_pkt.len);
        if (root) {
            cJSON *cmd_obj = cJSON_GetObjectItem(root, "command");
            if (cJSON_IsString(cmd_obj)) {
                const char *command = cmd_obj->valuestring;
                cJSON *params = cJSON_GetObjectItem(root, "params");
                int sockfd = httpd_req_to_sockfd(req);

                if (strcmp(command, "configure") == 0) {
                    handle_configure_cmd(params, sockfd);
                } else if (strcmp(command, "start_recording") == 0) {
                    handle_start_recording_cmd(params);
                } else if (strcmp(command, "stop_recording") == 0) {
                    handle_stop_recording_cmd();
                } else if (strcmp(command, "start_playback") == 0) {
                    handle_start_playback_cmd(params);
                } else if (strcmp(command, "stop_playback") == 0) {
                    handle_stop_playback_cmd();
                } else if (strcmp(command, "start_duplex") == 0) {
                    handle_start_duplex_cmd(params);
                } else if (strcmp(command, "stop_duplex") == 0) {
                    handle_stop_duplex_cmd();
                } else if (strcmp(command, "reset") == 0) {
                    handle_reset_cmd();
                } else {
                    ESP_LOGW(TAG, "Unknown command: %s", command);
                    send_status_to_clients("error", "Unknown command");
                }
            } else {
                ESP_LOGW(TAG, "Invalid command field in JSON message");
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "Failed to parse JSON message: %s", cJSON_GetErrorPtr());
        }
    } else if (ws_pkt.type == HTTPD_WS_TYPE_BINARY) {
        if (g_ws_ctx.state == WS_STATE_PLAYING || g_ws_ctx.state == WS_STATE_DUPLEX) {
            esp_gmf_db_handle_t playback_buffer = app_audio_get_playback_buffer();
            if (playback_buffer) {
                if (ws_pkt.len > 0 && ws_pkt.payload != NULL) {
                    esp_gmf_data_bus_block_t blk = {0};
                    blk.buf = ws_pkt.payload;
                    blk.buf_length = ws_pkt.len;
                    blk.valid_size = ws_pkt.len;

                    int ret = esp_gmf_db_acquire_write(playback_buffer, &blk, ws_pkt.len, 0);
                    if (ret >= 0) {
                        ret = esp_gmf_db_release_write(playback_buffer, &blk, pdMS_TO_TICKS(WS_DB_RELEASE_TIMEOUT_MS));
                        if (ret < 0) {
                            ESP_LOGE(TAG, "Failed to release write to playback buffer, ret: %d", ret);
                        }
                    } else {
                        ESP_LOGW(TAG, "Failed to write audio data to playback buffer, ret: %d", ret);
                    }
                } else {
                    ESP_LOGW(TAG, "Invalid binary data: len=%zu, payload=%p", ws_pkt.len, ws_pkt.payload);
                }
            } else {
                ESP_LOGW(TAG, "Playback buffer not available");
            }
        } else {
            ESP_LOGW(TAG, "Received audio data but not in playing/duplex state");
        }
    }

    free(buf);
    return ESP_OK;
}

#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
static esp_err_t redirect_to_audio_tool_handler(httpd_req_t *req)
{
    const char redirect_base_url[] = "https://audio-tools.espressif.com.cn";
    const size_t host_len = httpd_req_get_hdr_value_len(req, "Host");
    char redirect_url[192] = {0};

    if (host_len > 0 && host_len < 96) {
        char host[96] = {0};
        if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) == ESP_OK) {
            snprintf(redirect_url, sizeof(redirect_url), "%s/?wss=wss://%s/ws", redirect_base_url, host);
        }
    }

    if (redirect_url[0] == '\0') {
        snprintf(redirect_url, sizeof(redirect_url), "%s", redirect_base_url);
    }

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", redirect_url);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}
#endif  /* CONFIG_ESP_HTTPS_SERVER_ENABLE */

esp_err_t app_websocket_init(app_websocket_config_t *config)
{
    esp_err_t ret = ESP_OK;
    uint16_t server_port = 0;

    if (g_ws_ctx.running) {
        ESP_LOGW(TAG, "WebSocket server already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WebSocket server");

    memset(&g_ws_ctx, 0, sizeof(websocket_context_t));

    if (config != NULL) {
        g_ws_ctx.config = *config;
    } else {
        app_websocket_config_t default_cfg = APP_WEBSOCKET_CONFIG_DEFAULT();
        g_ws_ctx.config = default_cfg;
    }

    if (g_ws_ctx.config.max_clients == 0 || g_ws_ctx.config.max_clients > WS_CLIENT_FD_LIST_SIZE) {
        ESP_LOGE(TAG, "Invalid argument: max_clients=%u (valid: 1-%u)",
                 g_ws_ctx.config.max_clients, WS_CLIENT_FD_LIST_SIZE);
        return ESP_ERR_INVALID_ARG;
    }

    g_ws_ctx.audio_format.sample_rate = 16000;
    g_ws_ctx.audio_format.channels = 2;
    g_ws_ctx.audio_format.bits_per_sample = 16;
    g_ws_ctx.mic_gain = WS_CFG_VALUE_NOT_SET;
    g_ws_ctx.volume = WS_CFG_VALUE_NOT_SET;
    g_ws_ctx.channel_type[0] = '\0';

    g_ws_ctx.event_group = xEventGroupCreate();
    if (g_ws_ctx.event_group == NULL) {
        ESP_LOGE(TAG, "Create event group failed");
        return ESP_FAIL;
    }

#if CONFIG_IDF_TARGET_ESP32
    g_ws_ctx.send_buf = heap_caps_malloc(WS_MAX_PACKET_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (g_ws_ctx.send_buf == NULL) {
        ESP_LOGE(TAG, "Allocate internal send buffer failed");
        vEventGroupDelete(g_ws_ctx.event_group);
        g_ws_ctx.event_group = NULL;
        return ESP_ERR_NO_MEM;
    }
#endif  /* CONFIG_IDF_TARGET_ESP32 */

#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
    httpd_ssl_config_t https_cfg = HTTPD_SSL_CONFIG_DEFAULT();
    https_cfg.httpd.task_priority = WS_PRIORITY_HTTPD;
    https_cfg.httpd.max_open_sockets = g_ws_ctx.config.max_clients;
    https_cfg.httpd.lru_purge_enable = true;
    https_cfg.httpd.recv_wait_timeout = g_ws_ctx.config.recv_timeout_sec;
    https_cfg.httpd.send_wait_timeout = g_ws_ctx.config.send_timeout_sec;

    https_cfg.servercert = servercert_start;
    https_cfg.servercert_len = servercert_end - servercert_start;
    https_cfg.prvtkey_pem = prvtkey_pem_start;
    https_cfg.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;
    https_cfg.transport_mode = HTTPD_SSL_TRANSPORT_SECURE;

    ret = httpd_ssl_start(&g_ws_ctx.server_handle, &https_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start HTTPS server failed: %s (0x%x)", esp_err_to_name(ret), ret);
#if CONFIG_IDF_TARGET_ESP32
        free(g_ws_ctx.send_buf);
        g_ws_ctx.send_buf = NULL;
#endif  /* CONFIG_IDF_TARGET_ESP32 */
        vEventGroupDelete(g_ws_ctx.event_group);
        g_ws_ctx.event_group = NULL;
        return ret;
    }
    server_port = https_cfg.httpd.server_port;
#else
    httpd_config_t httpd_cfg = HTTPD_DEFAULT_CONFIG();
    httpd_cfg.task_priority = WS_PRIORITY_HTTPD;
    httpd_cfg.max_open_sockets = g_ws_ctx.config.max_clients;
    httpd_cfg.lru_purge_enable = true;
    httpd_cfg.recv_wait_timeout = g_ws_ctx.config.recv_timeout_sec;
    httpd_cfg.send_wait_timeout = g_ws_ctx.config.send_timeout_sec;

    ret = httpd_start(&g_ws_ctx.server_handle, &httpd_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start HTTP server failed: %s", esp_err_to_name(ret));
#if CONFIG_IDF_TARGET_ESP32
        free(g_ws_ctx.send_buf);
        g_ws_ctx.send_buf = NULL;
#endif  /* CONFIG_IDF_TARGET_ESP32 */
        vEventGroupDelete(g_ws_ctx.event_group);
        g_ws_ctx.event_group = NULL;
        return ret;
    }
    server_port = httpd_cfg.server_port;
#endif  /* CONFIG_ESP_HTTPS_SERVER_ENABLE */

    const httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = websocket_handler,
        .user_ctx = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = false,
    };

    ret = httpd_register_uri_handler(g_ws_ctx.server_handle, &ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Register WebSocket handler failed: %s", esp_err_to_name(ret));
        httpd_stop(g_ws_ctx.server_handle);
        g_ws_ctx.server_handle = NULL;
#if CONFIG_IDF_TARGET_ESP32
        free(g_ws_ctx.send_buf);
        g_ws_ctx.send_buf = NULL;
#endif  /* CONFIG_IDF_TARGET_ESP32 */
        vEventGroupDelete(g_ws_ctx.event_group);
        g_ws_ctx.event_group = NULL;
        return ret;
    }

#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
    const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = redirect_to_audio_tool_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(g_ws_ctx.server_handle, &root_uri);
#endif  /* CONFIG_ESP_HTTPS_SERVER_ENABLE */

    ESP_LOGI(TAG, "WebSocket server started");

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != NULL) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_str[WS_IPV4_STR_LEN];
            snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
#if CONFIG_ESP_HTTPS_SERVER_ENABLE
            ESP_LOGI(TAG, "========================================");
            ESP_LOGI(TAG, "  WSS Server Ready");
            ESP_LOGI(TAG, "  Step 1 - Trust the device:");
            ESP_LOGI(TAG, "    Open in browser: https://%s:%d", ip_str, server_port);
            ESP_LOGI(TAG, "    Click 'Advanced' -> 'Continue to %s (unsafe)'", ip_str);
            ESP_LOGI(TAG, "    Browser will then redirect to https://audio-tools.espressif.com.cn/");
            ESP_LOGI(TAG, "  Step 2 - Connect WebSocket:");
            ESP_LOGI(TAG, "    wss://%s:%d/ws", ip_str, server_port);
            ESP_LOGI(TAG, "========================================");
#else
            ESP_LOGI(TAG, "Connect to ws://%s:%d/ws", ip_str, server_port);
#endif  /* CONFIG_ESP_HTTPS_SERVER_ENABLE */
        }
    }

    g_ws_ctx.running = true;
    g_ws_ctx.state = WS_STATE_IDLE;
    return ESP_OK;
}

void app_websocket_deinit(void)
{
    if (!g_ws_ctx.running) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing WebSocket server");
    g_ws_ctx.running = false;

    handle_reset_cmd();

    if (g_ws_ctx.server_handle) {
        httpd_stop(g_ws_ctx.server_handle);
        g_ws_ctx.server_handle = NULL;
    }
    if (g_ws_ctx.event_group) {
        vEventGroupDelete(g_ws_ctx.event_group);
        g_ws_ctx.event_group = NULL;
    }
#if CONFIG_IDF_TARGET_ESP32
    free(g_ws_ctx.send_buf);
    g_ws_ctx.send_buf = NULL;
#endif  /* CONFIG_IDF_TARGET_ESP32 */

    memset(&g_ws_ctx, 0, sizeof(g_ws_ctx));
    ESP_LOGI(TAG, "WebSocket server stopped and all resources cleaned up");
}

httpd_handle_t app_websocket_get_handle(void)
{
    return g_ws_ctx.server_handle;
}

bool app_websocket_is_running(void)
{
    return g_ws_ctx.running;
}
