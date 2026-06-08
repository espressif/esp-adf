/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_gmf_err.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "esp_board_manager_includes.h"
#include "esp_cli_service.h"
#include "esp_media_db.h"
#include "esp_playlist.h"
#include "cli_music_control.h"

#define DEFAULT_SCAN_DIR       "/sdcard"
#define SCAN_MAX_DEPTH         1
#define DEFAULT_PLAYLIST_NAME  "playlist"
#define PLAYLIST_DB_DIR        "/sdcard/__playlist"
#define PLAYLIST_JSON_PATH     PLAYLIST_DB_DIR "/playlist.json"

static const char *SUPPORTED_AUDIO_EXT[] = {".mp3", ".wav", ".aac", ".m4a", ".flac", ".amr", ".ts"};
#define SUPPORTED_AUDIO_EXT_CNT  (sizeof(SUPPORTED_AUDIO_EXT) / sizeof(SUPPORTED_AUDIO_EXT[0]))

#define EXTRA_STREAM_URL_INIT_CAP  8

#define CLI_PLAYER_EVENT_QUEUE_SIZE   4
#define CLI_PLAYER_EVENT_TASK_STACK   4096
#define CLI_PLAYER_EVENT_TASK_PRIO    5
#define CLI_PLAYER_DEFAULT_VOLUME     80
#define CLI_PLAYER_VOLUME_MIN         0
#define CLI_PLAYER_VOLUME_MAX         100
#define CLI_PLAYER_POLL_INTERVAL_MS   20
#define CLI_PLAYER_SWITCH_TIMEOUT_MS  1000
#define CLI_PLAYER_DEINIT_TIMEOUT_MS  2000

typedef enum {
    CLI_PLAYER_EVENT_FINISHED = 0,
    CLI_PLAYER_EVENT_EXIT,
} cli_player_event_t;

typedef struct {
    char **urls;
    int    count;
    int    capacity;
} extra_stream_urls_t;

typedef struct {
    esp_asp_handle_t              simple_player;
    esp_cli_music_player_state_t  playback_state;
    int                           volume;
    bool                          is_muted;
} cli_player_snapshot_t;

typedef struct {
    esp_asp_handle_t              simple_player;
    esp_cli_music_player_state_t  playback_state;
    esp_cli_play_mode_t           play_mode;
    esp_media_db_handle_t         media_db;
    esp_playlist_handle_t         playlist;
    extra_stream_urls_t           extra_streams;
    int                           volume;
    bool                          is_muted;
    bool                          deiniting;
    SemaphoreHandle_t             mutex;
    QueueHandle_t                 event_queue;
    TaskHandle_t                  event_task;
    SemaphoreHandle_t             event_task_done;
} cli_music_player_ctx_t;

static const char *TAG = "CLI_MUSIC_CONTROL";

static cli_music_player_ctx_t *s_ctx = NULL;

static esp_err_t sync_playlist_from_media_db(void);
static esp_err_t media_db_scan_music_dir(void);
static esp_err_t media_db_add_extra_streams(void);
static esp_err_t extra_streams_add(const char *url, const char **stored_url);
static void extra_streams_clear(void);
static esp_err_t playlist_save(void);
static void apply_cli_play_mode_to_repeat(void);
static esp_err_t cli_player_get_snapshot(cli_player_snapshot_t *snapshot);
static void cli_player_event_task(void *arg);

