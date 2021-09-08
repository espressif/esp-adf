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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "audio_mem.h"
#include "audio_error.h"
#include "audio_mutex.h"
#include "audio_player.h"
#include "audio_player_type.h"
#include "playlist.h"
#include "sys/queue.h"
#include "audio_thread.h"
#include "audio_player_manager.h"
#include "esp_event_cast.h"

const static int EP_TSK_INIT_BIT                = BIT0;
const static int EP_TSK_DELETE_BIT              = BIT1;
const static int EP_TSK_PLAY_SYNC_TONE_BIT      = BIT2;
const static int EP_TSK_PLAY_SYNC_RUNNING_BIT   = BIT3;
const static int EP_TSK_PLAY_SYNC_PAUSED_BIT    = BIT4;
const static int EP_TSK_PLAY_SYNC_STOPPED_BIT   = BIT5;
const static int EP_TSK_PLAY_SYNC_FINISHED_BIT  = BIT6;
const static int EP_TSK_PLAY_SYNC_ERROR_BIT     = BIT7;

#define AM_ACTION_TIMEOUT                       (4000)


typedef struct ap_ops_item {
    STAILQ_ENTRY(ap_ops_item)   next;
    ap_ops_t                    *func;
    bool                        used;
} ap_ops_item_t;

typedef STAILQ_HEAD(ap_ops_list, ap_ops_item) ap_ops_list_t;

typedef struct audio_player_impl {
    esp_audio_handle_t              audio_handle;
    xSemaphoreHandle                lock_handle;
    audio_thread_t                  tsk_handle;
    QueueHandle_t                   sync_state_que;
    QueueHandle_t                   cmd_que;
    EventGroupHandle_t              sync_state;
    QueueSetHandle_t                set_que;
    esp_audio_info_t                backup_info;
    esp_event_cast_handle_t         event_cast;
    bool                            is_backup;
    ap_ops_list_t                   ops_list;
    ap_ops_t                        *cur_ops;
    audio_player_evt_callback       evt_cb;
    void                            *evt_ctx;
    playlist_handle_t               music_list;
    audio_player_mode_t             mode;
    audio_player_state_t            st;
    bool                            prepare_playing;
    bool                            is_abort_playing;
} audio_player_impl_t;

static const char *TAG = "AUDIO_MANAGER";
static audio_player_impl_t *s_player;

static const char *ESP_AUDIO_STATUS_STRING[] = {
    "UNKNOWN",
    "RUNNING",
    "PAUSED",
    "STOPPED",
    "FINISHED",
    "ERROR",
    "NULL",
};

typedef struct {
    int                 type;
    uint32_t            *pdata;
    int                 len;
} player_tsk_msg_t;

static esp_err_t play_mode_select(audio_player_mode_t mode, playlist_handle_t music_list, media_source_type_t type)
{
    esp_err_t ret = ESP_OK;
    if (music_list == NULL) {
        ESP_LOGW(TAG, "music_list is null");
        return ESP_FAIL;
    }
    char *url = NULL;
    switch (mode) {
        case AUDIO_PLAYER_MODE_REPEAT: {
                ESP_LOGI(TAG, "AUDIO_PLAYER_MODE_REPEAT");
                if (ESP_OK != playlist_get_current_list_url(music_list, &url)) {
                    ESP_LOGW(TAG, "playlist_get_current_list_url return failure");
                    ap_manager_event_request(AUDIO_PLAYER_STATUS_ERROR, ESP_ERR_AUDIO_PLAYLIST_ERROR, type);
                }
                break;
            }
        case AUDIO_PLAYER_MODE_SEQUENCE: {
                ESP_LOGI(TAG, "AUDIO_PLAYER_MODE_SEQUENCE");
                if (playlist_next(music_list, 1, &url) == ESP_OK) {
                    int k = playlist_get_current_list_url_id(music_list);
                    ESP_LOGI(TAG, "Playing index:%d, num:%d", k, playlist_get_current_list_url_num(music_list));
                    if (k == 0) {
                        ap_manager_event_request(AUDIO_PLAYER_STATUS_PLAYLIST_FINISHED, ESP_ERR_AUDIO_NO_ERROR, type);
                        url = NULL;
                        ESP_LOGI(TAG, "List SEQUENCE done");
                    }
                } else {
                    ESP_LOGW(TAG, "playlist_next return failure");
                    ap_manager_event_request(AUDIO_PLAYER_STATUS_ERROR, ESP_ERR_AUDIO_PLAYLIST_ERROR, type);
                }
            }
            break;
        case AUDIO_PLAYER_MODE_SHUFFLE: {
                ESP_LOGI(TAG, "AUDIO_PLAYER_MODE_SHUFFLE");
                int n = esp_random() % (playlist_get_current_list_url_num(music_list) - 1);
                if (playlist_choose(music_list, n, &url) != ESP_OK) {
                    ESP_LOGW(TAG, "playlist_choose return failure");
                    ap_manager_event_request(AUDIO_PLAYER_STATUS_ERROR, ESP_ERR_AUDIO_PLAYLIST_ERROR, type);
                }
            }
            break;
        case AUDIO_PLAYER_MODE_ONE_SONG:
            ESP_LOGI(TAG, "AUDIO_PLAYER_MODE_ONE_SONG");
            break;
        default:
            ESP_LOGW(TAG, "Unsupported play mode, %d", mode);
            break;
    }
    if (url) {
        ret = audio_player_music_play(url, 0, type);
        if (ret != ESP_ERR_AUDIO_NO_ERROR) {
            ap_manager_event_request(AUDIO_PLAYER_STATUS_ERROR, ret, type);
        }
    }
    return ret;
}

