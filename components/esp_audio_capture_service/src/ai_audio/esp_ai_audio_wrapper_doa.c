/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>

#include "esp_afe_doa.h"
#include "esp_gmf_audio_element.h"
#include "esp_gmf_cache.h"
#include "esp_gmf_cap.h"
#include "esp_gmf_caps_def.h"
#include "esp_gmf_err.h"
#include "esp_gmf_job.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_payload.h"
#include "esp_log.h"

#include "esp_ai_audio_wrapper_doa.h"

static const char *TAG = "AI_AUDIO_DOA_WRAPPER";

#define DOA_BYTES_PER_SAMPLE  (sizeof(int16_t))

typedef struct {
    esp_gmf_audio_element_t  parent;
    doa_handle_t            *doa_handle;
    esp_gmf_cache_t         *cache;
    esp_gmf_payload_t       *in_load;
    int16_t                 *mic_left;
    int16_t                 *mic_right;
    uint32_t                 bytes_per_iter;
    uint8_t                  m_idx[2];
    float                    last_result;
    bool                     has_last_result;
} doa_wrapper_priv_t;

static esp_gmf_job_err_t validate_input_format(const char *input_format, uint8_t *m_idx)
{
    if (!input_format || !m_idx) {
        return ESP_GMF_JOB_ERR_FAIL;
    }
    uint8_t mic_count = 0;
    uint8_t ref_count = 0;
    uint8_t mic_idx = 0;
    uint8_t ref_idx = 0;
    for (uint8_t i = 0; input_format[i] != '\0'; i++) {
        if (input_format[i] == 'M') {
            if (mic_count < 2) {
                m_idx[mic_count] = i;
            }
            if (mic_count == 0) {
                mic_idx = i;
            }
            mic_count++;
        } else if (input_format[i] == 'R') {
            if (ref_count == 0) {
                ref_idx = i;
            }
            ref_count++;
        }
    }
    if (mic_count == 2) {
        return ESP_GMF_JOB_ERR_OK;
    }
    /* One mic + one reference on a 2-ch board: reuse M+Ref as the DOA pair. */
    if (strlen(input_format) == 2 && mic_count == 1 && ref_count == 1) {
        ESP_LOGW(TAG, "DOA layout '%s' has one mic and one reference; using M+Ref",
                 input_format);
        m_idx[0] = mic_idx;
        m_idx[1] = ref_idx;
        return ESP_GMF_JOB_ERR_OK;
    }
    if (strlen(input_format) < 2) {
        ESP_LOGE(TAG, "DOA needs at least two input channels, format:%s", input_format);
    } else {
        ESP_LOGE(TAG, "DOA requires two microphone channels or a 2-ch M+Ref layout, format:%s",
                 input_format);
    }
    return ESP_GMF_JOB_ERR_FAIL;
}

static void extract_channels(doa_wrapper_priv_t *priv, int16_t *in_buf)
{
    esp_ai_audio_wrapper_doa_cfg_t *cfg = OBJ_GET_CFG(priv);
    size_t ch_num = strlen(cfg->input_format);
    size_t samples = priv->bytes_per_iter / sizeof(int16_t);
    int idx = 0;
    for (size_t i = 0; i < samples; i += ch_num, idx++) {
        priv->mic_left[idx] = in_buf[i + priv->m_idx[0]];
        priv->mic_right[idx] = in_buf[i + priv->m_idx[1]];
    }
}

