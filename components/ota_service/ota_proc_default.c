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

#include "audio_mem.h"
#include "esp_https_ota.h"
#include "esp_fs_ota.h"
#include "esp_log.h"
#include "fatfs_stream.h"
#include "http_stream.h"

#include "ota_proc_default.h"

#define READER_BUF_LEN (1024 * 2)

typedef struct {
    audio_element_handle_t r_stream;
    const esp_partition_t *partition;

    int write_offset;
    char read_buf[READER_BUF_LEN];
} ota_data_upgrade_ctx_t;

typedef struct {
    void *ota_handle;
    esp_err_t (*get_img_desc)(void *handle, esp_app_desc_t *new_app_info);
    esp_err_t (*perform)(void *handle);
    int (*get_image_len_read)(void *handle);
    bool (*all_read)(void *handle);
    esp_err_t (*finish)(void *handle);
} ota_app_upgrade_ctx_t;

static const char *TAG = "OTA_DEFAULT";

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s, the incoming firmware version %s", running_app_info.version, new_app_info->version);
    }

    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t ota_app_partition_prepare(void **handle, ota_node_attr_t *node)
{
    ota_app_upgrade_ctx_t *context = audio_calloc(1, sizeof(ota_app_upgrade_ctx_t));
    *handle = context;

    if (strstr(node->uri, "file://")) {
        context->get_img_desc = esp_fs_ota_get_img_desc;
        context->perform = esp_fs_ota_perform;
        context->get_image_len_read = esp_fs_ota_get_image_len_read;
        context->all_read = NULL;
        context->finish = esp_fs_ota_finish;

        esp_fs_ota_config_t ota_config = {
            .path = node->uri,
            .buffer_size = 5 * 1024,
        };
        esp_err_t err = esp_fs_ota_begin(&ota_config, &context->ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ESP FS OTA Begin failed");
            return err;
        }
    } else if (strstr(node->uri, "https://") || strstr(node->uri, "http://")) {
        context->get_img_desc = esp_https_ota_get_img_desc;
        context->perform = esp_https_ota_perform;
        context->get_image_len_read = esp_https_ota_get_image_len_read;
        context->all_read = esp_https_ota_is_complete_data_received;
        context->finish = esp_https_ota_finish;

        esp_http_client_config_t config = {
            .url = node->uri,
            .cert_pem = node->cert_pem,
            .timeout_ms = 1000,
        };
        esp_https_ota_config_t ota_config = {
            .http_config = &config,
        };
        esp_err_t err = esp_https_ota_begin(&ota_config, &context->ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
            return err;
        }
    } else {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static bool ota_app_partition_need_upgrade(void *handle, ota_node_attr_t *node)
{
    ota_app_upgrade_ctx_t *context = (ota_app_upgrade_ctx_t *)handle;

    esp_app_desc_t app_desc;
    esp_err_t err = context->get_img_desc(context->ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "get_img_desc failed");
        return false;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        return false;
    }
    return true;
}

static esp_err_t ota_app_partition_exec_upgrade(void *handle, ota_node_attr_t *node)
{
    ota_app_upgrade_ctx_t *context = (ota_app_upgrade_ctx_t *)handle;
    esp_err_t err = ESP_FAIL;

    while (1) {
        err = context->perform(context->ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS && err != ESP_ERR_FS_OTA_IN_PROGRESS ) {
            break;
        }
        ESP_LOGI(TAG, "Image bytes read: %d", context->get_image_len_read(context->ota_handle));
    }

    return err;
}

static esp_err_t ota_app_partition_finish(void *handle, ota_node_attr_t *node, esp_err_t result)
{
    ota_app_upgrade_ctx_t *context = (ota_app_upgrade_ctx_t *)handle;
    esp_err_t err = context->finish(context->ota_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        ESP_LOGE(TAG, "upgrade failed %d", err);
    }
    free(handle);
    return err;
}

void ota_app_get_default_proc(ota_upgrade_ops_t *ops)
{
    ops->prepare = ota_app_partition_prepare;
    ops->need_upgrade = ota_app_partition_need_upgrade;
    ops->execute_upgrade = ota_app_partition_exec_upgrade;
    ops->finished_check = ota_app_partition_finish;
}

static esp_err_t ota_data_partition_prepare(void **handle, ota_node_attr_t *node)
{
    ota_data_upgrade_ctx_t *context = audio_calloc(1, sizeof(ota_data_upgrade_ctx_t));
    *handle = context;

    context->partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, node->label);
    if (context->partition == NULL) {
        ESP_LOGE(TAG, "partition [%s] not found", node->label);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "data upgrade uri %s", node->uri);
    if (strstr(node->uri, "file://")) {
        fatfs_stream_cfg_t fs_cfg = FATFS_STREAM_CFG_DEFAULT();
        fs_cfg.type = AUDIO_STREAM_READER;

        context->r_stream = fatfs_stream_init(&fs_cfg);
    } else if (strstr(node->uri, "https://") || strstr(node->uri, "http://")) {
        http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
        http_cfg.type = AUDIO_STREAM_READER;

        context->r_stream = http_stream_init(&http_cfg);
    } else {
        ESP_LOGE(TAG, "not support uri");
        return ESP_FAIL;
    }
    audio_element_set_uri(context->r_stream, node->uri);
    if (audio_element_process_init(context->r_stream) != ESP_OK) {
        ESP_LOGE(TAG, "reader stream init failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t ota_data_partition_exec_upgrade(void *handle, ota_node_attr_t *node)
{
    int r_size = 0;
    ota_data_upgrade_ctx_t *context = (ota_data_upgrade_ctx_t *)handle;

    while ((r_size = audio_element_input(context->r_stream, context->read_buf, READER_BUF_LEN)) > 0) {
        ESP_LOGI(TAG, "write_offset %d, r_size %d", context->write_offset, r_size);
        if (esp_partition_write(context->partition, context->write_offset, context->read_buf, r_size) == ESP_OK) {
            context->write_offset += r_size;
        } else {
            return ESP_FAIL;
        }
    }
    if (r_size == AEL_IO_OK || r_size == AEL_IO_DONE) {
        ESP_LOGI(TAG, "partition %s upgrade successes", node->label);
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

static esp_err_t ota_data_partition_finish(void *handle, ota_node_attr_t *node, esp_err_t result)
{
    ota_data_upgrade_ctx_t *context = (ota_data_upgrade_ctx_t *)handle;
    audio_element_deinit(context->r_stream);

    free(handle);
    return result;
}

void ota_data_get_default_proc(ota_upgrade_ops_t *ops)
{
    ops->prepare = ota_data_partition_prepare;
    ops->need_upgrade = NULL;
    ops->execute_upgrade = ota_data_partition_exec_upgrade;
    ops->finished_check = ota_data_partition_finish;
}

esp_err_t ota_data_image_stream_read(void *handle, char *buf, int wanted_size)
{
    ota_data_upgrade_ctx_t *context = (ota_data_upgrade_ctx_t *)handle;

    if (context == NULL) {
        ESP_LOGE(TAG, "run prepare first");
        return ESP_ERR_INVALID_STATE;
    }
    int r_size = 0;
    do {
        int ret = audio_element_input(context->r_stream, buf, wanted_size - r_size);
        if (ret > 0) {
            r_size += ret;
        } else {
            break;
        }
    } while (r_size < wanted_size);

    if (r_size == wanted_size) {
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

esp_err_t ota_data_partition_write(void *handle, char *buf, int size)
{
    ota_data_upgrade_ctx_t *context = (ota_data_upgrade_ctx_t *)handle;

    if (context == NULL) {
        ESP_LOGE(TAG, "run prepare first");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "write_offset %d, size %d", context->write_offset, size);
    if (esp_partition_write(context->partition, context->write_offset, buf, size) == ESP_OK) {
        context->write_offset += size;
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}
