/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_gmf_err.h"
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "esp_audio_simple_player.h"
#include "esp_playlist.h"
#include "music_player_playback.h"
#include "music_player_ui.h"
#include "music_player_config.h"

static const char *TAG = "MUSIC_PLAYER_PLAYBACK";

typedef enum {
    MUSIC_PLAYER_MODE_REPEAT_ONE = 0,
    MUSIC_PLAYER_MODE_REPEAT_ALL,
    MUSIC_PLAYER_MODE_SHUFFLE,
} music_player_mode_t;

static QueueHandle_t s_cmd_queue = NULL;
static TaskHandle_t s_ctrl_task = NULL;
static esp_asp_handle_t s_player = NULL;
static esp_playlist_handle_t s_playlist = NULL;
static esp_media_db_handle_t s_media_db = NULL;
static esp_codec_dev_handle_t s_codec = NULL;
static bool s_is_playing = false;
static volatile bool s_ctrl_running = false;
static music_player_mode_t s_mode = MUSIC_PLAYER_MODE_REPEAT_ALL;

static const char *s_mode_text[] = {
    "单曲循环",
    "列表循环",
    "随机播放",
};

static inline esp_playlist_repeat_mode_t mode_to_playlist(music_player_mode_t mode)
{
    switch (mode) {
        case MUSIC_PLAYER_MODE_REPEAT_ONE:
            return ESP_PLAYLIST_REPEAT_ONE;
        case MUSIC_PLAYER_MODE_REPEAT_ALL:
            return ESP_PLAYLIST_REPEAT_ALL;
        case MUSIC_PLAYER_MODE_SHUFFLE:
        default:
            return ESP_PLAYLIST_REPEAT_SHUFFLE;
    }
}

static inline int clamp_volume(int volume)
{
    if (volume < MUSIC_PLAYER_VOLUME_MIN) {
        return MUSIC_PLAYER_VOLUME_MIN;
    }
    if (volume > MUSIC_PLAYER_VOLUME_MAX) {
        return MUSIC_PLAYER_VOLUME_MAX;
    }
    return volume;
}

static int get_playback_volume_or_default(void)
{
    int volume = MUSIC_PLAYER_DEFAULT_VOLUME;
    if (s_codec == NULL) {
        return volume;
    }
    if (esp_codec_dev_get_out_vol(s_codec, &volume) != ESP_OK) {
        return MUSIC_PLAYER_DEFAULT_VOLUME;
    }
    return clamp_volume(volume);
}

static esp_err_t post_message(const music_player_msg_t *msg, TickType_t timeout)
{
    ESP_GMF_CHECK(TAG, s_cmd_queue != NULL && msg != NULL, return ESP_ERR_INVALID_STATE, "Queue or message is NULL");
    ESP_GMF_CHECK(TAG, xQueueSend(s_cmd_queue, msg, timeout) == pdTRUE, return ESP_ERR_TIMEOUT,
                  "Failed to post playback message");
    return ESP_OK;
}

static inline esp_err_t post_cmd_internal(music_player_cmd_t cmd, int index, TickType_t timeout)
{
    music_player_msg_t msg = {
        .cmd = cmd,
        .index = index,
    };
    return post_message(&msg, timeout);
}

static void playlist_url_to_player_uri(const char *url, char *out, size_t out_size)
{
    if (url == NULL || out == NULL || out_size == 0) {
        return;
    }
    if (strncmp(url, "file:", 5) == 0) {
        const char *path = url + 5;
        while (path[0] == '/' && path[1] == '/') {
            path++;
        }
        snprintf(out, out_size, "%s", path);
    } else {
        snprintf(out, out_size, "%s", url);
    }
}

static int out_data_callback(uint8_t *data, int data_size, void *ctx)
{
    esp_codec_dev_handle_t codec = (esp_codec_dev_handle_t)ctx;
    if (codec == NULL || data == NULL || data_size <= 0) {
        return 0;
    }
    int ret = esp_codec_dev_write(codec, data, data_size);
    if (ret != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "Write PCM failed, ret=%d, size=%d", ret, data_size);
        return 0;
    }
    return data_size;
}