static esp_err_t ensure_playlist_db_dir(void)
{
    struct stat st;
    if (stat(PLAYLIST_DB_DIR, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Path %s exists but is not a directory", PLAYLIST_DB_DIR);
        return ESP_FAIL;
    }
    if (errno != ENOENT) {
        ESP_LOGE(TAG, "Failed to stat %s (errno=%d)", PLAYLIST_DB_DIR, errno);
        return ESP_FAIL;
    }
    if (mkdir(PLAYLIST_DB_DIR, 0777) != 0 && errno != EEXIST) {
        ESP_LOGE(TAG, "Failed to create %s (errno=%d)", PLAYLIST_DB_DIR, errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static inline bool is_stream_uri(const char *uri)
{
    if (uri == NULL) {
        return false;
    }
    return (strncmp(uri, "http://", 7) == 0 || strncmp(uri, "https://", 8) == 0 ||
            strncmp(uri, "embed://", 8) == 0 ||
            strncmp(uri, "file://", 7) == 0);
}

static void extra_streams_clear(void)
{
    ESP_GMF_CHECK(TAG, s_ctx, return, "Player not initialized");

    for (int i = 0; i < s_ctx->extra_streams.count; i++) {
        free(s_ctx->extra_streams.urls[i]);
    }
    free(s_ctx->extra_streams.urls);
    s_ctx->extra_streams.urls = NULL;
    s_ctx->extra_streams.count = 0;
    s_ctx->extra_streams.capacity = 0;
}

static esp_err_t extra_streams_add(const char *url, const char **stored_url)
{
    ESP_GMF_CHECK(TAG, s_ctx, return ESP_ERR_INVALID_STATE, "Player not initialized");
    ESP_GMF_CHECK(TAG, url && url[0] != '\0', return ESP_ERR_INVALID_ARG, "URL is empty");

    for (int i = 0; i < s_ctx->extra_streams.count; i++) {
        if (strcmp(s_ctx->extra_streams.urls[i], url) == 0) {
            if (stored_url) {
                *stored_url = s_ctx->extra_streams.urls[i];
            }
            return ESP_OK;
        }
    }
    if (s_ctx->extra_streams.count >= s_ctx->extra_streams.capacity) {
        int new_cap = s_ctx->extra_streams.capacity > 0 ? s_ctx->extra_streams.capacity * 2 : EXTRA_STREAM_URL_INIT_CAP;
        char **next = (char **)realloc(s_ctx->extra_streams.urls, (size_t)new_cap * sizeof(char *));
        if (next == NULL) {
            ESP_LOGE(TAG, "Expand stream URL list failed");
            return ESP_ERR_NO_MEM;
        }
        s_ctx->extra_streams.urls = next;
        s_ctx->extra_streams.capacity = new_cap;
    }
    char *copy = strdup(url);
    if (copy == NULL) {
        ESP_LOGE(TAG, "Copy stream URL failed");
        return ESP_ERR_NO_MEM;
    }
    s_ctx->extra_streams.urls[s_ctx->extra_streams.count++] = copy;
    if (stored_url) {
        *stored_url = copy;
    }
    return ESP_OK;
}

static esp_err_t playlist_save(void)
{
    ESP_GMF_CHECK(TAG, s_ctx && s_ctx->playlist, return ESP_ERR_INVALID_STATE, "Playlist not initialized");
    return esp_playlist_save(s_ctx->playlist, PLAYLIST_JSON_PATH);
}

static esp_err_t sync_playlist_from_media_db(void)
{
    ESP_GMF_CHECK(TAG, s_ctx && s_ctx->playlist && s_ctx->media_db,
                  return ESP_ERR_INVALID_STATE, "Playlist not initialized");
    ESP_GMF_RET_ON_ERROR(TAG, esp_playlist_clean(s_ctx->playlist), return err_rc_, "playlist clean");
    esp_err_t ret = esp_playlist_import_media(s_ctx->playlist, s_ctx->media_db, NULL);
    if (ret == ESP_ERR_NOT_FOUND) {
        /* Media DB is empty: remove stale playlist JSON to avoid
         * inconsistent state across reboots. */
        remove(PLAYLIST_JSON_PATH);
        return ESP_OK;
    }
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "import media");
    return playlist_save();
}

static esp_err_t media_db_add_extra_streams(void)
{
    ESP_GMF_CHECK(TAG, s_ctx && s_ctx->media_db, return ESP_ERR_INVALID_STATE, "Media DB not initialized");
    for (int i = 0; i < s_ctx->extra_streams.count; i++) {
        const char *url = s_ctx->extra_streams.urls[i];
        const char *name = strrchr(url, '/');
        name = (name && name[1] != '\0') ? name + 1 : url;
        esp_media_db_item_t item = {
            .name = name,
            .url = url,
        };
        esp_err_t ret = esp_media_db_add(s_ctx->media_db, &item, 1);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to add stream URL to media DB: %s, %s", url, esp_err_to_name(ret));
        }
    }
    return ESP_OK;
}

static esp_err_t media_db_scan_music_dir(void)
{
    esp_media_db_scan_cfg_t scan_cfg = {
        .skip_duplicate = false,
        .path = DEFAULT_SCAN_DIR,
        .scan_depth = SCAN_MAX_DEPTH,
        .file_extensions = (const char *const *)SUPPORTED_AUDIO_EXT,
        .file_extension_count = (int)SUPPORTED_AUDIO_EXT_CNT,
        .filter_cb = NULL,
        .filter_cb_ctx = NULL,
    };
    return esp_media_db_scan(s_ctx->media_db, &scan_cfg);
}

static esp_err_t reload_media_db_and_playlist(void)
{
    ESP_GMF_RET_ON_ERROR(TAG, esp_media_db_clean(s_ctx->media_db), return err_rc_, "media db clean");
    esp_err_t ret = media_db_scan_music_dir();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Scan failed: %s", esp_err_to_name(ret));
    }
    ESP_GMF_RET_ON_ERROR(TAG, media_db_add_extra_streams(), return err_rc_, "add extra streams");
    return sync_playlist_from_media_db();
}

static int get_playlist_music_count(void)
{
    int count = 0;
    if (s_ctx && s_ctx->playlist) {
        esp_playlist_get_count(s_ctx->playlist, &count);
    }
    return count;
}

static int get_current_music_index(void)
{
    esp_playlist_info_t info = {0};
    if (s_ctx == NULL || s_ctx->playlist == NULL ||
        esp_playlist_curr(s_ctx->playlist, &info) != ESP_OK) {
        return -1;
    }
    return info.index;
}

static esp_err_t get_current_playlist_info(esp_playlist_info_t *info)
{
    ESP_GMF_CHECK(TAG, info, return ESP_ERR_INVALID_ARG, "Playlist info is NULL");
    ESP_GMF_CHECK(TAG, s_ctx && s_ctx->playlist, return ESP_ERR_INVALID_STATE, "Playlist not initialized");
    return esp_playlist_curr(s_ctx->playlist, info);
}

static esp_err_t get_playback_handle(esp_codec_dev_handle_t *handle)
{
    ESP_GMF_CHECK(TAG, handle, return ESP_ERR_INVALID_ARG, "Playback handle output is NULL");

    dev_audio_codec_handles_t *dac_dev_handle = NULL;
    esp_err_t ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&dac_dev_handle);
    ESP_GMF_RET_ON_ERROR(TAG, ret, return err_rc_, "Get playback device handle");
    ESP_GMF_CHECK(TAG, dac_dev_handle && dac_dev_handle->codec_dev,
                  return ESP_ERR_NOT_FOUND, "Playback codec handle is NULL");

    *handle = dac_dev_handle->codec_dev;
    return ESP_OK;
}

