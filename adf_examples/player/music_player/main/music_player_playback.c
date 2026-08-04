/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/lock.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_gmf_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_codec_dev.h"
#include "esp_audio_simple_player.h"
#include "esp_extractor.h"
#include "esp_audio_es_extractor.h"
#include "esp_wav_extractor.h"
#include "esp_playlist.h"
#include "sdkconfig.h"
#include "music_player_playback.h"
#include "music_player_ui.h"
#include "music_player_config.h"

#define MUSIC_PLAYER_CTRL_IDLE_MS      50
#define MUSIC_PLAYER_PROBE_TIMEOUT_US  (1500 * 1000)

static const char *TAG = "MUSIC_PLAYER_PLAYBACK";

typedef enum {
    MUSIC_PLAYER_MODE_REPEAT_ONE = 0,
    MUSIC_PLAYER_MODE_REPEAT_ALL,
    MUSIC_PLAYER_MODE_SHUFFLE,
} music_player_mode_t;

typedef struct {
    FILE    *fp;
    int64_t  deadline_us;
} extractor_io_t;

typedef struct {
    _lock_t   lock;
    int       duration_ms;
    int       elapsed_acc_ms;
    int64_t   elapsed_base_us;
    bool      elapsed_running;
    uint32_t  track_gen;
} music_player_progress_t;

static QueueHandle_t s_cmd_queue = NULL;
static TaskHandle_t s_ctrl_task = NULL;
static esp_asp_handle_t s_player = NULL;
static esp_playlist_handle_t s_playlist = NULL;
static esp_media_db_handle_t s_media_db = NULL;
static esp_codec_dev_handle_t s_codec = NULL;
static bool s_is_playing = false;
static bool s_extractors_ready = false;
static volatile bool s_ctrl_running = false;
static volatile bool s_ui_refresh_pending = false;
static music_player_mode_t s_mode = MUSIC_PLAYER_MODE_REPEAT_ALL;
static music_player_progress_t s_progress = {0};
static int s_invalid_tracks = 0;

static const char *s_mode_text[] = {
    "单曲循环",
    "列表循环",
    "随机播放",
};

static void update_ui_from_current(bool playing);

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
    return (volume > MUSIC_PLAYER_VOLUME_MAX) ? MUSIC_PLAYER_VOLUME_MAX : volume;
}

static int get_playback_volume_or_default(void)
{
    int volume = MUSIC_PLAYER_DEFAULT_VOLUME;
    if (s_codec != NULL && esp_codec_dev_get_out_vol(s_codec, &volume) == ESP_OK) {
        return clamp_volume(volume);
    }
    return MUSIC_PLAYER_DEFAULT_VOLUME;
}

