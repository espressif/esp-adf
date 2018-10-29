/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

#include "bt_keycontrol.h"
#include "bluetooth_service.h"

static const char *TAG = "BLUETOOTH_SERVICE";

#define VALIDATE_BT(periph, ret) if (!(periph && esp_periph_get_id(periph) == PERIPH_ID_BLUETOOTH)) { \
    ESP_LOGE(TAG, "Invalid Bluetooth periph, at line %d", __LINE__);\
    return ret;\
}

typedef struct bluetooth_service {
    audio_element_handle_t stream;
    esp_periph_handle_t periph;
    audio_element_type_t stream_type;
    esp_bd_addr_t remote_bda;
    esp_a2d_connection_state_t connection_state;
    esp_a2d_audio_state_t audio_state;
    uint64_t pos;
    uint8_t tl;
    bool avrc_connected;
} bluetooth_service_t;

bluetooth_service_t *g_bt_service = NULL;

static const char *conn_state_str[] = { "Disconnected", "Connecting", "Connected", "Disconnecting" };
static const char *audio_state_str[] = { "Suspended", "Stopped", "Started" };

static void bt_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *p_param)
{
    esp_a2d_cb_param_t *a2d = NULL;
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            a2d = (esp_a2d_cb_param_t *)(p_param);
            uint8_t *bda = a2d->conn_stat.remote_bda;
            ESP_LOGD(TAG, "A2DP connection state: %s, [%02x:%02x:%02x:%02x:%02x:%02x]",
                     conn_state_str[a2d->conn_stat.state], bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

            if (g_bt_service->connection_state == ESP_A2D_CONNECTION_STATE_DISCONNECTED
                    && a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                memcpy(&g_bt_service->remote_bda, &a2d->conn_stat.remote_bda, sizeof(esp_bd_addr_t));
                g_bt_service->connection_state = a2d->conn_stat.state;
                ESP_LOGD(TAG, "A2DP connection state = CONNECTED");
                if (g_bt_service->periph) {
                    esp_periph_send_event(g_bt_service->periph, PERIPH_BLUETOOTH_CONNECTED, NULL, 0);
                }

            }
            if (memcmp(&g_bt_service->remote_bda, &a2d->conn_stat.remote_bda, sizeof(esp_bd_addr_t)) == 0
                    && g_bt_service->connection_state == ESP_A2D_CONNECTION_STATE_CONNECTED
                    && a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                memset(&g_bt_service->remote_bda, 0, sizeof(esp_bd_addr_t));
                g_bt_service->connection_state = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
                ESP_LOGD(TAG, "A2DP connection state =  DISCONNECTED");
                if (g_bt_service->stream) {
                    audio_element_report_status(g_bt_service->stream, AEL_STATUS_INPUT_DONE);
                }
                if (g_bt_service->periph) {
                    esp_periph_send_event(g_bt_service->periph, PERIPH_BLUETOOTH_DISCONNECTED, NULL, 0);
                }
            }

            break;
        case ESP_A2D_AUDIO_STATE_EVT:
            a2d = (esp_a2d_cb_param_t *)(p_param);
            ESP_LOGD(TAG, "A2DP audio state: %s", audio_state_str[a2d->audio_stat.state]);
            g_bt_service->audio_state = a2d->audio_stat.state;
            if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
                g_bt_service->pos = 0;
            }
            if (g_bt_service->periph == NULL) {
                break;
            }

            if (ESP_A2D_AUDIO_STATE_STARTED == a2d->audio_stat.state) {
                esp_periph_send_event(g_bt_service->periph, PERIPH_BLUETOOTH_AUDIO_STARTED, NULL, 0);
            } else if (ESP_A2D_AUDIO_STATE_REMOTE_SUSPEND == a2d->audio_stat.state) {
                esp_periph_send_event(g_bt_service->periph, PERIPH_BLUETOOTH_AUDIO_SUSPENDED, NULL, 0);
            } else if (ESP_A2D_AUDIO_STATE_STOPPED == a2d->audio_stat.state) {
                esp_periph_send_event(g_bt_service->periph, PERIPH_BLUETOOTH_AUDIO_STOPPED, NULL, 0);
            }

            break;
        case ESP_A2D_AUDIO_CFG_EVT:
            a2d = (esp_a2d_cb_param_t *)(p_param);
            ESP_LOGD(TAG, "A2DP audio stream configuration, codec type %d", a2d->audio_cfg.mcc.type);
            if (a2d->audio_cfg.mcc.type == ESP_A2D_MCT_SBC) {
                int sample_rate = 16000;
                char oct0 = a2d->audio_cfg.mcc.cie.sbc[0];
                if (oct0 & (0x01 << 6)) {
                    sample_rate = 32000;
                } else if (oct0 & (0x01 << 5)) {
                    sample_rate = 44100;
                } else if (oct0 & (0x01 << 4)) {
                    sample_rate = 48000;
                }
                ESP_LOGD(TAG, "Bluetooth configured, sample rate=%d", sample_rate);
                if (g_bt_service->stream == NULL) {
                    break;
                }
                audio_element_info_t bt_info = {0};
                audio_element_getinfo(g_bt_service->stream, &bt_info);
                bt_info.sample_rates = sample_rate;
                bt_info.channels = 2;
                bt_info.bits = 16;
                audio_element_setinfo(g_bt_service->stream, &bt_info);
                audio_element_report_info(g_bt_service->stream);
            }
            break;
        default:
            ESP_LOGD(TAG, "Unhandled A2DP event: %d", event);
            break;
    }
}