static void audio_player_manager_task (void *para)
{
    audio_player_impl_t *player = (audio_player_impl_t *) para;
    QueueSetMemberHandle_t activeMember = NULL;
    esp_audio_state_t st = {0};
    bool tsk_run = true;
    xEventGroupSetBits(player->sync_state, EP_TSK_INIT_BIT);
    while (tsk_run) {
        activeMember = xQueueSelectFromSet(player->set_que, portMAX_DELAY);
        if (activeMember == player->sync_state_que) {
            xQueueReceive(player->sync_state_que, &st, 0);
            ESP_LOGI(TAG, "AUDIO MANAGER RECV STATUS:%s, err:%x, media_src:%x", ESP_AUDIO_STATUS_STRING[st.status], st.err_msg, st.media_src);
            if (player && player->evt_cb && (player->cur_ops->attr.blocked == false)) {
                ap_manager_event_request(st.status, st.err_msg, st.media_src);
            }
            bool is_done = false;
            switch (st.status) {
                case AUDIO_STATUS_UNKNOWN: {
                        break;
                    }
                case AUDIO_STATUS_RUNNING: {
                        xEventGroupSetBits(player->sync_state, EP_TSK_PLAY_SYNC_RUNNING_BIT);
                        break;
                    }
                case AUDIO_STATUS_PAUSED: {
                        xEventGroupSetBits(player->sync_state, EP_TSK_PLAY_SYNC_PAUSED_BIT);
                        break;
                    }
                case AUDIO_STATUS_STOPPED: {
                        ESP_LOGI(TAG, "AUDIO_STATUS_STOPPED, resume:%d, is_backup:%d, prepare_playing:%d", player->cur_ops->attr.auto_resume, player->is_backup, s_player->prepare_playing);
                        if (player->cur_ops->attr.blocked == true) {
                            xEventGroupSetBits(player->sync_state, EP_TSK_PLAY_SYNC_TONE_BIT);
                        }
                        xEventGroupSetBits(player->sync_state, EP_TSK_PLAY_SYNC_STOPPED_BIT);
                        break;
                    }
                case AUDIO_STATUS_ERROR:
                    if (player->cur_ops->attr.blocked == true) {
                        xEventGroupSetBits(player->sync_state, EP_TSK_PLAY_SYNC_TONE_BIT);
                    }
                    xEventGroupSetBits(player->sync_state, EP_TSK_PLAY_SYNC_ERROR_BIT);
                    ESP_LOGI(TAG, "AUDIO_STATUS_ERROR, resume:%d, is_backup:%d, prepare_playing:%d", player->cur_ops->attr.auto_resume, player->is_backup, s_player->prepare_playing);
                    is_done = true;
                    break;
                case AUDIO_STATUS_FINISHED:
                    if (player->cur_ops->attr.blocked == true) {
                        xEventGroupSetBits(player->sync_state, EP_TSK_PLAY_SYNC_TONE_BIT);
                    }
                    xEventGroupSetBits(player->sync_state, EP_TSK_PLAY_SYNC_FINISHED_BIT);

                    ESP_LOGI(TAG, "AUDIO_STATUS_FINISHED, resume:%d, is_backup:%d, prepare_playing:%d", player->cur_ops->attr.auto_resume, player->is_backup, s_player->prepare_playing);
                    is_done = true;
                    break;
                default: break;
            }
            if (is_done) {
                bool cur_auto_resume = false;
                // player->is_backup = false;
                // if (player->is_backup) {
                //     cur_auto_resume = player->cur_ops->attr.auto_resume;
                //     ap_manager_restore_audio_info();
                // }
                if (cur_auto_resume) {
                    audio_err_t ret = ap_manager_resume_audio();
                    if (ret != ESP_ERR_AUDIO_NO_ERROR) {
                        ESP_LOGW(TAG, "Auto restore failure, media_src:%x", player->backup_info.st.media_src);
                        ap_manager_event_request(AUDIO_PLAYER_STATUS_ERROR, ESP_ERR_AUDIO_RESTORE_NOT_SUCCESS, player->backup_info.st.media_src);
                    }
                } else {
                    play_mode_select(player->mode, player->music_list, player->cur_ops->para.media_src);
                }
                is_done = false;
            }

        } else if (activeMember == player->cmd_que) {
            int quit = 0;
            xQueueReceive(activeMember, &quit, 0);
            tsk_run = false;
            ESP_LOGI(TAG, "esp_audio_wrapper_task will be destroyed");
        }
    }

    ESP_LOGI(TAG, "esp_player_state_task has been deleted");

    xEventGroupSetBits(player->sync_state, EP_TSK_DELETE_BIT);
    audio_thread_delete_task(&s_player->tsk_handle);
}