static int player_event_callback(esp_asp_event_pkt_t *event, void *ctx)
{
    (void)ctx;
    if (event == NULL || event->type != ESP_ASP_EVENT_TYPE_STATE || event->payload == NULL) {
        return 0;
    }
    if (event->payload_size < sizeof(esp_asp_state_t)) {
        ESP_LOGW(TAG, "Ignore state event with invalid payload size: %u", (unsigned)event->payload_size);
        return 0;
    }

    esp_asp_state_t state = ESP_ASP_STATE_NONE;
    memcpy(&state, event->payload, sizeof(state));
    if (state == ESP_ASP_STATE_FINISHED) {
        post_cmd_internal(MUSIC_PLAYER_CMD_TRACK_FINISHED, -1, 0);
    } else if (state == ESP_ASP_STATE_ERROR) {
        post_cmd_internal(MUSIC_PLAYER_CMD_TRACK_ERROR, -1, 0);
    } else if (state == ESP_ASP_STATE_RUNNING || state == ESP_ASP_STATE_PAUSED || state == ESP_ASP_STATE_STOPPED) {
        post_cmd_internal(MUSIC_PLAYER_CMD_UPDATE_UI, -1, 0);
    }
    return 0;
}

static void update_ui_from_current(bool playing)
{
    esp_playlist_info_t info = {0};
    int volume = get_playback_volume_or_default();
    if (s_playlist == NULL || esp_playlist_curr(s_playlist, &info) != ESP_OK) {
        music_player_ui_update("未找到音乐", s_mode_text[s_mode], volume, playing);
        return;
    }
    music_player_ui_update(info.media_name, s_mode_text[s_mode], volume, playing);
}

static esp_err_t set_playback_volume(int volume)
{
    ESP_GMF_CHECK(TAG, s_codec != NULL, return ESP_ERR_INVALID_STATE, "Codec is NULL");
    int new_volume = clamp_volume(volume);
    esp_err_t ret = esp_codec_dev_set_out_vol(s_codec, new_volume);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Set volume failed: volume=%d", new_volume);
    ESP_LOGI(TAG, "Playback volume: %d%%", new_volume);
    update_ui_from_current(s_is_playing);
    return ESP_OK;
}

static esp_err_t play_current_track(void)
{
    ESP_GMF_CHECK(TAG, s_player != NULL, return ESP_ERR_INVALID_STATE, "Player is NULL");
    esp_playlist_info_t info = {0};
    esp_err_t ret = esp_playlist_curr(s_playlist, &info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No track to play");
        s_is_playing = false;
        update_ui_from_current(false);
        return ret;
    }

    char uri[CONFIG_ESP_PLAYLIST_URL_MAX] = {0};
    playlist_url_to_player_uri(info.media_url, uri, sizeof(uri));
    ESP_LOGI(TAG, "Play file: %s", uri);

    esp_audio_simple_player_stop(s_player);
    esp_gmf_err_t err = esp_audio_simple_player_run(s_player, uri, NULL);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {
        s_is_playing = false;
        update_ui_from_current(false);
        return ESP_FAIL;
    }, "Failed to run player");
    s_is_playing = true;
    update_ui_from_current(true);
    return ESP_OK;
}

static esp_err_t navigate_track(bool next)
{
    esp_playlist_info_t info = {0};
    esp_err_t ret = next ? esp_playlist_next(s_playlist, &info) : esp_playlist_prev(s_playlist, &info);
    if (ret != ESP_OK) {
        return ret;
    }
    return play_current_track();
}

static esp_err_t play_track_by_index(int index)
{
    esp_err_t ret = esp_playlist_set_curr_index(s_playlist, index);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Play index failed: index=%d", index);
    ESP_LOGI(TAG, "Play index: %d", index);
    return play_current_track();
}