static void bt_a2d_data_cb(const uint8_t *data, uint32_t len)
{
    if (g_bt_service->stream) {
        if (audio_element_get_state(g_bt_service->stream) == AEL_STATE_RUNNING) {
            audio_element_output(g_bt_service->stream, (char *)data, len);
        }
    }
}

esp_err_t bluetooth_service_start(bluetooth_service_cfg_t *config)
{
    if (g_bt_service) {
        ESP_LOGE(TAG, "Bluetooth service have been initialized");
        return ESP_FAIL;
    }
    if (config->mode == BLUETOOTH_A2DP_SOUCE) {
        AUDIO_ERROR(TAG, "This working mode is not supported yet");
        return ESP_FAIL;
    }
    g_bt_service = calloc(1, sizeof(bluetooth_service_t));
    AUDIO_MEM_CHECK(TAG, g_bt_service, return ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        AUDIO_ERROR(TAG, "initialize controller failed");
        return ESP_FAIL;
    }

    if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT) != ESP_OK) {
        AUDIO_ERROR(TAG, "enable controller failed");
        return ESP_FAIL;
    }

    if (esp_bluedroid_init() != ESP_OK) {
        AUDIO_ERROR(TAG, "initialize bluedroid failed");
        return ESP_FAIL;
    }

    if (esp_bluedroid_enable() != ESP_OK) {
        AUDIO_ERROR(TAG, "enable bluedroid failed");
        return ESP_FAIL;
    }


    if (config->device_name) {
        esp_bt_dev_set_device_name(config->device_name);
    } else {
        esp_bt_dev_set_device_name("ESP32-ADF-SPEAKER");
    }


    if (config->mode == BLUETOOTH_A2DP_SINK) {
        esp_a2d_sink_init();
        g_bt_service->stream_type = AUDIO_STREAM_READER;
    } else {
        g_bt_service->stream_type = AUDIO_STREAM_WRITER;
    }

    /* initialize AVRCP controller */
    esp_a2d_register_callback(bt_a2d_cb);
    esp_a2d_sink_register_data_callback(bt_a2d_data_cb);
    // TODO: Use this function for IDF version higher 3.0
    // esp_a2d_sink_register_data_callback(bt_a2d_data_cb);
    /* set discoverable and connectable mode, wait to be connected */
    esp_bt_gap_set_scan_mode(ESP_BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE);

    return ESP_OK;
}

esp_err_t bluetooth_service_destroy()
{
    if (g_bt_service &&
            (g_bt_service->stream || g_bt_service->periph)) {

        AUDIO_ERROR(TAG, "Stream and periph need to stop first");
        return ESP_FAIL;
    }
    if (g_bt_service) {
        esp_a2d_sink_deinit();
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        esp_bt_controller_deinit();
        esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        free(g_bt_service);
        g_bt_service = NULL;
    }
    return ESP_OK;
}

static esp_err_t _bt_stream_destroy(audio_element_handle_t self)
{
    g_bt_service->stream = NULL;
    return ESP_OK;
}

audio_element_handle_t bluetooth_service_create_stream()
{
    if (g_bt_service && g_bt_service->stream) {
        ESP_LOGE(TAG, "Bluetooth stream have been created");
        return NULL;
    }

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.task_stack = -1; // No need task
    cfg.destroy = _bt_stream_destroy;
    cfg.tag = "bt";
    g_bt_service->stream = audio_element_init(&cfg);

    AUDIO_MEM_CHECK(TAG, g_bt_service->stream, return NULL);

    audio_element_setdata(g_bt_service->stream, g_bt_service);

    return g_bt_service->stream;
}