audio_err_t ap_manager_init(const audio_player_cfg_t *cfg)
{
    AUDIO_NULL_CHECK(TAG, cfg, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    AUDIO_NULL_CHECK(TAG, cfg->handle, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    s_player = audio_calloc(1, sizeof(audio_player_impl_t));
    AUDIO_MEM_CHECK(TAG, s_player, return ESP_ERR_AUDIO_MEMORY_LACK);
    s_player->sync_state_que = xQueueCreate(1, sizeof(esp_audio_state_t));
    AUDIO_MEM_CHECK(TAG, s_player->sync_state_que, {
        audio_free(s_player);
        s_player = NULL;
        return ESP_ERR_AUDIO_MEMORY_LACK;
    });

    audio_err_t ret               = ESP_OK;
    s_player->evt_cb              = cfg->evt_cb;
    s_player->evt_ctx             = cfg->evt_ctx;
    s_player->mode                = AUDIO_PLAYER_MODE_ONE_SONG;
    s_player->music_list          = cfg->music_list;
    s_player->audio_handle        = cfg->handle;
    s_player->sync_state          = xEventGroupCreate();
    s_player->event_cast           = esp_event_cast_create();
    AUDIO_NULL_CHECK(TAG, s_player->event_cast, goto ep_init_err);
    STAILQ_INIT(&s_player->ops_list);
    esp_audio_event_que_set(s_player->audio_handle, s_player->sync_state_que);
    AUDIO_NULL_CHECK(TAG, s_player->sync_state, goto ep_init_err);
    s_player->lock_handle = mutex_create();
    AUDIO_NULL_CHECK(TAG, s_player->lock_handle, goto ep_init_err);
    s_player->cmd_que = xQueueCreate(1, sizeof(player_tsk_msg_t));
    AUDIO_MEM_CHECK(TAG, s_player->cmd_que, goto ep_init_err;);
    xEventGroupClearBits(s_player->sync_state, EP_TSK_INIT_BIT);
    s_player->set_que = xQueueCreateSet(2);
    AUDIO_MEM_CHECK(TAG, s_player->set_que, goto ep_init_err;);
    xQueueAddToSet(s_player->sync_state_que, s_player->set_que);
    xQueueAddToSet(s_player->cmd_que, s_player->set_que);
    ret = audio_thread_create(&s_player->tsk_handle, "audio_manager_task", audio_player_manager_task,
                              s_player, cfg->task_stack, cfg->task_prio, cfg->external_ram_stack, 0);
    if (ret == ESP_FAIL) {
        ESP_LOGE(TAG, "Create audio manager task failure");
        ret = ESP_ERR_AUDIO_MEMORY_LACK;
        goto ep_init_err;
    }

    xEventGroupWaitBits(s_player->sync_state, EP_TSK_INIT_BIT, false, true, portMAX_DELAY);

    return ESP_ERR_AUDIO_NO_ERROR;
ep_init_err:
    if (s_player->sync_state_que) {
        vQueueDelete(s_player->sync_state_que);
        s_player->sync_state_que = NULL;
    }
    if (s_player->sync_state) {
        vEventGroupDelete(s_player->sync_state);
        s_player->sync_state = NULL;
    }
    if (s_player->event_cast) {
        esp_event_cast_destroy(s_player->event_cast);
        s_player->event_cast = NULL;
    }
    if (s_player->lock_handle) {
        mutex_destroy(s_player->lock_handle);
        s_player->lock_handle = NULL;
    }
    if (s_player->cmd_que) {
        vQueueDelete(s_player->cmd_que);
        s_player->cmd_que = NULL;
    }
    if (s_player->set_que) {
        vQueueDelete(s_player->set_que);
        s_player->set_que = NULL;
    }
    if (s_player->audio_handle) {
        esp_audio_destroy(s_player->audio_handle);
        s_player->audio_handle = NULL;
    }
    audio_free(s_player);
    s_player = NULL;
    return ret;
}

audio_err_t ap_manager_deinit(void)
{
    AUDIO_NULL_CHECK(TAG, s_player, return ESP_ERR_AUDIO_NOT_READY);
    int quit = 1;
    if (s_player->tsk_handle) {
        xEventGroupClearBits(s_player->sync_state, EP_TSK_DELETE_BIT);
        xQueueSend(s_player->cmd_que, &quit, portMAX_DELAY);
        ESP_LOGI(TAG, "audio_player_deinit, waiting for task deletion");
        xEventGroupWaitBits(s_player->sync_state, EP_TSK_DELETE_BIT, false, true, portMAX_DELAY);
        s_player->tsk_handle =  NULL;
    }
    ap_ops_item_t *item, *tmp;
    STAILQ_FOREACH_SAFE(item, &s_player->ops_list, next, tmp) {
        STAILQ_REMOVE(&s_player->ops_list, item, ap_ops_item, next);
        audio_free(item);
    }
    if (s_player->sync_state_que) {
        vQueueDelete(s_player->sync_state_que);
        s_player->sync_state_que = NULL;
    }
    if (s_player->sync_state) {
        vEventGroupDelete(s_player->sync_state);
        s_player->sync_state = NULL;
    }
    if (s_player->event_cast) {
        esp_event_cast_destroy(s_player->event_cast);
        s_player->event_cast = NULL;
    }
    if (s_player->lock_handle) {
        mutex_destroy(s_player->lock_handle);
        s_player->lock_handle = NULL;
    }
    if (s_player->cmd_que) {
        vQueueDelete(s_player->cmd_que);
        s_player->cmd_que = NULL;
    }
    if (s_player->audio_handle) {
        esp_audio_destroy(s_player->audio_handle);
        s_player->audio_handle = NULL;
    }
    if (s_player->set_que) {
        vQueueDelete(s_player->set_que);
        s_player->set_que = NULL;
    }
    if (s_player->tsk_handle) {
        audio_thread_cleanup(&s_player->tsk_handle);
    }

    audio_free(s_player);
    s_player = NULL;
    return ESP_OK;
}

audio_err_t ap_manager_event_request( audio_player_status_t st, audio_err_t err_num, media_source_type_t media_src)
{
    audio_player_state_t ap_st = {0};
    ap_st.status    = st;
    ap_st.err_msg   = err_num;
    ap_st.media_src = media_src;
    if (s_player && s_player->event_cast) {
        esp_event_cast_broadcasting(s_player->event_cast, &ap_st);
    }
    if (s_player && s_player->evt_cb) {
        s_player->evt_cb(&ap_st, s_player->evt_ctx);
        return ESP_OK;
    }
    return ESP_FAIL;
}

ap_ops_t *ap_manager_find_ops_by_src(int src_type)
{
    ap_ops_item_t *item, *tmp;
    STAILQ_FOREACH_SAFE(item,  &s_player->ops_list, next, tmp) {
        ESP_LOGD(TAG, "Find media Src[%x], %p",
                 item->func->para.media_src, item->func->para.ctx);
        if (item->func->para.media_src == src_type) {
            return item->func;
        }
    }
    ESP_LOGD(TAG, "The media_src type %x not found", src_type);
    return NULL;
}

ap_ops_t *ap_manager_get_cur_ops(void)
{
    AUDIO_NULL_CHECK(TAG, s_player, return NULL);
    return s_player->cur_ops;
}

audio_err_t ap_manager_set_cur_ops(ap_ops_t *cur_ops)
{
    AUDIO_NULL_CHECK(TAG, s_player, return ESP_ERR_AUDIO_NOT_READY);
    mutex_lock(s_player->lock_handle);
    s_player->cur_ops = cur_ops;
    mutex_unlock(s_player->lock_handle);
    return ESP_ERR_AUDIO_NO_ERROR;
}

audio_err_t ap_manager_backup_audio_info(void)
{
    esp_audio_prefer_t type;
    audio_err_t ret = ESP_OK;
    esp_audio_prefer_type_get(s_player->audio_handle, &type);
    esp_audio_state_t st = {0};
    esp_audio_state_get(s_player->audio_handle, &st);
    ESP_LOGE(TAG, "BACKUP Enter, prefer_type:%d, st:%d", type, st.status);
    if (type == ESP_AUDIO_PREFER_MEM) {
        if ((st.status == AUDIO_STATUS_RUNNING)
            || (st.status == AUDIO_STATUS_PAUSED)) {
            ret = esp_audio_info_get(s_player->audio_handle, &s_player->backup_info);
            if (ret == ESP_OK) {
                s_player->is_backup = true;
            }
            ESP_LOGI(TAG, "BACKUP, ret:%x, i:%p, c:%p, f:%p, o:%p,status:%d", ret,
                     s_player->backup_info.in_el,
                     s_player->backup_info.codec_el,
                     s_player->backup_info.filter_el,
                     s_player->backup_info.out_el, s_player->backup_info.st.status);
        }
        // Call the stop at anytimes
        ap_ops_t *cur_ops = ap_manager_get_cur_ops();
        if (cur_ops == NULL) {
            ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        }
        if (cur_ops && cur_ops->stop) {
            ESP_LOGI(TAG, "Call stop in backup, cur media type:%x, is_abort_playing:%d", cur_ops->para.media_src, s_player->is_abort_playing);
            xEventGroupClearBits(s_player->sync_state, EP_TSK_PLAY_SYNC_STOPPED_BIT | EP_TSK_PLAY_SYNC_ERROR_BIT | EP_TSK_PLAY_SYNC_FINISHED_BIT);
            ret = cur_ops->stop(&cur_ops->attr, &cur_ops->para);
            if (ret == ESP_ERR_AUDIO_NO_ERROR) {
                xEventGroupWaitBits(s_player->sync_state,
                                    EP_TSK_PLAY_SYNC_STOPPED_BIT | EP_TSK_PLAY_SYNC_ERROR_BIT | EP_TSK_PLAY_SYNC_FINISHED_BIT, false, false, AM_ACTION_TIMEOUT / portTICK_PERIOD_MS);
            } else {
                ESP_LOGW(TAG, "AP_MANAGER_PLAY, Call esp_audio_stop error, ret:%x", ret);
            }
        }
    } else if (type == ESP_AUDIO_PREFER_SPEED) {
        ESP_LOGW(TAG, "Call esp_audio_pause in BACKUP");
        esp_audio_pause(s_player->audio_handle);
        if ((st.status == AUDIO_STATUS_RUNNING)
            || (st.status == AUDIO_STATUS_PAUSED)) {
            esp_audio_info_get(s_player->audio_handle, &s_player->backup_info);
            s_player->is_backup = true;
            ESP_LOGI(TAG, "BACKUP, ret:%x,i:%p, c:%p, f:%p, o:%p,status:%d", ret,
                     s_player->backup_info.in_el,
                     s_player->backup_info.codec_el,
                     s_player->backup_info.filter_el,
                     s_player->backup_info.out_el, s_player->backup_info.st.status);

        }
    }
    ESP_LOGI(TAG, "BACKUP Exit, is_abort_playing:%d", s_player->is_abort_playing);
    return ret;
}

audio_err_t ap_manager_clear_audio_info(void)
{
    audio_err_t ret = ESP_OK;
    ESP_LOGW(TAG, "%s, is_backup:%d", __func__, s_player->is_backup);
    if (s_player->is_backup == true) {
        free(s_player->backup_info.codec_info.uri);
        s_player->backup_info.codec_info.uri = NULL;
        s_player->is_backup = false;
    }
    return ret;
}

audio_err_t ap_manager_restore_audio_info(void)
{
    audio_err_t ret = ESP_OK;
    ESP_LOGW(TAG, "%s, is_backup:%d", __func__, s_player->is_backup);
    if (s_player->is_backup == true) {
        ap_ops_t *tmp = ap_manager_find_ops_by_src(s_player->backup_info.st.media_src);
        if (tmp == NULL) {
            ESP_LOGW(TAG, "The media_src type %x not found, restore failed", s_player->backup_info.st.media_src);
            return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
        }
        s_player->cur_ops = tmp;
        ret = esp_audio_info_set(s_player->audio_handle, &s_player->backup_info);
        s_player->is_backup = false;
    } else {
        ESP_LOGW(TAG, "No audio information stored");
    }
    return ret;
}

audio_err_t ap_manager_resume_audio(void)
{
    esp_audio_prefer_t type;
    audio_err_t ret = ESP_OK;
    esp_audio_state_t st = {0};
    ret |= esp_audio_prefer_type_get(s_player->audio_handle, &type);
    esp_audio_state_get(s_player->audio_handle, &st);
    ESP_LOGI(TAG, "Resume Audio, prefer:%d, status:%s, media_src:%x", type, ESP_AUDIO_STATUS_STRING[st.status], st.media_src);
    if (type == ESP_AUDIO_PREFER_MEM) {
        if (st.status == AUDIO_STATUS_RUNNING) {
            ret |= esp_audio_play(s_player->audio_handle, AUDIO_CODEC_TYPE_DECODER, NULL, 0);
        }
    } else if (type == ESP_AUDIO_PREFER_SPEED) {
        if (st.status == AUDIO_STATUS_RUNNING) {
            ret |= esp_audio_resume(s_player->audio_handle);
        }
    }
    return ret;
}


audio_err_t ap_manager_ops_add(const ap_ops_t *ops, uint8_t cnt_of_ops)
{
    AUDIO_NULL_CHECK(TAG, ops, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    ap_ops_item_t *ap_ops_item = audio_calloc(1, sizeof(ap_ops_item_t) * cnt_of_ops);
    AUDIO_MEM_CHECK(TAG, ap_ops_item, return ESP_ERR_AUDIO_MEMORY_LACK);
    ap_ops_t *tmp_ops = (ap_ops_t *)ops;
    audio_err_t ret = ESP_OK;
    ap_ops_item_t *tmp = ap_ops_item;
    for (int i = 0; i < cnt_of_ops; ++i) {
        ap_ops_item->func = audio_calloc(1, sizeof(ap_ops_t));
        AUDIO_MEM_CHECK(TAG, ap_ops_item->func, {ret = ESP_ERR_AUDIO_MEMORY_LACK; goto _ops_add_err;});
        memcpy(ap_ops_item->func, tmp_ops, sizeof(ap_ops_t));
        ap_ops_item->used = false;
        ESP_LOGD(TAG, "INERT,Src[%x], %p, %x", ap_ops_item->func->para.media_src, ap_ops_item->func->para.ctx, ops->para.media_src);
        STAILQ_INSERT_TAIL(&s_player->ops_list, ap_ops_item, next);
        ap_ops_item++;
        tmp_ops++;
    }

    STAILQ_FOREACH_SAFE(ap_ops_item, &s_player->ops_list, next, tmp) {
        ESP_LOGD(TAG, "Add Media Src[%x], %p, %x", ap_ops_item->func->para.media_src, ap_ops_item->func->para.ctx, ops->para.media_src);
    }
    return ret;
_ops_add_err:
    ap_ops_item = tmp;
    for (int i = 0; i < cnt_of_ops; ++i) {
        if (ap_ops_item->func) {
            audio_free(ap_ops_item->func);
            ap_ops_item->func = NULL;
        }
        ap_ops_item++;
    }
    audio_free(tmp);
    return ret;
}

audio_err_t ap_manager_set_music_playlist(playlist_handle_t pl)
{
    AUDIO_NULL_CHECK(TAG, s_player, return ESP_ERR_AUDIO_NOT_READY);
    AUDIO_NULL_CHECK(TAG, pl, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    mutex_lock(s_player->lock_handle);
    s_player->music_list = pl;
    mutex_unlock(s_player->lock_handle);
    return ESP_OK;
}

audio_err_t ap_manager_get_music_playlist(playlist_handle_t *pl)
{
    AUDIO_NULL_CHECK(TAG, s_player, return ESP_ERR_AUDIO_NOT_READY);
    AUDIO_NULL_CHECK(TAG, pl, return ESP_ERR_AUDIO_INVALID_PARAMETER);
    mutex_lock(s_player->lock_handle);
    *pl = s_player->music_list;
    mutex_unlock(s_player->lock_handle);
    return ESP_OK;
}

audio_err_t ap_manager_set_callback(audio_player_evt_callback cb, void *cb_ctx)
{
    AUDIO_NULL_CHECK(TAG, s_player, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = ESP_OK;
    mutex_lock(s_player->lock_handle);
    s_player->evt_cb = cb;
    s_player->evt_ctx = cb_ctx;
    mutex_unlock(s_player->lock_handle);
    return ret;
}

audio_err_t ap_manager_set_mode(audio_player_mode_t mode)
{
    AUDIO_NULL_CHECK(TAG, s_player, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = ESP_OK;
    mutex_lock(s_player->lock_handle);
    s_player->mode = mode;
    mutex_unlock(s_player->lock_handle);
    return ret;
}

audio_err_t ap_manager_get_mode(audio_player_mode_t *mode)
{
    AUDIO_NULL_CHECK(TAG, s_player, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = ESP_OK;
    mutex_lock(s_player->lock_handle);
    *mode = s_player->mode;
    mutex_unlock(s_player->lock_handle);
    return ret;
}

audio_err_t ap_manager_event_register(void *que)
{
    AUDIO_NULL_CHECK(TAG, s_player, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = ESP_OK;
    ret = esp_event_cast_register(s_player->event_cast, que);
    return ret;
}

audio_err_t ap_manager_event_unregister(void *que)
{
    AUDIO_NULL_CHECK(TAG, s_player, return ESP_ERR_AUDIO_NOT_READY);
    audio_err_t ret = ESP_OK;
    ret = esp_event_cast_unregister(s_player->event_cast, que);
    return ret;
}

audio_err_t ap_manager_play(const char *url, uint32_t pos, bool blocked, bool auto_resume, bool mixed, bool interrupt, media_source_type_t type)
{
    audio_err_t ret = ESP_ERR_AUDIO_NO_ERROR;
    esp_audio_state_t st = {0};
    esp_audio_state_get(s_player->audio_handle, &st);
    s_player->prepare_playing = true;
    ESP_LOGI(TAG, "AP_MANAGER_PLAY, Enter:%s, pos:%d, block:%d, auto:%d, mix:%d, inter:%d, type:%x, st:%s", __func__, pos, blocked, auto_resume, mixed, interrupt,
             type, ESP_AUDIO_STATUS_STRING[st.status]);
    mutex_lock(s_player->lock_handle);
    if (interrupt == true) {
        ap_manager_backup_audio_info();
    } else {
        ESP_LOGI(TAG, "AP_MANAGER_PLAY, Call esp_audio_stop before playing");
        xEventGroupClearBits(s_player->sync_state, EP_TSK_PLAY_SYNC_STOPPED_BIT | EP_TSK_PLAY_SYNC_ERROR_BIT | EP_TSK_PLAY_SYNC_FINISHED_BIT);
        ret = esp_audio_stop(s_player->audio_handle, TERMINATION_TYPE_NOW);
        if (ret == ESP_ERR_AUDIO_NO_ERROR) {
            xEventGroupWaitBits(s_player->sync_state,
                                EP_TSK_PLAY_SYNC_STOPPED_BIT | EP_TSK_PLAY_SYNC_ERROR_BIT | EP_TSK_PLAY_SYNC_FINISHED_BIT, false, false, AM_ACTION_TIMEOUT / portTICK_PERIOD_MS);
        } else {
            ESP_LOGW(TAG, "AP_MANAGER_PLAY, Call esp_audio_stop error, ret:%x", ret);
        }
    }
    if (mixed == true) {
        // TODO
    }
    ap_ops_t *tmp = ap_manager_find_ops_by_src(type);
    if (tmp == NULL) {
        mutex_unlock(s_player->lock_handle);
        s_player->prepare_playing = false;
        ESP_LOGW(TAG, "AP_MANAGER_PLAY exit, The media_src type %x not found", type);
        return ESP_ERR_AUDIO_NOT_SUPPORT;
    }
    tmp->attr.blocked = blocked;
    tmp->attr.auto_resume = auto_resume;
    tmp->attr.mixed = mixed;
    tmp->attr.interrupt = interrupt;
    tmp->para.pos = pos;
    if (tmp->para.url) {
        audio_free(tmp->para.url);
        tmp->para.url = NULL;
    }
    tmp->para.url = audio_strdup(url);
    AUDIO_MEM_CHECK(TAG, tmp->para.url, {mutex_unlock(s_player->lock_handle);
                                         s_player->prepare_playing = false;
                                         return ESP_ERR_AUDIO_MEMORY_LACK;
                                        });
    if (s_player->is_abort_playing) {
        s_player->prepare_playing = false;
        s_player->is_abort_playing = false;
        mutex_unlock(s_player->lock_handle);
        ESP_LOGE(TAG, "AP_MANAGER_PLAY exit:%d", __LINE__);
        return ESP_ERR_AUDIO_FAIL;
    }

    s_player->cur_ops = tmp;
    ret = esp_audio_media_type_set(s_player->audio_handle, type);
    if (blocked == true) {
        ESP_LOGW(TAG, "AP_MANAGER_PLAY, Blocked playing, %s, type:%x", s_player->cur_ops->para.url, type);
        xEventGroupClearBits(s_player->sync_state, EP_TSK_PLAY_SYNC_TONE_BIT | EP_TSK_PLAY_SYNC_ERROR_BIT);
        mutex_unlock(s_player->lock_handle);
        ret = esp_audio_sync_play(s_player->audio_handle, s_player->cur_ops->para.url, 0);
        if (ret != ESP_ERR_AUDIO_NO_ERROR) {
            ESP_LOGW(TAG, "AP_MANAGER_PLAY, Blocked playing, esp_audio_play return error:%x", ret);
        }
    } else {
        ESP_LOGW(TAG, "AP_MANAGER_PLAY, Unblock playing, type:%x, auto:%d, block:%d", type, s_player->cur_ops->attr.auto_resume, s_player->cur_ops->attr.blocked);
        ret = esp_audio_play(s_player->audio_handle, AUDIO_CODEC_TYPE_DECODER, s_player->cur_ops->para.url, s_player->cur_ops->para.pos);
        mutex_unlock(s_player->lock_handle);
        if (ret == ESP_ERR_AUDIO_NO_ERROR) {
            EventBits_t uxBits = xEventGroupWaitBits(s_player->sync_state, EP_TSK_PLAY_SYNC_RUNNING_BIT | EP_TSK_PLAY_SYNC_ERROR_BIT, false, false,  AM_ACTION_TIMEOUT / portTICK_PERIOD_MS);
            if ((uxBits & EP_TSK_PLAY_SYNC_ERROR_BIT) == EP_TSK_PLAY_SYNC_ERROR_BIT) {
                esp_audio_state_t st = {0};
                esp_audio_state_get(s_player->audio_handle, &st);
                ESP_LOGW(TAG, "AP_MANAGER_PLAY, Unblock playing occur an error, src:%x, err:%x", st.media_src, st.err_msg);
                ret = st.err_msg;
            } else if ((uxBits & EP_TSK_PLAY_SYNC_RUNNING_BIT) == EP_TSK_PLAY_SYNC_RUNNING_BIT) {

            } else {
                ret = ESP_ERR_AUDIO_TIMEOUT;
                ESP_LOGW(TAG, "AP_MANAGER_PLAY, Unblock playing timeout occurred");
            }
        } else {
            ESP_LOGW(TAG, "AP_MANAGER_PLAY, Unblock playing, esp_audio_play return error:%x", ret);
        }
    }
    s_player->prepare_playing = false;
    if (s_player->is_abort_playing) {
        s_player->is_abort_playing = false;
    }
    ESP_LOGI(TAG, "AP_MANAGER_PLAY done exit");
    return ret;
}

audio_err_t ap_manager_stop(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    ESP_LOGI(TAG, "Enter:%s", __func__);
    if (cur_ops->stop) {
        if (s_player->prepare_playing) {
            s_player->is_abort_playing = true;
        }
        ESP_LOGI(TAG, "stop, cur media type:%x, is_abort_playing:%d", cur_ops->para.media_src, s_player->is_abort_playing);
        ret = cur_ops->stop(&cur_ops->attr, &cur_ops->para);
    }
    ESP_LOGI(TAG, "Exit:%s", __func__);
    return ret;
}

audio_err_t ap_manager_pause(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    if (cur_ops->pause) {
        if (s_player->prepare_playing) {
            s_player->is_abort_playing = true;
        }
        ESP_LOGI(TAG, "pause, cur media type:%x, is_abort_playing:%d", cur_ops->para.media_src, s_player->is_abort_playing);
        ret = cur_ops->pause(&cur_ops->attr, &cur_ops->para);
    }
    return ret;
}

audio_err_t ap_manager_resume(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_find_ops_by_src(s_player->backup_info.st.media_src);
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    mutex_lock(s_player->lock_handle);
    if (cur_ops->resume) {
        ESP_LOGI(TAG, "resume, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->resume(&cur_ops->attr, &cur_ops->para);
    }
    mutex_unlock(s_player->lock_handle);
    return ret;
}

audio_err_t ap_manager_next(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    mutex_lock(s_player->lock_handle);
    if (cur_ops->next) {
        ESP_LOGI(TAG, "next, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->next(&cur_ops->attr, &cur_ops->para);
    }
    mutex_unlock(s_player->lock_handle);
    return ret;
}

audio_err_t ap_manager_prev(void)
{
    int ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    mutex_lock(s_player->lock_handle);
    if (cur_ops->prev) {
        ESP_LOGI(TAG, "prev, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->prev(&cur_ops->attr, &cur_ops->para);
    }
    mutex_unlock(s_player->lock_handle);
    return ret;
}

audio_err_t ap_manager_seek(int seek_time_sec)
{
    audio_err_t ret = ESP_OK;
    ap_ops_t *cur_ops = ap_manager_get_cur_ops();
    if (cur_ops == NULL) {
        ESP_LOGW(TAG, "%s, not found the current operations", __func__);
        return ESP_ERR_AUDIO_NOT_FOUND_MEDIA_SRC;
    }
    mutex_lock(s_player->lock_handle);
    if (cur_ops->seek) {
        cur_ops->para.seek_time_sec = seek_time_sec;
        ESP_LOGI(TAG, "prev, cur media type:%x", cur_ops->para.media_src);
        ret = cur_ops->seek(&cur_ops->attr, &cur_ops->para);
    }
    mutex_unlock(s_player->lock_handle);
    return ret;
}

