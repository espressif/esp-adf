/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_afe_config.h"
#include "esp_capture_audio_src_if.h"
#include "esp_capture_types.h"
#include "esp_codec_dev.h"
#include "esp_gmf_aec.h"
#include "esp_gmf_afe.h"
#include "esp_gmf_afe_manager.h"
#include "esp_gmf_audio_param.h"
#include "esp_gmf_caps_def.h"
#include "esp_gmf_err.h"
#include "esp_gmf_info.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_thread.h"
#include "esp_gmf_pipeline.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_port.h"
#include "esp_gmf_alc.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_task.h"
#include "esp_gmf_wn.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_service_scheduler.h"
#include "model_path.h"

#include "esp_ai_audio_src.h"
#include "esp_ai_audio_wrapper_doa.h"

static const char *TAG = "AI_AUDIO_SRC";

#define AI_AUDIO_CODEC_PCM       0x204d4350U
#define AI_AUDIO_OUT_RATE        16000
#define AI_AUDIO_OUT_BITS        16
#define AI_AUDIO_OUT_CH          1
#define AI_AUDIO_RB_SIZE         (8 * 1024)
#define AI_MODEL_RUN_STACK       (8192)
#define AI_AUDIO_DUMP_PATH_MAX   256
#define AI_AUDIO_DUMP_FILE_NAME  "src.pcm"

#define AI_AUDIO_DEFAULT_SAMPLE_RATE         16000
#define AI_AUDIO_DEFAULT_BITS                16
#define AI_AUDIO_DEFAULT_DOA_RES             10.0f
#define AI_AUDIO_DEFAULT_DOA_DIST            0.08f
#define AI_AUDIO_DEFAULT_DOA_FRAME           64
#define AI_AUDIO_DEFAULT_AEC_FILTER_LENGTH   4
#define AI_AUDIO_DEFAULT_VAD_MODE            3
#define AI_AUDIO_DEFAULT_VAD_MIN_SPEECH_MS   128
#define AI_AUDIO_DEFAULT_VAD_MIN_NOISE_MS    256
#define AI_AUDIO_DEFAULT_VAD_DELAY_MS        128
#define AI_AUDIO_DEFAULT_DOA_CALLBACK_DELTA  5.0f

typedef struct {
    int (*body)(void *arg);
    void              *arg;
    int                ret;
    SemaphoreHandle_t  done;
} ai_ram_sync_arg_t;

typedef enum {
    AI_AUDIO_SETTING_ALC_GAIN = 0,
} ai_audio_setting_type_t;

typedef struct ai_audio_setting {
    ai_audio_setting_type_t  type;
    union {
        struct {
            int    channel;
            float  gain;
        } alc_gain;
    } data;
    struct ai_audio_setting *next;
} ai_audio_setting_t;

/* ---- internal state ---- */

typedef struct {
    esp_capture_audio_src_if_t                      base;
    esp_codec_dev_handle_t                          record_handle;
    const char                                     *mic_layout;
    const char                                     *service_name;
    esp_ai_audio_feature_t                          features;
    esp_capture_audio_info_t                        input_info;
    esp_capture_service_ai_audio_src_feature_cfg_t  runtime_cfg;
    const char                                     *model_partition;
    void                                           *models;
    void                                           *afe_manager;
    afe_config_t                                   *afe_cfg;
    bool                                            models_owned;
    bool                                            afe_manager_owned;
    bool                                            afe_el_registered;
    /* callbacks (set before start) */
    esp_capture_service_ai_audio_src_vad_cb_t       vad_cb;
    void                                           *vad_ctx;
    esp_capture_service_ai_audio_src_wn_cb_t        wn_cb;
    void                                           *wn_ctx;
    esp_capture_service_ai_audio_src_doa_cb_t       doa_cb;
    void                                           *doa_ctx;
    esp_capture_service_ai_audio_src_read_cb_t      read_cb;
    void                                           *read_ctx;
    /* GMF pipeline */
    esp_gmf_pool_handle_t                           pool;
    bool                                            pool_owned;
    bool                                            elements_registered;
    esp_gmf_pipeline_handle_t                       pipeline;
    esp_gmf_task_handle_t                           task;
    RingbufHandle_t                                 out_rb;
    uint8_t                                        *read_buf;
    uint32_t                                        read_buf_size;
    uint8_t                                        *out_buf;
    uint32_t                                        out_buf_size;
    char                                            dump_path[AI_AUDIO_DUMP_PATH_MAX];
    FILE                                           *dump_file;
    bool                                            dump_write_failed;
    /* deferred settings applied after the pipeline is built */
    ai_audio_setting_t                             *settings;
    /* output caps */
    esp_capture_audio_info_t                        out_info;
    bool                                            use_fixed_caps;
    esp_capture_audio_info_t                        fixed_caps;
    /* state */
    bool                                            opened;
    bool                                            started;
    uint64_t                                        frames;
    bool                                            is_error;
} ai_audio_src_t;

/* ---- helpers ---- */
static bool f_has(esp_ai_audio_feature_t f, esp_ai_audio_feature_t b)  {  return (f & b) != 0;  }

static bool afe_enabled(void)
{
#ifdef CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_AFE_SUPPORT
    return true;
#else
    return false;
#endif
}

static bool aec_enabled(void)
{
#ifdef CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_AEC_SUPPORT
    return true;
#else
    return false;
#endif
}

static bool vad_enabled(void)
{
#ifdef CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_VAD_SUPPORT
    return true;
#else
    return false;
#endif
}

static bool ns_enabled(void)
{
#ifdef CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_NS_SUPPORT
    return true;
#else
    return false;
#endif
}

static bool wn_enabled(void)
{
#ifdef CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_WN_SUPPORT
    return true;
#else
    return false;
#endif
}

static bool doa_enabled(void)
{
#ifdef CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_DOA_SUPPORT
    return true;
#else
    return false;
#endif
}

static bool ai_task_stack_in_ram(void)
{
#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0))
    uint8_t *task_stack = pxTaskGetStackStart(NULL);
#else
    uint8_t *task_stack = xTaskGetStackStart(NULL);
#endif  /* (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)) */
    return task_stack && esp_ptr_internal(task_stack);
}

static void ai_ram_sync_task(void *arg)
{
    ai_ram_sync_arg_t *sync = (ai_ram_sync_arg_t *)arg;
    if (sync->body != NULL) {
        sync->ret = sync->body(sync->arg);
    }
    if (sync->done != NULL) {
        xSemaphoreGive(sync->done);
    }
    esp_gmf_oal_thread_delete(NULL);
}

/**
 * @brief  Run flash/model APIs on an internal-RAM stack.
 *
 *         Same pattern as CAPTURE_RUN_SYNC_IN_RAM: model mmap disables cache and
 *         requires the current task stack to be in SRAM. Caller tasks often run
 *         with a PSRAM stack, so hop to a short-lived internal-stack thread.
 */