static void handle_track_end_or_error(bool is_error)
{
    if (is_error) {
        ESP_LOGW(TAG, "Playback error, try next track");
    }
    /**
     * In REPEAT_ONE mode, esp_playlist_next() returns the same track without advancing.
     * On error, this causes an infinite retry loop of the failed track.
     * Temporarily switch to REPEAT_ALL to advance past the failed track.
     */
    if (is_error && s_mode == MUSIC_PLAYER_MODE_REPEAT_ONE) {
        esp_playlist_set_repeat_mode(s_playlist, ESP_PLAYLIST_REPEAT_ALL);
    }
    esp_playlist_info_t info = {0};
    if (esp_playlist_next(s_playlist, &info) == ESP_OK) {
        play_current_track();
    } else {
        s_is_playing = false;
        update_ui_from_current(false);
    }
    if (is_error && s_mode == MUSIC_PLAYER_MODE_REPEAT_ONE) {
        esp_playlist_set_repeat_mode(s_playlist, ESP_PLAYLIST_REPEAT_ONE);
    }
}

static void handle_command(const music_player_msg_t *msg)
{
    if (msg == NULL) {
        return;
    }
    switch (msg->cmd) {
        case MUSIC_PLAYER_CMD_PLAY:
            play_current_track();
            break;
        case MUSIC_PLAYER_CMD_PAUSE:
            if (s_player != NULL) {
                esp_audio_simple_player_pause(s_player);
                s_is_playing = false;
                update_ui_from_current(false);
            }
            break;
        case MUSIC_PLAYER_CMD_RESUME:
            if (s_player != NULL && esp_audio_simple_player_resume(s_player) == ESP_GMF_ERR_OK) {
                s_is_playing = true;
                update_ui_from_current(true);
            } else {
                play_current_track();
            }
            break;
        case MUSIC_PLAYER_CMD_NEXT:
            navigate_track(true);
            break;
        case MUSIC_PLAYER_CMD_PREV:
            navigate_track(false);
            break;
        case MUSIC_PLAYER_CMD_TOGGLE_MODE:
            s_mode = (music_player_mode_t)((s_mode + 1) % 3);
            esp_playlist_set_repeat_mode(s_playlist, mode_to_playlist(s_mode));
            update_ui_from_current(s_is_playing);
            ESP_LOGI(TAG, "Repeat mode: %s", s_mode_text[s_mode]);
            break;
        case MUSIC_PLAYER_CMD_VOLUME_UP:
            set_playback_volume(get_playback_volume_or_default() + MUSIC_PLAYER_VOLUME_STEP);
            break;
        case MUSIC_PLAYER_CMD_VOLUME_DOWN:
            set_playback_volume(get_playback_volume_or_default() - MUSIC_PLAYER_VOLUME_STEP);
            break;
        case MUSIC_PLAYER_CMD_PLAY_INDEX:
            play_track_by_index(msg->index);
            break;
        case MUSIC_PLAYER_CMD_TRACK_FINISHED:
            handle_track_end_or_error(false);
            break;
        case MUSIC_PLAYER_CMD_TRACK_ERROR:
            handle_track_end_or_error(true);
            break;
        case MUSIC_PLAYER_CMD_UPDATE_UI:
            update_ui_from_current(s_is_playing);
            break;
        case MUSIC_PLAYER_CMD_SHUTDOWN:
            break;
        default:
            break;
    }
}