static esp_err_t cli_player_mute(void)
{
    ESP_GMF_CHECK(TAG, s_ctx, return ESP_ERR_INVALID_STATE, "Player not initialized");
    cli_player_snapshot_t snapshot = {0};
    esp_err_t ret = cli_player_get_snapshot(&snapshot);
    if (ret != ESP_OK) {
        return ret;
    }
    if (snapshot.is_muted) {
        return ESP_OK;
    }

    esp_codec_dev_handle_t playback_handle = NULL;
    ret = get_playback_handle(&playback_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_codec_dev_set_out_mute(playback_handle, true);
    if (ret == ESP_OK) {
        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        s_ctx->is_muted = true;
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGI(TAG, "Audio muted (volume remains: %d%%)", snapshot.volume);
    } else {
        ESP_LOGE(TAG, "Mute audio failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t cli_player_unmute(void)
{
    ESP_GMF_CHECK(TAG, s_ctx, return ESP_ERR_INVALID_STATE, "Player not initialized");
    cli_player_snapshot_t snapshot = {0};
    esp_err_t ret = cli_player_get_snapshot(&snapshot);
    if (ret != ESP_OK) {
        return ret;
    }
    if (!snapshot.is_muted) {
        return ESP_OK;
    }

    esp_codec_dev_handle_t playback_handle = NULL;
    ret = get_playback_handle(&playback_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_codec_dev_set_out_mute(playback_handle, false);
    if (ret == ESP_OK) {
        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        s_ctx->is_muted = false;
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGI(TAG, "Audio unmuted (volume: %d%%)", snapshot.volume);
    } else {
        ESP_LOGE(TAG, "Unmute audio failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static inline bool is_valid_uri(const char *uri)
{
    if (uri == NULL || strlen(uri) == 0) {
        return false;
    }
    return (strncmp(uri, "http://", 7) == 0 ||
            strncmp(uri, "https://", 8) == 0 ||
            strncmp(uri, "file://", 7) == 0 ||
            strncmp(uri, "embed://", 8) == 0 ||
            strncmp(uri, "raw://", 6) == 0);
}

static esp_err_t wait_player_idle(esp_asp_handle_t player, TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();
    while (true) {
        esp_asp_state_t state = ESP_ASP_STATE_NONE;
        esp_err_t ret = esp_audio_simple_player_get_state(player, &state);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Get player state failed: %s", esp_err_to_name(ret));
            return ret;
        }
        if (state != ESP_ASP_STATE_RUNNING && state != ESP_ASP_STATE_PAUSED) {
            return ESP_OK;
        }
        if ((xTaskGetTickCount() - start) >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(CLI_PLAYER_POLL_INTERVAL_MS));
    }
}

static esp_err_t cli_player_get_snapshot(cli_player_snapshot_t *snapshot)
{
    ESP_GMF_CHECK(TAG, s_ctx, return ESP_ERR_INVALID_STATE, "Player not initialized");
    ESP_GMF_CHECK(TAG, snapshot, return ESP_ERR_INVALID_ARG, "Player snapshot is NULL");

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    snapshot->simple_player = s_ctx->simple_player;
    snapshot->playback_state = s_ctx->playback_state;
    snapshot->volume = s_ctx->volume;
    snapshot->is_muted = s_ctx->is_muted;
    xSemaphoreGive(s_ctx->mutex);

    return ESP_OK;
}

static esp_err_t cli_player_play_url(const char *file_path, int display_index)
{
    ESP_GMF_CHECK(TAG, file_path && strlen(file_path) > 0, return ESP_FAIL, "File path is empty");

    char uri[512] = {0};
    if (file_path[0] == '/' && strstr(file_path, "://") == NULL) {
        snprintf(uri, sizeof(uri), "file://%s", file_path);
    } else if (strncmp(file_path, "file:/", 6) == 0 && strncmp(file_path, "file://", 7) != 0) {
        snprintf(uri, sizeof(uri), "file://%s", file_path + strlen("file:"));
    } else if (is_valid_uri(file_path)) {
        strncpy(uri, file_path, sizeof(uri) - 1);
        uri[sizeof(uri) - 1] = '\0';
    } else {
        strncpy(uri, file_path, sizeof(uri) - 1);
        uri[sizeof(uri) - 1] = '\0';
    }

    cli_player_snapshot_t snapshot = {0};
    esp_err_t ret = cli_player_get_snapshot(&snapshot);
    if (ret != ESP_OK) {
        return ret;
    }

    esp_asp_handle_t player = snapshot.simple_player;
    ESP_GMF_CHECK(TAG, player, return ESP_FAIL, "No player initialized");

    esp_asp_state_t state = ESP_ASP_STATE_NONE;
    ret = esp_audio_simple_player_get_state(player, &state);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get player state failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (state == ESP_ASP_STATE_RUNNING || state == ESP_ASP_STATE_PAUSED) {
        ret = esp_audio_simple_player_stop(player);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Stop player before switching failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ret = wait_player_idle(player, pdMS_TO_TICKS(CLI_PLAYER_SWITCH_TIMEOUT_MS));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Timed out waiting for player to stop before switching");
            return ret;
        }
    }

    ret = esp_audio_simple_player_run(player, uri, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start playing: %s (URI: %s)", file_path, uri);
        return ret;
    }

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    s_ctx->playback_state = ESP_CLI_MUSIC_PLAYER_STATE_PLAYING;
    int total = get_playlist_music_count();
    xSemaphoreGive(s_ctx->mutex);

    /* Unmute is best-effort: do not overwrite the playback result */
    esp_err_t unmute_ret = cli_player_unmute();
    if (unmute_ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unmute audio: %s", esp_err_to_name(unmute_ret));
    }

    if (display_index >= 0) {
        ESP_LOGI(TAG, "Playing song [%d/%d]: %s", display_index + 1, total, file_path);
    } else {
        ESP_LOGI(TAG, "Playing song [unknown/%d]: %s", total, file_path);
    }

    return ESP_OK;
}

static esp_err_t cli_player_pause(void)
{
    cli_player_snapshot_t snapshot = {0};
    esp_err_t ret = cli_player_get_snapshot(&snapshot);
    if (ret != ESP_OK) {
        return ret;
    }

    if (snapshot.playback_state != ESP_CLI_MUSIC_PLAYER_STATE_PLAYING) {
        ESP_LOGW(TAG, "No music playing to pause");
        return ESP_OK;
    }

    if (snapshot.simple_player) {
        ret = esp_audio_simple_player_pause(snapshot.simple_player);
    } else {
        ESP_LOGE(TAG, "No player initialized");
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK) {
        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        s_ctx->playback_state = ESP_CLI_MUSIC_PLAYER_STATE_PAUSED;
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGI(TAG, "Music paused");
    }

    return ret;
}

static esp_err_t cli_player_resume(void)
{
    cli_player_snapshot_t snapshot = {0};
    esp_err_t ret = cli_player_get_snapshot(&snapshot);
    if (ret != ESP_OK) {
        return ret;
    }

    if (snapshot.playback_state != ESP_CLI_MUSIC_PLAYER_STATE_PAUSED) {
        ESP_LOGW(TAG, "No music paused to resume");
        return ESP_OK;
    }

    if (snapshot.simple_player) {
        ret = esp_audio_simple_player_resume(snapshot.simple_player);
    } else {
        ESP_LOGE(TAG, "No player initialized");
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK) {
        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        s_ctx->playback_state = ESP_CLI_MUSIC_PLAYER_STATE_PLAYING;
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGI(TAG, "Music resumed");
    }

    return ret;
}

static esp_err_t cli_player_stop(void)
{
    cli_player_snapshot_t snapshot = {0};
    esp_err_t ret = cli_player_get_snapshot(&snapshot);
    if (ret != ESP_OK) {
        return ret;
    }

    if (snapshot.simple_player) {
        ret = esp_audio_simple_player_stop(snapshot.simple_player);
    } else {
        ESP_LOGE(TAG, "No player initialized");
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK) {
        ret = wait_player_idle(snapshot.simple_player, pdMS_TO_TICKS(CLI_PLAYER_SWITCH_TIMEOUT_MS));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Timed out waiting for player to stop");
        }
        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        s_ctx->playback_state = ESP_CLI_MUSIC_PLAYER_STATE_STOPPED;
        xSemaphoreGive(s_ctx->mutex);
        cli_player_mute();
    }

    return ret;
}

static esp_err_t cli_player_set_volume(int volume)
{
    ESP_GMF_CHECK(TAG, s_ctx, return ESP_ERR_INVALID_STATE, "Player not initialized");
    if (volume < CLI_PLAYER_VOLUME_MIN) {
        volume = CLI_PLAYER_VOLUME_MIN;
    }
    if (volume > CLI_PLAYER_VOLUME_MAX) {
        volume = CLI_PLAYER_VOLUME_MAX;
    }
    cli_player_snapshot_t snapshot = {0};
    esp_err_t ret = cli_player_get_snapshot(&snapshot);
    if (ret != ESP_OK) {
        return ret;
    }
    if (volume == snapshot.volume) {
        return ESP_OK;
    }

    esp_codec_dev_handle_t playback_handle = NULL;
    ret = get_playback_handle(&playback_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    ret = esp_codec_dev_set_out_vol(playback_handle, volume);
    if (ret == ESP_OK) {
        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        s_ctx->volume = volume;
        xSemaphoreGive(s_ctx->mutex);
    } else {
        ESP_LOGE(TAG, "Set volume failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void apply_cli_play_mode_to_repeat(void)
{
    ESP_GMF_CHECK(TAG, s_ctx && s_ctx->playlist, return, "Playlist not initialized");
    esp_playlist_repeat_mode_t mode = ESP_PLAYLIST_REPEAT_NONE;
    switch (s_ctx->play_mode) {
        case ESP_CLI_PLAY_MODE_SINGLE_LOOP:
            mode = ESP_PLAYLIST_REPEAT_ONE;
            break;
        case ESP_CLI_PLAY_MODE_LIST_LOOP:
            mode = ESP_PLAYLIST_REPEAT_ALL;
            break;
        case ESP_CLI_PLAY_MODE_STOP_AFTER:
        default:
            mode = ESP_PLAYLIST_REPEAT_NONE;
            break;
    }
    esp_playlist_set_repeat_mode(s_ctx->playlist, mode);
}

static esp_err_t cli_player_next(void)
{
    esp_playlist_info_t info = {0};

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    if (get_playlist_music_count() == 0) {
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGW(TAG, "No music files available");
        return ESP_FAIL;
    }
    /* Temporarily set REPEAT_ALL so that next/prev advances the index even
     * when the current play mode is SINGLE_LOOP (REPEAT_ONE). */
    esp_playlist_set_repeat_mode(s_ctx->playlist, ESP_PLAYLIST_REPEAT_ALL);
    esp_err_t ret = esp_playlist_next(s_ctx->playlist, &info);
    xSemaphoreGive(s_ctx->mutex);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get next playlist item failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return cli_player_play_url(info.media_url, info.index);
}

static esp_err_t cli_player_prev(void)
{
    esp_playlist_info_t info = {0};

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    if (get_playlist_music_count() == 0) {
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGW(TAG, "No music files available");
        return ESP_FAIL;
    }
    /* Temporarily set REPEAT_ALL so that next/prev advances the index even
     * when the current play mode is SINGLE_LOOP (REPEAT_ONE). */
    esp_playlist_set_repeat_mode(s_ctx->playlist, ESP_PLAYLIST_REPEAT_ALL);
    esp_err_t ret = esp_playlist_prev(s_ctx->playlist, &info);
    xSemaphoreGive(s_ctx->mutex);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Get previous playlist item failed: %s", esp_err_to_name(ret));
        return ret;
    }
    return cli_player_play_url(info.media_url, info.index);
}

static esp_err_t cli_player_add_source_url(const char *url)
{
    ESP_GMF_CHECK(TAG, url && strlen(url) > 0, return ESP_FAIL, "URL is empty");
    ESP_GMF_CHECK(TAG, s_ctx && s_ctx->media_db, return ESP_FAIL, "Media DB not initialized");

    if (!is_valid_uri(url)) {
        ESP_LOGE(TAG, "Invalid URI format: %s. Supported schemes: http://, https://, file://, embed://, raw://",
                 url);
        return ESP_FAIL;
    }

    const char *stored_url = url;
    if (is_stream_uri(url)) {
        ESP_GMF_RET_ON_ERROR(TAG, extra_streams_add(url, &stored_url), return err_rc_, "track stream url");
    }

    const char *name = strrchr(stored_url, '/');
    name = (name && name[1] != '\0') ? name + 1 : stored_url;
    esp_media_db_item_t item = {
        .name = name,
        .url = stored_url,
    };
    esp_err_t ret = esp_media_db_add(s_ctx->media_db, &item, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Add media DB item failed: %s, %s", stored_url, esp_err_to_name(ret));
        return ret;
    }
    int n = 0;
    esp_media_db_get_count(s_ctx->media_db, &n);
    ESP_LOGI(TAG, "Added to media DB (count=%d): %s", n, stored_url);
    return ESP_OK;
}

static void close_playlist_library(void)
{
    if (s_ctx && s_ctx->playlist) {
        playlist_save();
        esp_playlist_del(s_ctx->playlist);
        s_ctx->playlist = NULL;
    }
    if (s_ctx && s_ctx->media_db) {
        esp_media_db_deinit(s_ctx->media_db);
        s_ctx->media_db = NULL;
    }
    extra_streams_clear();
}

static struct {
    struct arg_int *index;
    struct arg_end *end;
} play_args;

static int cmd_play(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&play_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, play_args.end, argv[0]);
        return 1;
    }

    esp_playlist_info_t info = {0};
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    int total = get_playlist_music_count();
    if (total == 0) {
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGW(TAG, "No music files found. Please scan first.");
        return 1;
    }

    if (play_args.index->count > 0) {
        int file_index = play_args.index->ival[0];
        if (file_index < 1 || file_index > total) {
            xSemaphoreGive(s_ctx->mutex);
            ESP_LOGE(TAG, "Invalid file index: %d. Valid range: 1-%d", file_index, total);
            return 1;
        }
        ret = esp_playlist_get_info(s_ctx->playlist, file_index - 1, &info);
        if (ret != ESP_OK) {
            xSemaphoreGive(s_ctx->mutex);
            ESP_LOGE(TAG, "Get playlist item %d failed: %s", file_index, esp_err_to_name(ret));
            return 1;
        }
        ret = esp_playlist_set_curr_index(s_ctx->playlist, file_index - 1);
        xSemaphoreGive(s_ctx->mutex);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Set playlist index %d failed: %s", file_index, esp_err_to_name(ret));
            return 1;
        }
        ret = cli_player_play_url(info.media_url, info.index);
        return (ret == ESP_OK) ? 0 : 1;
    }

    ret = get_current_playlist_info(&info);
    xSemaphoreGive(s_ctx->mutex);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No current music selected");
        return 1;
    }
    ret = cli_player_play_url(info.media_url, info.index);
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_pause(int argc, char **argv)
{
    esp_err_t ret = cli_player_pause();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to pause music");
    }
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_resume(int argc, char **argv)
{
    esp_err_t ret = cli_player_resume();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to resume music");
    }
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_stop(int argc, char **argv)
{
    esp_err_t ret = cli_player_stop();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Music stopped");
    } else {
        ESP_LOGE(TAG, "Failed to stop music");
    }
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_get_vol(int argc, char **argv)
{
    ESP_GMF_CHECK(TAG, s_ctx, return 1, "Player not initialized");
    cli_player_snapshot_t snapshot = {0};
    esp_err_t ret = cli_player_get_snapshot(&snapshot);
    if (ret != ESP_OK) {
        return 1;
    }
    if (snapshot.is_muted) {
        ESP_LOGI(TAG, "Volume: %d%% (MUTED)", snapshot.volume);
    } else {
        ESP_LOGI(TAG, "Volume: %d%%", snapshot.volume);
    }
    return 0;
}

static struct {
    struct arg_int *volume;
    struct arg_end *end;
} set_vol_args;

static int cmd_set_vol(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&set_vol_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_vol_args.end, argv[0]);
        return 1;
    }

    if (set_vol_args.volume->count == 0) {
        ESP_LOGE(TAG, "Volume level required. Usage: set_vol <0-100>");
        return 1;
    }

    int vol = set_vol_args.volume->ival[0];
    if (vol < CLI_PLAYER_VOLUME_MIN) {
        vol = CLI_PLAYER_VOLUME_MIN;
    } else if (vol > CLI_PLAYER_VOLUME_MAX) {
        vol = CLI_PLAYER_VOLUME_MAX;
    }
    esp_err_t ret = cli_player_set_volume(vol);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Volume set to %d%%", vol);
    } else {
        ESP_LOGE(TAG, "Failed to set volume");
    }
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_mute(int argc, char **argv)
{
    ESP_GMF_CHECK(TAG, s_ctx, return 1, "Player not initialized");
    esp_err_t ret = cli_player_mute();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mute audio");
    }
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_unmute(int argc, char **argv)
{
    ESP_GMF_CHECK(TAG, s_ctx, return 1, "Player not initialized");
    esp_err_t ret = cli_player_unmute();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmute audio");
    }
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_next(int argc, char **argv)
{
    esp_err_t ret = cli_player_next();
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_prev(int argc, char **argv)
{
    esp_err_t ret = cli_player_prev();
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_scan(int argc, char **argv)
{
    ESP_GMF_CHECK(TAG, s_ctx, return 1, "Player not initialized");

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    if (s_ctx->playback_state == ESP_CLI_MUSIC_PLAYER_STATE_PLAYING ||
        s_ctx->playback_state == ESP_CLI_MUSIC_PLAYER_STATE_PAUSED) {
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGW(TAG, "Scan skipped: stop playback before scanning");
        return 1;
    }

    ESP_LOGI(TAG, "Starting scan of music directory: %s (stream URLs: %d)",
             DEFAULT_SCAN_DIR, s_ctx->extra_streams.count);

    esp_err_t ret = reload_media_db_and_playlist();
    if (ret != ESP_OK) {
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGE(TAG, "Scan failed: %s", esp_err_to_name(ret));
        return 1;
    }

    int db_count = 0;
    int pl_count = 0;
    esp_media_db_get_count(s_ctx->media_db, &db_count);
    esp_playlist_get_count(s_ctx->playlist, &pl_count);
    xSemaphoreGive(s_ctx->mutex);
    ESP_LOGI(TAG, "Scan completed: media_db=%d playlist=%d", db_count, pl_count);
    ESP_LOGI(TAG, "Music library ready! Use 'list' to see all files.");

    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_list(int argc, char **argv)
{
    ESP_GMF_CHECK(TAG, s_ctx, return 1, "Player not initialized");

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);

    int total = get_playlist_music_count();
    if (total == 0) {
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGW(TAG, "No music files found. Use 'scan' command first.");
        return 1;
    }

    int cur_idx = get_current_music_index();
    const char *file_scheme = "file://";
    const size_t file_scheme_len = 7;
    const char *adf_file_scheme = "file:/";
    const size_t adf_file_scheme_len = 6;

    ESP_LOGI(TAG, "=== Music File List (Total: %d) ===", total);

    for (int i = 0; i < total; i++) {
        esp_playlist_info_t info = {0};
        if (esp_playlist_get_info(s_ctx->playlist, i, &info) != ESP_OK) {
            ESP_LOGW(TAG, "Skip invalid playlist entry at index %d", i);
            continue;
        }
        const char *url = info.media_url;
        const char *display_path = url;
        const char *type_indicator = "[URL] ";
        if (strncmp(url, file_scheme, file_scheme_len) == 0) {
            type_indicator = "[FILE]";
            display_path = url + file_scheme_len;
        } else if (strncmp(url, adf_file_scheme, adf_file_scheme_len) == 0) {
            type_indicator = "[FILE]";
            display_path = url + strlen("file:");
        } else if (strncmp(url, "embed://", 8) == 0) {
            type_indicator = "[EMBED]";
        }
        char marker = (i == cur_idx) ? '*' : ' ';
        ESP_LOGI(TAG, "%c [%d] %s %s", marker, i + 1, type_indicator, display_path);
    }
    ESP_LOGI(TAG, "=============================\n");

    xSemaphoreGive(s_ctx->mutex);
    return 0;
}

static struct {
    struct arg_str *mode;
    struct arg_end *end;
} mode_args;

static int cmd_mode(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&mode_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, mode_args.end, argv[0]);
        return 1;
    }

    if (mode_args.mode->count > 0) {
        const char *arg = mode_args.mode->sval[0];
        if (strcmp(arg, "stop") == 0) {
            esp_cli_music_player_set_play_mode(ESP_CLI_PLAY_MODE_STOP_AFTER);
            ESP_LOGI(TAG, "Play mode: stop after current");
        } else if (strcmp(arg, "single") == 0) {
            esp_cli_music_player_set_play_mode(ESP_CLI_PLAY_MODE_SINGLE_LOOP);
            ESP_LOGI(TAG, "Play mode: single loop (repeat current)");
        } else if (strcmp(arg, "list") == 0) {
            esp_cli_music_player_set_play_mode(ESP_CLI_PLAY_MODE_LIST_LOOP);
            ESP_LOGI(TAG, "Play mode: list loop");
        } else {
            ESP_LOGE(TAG, "Unknown mode: %s. Use: stop | single | list", arg);
            return 1;
        }
    } else {
        const char *mode_str = "stop";
        esp_cli_play_mode_t mode = ESP_CLI_PLAY_MODE_STOP_AFTER;
        esp_err_t ret = esp_cli_music_player_get_play_mode(&mode);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Get play mode failed: %s", esp_err_to_name(ret));
            return 1;
        }
        switch (mode) {
            case ESP_CLI_PLAY_MODE_STOP_AFTER:
                mode_str = "stop";
                break;
            case ESP_CLI_PLAY_MODE_SINGLE_LOOP:
                mode_str = "single";
                break;
            case ESP_CLI_PLAY_MODE_LIST_LOOP:
                mode_str = "list";
                break;
        }
        ESP_LOGI(TAG, "Play mode: %s", mode_str);
    }
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    ESP_GMF_CHECK(TAG, s_ctx, return 1, "Player not initialized");

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);

    esp_playlist_info_t curr_info = {0};
    const char *filename = "N/A";
    if (get_current_playlist_info(&curr_info) == ESP_OK) {
        if (curr_info.media_name[0] != '\0') {
            filename = curr_info.media_name;
        } else if (curr_info.media_url[0] != '\0') {
            const char *slash = strrchr(curr_info.media_url, '/');
            filename = slash ? slash + 1 : curr_info.media_url;
        }
    }

    const char *state_str = "Unknown";
    switch (s_ctx->playback_state) {
        case ESP_CLI_MUSIC_PLAYER_STATE_STOPPED:
            state_str = "Stopped";
            break;
        case ESP_CLI_MUSIC_PLAYER_STATE_PLAYING:
            state_str = "Playing";
            break;
        case ESP_CLI_MUSIC_PLAYER_STATE_PAUSED:
            state_str = "Paused";
            break;
        case ESP_CLI_MUSIC_PLAYER_STATE_INITIALIZED:
            state_str = "Initialized";
            break;
    }

    const char *mode_str = "stop";
    switch (s_ctx->play_mode) {
        case ESP_CLI_PLAY_MODE_STOP_AFTER:
            mode_str = "stop";
            break;
        case ESP_CLI_PLAY_MODE_SINGLE_LOOP:
            mode_str = "single";
            break;
        case ESP_CLI_PLAY_MODE_LIST_LOOP:
            mode_str = "list";
            break;
    }

    int total = get_playlist_music_count();
    int cur_idx = get_current_music_index();

    ESP_LOGI(TAG, "=== Music Player Status ===");
    ESP_LOGI(TAG, "State: %s", state_str);
    ESP_LOGI(TAG, "Play mode: %s (stop=after current, single=repeat one, list=loop list)", mode_str);
    if (s_ctx->is_muted) {
        ESP_LOGI(TAG, "Volume: %d%% (MUTED)", s_ctx->volume);
    } else {
        ESP_LOGI(TAG, "Volume: %d%%", s_ctx->volume);
    }
    ESP_LOGI(TAG, "Total songs: %d", total);
    if (total > 0) {
        ESP_LOGI(TAG, "Current: [%d/%d] %s", cur_idx + 1, total, filename);
    }
    ESP_LOGI(TAG, "===========================\n");

    xSemaphoreGive(s_ctx->mutex);
    return 0;
}

esp_err_t esp_cli_music_player_register_commands(esp_cli_service_t *cli)
{
    ESP_GMF_CHECK(TAG, cli, return ESP_ERR_INVALID_ARG, "CLI is NULL");

    play_args.index = arg_int0(NULL, NULL, "<1-N>", "File index from list (optional)");
    play_args.end = arg_end(2);
    ESP_GMF_CHECK(TAG, play_args.index && play_args.end, return ESP_ERR_NO_MEM,
                  "Failed to allocate argtable for play command");
    const esp_console_cmd_t play_cmd = {
        .command = "play",
        .help = "Start playing current or specified song",
        .hint = NULL,
        .func = &cmd_play,
        .argtable = &play_args};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &play_cmd), return err_rc_,
                         "Failed to register play command");

    const esp_console_cmd_t pause_cmd = {
        .command = "pause",
        .help = "Pause current song",
        .hint = NULL,
        .func = &cmd_pause,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &pause_cmd), return err_rc_,
                         "Failed to register pause command");

    const esp_console_cmd_t resume_cmd = {
        .command = "resume",
        .help = "Resume paused song",
        .hint = NULL,
        .func = &cmd_resume,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &resume_cmd), return err_rc_,
                         "Failed to register resume command");

    const esp_console_cmd_t stop_cmd = {
        .command = "stop",
        .help = "Stop current song",
        .hint = NULL,
        .func = &cmd_stop,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &stop_cmd), return err_rc_,
                         "Failed to register stop command");

    const esp_console_cmd_t get_vol_cmd = {
        .command = "get_vol",
        .help = "Get current volume level",
        .hint = NULL,
        .func = &cmd_get_vol,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &get_vol_cmd), return err_rc_,
                         "Failed to register get_vol command");

    set_vol_args.volume = arg_int1(NULL, NULL, "<0-100>", "Volume level (0-100)");
    set_vol_args.end = arg_end(2);
    ESP_GMF_CHECK(TAG, set_vol_args.volume && set_vol_args.end, return ESP_ERR_NO_MEM,
                  "Failed to allocate argtable for set_vol command");
    const esp_console_cmd_t set_vol_cmd = {
        .command = "set_vol",
        .help = "Set volume level (0-100)",
        .hint = NULL,
        .func = &cmd_set_vol,
        .argtable = &set_vol_args};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &set_vol_cmd), return err_rc_,
                         "Failed to register set_vol command");

    const esp_console_cmd_t mute_cmd = {
        .command = "mute",
        .help = "Mute audio output",
        .hint = NULL,
        .func = &cmd_mute,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &mute_cmd), return err_rc_,
                         "Failed to register mute command");

    const esp_console_cmd_t unmute_cmd = {
        .command = "unmute",
        .help = "Unmute audio output",
        .hint = NULL,
        .func = &cmd_unmute,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &unmute_cmd), return err_rc_,
                         "Failed to register unmute command");

    const esp_console_cmd_t next_cmd = {
        .command = "next",
        .help = "Play next song",
        .hint = NULL,
        .func = &cmd_next,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &next_cmd), return err_rc_,
                         "Failed to register next command");

    const esp_console_cmd_t prev_cmd = {
        .command = "prev",
        .help = "Play previous song",
        .hint = NULL,
        .func = &cmd_prev,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &prev_cmd), return err_rc_,
                         "Failed to register prev command");

    const esp_console_cmd_t scan_cmd = {
        .command = "scan",
        .help = "Scan for music files in SD card",
        .hint = NULL,
        .func = &cmd_scan,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &scan_cmd), return err_rc_,
                         "Failed to register scan command");

    const esp_console_cmd_t list_cmd = {
        .command = "list",
        .help = "List all music files",
        .hint = NULL,
        .func = &cmd_list,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &list_cmd), return err_rc_,
                         "Failed to register list command");

    const esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Show current player status",
        .hint = NULL,
        .func = &cmd_status,
        .argtable = NULL};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &status_cmd), return err_rc_,
                         "Failed to register status command");

    mode_args.mode = arg_str0(NULL, NULL, "<stop|single|list>",
                              "Play mode when song ends (optional: show current if omitted)");
    mode_args.end = arg_end(2);
    ESP_GMF_CHECK(TAG, mode_args.mode && mode_args.end, return ESP_ERR_NO_MEM,
                  "Failed to allocate argtable for mode command");
    const esp_console_cmd_t mode_cmd = {
        .command = "mode",
        .help = "Get or set play mode: stop (after current), single (repeat one), list (loop list)",
        .hint = NULL,
        .func = &cmd_mode,
        .argtable = &mode_args};
    ESP_GMF_RET_ON_ERROR(TAG, esp_cli_service_register_static_command(cli, &mode_cmd), return err_rc_,
                         "Failed to register mode command");
    return ESP_OK;
}

