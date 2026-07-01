/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "esp_cpu.h"
#include "esp_log.h"

#include "codec_ref_mgr.h"
#include "esp_codec_dev_os.h"

static const char *TAG = "CODEC_REF_MGR";

#define REF_LOCK_TIMEOUT_MS  (1000)

/**
 * @brief  Codec device reference list node
 */
typedef struct codec_dev_ref_node {
    audio_codec_ctrl_info_t    info;                        /*!< Control interface identity */
    uint8_t                    count[CODEC_REF_STAGE_NUM];  /*!< Reference counts per stage */
    struct codec_dev_ref_node *next;                        /*!< Next node in the list */
} codec_dev_ref_node_t;

static codec_dev_ref_node_t *s_codec_ref_list;
static esp_codec_dev_mutex_handle_t s_codec_ref_mutex;

static bool ctrl_info_equal(const audio_codec_ctrl_info_t *a, const audio_codec_ctrl_info_t *b)
{
    if (a->type != b->type) {
        return false;
    }
    switch (a->type) {
        case AUDIO_CODEC_CTRL_I2C:
            return a->i2c.addr == b->i2c.addr && a->i2c.port == b->i2c.port;
        case AUDIO_CODEC_CTRL_SPI:
            return a->spi.cs_pin == b->spi.cs_pin;
        default:
            return false;
    }
}

static esp_codec_dev_mutex_handle_t codec_ref_get_mutex(void)
{
    if (s_codec_ref_mutex != NULL) {
        return s_codec_ref_mutex;
    }

    esp_codec_dev_mutex_handle_t mutex = esp_codec_dev_mutex_create();
    if (mutex == NULL) {
        return NULL;
    }

    /* esp_cpu_compare_and_set operates on 32-bit values (32-bit pointer targets). */
    if (esp_cpu_compare_and_set((volatile uint32_t *)&s_codec_ref_mutex,
                                (uint32_t)NULL,
                                (uint32_t)mutex)) {
        return mutex;
    }

    esp_codec_dev_mutex_destroy(mutex);
    return s_codec_ref_mutex;
}

static codec_dev_ref_node_t *codec_ref_mgr_find_node(const audio_codec_ctrl_info_t *info)
{
    codec_dev_ref_node_t *p = s_codec_ref_list;
    while (p) {
        if (ctrl_info_equal(&p->info, info)) {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

int codec_ref_acquire(const audio_codec_ctrl_info_t *info, codec_ref_stage_t stage)
{
    if (info == NULL || stage >= CODEC_REF_STAGE_NUM) {
        return -1;
    }
    esp_codec_dev_mutex_handle_t mutex = codec_ref_get_mutex();
    if (mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create codec ref list mutex");
        return -1;
    }
    if (esp_codec_dev_mutex_lock(mutex, REF_LOCK_TIMEOUT_MS) != 0) {
        ESP_LOGE(TAG, "Failed to lock codec ref list");
        return -1;
    }
    int ref_count = 0;
    codec_dev_ref_node_t *p = codec_ref_mgr_find_node(info);
    if (p) {
        if (p->count[stage] < UINT8_MAX) {
            p->count[stage]++;
        }
        ref_count = p->count[stage];
        goto unlock;
    }
    codec_dev_ref_node_t *new_node = (codec_dev_ref_node_t *)calloc(1, sizeof(codec_dev_ref_node_t));
    if (new_node == NULL) {
        goto unlock;
    }
    memcpy(&new_node->info, info, sizeof(audio_codec_ctrl_info_t));
    new_node->count[stage] = 1;
    new_node->next = s_codec_ref_list;
    s_codec_ref_list = new_node;
    ref_count = 1;
unlock:
    esp_codec_dev_mutex_unlock(mutex);
    return ref_count > 0 ? ref_count : -1;
}

int codec_ref_release(const audio_codec_ctrl_info_t *info, codec_ref_stage_t stage)
{
    if (info == NULL || stage >= CODEC_REF_STAGE_NUM) {
        ESP_LOGE(TAG, "Invalid info or stage");
        return -1;
    }
    if (s_codec_ref_mutex == NULL) {
        ESP_LOGE(TAG, "Codec ref list is not initialized");
        return -1;
    }
    if (esp_codec_dev_mutex_lock(s_codec_ref_mutex, REF_LOCK_TIMEOUT_MS) != 0) {
        ESP_LOGE(TAG, "Failed to lock codec ref list");
        return -1;
    }
    int ref_count = -1;
    codec_dev_ref_node_t *p = s_codec_ref_list;
    codec_dev_ref_node_t *prev = NULL;
    while (p) {
        if (ctrl_info_equal(&p->info, info)) {
            if (p->count[stage] > 0) {
                p->count[stage]--;
            }
            ref_count = p->count[stage];
            if (stage == CODEC_REF_STAGE_OPEN && p->count[CODEC_REF_STAGE_OPEN] == 0) {
                if (prev) {
                    prev->next = p->next;
                } else {
                    s_codec_ref_list = p->next;
                }
                free(p);
            }
            goto unlock;
        }
        prev = p;
        p = p->next;
    }
unlock:
    esp_codec_dev_mutex_unlock(s_codec_ref_mutex);
    return ref_count;
}