static void bt_avrc_ct_cb(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *p_param)
{
    esp_avrc_ct_cb_param_t *rc = p_param;
    switch (event) {
        case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
                uint8_t *bda = rc->conn_stat.remote_bda;
                g_bt_service->avrc_connected = rc->conn_stat.connected;
                if (rc->conn_stat.connected) {
                    ESP_LOGD(TAG, "ESP_AVRC_CT_CONNECTION_STATE_EVT");
                    bt_key_act_sm_init();
                } else if (0 == rc->conn_stat.connected){
                    bt_key_act_sm_deinit();
                }

                ESP_LOGD(TAG, "AVRC conn_state evt: state %d, [%02x:%02x:%02x:%02x:%02x:%02x]",
                                         rc->conn_stat.connected, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
                break;
            }
        case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT: {
                if(g_bt_service->avrc_connected) {
                    ESP_LOGD(TAG, "AVRC passthrough rsp: key_code 0x%x, key_state %d", rc->psth_rsp.key_code, rc->psth_rsp.key_state);
                    bt_key_act_param_t param;
                    memset(&param, 0, sizeof(bt_key_act_param_t));
                    param.evt = event;
                    param.tl = rc->psth_rsp.tl;
                    param.key_code = rc->psth_rsp.key_code;
                    param.key_state = rc->psth_rsp.key_state;
                    bt_key_act_state_machine(&param);
                }
                break;
            }
        case ESP_AVRC_CT_METADATA_RSP_EVT: {
                ESP_LOGD(TAG, "AVRC metadata rsp: attribute id 0x%x, %s", rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
                // free(rc->meta_rsp.attr_text);
                break;
            }
        case ESP_AVRC_CT_CHANGE_NOTIFY_EVT: {
                ESP_LOGD(TAG, "AVRC event notification: %d, param: %d", rc->change_ntf.event_id, rc->change_ntf.event_parameter);
                break;
            }
        case ESP_AVRC_CT_REMOTE_FEATURES_EVT: {
                ESP_LOGD(TAG, "AVRC remote features %x", rc->rmt_feats.feat_mask);
                break;
            }
        default:
            ESP_LOGE(TAG, "%s unhandled evt %d", __func__, event);
            break;
    }
}

static esp_err_t _bt_periph_init(esp_periph_handle_t periph)
{
    esp_avrc_ct_init();
    esp_avrc_ct_register_callback(bt_avrc_ct_cb);
    return ESP_OK;
}

static esp_err_t _bt_periph_run(esp_periph_handle_t self, audio_event_iface_msg_t *msg)
{
    return ESP_OK;
}

static esp_err_t _bt_periph_destroy(esp_periph_handle_t periph)
{
    esp_avrc_ct_deinit();
    g_bt_service->periph = NULL;
    return ESP_OK;
}


esp_periph_handle_t bluetooth_service_create_periph()
{
    if (g_bt_service && g_bt_service->periph) {
        ESP_LOGE(TAG, "Bluetooth periph have been created");
        return NULL;
    }

    g_bt_service->periph = esp_periph_create(PERIPH_ID_BLUETOOTH, "periph_bt");
    esp_periph_set_function(g_bt_service->periph, _bt_periph_init, _bt_periph_run, _bt_periph_destroy);
    return g_bt_service->periph;
}

static esp_err_t periph_bluetooth_passthrough_cmd(esp_periph_handle_t periph, uint8_t cmd)
{
    VALIDATE_BT(periph, ESP_FAIL);
    if (g_bt_service->audio_state != ESP_A2D_AUDIO_STATE_STARTED) {
        //return ESP_FAIL;
    }
    esp_err_t err = ESP_OK;

    if (g_bt_service->avrc_connected) {
        bt_key_act_param_t param;
        memset(&param, 0, sizeof(bt_key_act_param_t));
        param.evt = ESP_AVRC_CT_KEY_STATE_CHG_EVT;
        param.key_code = cmd;
        param.key_state = 0;
        param.tl = (g_bt_service->tl) & 0x0F;
        g_bt_service->tl = (g_bt_service->tl + 2) & 0x0f;
        bt_key_act_state_machine(&param);
    }

    return err;
}

esp_err_t periph_bluetooth_play(esp_periph_handle_t periph)
{
    return periph_bluetooth_passthrough_cmd(periph, ESP_AVRC_PT_CMD_PLAY);
}

esp_err_t periph_bluetooth_pause(esp_periph_handle_t periph)
{
    return periph_bluetooth_passthrough_cmd(periph, ESP_AVRC_PT_CMD_PAUSE);
}

esp_err_t periph_bluetooth_stop(esp_periph_handle_t periph)
{
    return periph_bluetooth_passthrough_cmd(periph, ESP_AVRC_PT_CMD_STOP);
}

esp_err_t periph_bluetooth_next(esp_periph_handle_t periph)
{
    return periph_bluetooth_passthrough_cmd(periph, ESP_AVRC_PT_CMD_FORWARD);
}

esp_err_t periph_bluetooth_prev(esp_periph_handle_t periph)
{
    return periph_bluetooth_passthrough_cmd(periph, ESP_AVRC_PT_CMD_BACKWARD);
}

esp_err_t periph_bluetooth_rewind(esp_periph_handle_t periph)
{
    return periph_bluetooth_passthrough_cmd(periph, ESP_AVRC_PT_CMD_REWIND);
}

esp_err_t periph_bluetooth_fast_forward(esp_periph_handle_t periph)
{
    return periph_bluetooth_passthrough_cmd(periph, ESP_AVRC_PT_CMD_FAST_FORWARD);
}
#endif