void esp_cli_music_player_set_simple_player(esp_asp_handle_t player)
{
    if (s_ctx) {
        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        s_ctx->simple_player = player;
        xSemaphoreGive(s_ctx->mutex);
    }
}

void esp_cli_music_player_set_playback_state(esp_cli_music_player_state_t state)
{
    ESP_GMF_CHECK(TAG, s_ctx, return, "Player not initialized");
    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    s_ctx->playback_state = state;
    xSemaphoreGive(s_ctx->mutex);
}

void esp_cli_music_player_on_playback_finished(void)
{
    ESP_GMF_CHECK(TAG, s_ctx && s_ctx->event_queue, return, "Player event queue not initialized");
    cli_player_event_t event = CLI_PLAYER_EVENT_FINISHED;
    if (xQueueSend(s_ctx->event_queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Drop playback finished event: queue full");
    }
}

static void cli_player_handle_finished(void)
{
    ESP_GMF_CHECK(TAG, s_ctx, return, "Player not initialized");

    char *next_url = NULL;
    int next_index = -1;
    bool should_mute = false;

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    if (s_ctx->deiniting || s_ctx->playlist == NULL || get_playlist_music_count() == 0) {
        xSemaphoreGive(s_ctx->mutex);
        return;
    }

    switch (s_ctx->play_mode) {
        case ESP_CLI_PLAY_MODE_STOP_AFTER:
            s_ctx->playback_state = ESP_CLI_MUSIC_PLAYER_STATE_STOPPED;
            should_mute = true;
            break;
        case ESP_CLI_PLAY_MODE_SINGLE_LOOP: {
            esp_playlist_info_t info = {0};
            esp_err_t ret = esp_playlist_curr(s_ctx->playlist, &info);
            if (ret == ESP_OK) {
                next_url = strdup(info.media_url);
                next_index = info.index;
            } else {
                ESP_LOGW(TAG, "Get current playlist item failed: %s", esp_err_to_name(ret));
                s_ctx->playback_state = ESP_CLI_MUSIC_PLAYER_STATE_STOPPED;
                should_mute = true;
            }
            break;
        }
        case ESP_CLI_PLAY_MODE_LIST_LOOP: {
            esp_playlist_set_repeat_mode(s_ctx->playlist, ESP_PLAYLIST_REPEAT_ALL);
            esp_playlist_info_t info = {0};
            esp_err_t ret = esp_playlist_next(s_ctx->playlist, &info);
            if (ret == ESP_OK) {
                next_url = strdup(info.media_url);
                next_index = info.index;
            } else {
                ESP_LOGW(TAG, "Get next playlist item failed: %s", esp_err_to_name(ret));
                s_ctx->playback_state = ESP_CLI_MUSIC_PLAYER_STATE_STOPPED;
                should_mute = true;
            }
            break;
        }
        default:
            break;
    }

    xSemaphoreGive(s_ctx->mutex);

    if (next_index >= 0 && next_url == NULL) {
        ESP_LOGE(TAG, "Copy next playlist URL failed");
        return;
    }

    if (should_mute) {
        cli_player_mute();
    }

    if (next_url != NULL) {
        esp_err_t ret = cli_player_play_url(next_url, next_index);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Auto-play failed: %s", esp_err_to_name(ret));
            /* Update state to STOPPED so that scan/add-URL operations are not blocked */
            xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
            s_ctx->playback_state = ESP_CLI_MUSIC_PLAYER_STATE_STOPPED;
            xSemaphoreGive(s_ctx->mutex);
            cli_player_mute();
        }
        free(next_url);
    }
}