static void control_task(void *arg)
{
    (void)arg;
    music_player_msg_t msg = {0};
    while (s_ctrl_running) {
        if (xQueueReceive(s_cmd_queue, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (msg.cmd == MUSIC_PLAYER_CMD_SHUTDOWN) {
            break;
        }
        handle_command(&msg);
    }
    s_ctrl_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t music_player_playback_scan(const char *scan_dir)
{
    ESP_GMF_CHECK(TAG, scan_dir != NULL, return ESP_ERR_INVALID_ARG, "Scan dir is NULL");
    ESP_GMF_CHECK(TAG, s_media_db == NULL && s_playlist == NULL,
                  return ESP_ERR_INVALID_STATE, "Previous scan not cleaned up, call music_player_playback_stop() first");

    const esp_media_db_cfg_t db_cfg = {
        .storage_type = ESP_DB_STORAGE_RAM,
        .storage_path = "music_player_db",
    };
    esp_err_t ret = esp_media_db_init(&db_cfg, &s_media_db);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Failed to init media DB");

    const char *exts[] = {".mp3", ".aac", ".wav"};
    const esp_media_db_scan_cfg_t scan_cfg = {
        .skip_duplicate = true,
        .path = scan_dir,
        .scan_depth = MUSIC_PLAYER_SCAN_DEPTH,
        .file_extensions = exts,
        .file_extension_count = 3,
    };
    ret = esp_media_db_scan(s_media_db, &scan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Scan failed for %s", scan_dir);
        esp_media_db_deinit(s_media_db);
        s_media_db = NULL;
        return ret;
    }

    int count = 0;
    esp_media_db_get_count(s_media_db, &count);
    ESP_LOGI(TAG, "Scanned %d tracks under %s", count, scan_dir);
    if (count <= 0) {
        esp_media_db_deinit(s_media_db);
        s_media_db = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    ret = esp_playlist_new(&(esp_playlist_cfg_t) {
                               .playlist_name = "music_player",
                           },
                           &s_playlist);
    if (ret != ESP_OK) {
        esp_media_db_deinit(s_media_db);
        s_media_db = NULL;
        return ret;
    }

    ret = esp_playlist_import_media(s_playlist, s_media_db, NULL);
    if (ret != ESP_OK) {
        esp_playlist_del(s_playlist);
        s_playlist = NULL;
        esp_media_db_deinit(s_media_db);
        s_media_db = NULL;
        return ret;
    }

    esp_playlist_set_repeat_mode(s_playlist, mode_to_playlist(s_mode));
    return ESP_OK;
}

esp_err_t music_player_playback_start(QueueHandle_t cmd_queue, esp_codec_dev_handle_t codec)
{
    esp_err_t ret = ESP_OK;

    ESP_GMF_CHECK(TAG, cmd_queue != NULL && codec != NULL, return ESP_ERR_INVALID_ARG, "Invalid queue or codec");
    ESP_GMF_CHECK(TAG, s_player == NULL && s_ctrl_task == NULL, return ESP_ERR_INVALID_STATE,
                  "Playback controller already started");
    s_cmd_queue = cmd_queue;
    s_codec = codec;

    esp_asp_cfg_t cfg = {
        .out.cb = out_data_callback,
        .out.user_ctx = s_codec,
        .task_prio = MUSIC_PLAYER_ASP_TASK_PRIO,
        .task_stack = MUSIC_PLAYER_ASP_TASK_STACK,
    };
    esp_gmf_err_t err = esp_audio_simple_player_new(&cfg, &s_player);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { ret = ESP_FAIL; goto err_cleanup;}, "Failed to create player");
    ESP_GMF_CHECK(TAG, s_player != NULL, { ret = ESP_FAIL; goto err_cleanup;}, "Player handle is NULL");
    err = esp_audio_simple_player_set_event(s_player, player_event_callback, NULL);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, { ret = ESP_FAIL; goto err_cleanup;}, "Failed to set event callback");

    s_ctrl_running = true;
    BaseType_t task_ret = xTaskCreatePinnedToCore(control_task, "music_ctrl",
                                                  MUSIC_PLAYER_CONTROL_TASK_STACK,
                                                  NULL, MUSIC_PLAYER_CONTROL_TASK_PRIO,
                                                  &s_ctrl_task, 1);
    ESP_GMF_CHECK(TAG, task_ret == pdPASS, { ret = ESP_ERR_NO_MEM; goto err_cleanup;},
                  "Failed to create control task");
    return ESP_OK;

err_cleanup:
    s_ctrl_running = false;
    if (s_player != NULL) {
        esp_audio_simple_player_destroy(s_player);
        s_player = NULL;
    }
    s_cmd_queue = NULL;
    s_codec = NULL;
    return ret;
}

esp_err_t music_player_playback_post(music_player_cmd_t cmd)
{
    return post_cmd_internal(cmd, -1, pdMS_TO_TICKS(1000));
}

esp_err_t music_player_playback_post_index(music_player_cmd_t cmd, int index)
{
    return post_cmd_internal(cmd, index, pdMS_TO_TICKS(1000));
}

esp_err_t music_player_playback_has_playlist(bool *has_playlist)
{
    ESP_GMF_CHECK(TAG, has_playlist != NULL, return ESP_ERR_INVALID_ARG, "Invalid playlist state buffer");
    *has_playlist = (s_playlist != NULL);
    return ESP_OK;
}

esp_err_t music_player_playback_get_volume(int *volume)
{
    ESP_GMF_CHECK(TAG, volume != NULL, return ESP_ERR_INVALID_ARG, "Invalid volume buffer");
    *volume = get_playback_volume_or_default();
    return ESP_OK;
}

esp_err_t music_player_playback_get_track_count(int *count)
{
    ESP_GMF_CHECK(TAG, count != NULL, return ESP_ERR_INVALID_ARG, "Invalid track count buffer");
    *count = 0;
    ESP_GMF_CHECK(TAG, s_playlist != NULL, return ESP_ERR_INVALID_STATE, "Playlist is NULL");
    return esp_playlist_get_count(s_playlist, count);
}

esp_err_t music_player_playback_get_current_index(int *index)
{
    ESP_GMF_CHECK(TAG, index != NULL, return ESP_ERR_INVALID_ARG, "Invalid current index buffer");
    *index = -1;
    esp_playlist_info_t info = {0};
    ESP_GMF_CHECK(TAG, s_playlist != NULL, return ESP_ERR_INVALID_STATE, "Playlist is NULL");
    esp_err_t ret = esp_playlist_curr(s_playlist, &info);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Get current track failed");
    *index = info.index;
    return ESP_OK;
}

esp_err_t music_player_playback_get_track_title(int index, char *title, size_t title_size)
{
    ESP_GMF_CHECK(TAG, title != NULL && title_size > 0, return ESP_ERR_INVALID_ARG, "Invalid title buffer");
    title[0] = '\0';
    ESP_GMF_CHECK(TAG, s_playlist != NULL, return ESP_ERR_INVALID_STATE, "Playlist is NULL");
    esp_playlist_info_t info = {0};
    esp_err_t ret = esp_playlist_get_info(s_playlist, index, &info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Get track title: index=%d unavailable, ret=%s", index, esp_err_to_name(ret));
        return ret;
    }
    snprintf(title, title_size, "%s", info.media_name);
    return ESP_OK;
}

void music_player_playback_stop(void)
{
    if (s_player == NULL && s_ctrl_task == NULL && s_playlist == NULL && s_media_db == NULL) {
        return;
    }

    if (s_ctrl_task != NULL && s_cmd_queue != NULL) {
        s_ctrl_running = false;
        post_cmd_internal(MUSIC_PLAYER_CMD_SHUTDOWN, -1, pdMS_TO_TICKS(1000));
        for (int i = 0; i < 100 && s_ctrl_task != NULL; i++) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if (s_ctrl_task != NULL) {
            ESP_LOGW(TAG, "Control task did not exit in time");
        }
        /* Drain stale messages (e.g. SHUTDOWN if task already exited) to prevent
         * immediate exit on restart */
        music_player_msg_t dummy;
        while (xQueueReceive(s_cmd_queue, &dummy, 0) == pdTRUE) {
            /* discard */
        }
    }

    if (s_player != NULL) {
        esp_audio_simple_player_stop(s_player);
        esp_audio_simple_player_destroy(s_player);
        s_player = NULL;
    }

    if (s_playlist != NULL) {
        esp_playlist_del(s_playlist);
        s_playlist = NULL;
    }
    if (s_media_db != NULL) {
        esp_media_db_deinit(s_media_db);
        s_media_db = NULL;
    }
    s_codec = NULL;
    s_cmd_queue = NULL;
    s_is_playing = false;
}