static esp_err_t post_message(const music_player_msg_t *msg, TickType_t timeout)
{
    ESP_GMF_CHECK(TAG, s_cmd_queue != NULL && msg != NULL, return ESP_ERR_INVALID_STATE, "Queue or message is NULL");
    if (xQueueSend(s_cmd_queue, msg, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static bool is_track_switch_cmd(music_player_cmd_t cmd)
{
    return cmd == MUSIC_PLAYER_CMD_NEXT || cmd == MUSIC_PLAYER_CMD_PREV || cmd == MUSIC_PLAYER_CMD_PLAY_INDEX;
}

static inline esp_err_t post_cmd_internal(music_player_cmd_t cmd, int index, TickType_t timeout)
{
    if (is_track_switch_cmd(cmd)) {
        ESP_GMF_CHECK(TAG, s_ctrl_task != NULL, return ESP_ERR_INVALID_STATE, "Control task is NULL");
        uint32_t value = ((uint32_t)cmd << 24) | ((uint32_t)(index + 1) & 0x00FFFFFF);
        return xTaskNotify(s_ctrl_task, value, eSetValueWithOverwrite) == pdPASS ? ESP_OK : ESP_FAIL;
    }
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
    const char *path = url;
    if (strncmp(url, "file:", 5) == 0) {
        path = url + 5;
        while (path[0] == '/' && path[1] == '/') {
            path++;
        }
    }
    snprintf(out, out_size, "%s", path);
}

static void progress_elapsed_pause(void)
{
    _lock_acquire(&s_progress.lock);
    if (s_progress.elapsed_running) {
        int64_t now = esp_timer_get_time();
        s_progress.elapsed_acc_ms += (int)((now - s_progress.elapsed_base_us) / 1000);
        if (s_progress.elapsed_acc_ms < 0) {
            s_progress.elapsed_acc_ms = 0;
        }
        s_progress.elapsed_running = false;
    }
    _lock_release(&s_progress.lock);
}

static void progress_elapsed_resume(void)
{
    _lock_acquire(&s_progress.lock);
    s_progress.elapsed_base_us = esp_timer_get_time();
    s_progress.elapsed_running = true;
    _lock_release(&s_progress.lock);
}

static int progress_get_elapsed_ms_unlocked(void)
{
    int elapsed = s_progress.elapsed_acc_ms;
    if (s_progress.elapsed_running) {
        elapsed += (int)((esp_timer_get_time() - s_progress.elapsed_base_us) / 1000);
    }
    if (elapsed < 0) {
        elapsed = 0;
    }
    if (s_progress.duration_ms > 0 && elapsed > s_progress.duration_ms) {
        elapsed = s_progress.duration_ms;
    }
    return elapsed;
}

static esp_err_t register_extractors(void)
{
    if (s_extractors_ready) {
        return ESP_OK;
    }
    if (esp_audio_es_extractor_register() != ESP_EXTRACTOR_ERR_OK) {
        return ESP_FAIL;
    }
    if (esp_wav_extractor_register() != ESP_EXTRACTOR_ERR_OK) {
        esp_audio_es_extractor_unregister();
        return ESP_FAIL;
    }
    s_extractors_ready = true;
    return ESP_OK;
}

static void unregister_extractors(void)
{
    if (!s_extractors_ready) {
        return;
    }
    esp_wav_extractor_unregister();
    esp_audio_es_extractor_unregister();
    s_extractors_ready = false;
}

static int extractor_read(void *buffer, uint32_t size, void *ctx)
{
    extractor_io_t *io = (extractor_io_t *)ctx;
    return esp_timer_get_time() >= io->deadline_us ? -1 : (int)fread(buffer, 1, size, io->fp);
}

static int extractor_seek(uint32_t position, void *ctx)
{
    extractor_io_t *io = (extractor_io_t *)ctx;
    return esp_timer_get_time() >= io->deadline_us ? -1 : fseek(io->fp, position, SEEK_SET);
}

static uint32_t extractor_size(void *ctx)
{
    extractor_io_t *io = (extractor_io_t *)ctx;
    if (esp_timer_get_time() >= io->deadline_us) {
        return 0;
    }
    FILE *fp = io->fp;
    long current = ftell(fp);
    if (current < 0 || fseek(fp, 0, SEEK_END) != 0) {
        return 0;
    }
    long size = ftell(fp);
    fseek(fp, current, SEEK_SET);
    return (size > 0) ? (uint32_t)size : 0;
}

static int get_duration_ms(const char *path)
{
    if (!s_extractors_ready) {
        return -1;
    }
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    int duration = -1;
    extractor_io_t io = {
        .fp = fp,
        .deadline_us = esp_timer_get_time() + MUSIC_PLAYER_PROBE_TIMEOUT_US,
    };
    esp_extractor_handle_t extractor = NULL;
    esp_extractor_config_t cfg = {
        .extract_mask = ESP_EXTRACT_MASK_AUDIO,
        .in_read_cb = extractor_read,
        .in_seek_cb = extractor_seek,
        .in_size_cb = extractor_size,
        .in_ctx = &io,
    };
    if (esp_extractor_open(&cfg, &extractor) == ESP_EXTRACTOR_ERR_OK &&
        esp_extractor_parse_stream(extractor) == ESP_EXTRACTOR_ERR_OK) {
        esp_extractor_stream_info_t info = {0};
        if (esp_extractor_get_stream_info(extractor, ESP_EXTRACTOR_STREAM_TYPE_AUDIO, 0, &info) ==
            ESP_EXTRACTOR_ERR_OK) {
            duration = info.duration;
        }
    }
    if (extractor != NULL) {
        esp_extractor_close(extractor);
    }
    fclose(fp);
    return duration;
}

static void reset_progress_state(int duration_ms)
{
    _lock_acquire(&s_progress.lock);
    s_progress.duration_ms = (duration_ms > 0) ? duration_ms : 0;
    s_progress.elapsed_acc_ms = 0;
    s_progress.elapsed_base_us = esp_timer_get_time();
    s_progress.elapsed_running = true;
    _lock_release(&s_progress.lock);
}

static void request_ui_refresh(void)
{
    s_ui_refresh_pending = true;
}

static void flush_pending_ui_refresh(void)
{
    if (!s_ui_refresh_pending) {
        return;
    }
    s_ui_refresh_pending = false;
    update_ui_from_current(s_is_playing);
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
    if (event == NULL || event->payload == NULL) {
        return 0;
    }

    if (event->type != ESP_ASP_EVENT_TYPE_STATE || event->payload_size < sizeof(esp_asp_state_t)) {
        return 0;
    }

    esp_asp_state_t state = ESP_ASP_STATE_NONE;
    memcpy(&state, event->payload, sizeof(state));
    if (state == ESP_ASP_STATE_FINISHED || state == ESP_ASP_STATE_ERROR) {
        uint32_t gen = 0;
        _lock_acquire(&s_progress.lock);
        gen = s_progress.track_gen;
        _lock_release(&s_progress.lock);
        music_player_cmd_t cmd = (state == ESP_ASP_STATE_FINISHED) ? MUSIC_PLAYER_CMD_TRACK_FINISHED : MUSIC_PLAYER_CMD_TRACK_ERROR;
        if (post_cmd_internal(cmd, (int)gen, 0) != ESP_OK) {
            ESP_LOGW(TAG, "Drop end-of-track cmd, queue full");
        }
    } else if (state == ESP_ASP_STATE_RUNNING || state == ESP_ASP_STATE_PAUSED || state == ESP_ASP_STATE_STOPPED) {
        request_ui_refresh();
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
    _lock_acquire(&s_progress.lock);
    s_progress.track_gen++;
    uint32_t track_gen = s_progress.track_gen;
    _lock_release(&s_progress.lock);
    int duration_ms = get_duration_ms(uri);
    if (duration_ms < 0) {
        reset_progress_state(0);
        s_is_playing = false;
        ESP_LOGW(TAG, "Skip invalid audio: %s", uri);
        if (post_cmd_internal(MUSIC_PLAYER_CMD_TRACK_ERROR, (int)track_gen, 0) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to queue invalid track skip");
        }
        return ESP_ERR_INVALID_RESPONSE;
    }
    reset_progress_state(duration_ms);
    ESP_LOGI(TAG, "Track init: duration=%d ms gen=%" PRIu32, duration_ms, track_gen);

    esp_gmf_err_t err = esp_audio_simple_player_run(s_player, uri, NULL);
    ESP_GMF_RET_ON_NOT_OK(TAG, err, {
        s_is_playing = false;
        progress_elapsed_pause();
        update_ui_from_current(false);
        return ESP_FAIL;
    }, "Failed to run player");
    s_is_playing = true;
    update_ui_from_current(true);
    return ESP_OK;
}

static esp_err_t start_track_playback(void)
{
    s_invalid_tracks = 0;
    return play_current_track();
}

static esp_err_t navigate_track(bool next)
{
    s_invalid_tracks = 0;
    esp_playlist_info_t info = {0};
    esp_err_t ret = next ? esp_playlist_next(s_playlist, &info) : esp_playlist_prev(s_playlist, &info);
    if (ret != ESP_OK) {
        return ret;
    }
    return play_current_track();
}

static esp_err_t play_track_by_index(int index)
{
    s_invalid_tracks = 0;
    esp_err_t ret = esp_playlist_set_curr_index(s_playlist, index);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Play index failed: index=%d", index);
    ESP_LOGI(TAG, "Play index: %d", index);
    return play_current_track();
}

static void handle_track_end_or_error(bool is_error, uint32_t event_gen)
{
    _lock_acquire(&s_progress.lock);
    uint32_t cur_gen = s_progress.track_gen;
    _lock_release(&s_progress.lock);
    if (event_gen != cur_gen) {
        ESP_LOGD(TAG, "Ignore stale %s event: gen=%" PRIu32 " current=%" PRIu32,
                 is_error ? "TRACK_ERROR" : "TRACK_FINISHED", event_gen, cur_gen);
        return;
    }

    if (is_error) {
        int count = 0;
        music_player_playback_get_track_count(&count);
        if (count > 0 && ++s_invalid_tracks >= count) {
            s_is_playing = false;
            progress_elapsed_pause();
            ESP_LOGE(TAG, "No playable audio found");
            update_ui_from_current(false);
            return;
        }
    } else {
        s_invalid_tracks = 0;
    }

    /* REPEAT_ONE + error would retry forever; advance with REPEAT_ALL temporarily. */
    if (is_error && s_mode == MUSIC_PLAYER_MODE_REPEAT_ONE) {
        esp_playlist_set_repeat_mode(s_playlist, ESP_PLAYLIST_REPEAT_ALL);
    }
    esp_playlist_info_t info = {0};
    if (esp_playlist_next(s_playlist, &info) == ESP_OK) {
        play_current_track();
    } else {
        s_is_playing = false;
        progress_elapsed_pause();
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
            start_track_playback();
            break;
        case MUSIC_PLAYER_CMD_PAUSE:
            if (s_player != NULL) {
                esp_audio_simple_player_pause(s_player);
                progress_elapsed_pause();
                s_is_playing = false;
                update_ui_from_current(false);
            }
            break;
        case MUSIC_PLAYER_CMD_RESUME:
            if (s_player != NULL && esp_audio_simple_player_resume(s_player) == ESP_GMF_ERR_OK) {
                progress_elapsed_resume();
                s_is_playing = true;
                update_ui_from_current(true);
            } else {
                start_track_playback();
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
            handle_track_end_or_error(false, (uint32_t)msg->index);
            break;
        case MUSIC_PLAYER_CMD_TRACK_ERROR:
            handle_track_end_or_error(true, (uint32_t)msg->index);
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

static bool take_track_switch_command(music_player_msg_t *msg)
{
    uint32_t value = 0;
    if (msg == NULL || xTaskNotifyWait(0, UINT32_MAX, &value, 0) != pdTRUE) {
        return false;
    }
    msg->cmd = (music_player_cmd_t)(value >> 24);
    msg->index = (int)(value & 0x00FFFFFF) - 1;
    return is_track_switch_cmd(msg->cmd);
}

static void control_task(void *arg)
{
    (void)arg;
    music_player_msg_t msg = {0};
    while (s_ctrl_running) {
        if (take_track_switch_command(&msg)) {
            handle_command(&msg);
            flush_pending_ui_refresh();
            continue;
        }
        BaseType_t got = xQueueReceive(s_cmd_queue, &msg, pdMS_TO_TICKS(MUSIC_PLAYER_CTRL_IDLE_MS));
        if (got == pdTRUE) {
            if (msg.cmd == MUSIC_PLAYER_CMD_SHUTDOWN) {
                break;
            }
            handle_command(&msg);
        }
        flush_pending_ui_refresh();
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

    ESP_GMF_RET_ON_ERROR(TAG, register_extractors(), { ret = ESP_FAIL; goto err_cleanup;},
                         "Failed to register extractors");

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
    unregister_extractors();
    s_cmd_queue = NULL;
    s_codec = NULL;
    return ret;
}

esp_err_t music_player_playback_post(music_player_cmd_t cmd)
{
    esp_err_t ret = post_cmd_internal(cmd, -1, 0);
    if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Failed to post playback command: %d (queue full/timeout)", cmd);
    }
    return ret;
}

esp_err_t music_player_playback_post_index(music_player_cmd_t cmd, int index)
{
    esp_err_t ret = post_cmd_internal(cmd, index, 0);
    if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Failed to post play index: cmd=%d index=%d (queue full/timeout)", cmd, index);
    }
    return ret;
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

esp_err_t music_player_playback_get_progress(int *elapsed_ms, int *duration_ms)
{
    ESP_GMF_CHECK(TAG, elapsed_ms != NULL && duration_ms != NULL, return ESP_ERR_INVALID_ARG,
                  "Invalid progress buffers");

    _lock_acquire(&s_progress.lock);
    *elapsed_ms = progress_get_elapsed_ms_unlocked();
    *duration_ms = s_progress.duration_ms;
    _lock_release(&s_progress.lock);
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
        music_player_msg_t dummy;
        while (xQueueReceive(s_cmd_queue, &dummy, 0) == pdTRUE) {
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
    s_invalid_tracks = 0;
    reset_progress_state(0);
    progress_elapsed_pause();
    unregister_extractors();
}