static void cli_player_event_task(void *arg)
{
    cli_music_player_ctx_t *ctx = (cli_music_player_ctx_t *)arg;
    cli_player_event_t event;

    while (xQueueReceive(ctx->event_queue, &event, portMAX_DELAY) == pdTRUE) {
        if (event == CLI_PLAYER_EVENT_EXIT) {
            break;
        }
        if (event == CLI_PLAYER_EVENT_FINISHED) {
            cli_player_handle_finished();
        }
    }

    if (ctx->event_task_done) {
        xSemaphoreGive(ctx->event_task_done);
    }
    vTaskDelete(NULL);
}

static void cli_player_stop_event_task(void)
{
    ESP_GMF_CHECK(TAG, s_ctx && s_ctx->event_task, return, "Player event task not running");

    cli_player_event_t event = CLI_PLAYER_EVENT_EXIT;
    (void)xQueueSend(s_ctx->event_queue, &event, portMAX_DELAY);
    if (s_ctx->event_task_done) {
        (void)xSemaphoreTake(s_ctx->event_task_done, portMAX_DELAY);
    }
    s_ctx->event_task = NULL;
}

void esp_cli_music_player_set_play_mode(esp_cli_play_mode_t mode)
{
    if (s_ctx) {
        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        s_ctx->play_mode = mode;
        apply_cli_play_mode_to_repeat();
        xSemaphoreGive(s_ctx->mutex);
    }
}