static esp_gmf_err_t doa_wrapper_event_receiver(esp_gmf_event_pkt_t *evt, void *ctx)
{
    if (!evt || !ctx) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    esp_gmf_element_handle_t self = (esp_gmf_element_handle_t)ctx;
    esp_gmf_event_state_t state = ESP_GMF_EVENT_STATE_NONE;
    esp_gmf_element_get_state(self, &state);
    esp_gmf_element_handle_t prev = NULL;
    esp_gmf_element_get_prev_el(self, &prev);
    if ((state == ESP_GMF_EVENT_STATE_NONE) || (prev == evt->from)) {
        if (evt->sub == ESP_GMF_INFO_SOUND) {
            if (!evt->payload || evt->payload_size < sizeof(esp_gmf_info_sound_t)) {
                return ESP_GMF_ERR_INVALID_ARG;
            }
            esp_gmf_element_set_state(self, ESP_GMF_EVENT_STATE_INITIALIZED);
        }
    }
    return ESP_GMF_ERR_OK;
}

static esp_gmf_job_err_t doa_wrapper_open(esp_gmf_audio_element_handle_t self, void *para)
{
    doa_wrapper_priv_t *priv = (doa_wrapper_priv_t *)self;
    esp_ai_audio_wrapper_doa_cfg_t *cfg = OBJ_GET_CFG(self);
    if (!priv || !cfg || !cfg->input_format) {
        return ESP_GMF_JOB_ERR_FAIL;
    }

    size_t ch_num = strlen(cfg->input_format);
    if (cfg->sample_rate == 0 || cfg->frame_ms == 0 || ch_num < 2) {
        ESP_LOGE(TAG, "Invalid DOA config rate:%lu frame:%u ch:%u",
                 (unsigned long)cfg->sample_rate, cfg->frame_ms, (unsigned)ch_num);
        return ESP_GMF_JOB_ERR_FAIL;
    }
    if (validate_input_format(cfg->input_format, priv->m_idx) != ESP_GMF_JOB_ERR_OK) {
        return ESP_GMF_JOB_ERR_FAIL;
    }

    uint32_t samples_per_channel = cfg->frame_ms * cfg->sample_rate / 1000U;
    uint32_t bytes_per_channel = samples_per_channel * DOA_BYTES_PER_SAMPLE;
    priv->bytes_per_iter = bytes_per_channel * ch_num;
    priv->doa_handle = esp_doa_create(cfg->sample_rate, cfg->resolution, cfg->d_mics, (int)samples_per_channel);
    if (!priv->doa_handle) {
        ESP_LOGE(TAG, "Failed to create DOA handle");
        return ESP_GMF_JOB_ERR_FAIL;
    }
    esp_gmf_cache_new(priv->bytes_per_iter, &priv->cache);
    priv->mic_left = esp_gmf_oal_calloc(1, bytes_per_channel);
    priv->mic_right = esp_gmf_oal_calloc(1, bytes_per_channel);
    if (!priv->cache || !priv->mic_left || !priv->mic_right) {
        return ESP_GMF_JOB_ERR_FAIL;
    }

    ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                     ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, priv->bytes_per_iter);
    ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(ESP_GMF_ELEMENT_GET(self)->out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                      ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, priv->bytes_per_iter);
    esp_gmf_info_sound_t snd_info = {
        .sample_rates = cfg->sample_rate,
        .bits = 16,
        .channels = ch_num,
    };
    esp_gmf_element_notify_snd_info(self, &snd_info);
    return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_job_err_t doa_wrapper_process(esp_gmf_audio_element_handle_t self, void *para)
{
    int ret = ESP_GMF_JOB_ERR_OK;
    doa_wrapper_priv_t *priv = (doa_wrapper_priv_t *)self;
    esp_ai_audio_wrapper_doa_cfg_t *cfg = OBJ_GET_CFG(self);
    esp_gmf_port_handle_t in_port = ESP_GMF_ELEMENT_GET(self)->in;
    esp_gmf_port_handle_t out_port = ESP_GMF_ELEMENT_GET(self)->out;
    esp_gmf_payload_t *cache_load = NULL;
    esp_gmf_payload_t *out_load = NULL;
    bool need_load = false;

    esp_gmf_cache_ready_for_load(priv->cache, &need_load);
    if (need_load) {
        esp_gmf_err_io_t load_ret = esp_gmf_port_acquire_in(in_port, &priv->in_load, priv->bytes_per_iter, in_port->wait_ticks);
        ESP_GMF_PORT_ACQUIRE_IN_CHECK(TAG, load_ret, ret, {goto done;});
        esp_gmf_cache_load(priv->cache, priv->in_load);
        esp_gmf_port_release_in(in_port, priv->in_load, ESP_GMF_MAX_DELAY);
        priv->in_load = NULL;
    }
    esp_gmf_cache_acquire(priv->cache, priv->bytes_per_iter, &cache_load);
    if (cache_load->valid_size != priv->bytes_per_iter) {
        ret = cache_load->is_done ? ESP_GMF_JOB_ERR_DONE : ESP_GMF_JOB_ERR_CONTINUE;
        goto done;
    }

    esp_gmf_err_io_t out_ret = esp_gmf_port_acquire_out(out_port, &out_load, cache_load->valid_size, ESP_GMF_MAX_DELAY);
    ESP_GMF_PORT_ACQUIRE_OUT_CHECK(TAG, out_ret, ret, {goto done;});
    memcpy(out_load->buf, cache_load->buf, cache_load->valid_size);
    out_load->valid_size = cache_load->valid_size;
    out_load->is_done = cache_load->is_done;

    extract_channels(priv, (int16_t *)cache_load->buf);
    float doa_result = esp_doa_process(priv->doa_handle, priv->mic_left, priv->mic_right);
    if (cfg->result_callback) {
        float delta = doa_result - priv->last_result;
        if (delta < 0.0f) {
            delta = -delta;
        }
        if (!priv->has_last_result || cfg->callback_delta <= 0.0f || delta >= cfg->callback_delta) {
            cfg->result_callback(doa_result, cfg->ctx);
            priv->last_result = doa_result;
            priv->has_last_result = true;
        }
    }

    ret = cache_load->is_done ? ESP_GMF_JOB_ERR_DONE : ESP_GMF_JOB_ERR_OK;

done:
    if (out_load) {
        esp_gmf_port_release_out(out_port, out_load, ESP_GMF_MAX_DELAY);
    }
    if (priv->in_load) {
        esp_gmf_port_release_in(in_port, priv->in_load, ESP_GMF_MAX_DELAY);
        priv->in_load = NULL;
    }
    if (cache_load) {
        esp_gmf_cache_release(priv->cache, cache_load);
    }
    return ret;
}