static int ai_run_sync_in_ram(const char *name, int (*body)(void *arg), void *arg, uint32_t stack_size)
{
    if (body == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ai_task_stack_in_ram()) {
        return body(arg);
    }

    ai_ram_sync_arg_t sync = {
        .body = body,
        .arg = arg,
        .ret = ESP_FAIL,
    };
    sync.done = xSemaphoreCreateBinary();
    if (sync.done == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_gmf_oal_thread_t thr = NULL;
    if (esp_gmf_oal_thread_create(&thr, name, ai_ram_sync_task, &sync, stack_size, 5, false,
                                  tskNO_AFFINITY) != ESP_GMF_ERR_OK) {
        vSemaphoreDelete(sync.done);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(sync.done, portMAX_DELAY);
    vSemaphoreDelete(sync.done);
    return sync.ret;
}

static bool needs_afe(esp_ai_audio_feature_t f)
{
    return f_has(f, ESP_AI_AUDIO_FEATURE_NS) ||
           f_has(f, ESP_AI_AUDIO_FEATURE_VAD) ||
           (f_has(f, ESP_AI_AUDIO_FEATURE_AEC) && !aec_enabled()) ||
           (f_has(f, ESP_AI_AUDIO_FEATURE_WN) && !wn_enabled());
}

static const char *input_format_or_default(const char *mic_layout)
{
    if (mic_layout && mic_layout[0]) {
        return mic_layout;
    }
    return "MR";
}

static uint8_t channel_count_in_format(const char *input_format)
{
    return input_format ? (uint8_t)strlen(input_format) : 0;
}

static uint8_t mic_count_in_format(const char *input_format)
{
    uint8_t count = 0;
    if (!input_format) {
        return 0;
    }
    for (uint8_t i = 0; input_format[i] != '\0'; i++) {
        if (input_format[i] == 'M') {
            count++;
        }
    }
    return count;
}

static uint8_t ref_count_in_format(const char *input_format)
{
    uint8_t count = 0;
    if (!input_format) {
        return 0;
    }
    for (uint8_t i = 0; input_format[i] != '\0'; i++) {
        if (input_format[i] == 'R') {
            count++;
        }
    }
    return count;
}

static esp_capture_err_t validate_doa_layout(const char *input_format)
{
    uint8_t channels = channel_count_in_format(input_format);
    uint8_t mics = mic_count_in_format(input_format);
    uint8_t refs = ref_count_in_format(input_format);

    if (mics == 2) {
        return ESP_CAPTURE_ERR_OK;
    }
    /* Boards with one mic + DAC echo reference (e.g. "RM"/"MR") still provide
       two physical channels. Fall back to M+Ref so DOA can continue. */
    if (channels == 2 && mics == 1 && refs == 1) {
        ESP_LOGW(TAG, "DOA layout '%s' has one mic and one reference; using M+Ref",
                 input_format);
        return ESP_CAPTURE_ERR_OK;
    }
    if (channels < 2) {
        ESP_LOGE(TAG, "DOA needs at least two input channels, format:%s", input_format);
    } else {
        ESP_LOGE(TAG, "DOA requires two microphone channels or a 2-ch M+Ref layout, format:%s",
                 input_format);
    }
    return ESP_CAPTURE_ERR_NOT_SUPPORTED;
}

static int read_input(ai_audio_src_t *s, uint8_t *buf, uint32_t size)
{
    int ret = 0;
    if (s->read_cb) {
        ret = s->read_cb(buf, size, s->read_ctx);
    } else if (s->record_handle) {
        /* esp_codec_dev_read returns ESP_CODEC_DEV_OK (0) on success, not byte count. */
        int codec_ret = esp_codec_dev_read(s->record_handle, buf, size);
        ret = (codec_ret == ESP_CODEC_DEV_OK) ? (int)size : ((codec_ret < 0) ? codec_ret : -codec_ret);
    }
    if (ret > 0 && s->dump_file != NULL && !s->dump_write_failed) {
        size_t written = fwrite(buf, 1, (size_t)ret, s->dump_file);
        if (written != (size_t)ret) {
            ESP_LOGE(TAG, "Failed to write AI source dump: %s", s->dump_path);
            s->dump_write_failed = true;
        }
    }
    return ret;
}

static void afe_event_cb(esp_gmf_element_handle_t el, esp_gmf_afe_evt_t *event, void *user_data)
{
    (void)el;
    ai_audio_src_t *s = (ai_audio_src_t *)user_data;
    if (!s || !event) {
        return;
    }
    if (event->type == ESP_GMF_AFE_EVT_VAD_START) {
        if (s->vad_cb) {
            s->vad_cb(1, s->vad_ctx);
        }
    } else if (event->type == ESP_GMF_AFE_EVT_VAD_END) {
        if (s->vad_cb) {
            s->vad_cb(0, s->vad_ctx);
        }
    } else if (event->type == ESP_GMF_AFE_EVT_WAKEUP_START && s->wn_cb) {
        int trigger_ch = 0;
        if (event->event_data && event->data_len >= sizeof(esp_gmf_afe_wakeup_info_t)) {
            esp_gmf_afe_wakeup_info_t *info = (esp_gmf_afe_wakeup_info_t *)event->event_data;
            trigger_ch = info->wake_word_index;
        }
        s->wn_cb(trigger_ch, s->wn_ctx);
    }
}

static void wn_detect_cb(esp_gmf_element_handle_t handle, int32_t trigger_ch, void *user_ctx)
{
    (void)handle;
    ai_audio_src_t *s = (ai_audio_src_t *)user_ctx;
    if (s && s->wn_cb) {
        s->wn_cb((int)trigger_ch, s->wn_ctx);
    }
}

static esp_gmf_err_io_t src_acquire(void *h, esp_gmf_payload_t *load, uint32_t want, int ticks)
{
    ai_audio_src_t *s = (ai_audio_src_t *)h;
    (void)want;
    (void)ticks;
    if (!s) {
        return ESP_GMF_IO_FAIL;
    }
    uint32_t need = s->input_info.sample_rate * (s->input_info.bits_per_sample / 8)
                    * s->input_info.channel * 20 / 1000;
    if (want > need) {
        need = want;
    }
    if (!s->read_buf || s->read_buf_size < need) {
        uint8_t *nb = esp_gmf_oal_realloc(s->read_buf, need);
        if (!nb) {
            return ESP_GMF_IO_FAIL;
        }
        s->read_buf = nb;
        s->read_buf_size = need;
    }
    int r = read_input(s, s->read_buf, need);
    if (r <= 0) {
        return ESP_GMF_IO_FAIL;
    }
    load->buf = s->read_buf;
    load->buf_length = (uint32_t)r;
    load->valid_size = (uint32_t)r;
    return ESP_GMF_IO_OK;
}
static esp_gmf_err_io_t src_release(void *h, esp_gmf_payload_t *load, uint32_t want, int ticks)
{
    (void)h;
    (void)load;
    (void)want;
    (void)ticks;
    return ESP_GMF_IO_OK;
}

static esp_gmf_err_io_t sink_acquire(void *h, esp_gmf_payload_t *load, uint32_t want, int ticks)
{
    (void)ticks;
    ai_audio_src_t *s = (ai_audio_src_t *)h;
    if (!s) {
        return ESP_GMF_IO_FAIL;
    }
    if (!s->out_buf || s->out_buf_size < want) {
        if (s->out_buf) {
            esp_gmf_oal_free(s->out_buf);
        }
        s->out_buf = esp_gmf_oal_malloc_align(16, want);
        if (!s->out_buf) {
            return ESP_GMF_IO_FAIL;
        }
        s->out_buf_size = want;
    }
    load->buf = s->out_buf;
    load->buf_length = s->out_buf_size;
    load->valid_size = 0;
    return ESP_GMF_IO_OK;
}
static esp_gmf_err_io_t sink_release(void *h, esp_gmf_payload_t *load, uint32_t want, int ticks)
{
    (void)want;
    (void)ticks;
    ai_audio_src_t *s = (ai_audio_src_t *)h;
    if (!s || !s->out_rb || !load->buf || load->valid_size == 0) {
        return ESP_GMF_IO_OK;
    }
    if (xRingbufferSend(s->out_rb, load->buf, load->valid_size, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_GMF_IO_TIMEOUT;
    }
    return ESP_GMF_IO_OK;
}

static esp_gmf_err_t pipe_evt(esp_gmf_event_pkt_t *pkt, void *ctx)
{
    ai_audio_src_t *s = (ai_audio_src_t *)ctx;
    if (pkt && pkt->type == ESP_GMF_EVT_TYPE_CHANGE_STATE && ESP_GMF_EVENT_STATE_ERROR == pkt->sub) {
        s->is_error = true;
    }
    return ESP_GMF_ERR_OK;
}

static const char *tag_by_caps(esp_gmf_pool_handle_t pool, uint64_t cc)
{
    const void *it = NULL;
    esp_gmf_element_handle_t el = NULL;
    while (esp_gmf_pool_iterate_element(pool, &it, &el) == ESP_GMF_ERR_OK) {
        const esp_gmf_cap_t *caps = NULL;
        esp_gmf_element_get_caps(el, &caps);
        for (; caps; caps = caps->next) {
            if (caps->cap_eightcc == cc) {
                return OBJ_GET_TAG(el);
            }
        }
    }
    return NULL;
}

static void apply_runtime_defaults(ai_audio_src_t *s)
{
    if (!s->runtime_cfg.aec.mode) {
        s->runtime_cfg.aec.mode = AEC_MODE_SR_LOW_COST;
    }
    if (!s->runtime_cfg.aec.filter_length) {
        s->runtime_cfg.aec.filter_length = AI_AUDIO_DEFAULT_AEC_FILTER_LENGTH;
    }
    if (!s->runtime_cfg.vad.mode) {
        s->runtime_cfg.vad.mode = AI_AUDIO_DEFAULT_VAD_MODE;
    }
    if (!s->runtime_cfg.vad.min_speech_ms) {
        s->runtime_cfg.vad.min_speech_ms = AI_AUDIO_DEFAULT_VAD_MIN_SPEECH_MS;
    }
    if (!s->runtime_cfg.vad.min_noise_ms) {
        s->runtime_cfg.vad.min_noise_ms = AI_AUDIO_DEFAULT_VAD_MIN_NOISE_MS;
    }
    if (!s->runtime_cfg.vad.delay_ms) {
        s->runtime_cfg.vad.delay_ms = AI_AUDIO_DEFAULT_VAD_DELAY_MS;
    }
    if (s->runtime_cfg.doa.resolution <= 0.0f) {
        s->runtime_cfg.doa.resolution = AI_AUDIO_DEFAULT_DOA_RES;
    }
    if (s->runtime_cfg.doa.mic_distance <= 0.0f) {
        s->runtime_cfg.doa.mic_distance = AI_AUDIO_DEFAULT_DOA_DIST;
    }
    if (!s->runtime_cfg.doa.frame_ms) {
        s->runtime_cfg.doa.frame_ms = AI_AUDIO_DEFAULT_DOA_FRAME;
    }
    if (s->runtime_cfg.doa.callback_delta < 0.0f) {
        s->runtime_cfg.doa.callback_delta = 0.0f;
    } else if (s->runtime_cfg.doa.callback_delta == 0.0f) {
        s->runtime_cfg.doa.callback_delta = AI_AUDIO_DEFAULT_DOA_CALLBACK_DELTA;
    }
}

static esp_err_t validate_features(ai_audio_src_t *s)
{
    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_DOA) && !doa_enabled()) {
        ESP_LOGE(TAG, "DOA feature requested but DOA element is disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_VAD) && !vad_enabled()) {
        ESP_LOGE(TAG, "VAD feature requested but VAD support is disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_NS) && !ns_enabled()) {
        ESP_LOGE(TAG, "NS feature requested but NS support is disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (needs_afe(s->features) && !afe_enabled()) {
        ESP_LOGE(TAG, "Feature combination requires compact AFE but AFE is disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_AEC) && !aec_enabled() && !afe_enabled()) {
        ESP_LOGE(TAG, "AEC feature requested but AEC and AFE elements are disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_WN) && !wn_enabled() && !afe_enabled()) {
        ESP_LOGE(TAG, "WN feature requested but WN and AFE elements are disabled");
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static void config_aec(esp_gmf_element_handle_t el, ai_audio_src_t *s)
{
    esp_gmf_aec_cfg_t *cfg = (esp_gmf_aec_cfg_t *)OBJ_GET_CFG(el);
    if (!cfg) {
        return;
    }
    cfg->input_format = (char *)input_format_or_default(s->mic_layout);
    cfg->filter_len = s->runtime_cfg.aec.filter_length;
    cfg->type = AFE_TYPE_SR;
    cfg->mode = AFE_MODE_LOW_COST;
}

static void config_wn(esp_gmf_element_handle_t el, ai_audio_src_t *s)
{
    esp_gmf_wn_cfg_t *cfg = (esp_gmf_wn_cfg_t *)OBJ_GET_CFG(el);
    if (!cfg) {
        return;
    }
    cfg->input_format = (char *)input_format_or_default(s->mic_layout);
    if (s->models) {
        cfg->models = s->models;
    }
    cfg->detect_cb = wn_detect_cb;
    cfg->user_ctx = s;
}

static void config_afe(esp_gmf_element_handle_t el, ai_audio_src_t *s)
{
    esp_gmf_afe_cfg_t *cfg = (esp_gmf_afe_cfg_t *)OBJ_GET_CFG(el);
    if (!cfg) {
        return;
    }
    if (s->models) {
        cfg->models = s->models;
    }
    cfg->event_cb = afe_event_cb;
    cfg->event_ctx = s;
}

static void config_doa(esp_gmf_element_handle_t el, ai_audio_src_t *s)
{
    esp_ai_audio_wrapper_doa_cfg_t *cfg = (esp_ai_audio_wrapper_doa_cfg_t *)OBJ_GET_CFG(el);
    if (!cfg) {
        return;
    }
    cfg->sample_rate = s->input_info.sample_rate;
    cfg->input_format = input_format_or_default(s->mic_layout);
    cfg->resolution = s->runtime_cfg.doa.resolution;
    cfg->d_mics = s->runtime_cfg.doa.mic_distance;
    cfg->frame_ms = s->runtime_cfg.doa.frame_ms;
    cfg->callback_delta = s->runtime_cfg.doa.callback_delta;
    cfg->result_callback = s->doa_cb;
    cfg->ctx = s->doa_ctx;
}

static int load_models_in_ram(void *arg)
{
    ai_audio_src_t *s = (ai_audio_src_t *)arg;
    if (s->models == NULL) {
        s->models = esp_srmodel_init(s->model_partition);
        s->models_owned = s->models != NULL;
        if (s->models == NULL) {
            ESP_LOGW(TAG, "No SR model loaded from partition '%s'",
                     s->model_partition ? s->model_partition : "model");
            return ESP_FAIL;
        }
    }
    return 0;
}

static esp_err_t ensure_models_loaded(ai_audio_src_t *s)
{
    if (s == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s->models != NULL) {
        return ESP_OK;
    }
    int ret = ai_run_sync_in_ram("ai_model_load", load_models_in_ram, s, AI_MODEL_RUN_STACK);
    return (ret == 0) ? ESP_OK : ESP_FAIL;
}

static const char *ai_scheduler_service_name(const ai_audio_src_t *s)
{
    return (s != NULL && s->service_name != NULL) ? s->service_name : ESP_AUDIO_CAPTURE_SERVICE_NAME;
}

static void apply_scheduler_to_afe_task(ai_audio_src_t *s,
                                        const char *thread_name,
                                        esp_gmf_afe_manager_task_setting_t *task)
{
    if (thread_name == NULL || task == NULL) {
        return;
    }
    esp_service_thread_cfg_t def = {
        .stack_size = task->stack_size,
        .priority = task->prio,
        .core_id = task->core,
    };
    esp_service_thread_cfg_t out = def;
    esp_service_thread_request_t req = {
        .service_name = ai_scheduler_service_name(s),
        .thread_name = thread_name,
    };
    if (esp_service_scheduler_get_thread_cfg(&req, &def, &out) != ESP_OK) {
        return;
    }
    task->stack_size = out.stack_size;
    task->prio = out.priority;
    task->core = (out.core_id < 0) ? 0 : (uint8_t)out.core_id;
}

static int setup_default_afe_runtime_in_ram(void *arg)
{
    ai_audio_src_t *s = (ai_audio_src_t *)arg;
    (void)load_models_in_ram(s);

    const char *input_format = input_format_or_default(s->mic_layout);
    s->afe_cfg = afe_config_init(input_format, (srmodel_list_t *)s->models, AFE_TYPE_SR, AFE_MODE_LOW_COST);
    if (!s->afe_cfg) {
        ESP_LOGE(TAG, "Failed to create AFE config");
        return ESP_GMF_ERR_FAIL;
    }

    s->afe_cfg->aec_init = f_has(s->features, ESP_AI_AUDIO_FEATURE_AEC);
    s->afe_cfg->aec_mode = s->runtime_cfg.aec.mode;
    s->afe_cfg->aec_filter_length = s->runtime_cfg.aec.filter_length;
    s->afe_cfg->se_init = false;
    s->afe_cfg->ns_init = f_has(s->features, ESP_AI_AUDIO_FEATURE_NS);
    s->afe_cfg->wakenet_init = f_has(s->features, ESP_AI_AUDIO_FEATURE_WN);
    s->afe_cfg->vad_init = f_has(s->features, ESP_AI_AUDIO_FEATURE_VAD);
    s->afe_cfg->vad_mode = s->runtime_cfg.vad.mode;
    s->afe_cfg->vad_min_speech_ms = s->runtime_cfg.vad.min_speech_ms;
    s->afe_cfg->vad_min_noise_ms = s->runtime_cfg.vad.min_noise_ms;
    s->afe_cfg->vad_delay_ms = s->runtime_cfg.vad.delay_ms;
    s->afe_cfg->fixed_first_channel = true;
    s->afe_cfg->fixed_output_channel = true;

    esp_gmf_afe_manager_cfg_t manager_cfg = DEFAULT_GMF_AFE_MANAGER_CFG(s->afe_cfg, NULL, NULL, NULL, NULL);
    apply_scheduler_to_afe_task(s, ESP_AUDIO_CAPTURE_TASK_AFE_FEED, &manager_cfg.feed_task_setting);
    apply_scheduler_to_afe_task(s, ESP_AUDIO_CAPTURE_TASK_AFE_FETCH, &manager_cfg.fetch_task_setting);
    if (esp_gmf_afe_manager_create(&manager_cfg, (esp_gmf_afe_manager_handle_t *)&s->afe_manager) != ESP_GMF_ERR_OK) {
        ESP_LOGE(TAG, "Failed to create AFE manager");
        afe_config_free(s->afe_cfg);
        s->afe_cfg = NULL;
        return ESP_GMF_ERR_FAIL;
    }
    s->afe_manager_owned = true;
    return ESP_GMF_ERR_OK;
}

static esp_gmf_err_t setup_default_afe_runtime(ai_audio_src_t *s)
{
    if (!s || !needs_afe(s->features) || s->afe_manager) {
        return ESP_GMF_ERR_OK;
    }
    return (esp_gmf_err_t)ai_run_sync_in_ram("ai_afe_open", setup_default_afe_runtime_in_ram, s, AI_MODEL_RUN_STACK);
}

static int clear_model_runtime_in_ram(void *arg)
{
    ai_audio_src_t *s = (ai_audio_src_t *)arg;
    if (s->afe_manager_owned && s->afe_manager != NULL) {
        /* Suspend first so feed/fetch leave their run loops promptly. */
        (void)esp_gmf_afe_manager_suspend((esp_gmf_afe_manager_handle_t)s->afe_manager, true);
        esp_gmf_afe_manager_destroy((esp_gmf_afe_manager_handle_t)s->afe_manager);
        s->afe_manager = NULL;
        s->afe_manager_owned = false;
    }
    if (s->afe_cfg != NULL) {
        afe_config_free(s->afe_cfg);
        s->afe_cfg = NULL;
    }
    if (s->models_owned && s->models != NULL) {
        esp_srmodel_deinit((srmodel_list_t *)s->models);
        s->models = NULL;
        s->models_owned = false;
    }
    return 0;
}

static void clear_internal_runtime(ai_audio_src_t *s)
{
    if (s == NULL) {
        return;
    }
    if (s->pool_owned && s->pool != NULL) {
        esp_gmf_pool_deinit(s->pool);
        s->pool = NULL;
        s->pool_owned = false;
    }
    (void)ai_run_sync_in_ram("ai_afe_close", clear_model_runtime_in_ram, s, AI_MODEL_RUN_STACK);
    s->afe_el_registered = false;
    s->elements_registered = false;
}

static esp_gmf_err_t ensure_afe_element(ai_audio_src_t *s)
{
    if (!s || !needs_afe(s->features)) {
        return ESP_GMF_ERR_OK;
    }
    esp_gmf_err_t ret = setup_default_afe_runtime(s);
    if (ret != ESP_GMF_ERR_OK) {
        return ret;
    }
    if (!s->afe_manager) {
        return ESP_GMF_ERR_FAIL;
    }
    if (s->afe_el_registered) {
        return ESP_GMF_ERR_OK;
    }
    esp_gmf_afe_cfg_t ac = DEFAULT_GMF_AFE_CFG(s->afe_manager, afe_event_cb, s, s->models);
    esp_gmf_obj_handle_t ael = NULL;
    ret = esp_gmf_afe_init(&ac, &ael);
    if (ret != ESP_GMF_ERR_OK) {
        return ret;
    }
    ret = esp_gmf_pool_register_element(s->pool, (esp_gmf_element_handle_t)ael, NULL);
    if (ret != ESP_GMF_ERR_OK) {
        esp_gmf_obj_delete(ael);
        return ret;
    }
    s->afe_el_registered = true;
    return ESP_GMF_ERR_OK;
}

static esp_err_t ensure_pool_and_elements(ai_audio_src_t *s)
{
    if (s == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s->pool == NULL && esp_gmf_pool_init((esp_gmf_pool_handle_t *)&s->pool) == ESP_GMF_ERR_OK) {
        s->pool_owned = true;
    }
    if (s->pool == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (s->elements_registered) {
        return ESP_OK;
    }
    const char *input_format = input_format_or_default(s->mic_layout);

    esp_ae_rate_cvt_cfg_t rc = DEFAULT_ESP_GMF_RATE_CVT_CONFIG();
    esp_gmf_obj_handle_t rel = NULL;
    if (esp_gmf_rate_cvt_init(&rc, &rel) == ESP_GMF_ERR_OK) {
        esp_gmf_pool_register_element(s->pool, (esp_gmf_element_handle_t)rel, NULL);
    }
    esp_ae_alc_cfg_t ac = DEFAULT_ESP_GMF_ALC_CONFIG();
    esp_gmf_element_handle_t alc_el = NULL;
    if (esp_gmf_alc_init(&ac, &alc_el) == ESP_GMF_ERR_OK) {
        esp_gmf_pool_register_element(s->pool, alc_el, NULL);
    }

#if CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_AEC_SUPPORT
    if (!needs_afe(s->features) && f_has(s->features, ESP_AI_AUDIO_FEATURE_AEC)) {
        esp_gmf_aec_cfg_t acfg = {
            .filter_len = s->runtime_cfg.aec.filter_length,
            .type = AFE_TYPE_SR,
            .mode = AFE_MODE_LOW_COST,
            .input_format = (char *)input_format,
        };
        esp_gmf_obj_handle_t el = NULL;
        if (esp_gmf_aec_init(&acfg, &el) == ESP_GMF_ERR_OK) {
            esp_gmf_pool_register_element(s->pool, (esp_gmf_element_handle_t)el, NULL);
        }
    }
#endif  /* CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_AEC_SUPPORT */
#if CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_DOA_SUPPORT
    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_DOA)) {
        esp_ai_audio_wrapper_doa_cfg_t dcfg = {
            .sample_rate = s->input_info.sample_rate,
            .resolution = s->runtime_cfg.doa.resolution,
            .d_mics = s->runtime_cfg.doa.mic_distance,
            .frame_ms = s->runtime_cfg.doa.frame_ms,
            .callback_delta = s->runtime_cfg.doa.callback_delta,
            .input_format = input_format,
            .result_callback = s->doa_cb,
            .ctx = s->doa_ctx,
        };
        esp_gmf_obj_handle_t el = NULL;
        if (esp_ai_audio_wrapper_doa_init(&dcfg, &el) == ESP_GMF_ERR_OK) {
            esp_gmf_pool_register_element(s->pool, (esp_gmf_element_handle_t)el, NULL);
        }
    }
#endif  /* CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_DOA_SUPPORT */
#if CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_WN_SUPPORT
    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_WN) && !needs_afe(s->features)) {
        /* Standalone WakeNet does not go through AFE setup, so load models here. */
        if (ensure_models_loaded(s) != ESP_OK) {
            ESP_LOGE(TAG, "WakeNet requires SR models but none were loaded");
            return ESP_ERR_NOT_FOUND;
        }
        esp_gmf_wn_cfg_t wcfg = {
            .models = s->models,
            .input_format = (char *)input_format,
            .detect_cb = wn_detect_cb,
            .user_ctx = s,
        };
        esp_gmf_element_handle_t el = NULL;
        if (esp_gmf_wn_init(&wcfg, &el) == ESP_GMF_ERR_OK) {
            esp_gmf_pool_register_element(s->pool, el, NULL);
        }
    }
#endif  /* CONFIG_ESP_AUDIO_CAPTURE_SERVICE_AI_SRC_WN_SUPPORT */
    s->elements_registered = true;
    return ESP_OK;
}

static esp_err_t build_pipeline(ai_audio_src_t *s)
{
    if (!s->pool) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ensure_afe_element(s) != ESP_GMF_ERR_OK) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    const char *names[8];
    int n = 0;
    if (s->features != ESP_AI_AUDIO_FEATURE_NONE) {
        bool alc_added = false;
        const char *alc_tag = tag_by_caps(s->pool, ESP_GMF_CAPS_AUDIO_ALC);
        if (alc_tag && s->input_info.sample_rate < AI_AUDIO_OUT_RATE) {
            names[n++] = "aud_alc";
            alc_added = true;
        }
        bool need_rs = (s->input_info.sample_rate != AI_AUDIO_OUT_RATE);
        const char *rs_tag = tag_by_caps(s->pool, ESP_GMF_CAPS_AUDIO_RATE_CONVERT);
        if (need_rs && rs_tag) {
            names[n++] = rs_tag;
        }
        if (alc_tag && alc_added == false) {
            names[n++] = alc_tag;
        }
    }

    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_DOA)) {
        if (tag_by_caps(s->pool, ESP_GMF_CAPS_AUDIO_DOA)) {
            names[n++] = "ai_doa";
        } else {
            return ESP_ERR_NOT_SUPPORTED;
        }
    }

    if (needs_afe(s->features)) {
        if (tag_by_caps(s->pool, ESP_GMF_CAPS_AUDIO_VAD) || tag_by_caps(s->pool, ESP_GMF_CAPS_AUDIO_NS)) {
            names[n++] = "ai_afe";
        } else {
            return ESP_ERR_NOT_SUPPORTED;
        }
    } else if (f_has(s->features, ESP_AI_AUDIO_FEATURE_AEC)) {
        names[n++] = "ai_aec";
    }
    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_WN) && !needs_afe(s->features)) {
        names[n++] = "ai_wn";
    }

    if (n == 0) {
        return ESP_OK;
    }

    /* Create pipeline. */
    esp_gmf_err_t r = esp_gmf_pool_new_pipeline(s->pool, NULL, names, n, NULL, &s->pipeline);
    if (r) {
        ESP_LOGE(TAG, "pipeline create: %d", r);
        return ESP_FAIL;
    }

    /* I/O ports. */
    esp_gmf_port_handle_t ip = NEW_ESP_GMF_PORT_IN_BLOCK(src_acquire, src_release, NULL, s, 2048, 20);
    esp_gmf_port_handle_t op = NEW_ESP_GMF_PORT_OUT_BLOCK(sink_acquire, sink_release, NULL, s, 0, ESP_GMF_MAX_DELAY);
    if (!ip || !op) {
        esp_gmf_pipeline_destroy(s->pipeline);
        s->pipeline = NULL;
        return ESP_FAIL;
    }
    esp_gmf_element_handle_t head = NULL;
    esp_gmf_pipeline_get_head_el(s->pipeline, &head);
    esp_gmf_element_register_in_port(head, ip);
    esp_gmf_element_register_out_port(s->pipeline->last_el, op);

    /* ---- Configure each element from head to tail ---- */
    const void *it = NULL;
    esp_gmf_element_handle_t el = NULL;
    while (esp_gmf_pipeline_iterate_element(s->pipeline, &it, &el) == ESP_GMF_ERR_OK) {
        const char *tag = OBJ_GET_TAG(el);
        if (!tag) {
            continue;
        }
        if (strstr(tag, "rate_cvt") || strstr(tag, "resample") || strstr(tag, "rate")) {
            esp_ae_rate_cvt_cfg_t *rc = (esp_ae_rate_cvt_cfg_t *)OBJ_GET_CFG(el);
            if (rc) {
                rc->src_rate = s->input_info.sample_rate;
                rc->dest_rate = AI_AUDIO_OUT_RATE;
                rc->bits_per_sample = AI_AUDIO_OUT_BITS;
                rc->channel = s->input_info.channel;
            }
        } else if (strstr(tag, "aec")) {
            config_aec(el, s);
        } else if (strstr(tag, "wn") || strstr(tag, "wakenet")) {
            config_wn(el, s);
        } else if (strstr(tag, "afe")) {
            config_afe(el, s);
        } else if (strstr(tag, "doa")) {
            config_doa(el, s);
        }
    }

    esp_gmf_info_sound_t info = {
        .format_id = s->input_info.format_id,
        .sample_rates = s->input_info.sample_rate,
        .channels = s->input_info.channel,
        .bits = s->input_info.bits_per_sample,
    };
    esp_gmf_pipeline_report_info(s->pipeline, ESP_GMF_INFO_SOUND, &info, sizeof(info));

    /* ---- Task with scheduler support ---- */
    esp_service_thread_cfg_t def = {
        .stack_size = 4 * 1024, .priority = 5, .core_id = 0};
    esp_service_thread_cfg_t out = def;
    esp_service_thread_request_t req = {
        .service_name = ai_scheduler_service_name(s),
        .thread_name = ESP_AUDIO_CAPTURE_TASK_AI_PIPE,
    };
    esp_service_scheduler_get_thread_cfg(&req, &def, &out);

    esp_gmf_task_cfg_t tcfg = {
        .thread = {.stack = out.stack_size, .prio = out.priority, .core = out.core_id},
        .name = ESP_AUDIO_CAPTURE_TASK_AI_PIPE,
    };
    r = esp_gmf_task_init(&tcfg, &s->task);
    if (r) {
        esp_gmf_pipeline_destroy(s->pipeline);
        s->pipeline = NULL;
        return ESP_FAIL;
    }

    esp_gmf_pipeline_bind_task(s->pipeline, s->task);
    esp_gmf_pipeline_set_event(s->pipeline, pipe_evt, s);
    r = esp_gmf_pipeline_loading_jobs(s->pipeline);
    if (r) {
        esp_gmf_task_deinit(s->task);
        s->task = NULL;
        esp_gmf_pipeline_destroy(s->pipeline);
        s->pipeline = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_capture_err_t a_stop(esp_capture_audio_src_if_t *b);
static esp_capture_err_t a_open(esp_capture_audio_src_if_t *b)
{
    ai_audio_src_t *s = (ai_audio_src_t *)b;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    /* Capture may open the shared source twice (auto pipeline + aud_src
       negotiate). Keep open idempotent so dump FILE* is not leaked. */
    if (s->opened) {
        return ESP_CAPTURE_ERR_OK;
    }
    if (s->dump_path[0] != '\0') {
        if (s->dump_file != NULL) {
            fclose(s->dump_file);
            s->dump_file = NULL;
        }
        s->dump_file = fopen(s->dump_path, "wb");
        if (s->dump_file == NULL) {
            ESP_LOGE(TAG, "Failed to open AI source dump: %s", s->dump_path);
            return ESP_CAPTURE_ERR_INTERNAL;
        }
        s->dump_write_failed = false;
        ESP_LOGI(TAG, "Opened AI source dump: %s", s->dump_path);
    }
    s->opened = true;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t a_codecs(esp_capture_audio_src_if_t *b, const esp_capture_format_id_t **c, uint8_t *n)
{
    static const esp_capture_format_id_t cc[] = {AI_AUDIO_CODEC_PCM};
    *c = cc;
    *n = 1;
    (void)b;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t a_fixcaps(esp_capture_audio_src_if_t *b, const esp_capture_audio_info_t *fc)
{
    ai_audio_src_t *s = (ai_audio_src_t *)b;
    if (!s || !fc) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    s->fixed_caps = *fc;
    s->use_fixed_caps = true;
    return ESP_CAPTURE_ERR_OK;
}

/**
 * @brief  Output channel count after the AI pipeline.
 *
 *         AEC / AFE (NS, VAD) / WakeNet collapse to mono. DOA is pass-through and
 *         keeps the physical layout channels. With no AI elements the source
 *         also returns the layout channel count.
 */
static uint8_t ai_output_channel_count(const ai_audio_src_t *s, uint8_t input_channels)
{
    if (needs_afe(s->features) ||
        f_has(s->features, ESP_AI_AUDIO_FEATURE_AEC) ||
        f_has(s->features, ESP_AI_AUDIO_FEATURE_WN)) {
        return AI_AUDIO_OUT_CH;
    }
    return input_channels ? input_channels : AI_AUDIO_OUT_CH;
}

static esp_capture_err_t a_negotiate(esp_capture_audio_src_if_t *b, esp_capture_audio_info_t *in, esp_capture_audio_info_t *out)
{
    ai_audio_src_t *s = (ai_audio_src_t *)b;
    if (!s || !out) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    const char *input_format = input_format_or_default(s->mic_layout);
    uint8_t layout_channels = channel_count_in_format(input_format);

    /* For AI sources, fixed_caps only pins ADC sample rate. Channel count and
       bit depth are taken from mic_layout / AI defaults so AEC/DOA always open
       the codec with the physical layout (e.g. "RM" => 2ch). */
    uint32_t sample_rate = AI_AUDIO_DEFAULT_SAMPLE_RATE;
    if (s->use_fixed_caps && s->fixed_caps.sample_rate) {
        sample_rate = s->fixed_caps.sample_rate;
    } else if (in != NULL && in->sample_rate) {
        sample_rate = in->sample_rate;
    }

    s->input_info.format_id = ESP_CAPTURE_FMT_ID_PCM;
    s->input_info.sample_rate = sample_rate;
    s->input_info.bits_per_sample = AI_AUDIO_DEFAULT_BITS;
    s->input_info.channel = layout_channels ? layout_channels : 1;

    out->format_id = AI_AUDIO_CODEC_PCM;
    out->sample_rate = AI_AUDIO_OUT_RATE;
    out->bits_per_sample = AI_AUDIO_OUT_BITS;
    out->channel = ai_output_channel_count(s, s->input_info.channel);
    s->out_info = *out;
    return ESP_CAPTURE_ERR_OK;
}

static void ensure_input_info_defaults(ai_audio_src_t *s)
{
    if (s->input_info.format_id == 0) {
        s->input_info.format_id = ESP_CAPTURE_FMT_ID_PCM;
    }
    if (s->input_info.sample_rate == 0) {
        s->input_info.sample_rate = AI_AUDIO_DEFAULT_SAMPLE_RATE;
    }
    if (s->input_info.bits_per_sample == 0) {
        s->input_info.bits_per_sample = AI_AUDIO_DEFAULT_BITS;
    }
    if (s->input_info.channel == 0) {
        s->input_info.channel = channel_count_in_format(input_format_or_default(s->mic_layout));
        if (s->input_info.channel == 0) {
            s->input_info.channel = 1;
        }
    }
}

static bool setting_same(const ai_audio_setting_t *a, const ai_audio_setting_t *b)
{
    if (a == NULL || b == NULL || a->type != b->type) {
        return false;
    }
    switch (a->type) {
        case AI_AUDIO_SETTING_ALC_GAIN:
            return a->data.alc_gain.channel == b->data.alc_gain.channel;
        default:
            return true;
    }
}

static esp_err_t cache_setting(ai_audio_src_t *s, const ai_audio_setting_t *setting)
{
    if (s == NULL || setting == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    for (ai_audio_setting_t *cur = s->settings; cur != NULL; cur = cur->next) {
        if (setting_same(cur, setting)) {
            ai_audio_setting_t *next = cur->next;
            *cur = *setting;
            cur->next = next;
            return ESP_OK;
        }
    }
    ai_audio_setting_t *node = calloc(1, sizeof(*node));
    if (node == NULL) {
        return ESP_ERR_NO_MEM;
    }
    *node = *setting;
    node->next = s->settings;
    s->settings = node;
    return ESP_OK;
}

static esp_err_t apply_setting(ai_audio_src_t *s, const ai_audio_setting_t *setting)
{
    if (s == NULL || setting == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (setting->type) {
        case AI_AUDIO_SETTING_ALC_GAIN: {
            esp_gmf_element_handle_t el = NULL;
            esp_gmf_pipeline_get_el_by_name(s->pipeline, "aud_alc", &el);
            if (el == NULL ||
                esp_gmf_audio_param_set_alc_channel_gain(el, setting->data.alc_gain.channel,
                                                         setting->data.alc_gain.gain) != ESP_GMF_ERR_OK) {
                ESP_LOGE(TAG, "apply ALC gain ch:%d failed", setting->data.alc_gain.channel);
                return ESP_FAIL;
            }
            return ESP_OK;
        }
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t apply_settings(ai_audio_src_t *s)
{
    if (s == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    for (const ai_audio_setting_t *cur = s->settings; cur != NULL; cur = cur->next) {
        esp_err_t ret = apply_setting(s, cur);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    return ESP_OK;
}

static void clear_settings(ai_audio_src_t *s)
{
    if (s == NULL) {
        return;
    }
    ai_audio_setting_t *cur = s->settings;
    while (cur != NULL) {
        ai_audio_setting_t *next = cur->next;
        free(cur);
        cur = next;
    }
    s->settings = NULL;
}

static esp_capture_err_t a_start(esp_capture_audio_src_if_t *b)
{
    ai_audio_src_t *s = (ai_audio_src_t *)b;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (s->started) {
        return ESP_CAPTURE_ERR_OK;
    }
    /* Capture negotiates before start; defaults cover direct/unit use. */
    ensure_input_info_defaults(s);
    if (ensure_pool_and_elements(s) != ESP_OK) {
        return ESP_CAPTURE_ERR_INTERNAL;
    }
     /* Build + run pipeline. */
     esp_err_t ret = build_pipeline(s);
     if (ret != ESP_OK) {
         return ESP_CAPTURE_ERR_INTERNAL;
     }

    if (s->record_handle) {
        esp_codec_dev_sample_info_t di = {.sample_rate = s->input_info.sample_rate,
                                          .bits_per_sample = s->input_info.bits_per_sample,
                                          .channel = s->input_info.channel};
        if (esp_codec_dev_open(s->record_handle, &di) != ESP_OK) {
            return ESP_CAPTURE_ERR_INTERNAL;
        }
    }

    if (apply_settings(s) != ESP_OK) {
        a_stop(b);
        return ESP_CAPTURE_ERR_INTERNAL;
    }
    if (s->pipeline) {
        s->out_rb = xRingbufferCreate(AI_AUDIO_RB_SIZE, RINGBUF_TYPE_BYTEBUF);
        if (s->out_rb == NULL) {
            a_stop(b);
            return ESP_CAPTURE_ERR_NO_MEM;
        }
        if (esp_gmf_pipeline_run(s->pipeline) != ESP_GMF_ERR_OK) {
            a_stop(b);
            return ESP_CAPTURE_ERR_INTERNAL;
        }
    }

    s->started = true;
    s->frames = 0;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t a_read(esp_capture_audio_src_if_t *b, esp_capture_stream_frame_t *f)
{
    ai_audio_src_t *s = (ai_audio_src_t *)b;
    if (!s || !f || !s->started) {
        return ESP_CAPTURE_ERR_INTERNAL;
    }
    if (s->out_rb) {
        size_t item_size = 0;
        uint8_t *item = (uint8_t *)xRingbufferReceiveUpTo(s->out_rb, &item_size, pdMS_TO_TICKS(500), f->size);
        if (item == NULL || item_size == 0) {
            return ESP_CAPTURE_ERR_TIMEOUT;
        }
        memcpy(f->data, item, item_size);
        vRingbufferReturnItem(s->out_rb, item);
        f->size = item_size;
        f->stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO;
        f->pts = (uint32_t)(s->frames * 20000);
        s->frames++;
        return ESP_CAPTURE_ERR_OK;
    }
    /* Fallback: direct read for pass-through tests or no-feature source. */
    if (s->read_cb || s->record_handle) {
        int r = read_input(s, f->data, f->size);
        if (r <= 0) {
            return ESP_CAPTURE_ERR_TIMEOUT;
        }
        f->size = r;
        f->stream_type = ESP_CAPTURE_STREAM_TYPE_AUDIO;
        f->pts = (uint32_t)(s->frames * 20000);
        s->frames++;
        return ESP_CAPTURE_ERR_OK;
    }
    return ESP_CAPTURE_ERR_INTERNAL;
}

static esp_capture_err_t a_stop(esp_capture_audio_src_if_t *b)
{
    ai_audio_src_t *s = (ai_audio_src_t *)b;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    /* Suspend AFE before pipeline stop so feed/fetch are not mid-read when
       the AFE element clears its read callback. */
    if (s->afe_manager != NULL) {
        (void)esp_gmf_afe_manager_suspend((esp_gmf_afe_manager_handle_t)s->afe_manager, true);
    }
    if (s->pipeline) {
        esp_gmf_pipeline_stop(s->pipeline);
        esp_gmf_pipeline_destroy(s->pipeline);
        s->pipeline = NULL;
    }
    if (s->task) {
        esp_gmf_task_deinit(s->task);
        s->task = NULL;
    }
    if (s->out_rb) {
        vRingbufferDelete(s->out_rb);
        s->out_rb = NULL;
    }
    if (s->record_handle) {
        esp_codec_dev_close(s->record_handle);
    }
    s->started = false;
    return ESP_CAPTURE_ERR_OK;
}

static esp_capture_err_t a_close(esp_capture_audio_src_if_t *b)
{
    ai_audio_src_t *s = (ai_audio_src_t *)b;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    a_stop(b);
    clear_internal_runtime(s);
    clear_settings(s);
    if (s->read_buf) {
        esp_gmf_oal_free(s->read_buf);
        s->read_buf = NULL;
        s->read_buf_size = 0;
    }
    if (s->out_buf) {
        esp_gmf_oal_free(s->out_buf);
        s->out_buf = NULL;
        s->out_buf_size = 0;
    }
    if (s->dump_file != NULL) {
        fclose(s->dump_file);
        s->dump_file = NULL;
        ESP_LOGI(TAG, "Closed AI source dump: %s", s->dump_path);
    }
    s->opened = false;
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_err_t esp_ai_audio_set_vad_cb(esp_capture_audio_src_if_t *src, esp_capture_service_ai_audio_src_vad_cb_t cb, void *ctx)
{
    ai_audio_src_t *s = (ai_audio_src_t *)src;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (s->started) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    s->vad_cb = cb;
    s->vad_ctx = ctx;
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_err_t esp_ai_audio_set_wn_cb(esp_capture_audio_src_if_t *src, esp_capture_service_ai_audio_src_wn_cb_t cb, void *ctx)
{
    ai_audio_src_t *s = (ai_audio_src_t *)src;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (s->started) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    s->wn_cb = cb;
    s->wn_ctx = ctx;
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_err_t esp_ai_audio_set_doa_cb(esp_capture_audio_src_if_t *src, esp_capture_service_ai_audio_src_doa_cb_t cb, void *ctx)
{
    ai_audio_src_t *s = (ai_audio_src_t *)src;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (s->started) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    s->doa_cb = cb;
    s->doa_ctx = ctx;
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_err_t esp_ai_audio_set_read_cb(esp_capture_audio_src_if_t *src, esp_capture_service_ai_audio_src_read_cb_t cb, void *ctx)
{
    ai_audio_src_t *s = (ai_audio_src_t *)src;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (s->started) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    s->read_cb = cb;
    s->read_ctx = ctx;
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_err_t esp_ai_audio_enable_dump(esp_capture_audio_src_if_t *src, const char *dir)
{
    ai_audio_src_t *s = (ai_audio_src_t *)src;
    if (!s || !dir || dir[0] == '\0') {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (s->opened || s->started) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    size_t dir_len = strlen(dir);
    bool has_separator = dir[dir_len - 1] == '/';
    int written = snprintf(s->dump_path, sizeof(s->dump_path), "%s%s%s",
                           dir, has_separator ? "" : "/", AI_AUDIO_DUMP_FILE_NAME);
    if (written < 0 || written >= (int)sizeof(s->dump_path)) {
        s->dump_path[0] = '\0';
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_err_t esp_ai_audio_set_alc_gain(esp_capture_audio_src_if_t *src, int channel, float gain)
{
    ai_audio_src_t *s = (ai_audio_src_t *)src;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (s->started) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    ai_audio_setting_t setting = {
        .type = AI_AUDIO_SETTING_ALC_GAIN,
        .data.alc_gain = {
            .channel = channel,
            .gain = gain,
        },
    };
    esp_err_t ret = cache_setting(s, &setting);
    if (ret == ESP_ERR_NO_MEM) {
        return ESP_CAPTURE_ERR_NO_MEM;
    }
    return (ret == ESP_OK) ? ESP_CAPTURE_ERR_OK : ESP_CAPTURE_ERR_INTERNAL;
}

esp_capture_err_t esp_ai_audio_set_feature(esp_capture_audio_src_if_t *src,
                                           esp_ai_audio_feature_t features,
                                           const esp_capture_service_ai_audio_src_feature_cfg_t *cfg)
{
    ai_audio_src_t *s = (ai_audio_src_t *)src;
    if (!s) {
        return ESP_CAPTURE_ERR_INVALID_ARG;
    }
    if (s->started || s->pipeline) {
        return ESP_CAPTURE_ERR_INVALID_STATE;
    }
    esp_capture_service_ai_audio_src_feature_cfg_t runtime_cfg = {0};
    if (cfg != NULL) {
        runtime_cfg = *cfg;
    }
    const char *model_partition = runtime_cfg.afe.model_partition ? runtime_cfg.afe.model_partition : "model";
    void *models = runtime_cfg.afe.models;
    void *afe_manager = runtime_cfg.afe.afe_manager;
    void *pool = runtime_cfg.afe.pool;

    clear_internal_runtime(s);
    s->features = features;
    s->runtime_cfg = runtime_cfg;
    s->model_partition = model_partition;
    s->models = models;
    s->afe_manager = afe_manager;
    /* Keep create-time / external pool when afe.pool is not provided. */
    if (pool != NULL) {
        s->pool = pool;
    }
    apply_runtime_defaults(s);
    if (s->input_info.channel == 0) {
        s->input_info.channel = channel_count_in_format(input_format_or_default(s->mic_layout));
    }
    s->input_info.format_id = ESP_CAPTURE_FMT_ID_PCM;
    if (validate_features(s) != ESP_OK) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    if (f_has(s->features, ESP_AI_AUDIO_FEATURE_DOA) &&
        validate_doa_layout(input_format_or_default(s->mic_layout)) != ESP_CAPTURE_ERR_OK) {
        return ESP_CAPTURE_ERR_NOT_SUPPORTED;
    }
    return ESP_CAPTURE_ERR_OK;
}

esp_capture_audio_src_if_t *esp_ai_audio_new_src(esp_ai_audio_src_cfg_t *cfg)
{
    if (!cfg) {
        return NULL;
    }
    ai_audio_src_t *s = calloc(1, sizeof(*s));
    if (!s) {
        return NULL;
    }

    s->base.open = a_open;
    s->base.get_support_codecs = a_codecs;
    s->base.set_fixed_caps = a_fixcaps;
    s->base.negotiate_caps = a_negotiate;
    s->base.start = a_start;
    s->base.read_frame = a_read;
    s->base.stop = a_stop;
    s->base.close = a_close;

    s->record_handle = cfg->record_handle;
    s->mic_layout = cfg->mic_layout;
    s->service_name = cfg->service_name ? cfg->service_name : ESP_AUDIO_CAPTURE_SERVICE_NAME;
    s->model_partition = "model";
    s->pool = cfg->pool;

    return &s->base;
}