esp_err_t esp_cli_music_player_get_play_mode(esp_cli_play_mode_t *mode)
{
    ESP_GMF_CHECK(TAG, mode, return ESP_ERR_INVALID_ARG, "Play mode buffer is NULL");
    *mode = ESP_CLI_PLAY_MODE_STOP_AFTER;
    ESP_GMF_CHECK(TAG, s_ctx, return ESP_ERR_INVALID_STATE, "Player not initialized");
    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    *mode = s_ctx->play_mode;
    xSemaphoreGive(s_ctx->mutex);
    return ESP_OK;
}

esp_err_t esp_cli_music_player_init(void)
{
    esp_err_t ret = ESP_OK;
    bool event_task_started = false;

    s_ctx = calloc(1, sizeof(cli_music_player_ctx_t));
    if (s_ctx == NULL) {
        ESP_LOGE(TAG, "Failed to allocate music context");
        return ESP_ERR_NO_MEM;
    }

    s_ctx->simple_player = NULL;
    s_ctx->playback_state = ESP_CLI_MUSIC_PLAYER_STATE_INITIALIZED;
    s_ctx->play_mode = ESP_CLI_PLAY_MODE_STOP_AFTER;
    s_ctx->volume = CLI_PLAYER_DEFAULT_VOLUME;
    s_ctx->is_muted = false;
    s_ctx->media_db = NULL;
    s_ctx->playlist = NULL;
    s_ctx->extra_streams.urls = NULL;
    s_ctx->extra_streams.count = 0;
    s_ctx->extra_streams.capacity = 0;

    s_ctx->mutex = xSemaphoreCreateMutex();
    if (s_ctx->mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    s_ctx->event_queue = xQueueCreate(CLI_PLAYER_EVENT_QUEUE_SIZE, sizeof(cli_player_event_t));
    s_ctx->event_task_done = xSemaphoreCreateBinary();
    if (s_ctx->event_queue == NULL || s_ctx->event_task_done == NULL) {
        ESP_LOGE(TAG, "Failed to create event task resources");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    if (xTaskCreate(cli_player_event_task, "cli_player_evt", CLI_PLAYER_EVENT_TASK_STACK, s_ctx,
                    CLI_PLAYER_EVENT_TASK_PRIO, &s_ctx->event_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create event task");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    event_task_started = true;

    /* Check playback codec early to avoid wasting resources on scan/playlist
     * setup when the audio hardware is not available. */
    esp_codec_dev_handle_t playback_handle = NULL;
    ret = get_playback_handle(&playback_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get playback codec handle");
        goto cleanup;
    }

    if (ensure_playlist_db_dir() != ESP_OK) {
        ret = ESP_FAIL;
        goto cleanup;
    }

    const esp_media_db_cfg_t db_cfg = {
        .storage_type = ESP_DB_STORAGE_FS,
        .storage_path = PLAYLIST_DB_DIR,
    };
    if (esp_media_db_init(&db_cfg, &s_ctx->media_db) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize media DB");
        ret = ESP_FAIL;
        goto cleanup;
    }

    if (esp_media_db_load(s_ctx->media_db) != ESP_OK) {
        ESP_LOGW(TAG, "Media DB load failed, starting with empty catalog");
    }

    const esp_playlist_cfg_t playlist_cfg = {
        .playlist_name = DEFAULT_PLAYLIST_NAME,
    };
    if (esp_playlist_new(&playlist_cfg, &s_ctx->playlist) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create playlist");
        ret = ESP_FAIL;
        goto cleanup;
    }

    int count = 0;
    if (esp_playlist_load(s_ctx->playlist, PLAYLIST_JSON_PATH) == ESP_OK) {
        esp_playlist_get_count(s_ctx->playlist, &count);
    }
    if (count == 0) {
        int music_count = 0;
        esp_media_db_get_count(s_ctx->media_db, &music_count);
        if (music_count == 0) {
            ESP_LOGI(TAG, "No media DB at %s, scanning: %s", PLAYLIST_DB_DIR, DEFAULT_SCAN_DIR);
            if (reload_media_db_and_playlist() != ESP_OK) {
                ESP_LOGW(TAG, "Initial scan failed");
            }
        } else {
            ESP_LOGI(TAG, "Loaded %d entries from media DB, importing playlist", music_count);
            if (sync_playlist_from_media_db() != ESP_OK) {
                ESP_LOGW(TAG, "Failed to import playlist from media DB");
            }
        }
        count = get_playlist_music_count();
        if (count > 0) {
            ESP_LOGI(TAG, "Playlist ready with %d entries (saved to %s)", count, PLAYLIST_JSON_PATH);
        }
    } else {
        ESP_LOGI(TAG, "Loaded %d entries from playlist JSON (skip import)", count);
    }
    apply_cli_play_mode_to_repeat();
    if (count > 0) {
        ESP_LOGI(TAG, "Music library ready! Use 'list' to see all files.");
    }

    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "     ESP-GMF Music Player - CLI Control");
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "Available commands:");
    ESP_LOGI(TAG, "  scan      - Scan for music files in SD card");
    ESP_LOGI(TAG, "  list      - List all music files and URLs");
    ESP_LOGI(TAG, "  play [N]  - Start playing current or specified song (N=file index)");
    ESP_LOGI(TAG, "  pause     - Pause current song");
    ESP_LOGI(TAG, "  resume    - Resume paused song");
    ESP_LOGI(TAG, "  stop      - Stop current song");
    ESP_LOGI(TAG, "  next      - Play next song");
    ESP_LOGI(TAG, "  prev      - Play previous song");
    ESP_LOGI(TAG, "  get_vol   - Get current volume level");
    ESP_LOGI(TAG, "  set_vol   - Set volume level (0-100)");
    ESP_LOGI(TAG, "  mute      - Mute audio output");
    ESP_LOGI(TAG, "  unmute    - Unmute audio output");
    ESP_LOGI(TAG, "  status    - Show current player status");
    ESP_LOGI(TAG, "  mode [stop|single|list] - Get/set play mode (stop/single loop/list loop)");
    ESP_LOGI(TAG, "  help      - Show help information");
    ESP_LOGI(TAG, "  restart   - Restart system");
    ESP_LOGI(TAG, "  free      - Show free memory");
    ESP_LOGI(TAG, "================================================");

    if (count > 0) {
        ESP_LOGI(TAG, "Found %d music files. Use 'list' to view them.", count);
        ESP_LOGI(TAG, "Use 'play' to start playing the first song.");
    } else {
        ESP_LOGW(TAG, "No music files found. Use 'scan' to search for files.");
    }
    ESP_LOGI(TAG, "================================================\n");

    /* Codec handle already validated early in this function */
    {
        esp_codec_dev_handle_t ph = NULL;
        if (get_playback_handle(&ph) == ESP_OK) {
            esp_codec_dev_get_out_vol(ph, &s_ctx->volume);
        }
    }
    cli_player_mute();
    return ESP_OK;

cleanup:
    if (s_ctx) {
        if (event_task_started) {
            cli_player_stop_event_task();
        }
        if (s_ctx->playlist) {
            esp_playlist_del(s_ctx->playlist);
            s_ctx->playlist = NULL;
        }
        if (s_ctx->media_db) {
            esp_media_db_deinit(s_ctx->media_db);
            s_ctx->media_db = NULL;
        }
        extra_streams_clear();
        if (s_ctx->event_task_done) {
            vSemaphoreDelete(s_ctx->event_task_done);
            s_ctx->event_task_done = NULL;
        }
        if (s_ctx->event_queue) {
            vQueueDelete(s_ctx->event_queue);
            s_ctx->event_queue = NULL;
        }
        if (s_ctx->mutex) {
            vSemaphoreDelete(s_ctx->mutex);
            s_ctx->mutex = NULL;
        }
        free(s_ctx);
        s_ctx = NULL;
    }
    return ret;
}

void esp_cli_music_player_deinit(void)
{
    if (s_ctx) {
        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        s_ctx->deiniting = true;
        esp_asp_handle_t player = s_ctx->simple_player;
        xSemaphoreGive(s_ctx->mutex);

        if (player) {
            (void)esp_audio_simple_player_stop(player);
            (void)wait_player_idle(player, pdMS_TO_TICKS(CLI_PLAYER_DEINIT_TIMEOUT_MS));
        }

        cli_player_stop_event_task();

        xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
        close_playlist_library();
        xSemaphoreGive(s_ctx->mutex);

        if (s_ctx->event_task_done) {
            vSemaphoreDelete(s_ctx->event_task_done);
            s_ctx->event_task_done = NULL;
        }
        if (s_ctx->event_queue) {
            vQueueDelete(s_ctx->event_queue);
            s_ctx->event_queue = NULL;
        }
        if (s_ctx->mutex) {
            vSemaphoreDelete(s_ctx->mutex);
            s_ctx->mutex = NULL;
        }
        free(s_ctx);
        s_ctx = NULL;
    }
}

esp_err_t esp_cli_music_player_add_source_urls(const char **urls, int count)
{
    ESP_GMF_CHECK(TAG, urls != NULL, return ESP_ERR_INVALID_ARG, "URLs array is NULL");
    ESP_GMF_CHECK(TAG, count > 0, return ESP_ERR_INVALID_ARG, "URL count must be positive");
    ESP_GMF_CHECK(TAG, s_ctx && s_ctx->playlist && s_ctx->media_db,
                  return ESP_FAIL, "Playlist not initialized");

    ESP_LOGI(TAG, "Adding %d source URLs to media DB and playlist...", count);

    xSemaphoreTake(s_ctx->mutex, portMAX_DELAY);
    if (s_ctx->playback_state == ESP_CLI_MUSIC_PLAYER_STATE_PLAYING ||
        s_ctx->playback_state == ESP_CLI_MUSIC_PLAYER_STATE_PAUSED) {
        xSemaphoreGive(s_ctx->mutex);
        ESP_LOGW(TAG, "Add source URLs skipped: stop playback before updating playlist");
        return ESP_ERR_INVALID_STATE;
    }

    int success_count = 0;
    for (int i = 0; i < count; i++) {
        if (urls[i] == NULL) {
            ESP_LOGW(TAG, "Skipping NULL URL at index %d", i);
            continue;
        }
        if (cli_player_add_source_url(urls[i]) == ESP_OK) {
            success_count++;
        } else {
            ESP_LOGE(TAG, "Failed to add URL at index %d: %s", i, urls[i]);
        }
    }
    esp_err_t ret = ESP_FAIL;
    if (success_count > 0) {
        ret = sync_playlist_from_media_db();
    }
    xSemaphoreGive(s_ctx->mutex);

    ESP_LOGI(TAG, "Successfully added %d/%d source URLs (playlist count=%d)",
             success_count, count, get_playlist_music_count());
    return (success_count > 0 && ret == ESP_OK) ? ESP_OK : ESP_FAIL;
}