static esp_gmf_job_err_t doa_wrapper_close(esp_gmf_audio_element_handle_t self, void *para)
{
    doa_wrapper_priv_t *priv = (doa_wrapper_priv_t *)self;
    if (priv->in_load) {
        esp_gmf_port_handle_t in_port = ESP_GMF_ELEMENT_GET(self)->in;
        if (in_port) {
            esp_gmf_port_release_in(in_port, priv->in_load, ESP_GMF_MAX_DELAY);
        }
        priv->in_load = NULL;
    }
    if (priv->doa_handle) {
        esp_doa_destroy(priv->doa_handle);
        priv->doa_handle = NULL;
    }
    if (priv->cache) {
        esp_gmf_cache_delete(priv->cache);
        priv->cache = NULL;
    }
    if (priv->mic_left) {
        esp_gmf_oal_free(priv->mic_left);
        priv->mic_left = NULL;
    }
    if (priv->mic_right) {
        esp_gmf_oal_free(priv->mic_right);
        priv->mic_right = NULL;
    }
    priv->has_last_result = false;
    return ESP_GMF_JOB_ERR_OK;
}

static esp_gmf_err_t load_doa_caps(esp_gmf_element_handle_t handle)
{
    esp_gmf_cap_t *caps = NULL;
    esp_gmf_cap_t doa_caps = {
        .cap_eightcc = ESP_GMF_CAPS_AUDIO_DOA,
        .attr_fun = NULL,
    };
    esp_gmf_err_t ret = esp_gmf_cap_append(&caps, &doa_caps);
    ESP_GMF_RET_ON_NOT_OK(TAG, ret, {return ret;}, "Failed to create DOA capability");
    ((esp_gmf_element_t *)handle)->caps = caps;
    return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t doa_wrapper_destroy(esp_gmf_audio_element_handle_t self)
{
    if (!self) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    if (OBJ_GET_CFG(self)) {
        esp_gmf_oal_free(OBJ_GET_CFG(self));
    }
    doa_wrapper_close(self, NULL);
    esp_gmf_audio_el_deinit(self);
    esp_gmf_oal_free(self);
    return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t doa_wrapper_new(void *cfg, esp_gmf_obj_handle_t *handle)
{
    return esp_ai_audio_wrapper_doa_init((esp_ai_audio_wrapper_doa_cfg_t *)cfg, handle);
}

esp_gmf_err_t esp_ai_audio_wrapper_doa_init(esp_ai_audio_wrapper_doa_cfg_t *cfg,
                                            esp_gmf_obj_handle_t *out_handle)
{
    if (cfg == NULL || out_handle == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }

    doa_wrapper_priv_t *priv = esp_gmf_oal_calloc(1, sizeof(*priv));
    ESP_GMF_MEM_VERIFY(TAG, priv, {return ESP_GMF_ERR_MEMORY_LACK;}, "doa_wrapper", sizeof(*priv));
    esp_gmf_obj_t *obj = (esp_gmf_obj_t *)priv;
    obj->new_obj = doa_wrapper_new;
    obj->del_obj = doa_wrapper_destroy;

    esp_ai_audio_wrapper_doa_cfg_t *obj_cfg = esp_gmf_oal_calloc(1, sizeof(*obj_cfg));
    if (!obj_cfg) {
        esp_gmf_oal_free(priv);
        return ESP_GMF_ERR_MEMORY_LACK;
    }
    memcpy(obj_cfg, cfg, sizeof(*obj_cfg));
    esp_gmf_obj_set_config(obj, obj_cfg, sizeof(*obj_cfg));
    esp_gmf_obj_set_tag(obj, "ai_doa");

    esp_gmf_element_cfg_t el_cfg = {0};
    ESP_GMF_ELEMENT_IN_PORT_ATTR_SET(el_cfg.in_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                     ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
    ESP_GMF_ELEMENT_OUT_PORT_ATTR_SET(el_cfg.out_attr, ESP_GMF_EL_PORT_CAP_SINGLE, 16, 0,
                                      ESP_GMF_PORT_TYPE_BLOCK | ESP_GMF_PORT_TYPE_BYTE, 1024);
    el_cfg.dependency = true;
    esp_gmf_err_t ret = esp_gmf_audio_el_init(priv, &el_cfg);
    if (ret != ESP_GMF_ERR_OK) {
        doa_wrapper_destroy(priv);
        return ret;
    }
    ESP_GMF_ELEMENT_GET(priv)->ops.open = doa_wrapper_open;
    ESP_GMF_ELEMENT_GET(priv)->ops.process = doa_wrapper_process;
    ESP_GMF_ELEMENT_GET(priv)->ops.close = doa_wrapper_close;
    ESP_GMF_ELEMENT_GET(priv)->ops.event_receiver = doa_wrapper_event_receiver;
    ESP_GMF_ELEMENT_GET(priv)->ops.load_caps = load_doa_caps;
    *out_handle = obj;
    return ret;
}

esp_gmf_err_t esp_ai_audio_wrapper_doa_set_result_cb(esp_gmf_obj_handle_t handle,
                                                     esp_ai_audio_doa_result_cb_t cb,
                                                     void *ctx)
{
    if (handle == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    esp_ai_audio_wrapper_doa_cfg_t *cfg = OBJ_GET_CFG(handle);
    if (!cfg) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    cfg->result_callback = cb;
    cfg->ctx = ctx;
    return ESP_GMF_ERR_OK;
}
