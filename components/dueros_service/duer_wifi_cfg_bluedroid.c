/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "lightduer_lib.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"

#include "audio_mem.h"
#include "duer_wifi_cfg.h"
#include "duer_wifi_cfg_if.h"

#ifdef CONFIG_BT_BLE_ENABLED
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"

enum {
    BLE_IDX_SVC,
    BLE_IDX_TX_CHAR,
    BLE_IDX_TX_VAL,
    BLE_IDX_TX_NOTIFY_CFG,
    BLE_IDX_RX_CHAR,
    BLE_IDX_RX_VAL,
    BLE_IDX_RX_NOTIFY_CFG,
    BLE_IDX_NB,
};

enum {
    DUER_BLE_MSG_DATA,
    DUER_BLE_MSG_CONNECT,
    DUER_BLE_MSG_DISCONNECT,
    DUER_BLE_MSG_EXIT
};

typedef struct {
    uint8_t msg_id;
    uint8_t *data;
    uint16_t data_len;
    uint16_t handle;
} duer_ble_msg_t;

#define BLE_WIFI_CFG_GATTC_MTU_SIZE    (23)
#define PREPARE_BUF_MAX_SIZE           (1024)
#define BLE_WIFI_CFG_SERVICE_UUID      (0x1111)
#define BLE_WIFI_CFG_PRIMARY_CHAR_UUID (0x2222)
#define BLE_WIFI_CFG_SECOND_CHAR_UUID  (0x3333)
#define ADV_CONFIG_FLAG                (1 << 0)
#define SCAN_RSP_CONFIG_FLAG           (1 << 1)
#define ESP_BLE_WIFI_CFG_APP_ID        (0x3344)
#define MTU_RESERVED_SIZE              (3)

static const char *TAG = "DUER_WIFI_CFG_BLE";

static esp_gatt_if_t s_sg_gatts_if  = 0xff;
static uint16_t s_sg_conn_id        = 0xffff;
static uint16_t s_service_handle    = 0xffff;
static uint8_t s_adv_config_done    = 0;
static bool s_blecfg_alive          = 0;
static uint16_t s_prepare_handle    = 0;
static uint8_t *s_prepare_buf       = NULL;
static int s_prepare_buf_len        = 0;
static int s_ble_mtu                = BLE_WIFI_CFG_GATTC_MTU_SIZE;
static xQueueHandle s_msg_queue     = 0;
static EventGroupHandle_t s_send_done = 0;

static uint8_t MANUFACTURER_DATA[4] = { 0x1C, 0x01, 0x01, 0x11 };

static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static uint8_t ble_wifi_cfg_service_uuid128[] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00
};

static esp_ble_adv_data_t s_adv_data = {
    .set_scan_rsp        = 0,
    .include_name        = 0,
    .include_txpower     = 1,
    .min_interval        = 0x20,
    .max_interval        = 0x40,
    .appearance          = 0x00,
    .manufacturer_len    = sizeof(MANUFACTURER_DATA),
    .p_manufacturer_data = MANUFACTURER_DATA,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = sizeof(ble_wifi_cfg_service_uuid128),
    .p_service_uuid      = ble_wifi_cfg_service_uuid128,
    .flag                = (ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t s_scan_rsp_data = {
    .set_scan_rsp        = 1,
    .include_name        = 1,
    .include_txpower     = 1,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .flag                = (ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static const uint8_t s_char_prop = (ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE
        | ESP_GATT_CHAR_PROP_BIT_WRITE_NR | ESP_GATT_CHAR_PROP_BIT_INDICATE | ESP_GATT_CHAR_PROP_BIT_NOTIFY);

static const uint16_t s_primary_service_uuid    = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t s_service_uuid            = BLE_WIFI_CFG_SERVICE_UUID;
static const uint16_t s_primary_char_uuid       = BLE_WIFI_CFG_PRIMARY_CHAR_UUID;
static const uint16_t s_second_char_uuid        = BLE_WIFI_CFG_SECOND_CHAR_UUID;
static const uint16_t s_desc_uuid               = ESP_GATT_UUID_CHAR_DECLARE; // ESP_GATT_UUID_CHAR_CLIENT_CONFIG
static const uint16_t s_client_config_uuid      = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;

static duer_ble_wifi_cfg_callbacks_t s_callbacks;

static uint16_t s_primary_char_handle;
static uint16_t s_second_char_handle;

/* Full Database Description - Used to add attributes into the database */
static const esp_gatts_attr_db_t s_ble_gatt_db[BLE_IDX_NB] =
{
    // Service Declaration
    [BLE_IDX_SVC]            =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&s_primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(s_service_uuid), sizeof(s_service_uuid), (uint8_t *)&s_service_uuid}},

    /* Characteristic Declaration */
    [BLE_IDX_TX_CHAR]        =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&s_desc_uuid, ESP_GATT_PERM_READ,
      sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&s_char_prop}},

    /* Characteristic Value */
    [BLE_IDX_TX_VAL]         =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&s_primary_char_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      0, 0, NULL}},

    /* Characteristic Configuration Descriptor */
    [BLE_IDX_TX_NOTIFY_CFG]  =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&s_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      0, 0, NULL}},

    /* Characteristic Declaration */
    [BLE_IDX_RX_CHAR]        =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&s_desc_uuid, ESP_GATT_PERM_READ,
      sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&s_char_prop}},

    /* Characteristic Value */
    [BLE_IDX_RX_VAL]         =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&s_second_char_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      0, 0, NULL}},

    /* Characteristic Configuration Descriptor */
    [BLE_IDX_RX_NOTIFY_CFG]  =
    {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&s_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
      0, 0, NULL}}
};

extern char *duer_get_device_name();

static void duer_ble_data_handle_task(void *pdata)
{
    duer_ble_msg_t msg;

    while (1) {
        if (pdTRUE == xQueueReceive(s_msg_queue, &msg, portMAX_DELAY)) {
            ESP_LOGD(TAG, "msg.msg_id = %d, msg.data_len = %d", msg.msg_id, msg.data_len);
            if (msg.msg_id == DUER_BLE_MSG_DATA) {
                if (msg.data) {
                    if (s_callbacks.on_ble_recv_data) {
                        s_callbacks.on_ble_recv_data(msg.data, msg.data_len, msg.handle);
                    }
                    audio_free(msg.data);
                }
            } else if (msg.msg_id == DUER_BLE_MSG_CONNECT) {
                if (s_callbacks.on_ble_connect_status) {
                    s_callbacks.on_ble_connect_status(true);
                }
            } else if (msg.msg_id == DUER_BLE_MSG_DISCONNECT) {
                if (s_callbacks.on_ble_connect_status) {
                    s_callbacks.on_ble_connect_status(false);
                }
            } else if (msg.msg_id == DUER_BLE_MSG_EXIT) {
                break;
            } else {
                ESP_LOGI(TAG, "unkown msg");
            }
        }
    }

    vQueueDelete(s_msg_queue);
    s_msg_queue = NULL;

    vTaskDelete(NULL);
}

static int duer_ble_data_post(uint8_t *data, uint16_t data_len, uint16_t handle)
{
    duer_ble_msg_t msg;

    if (s_msg_queue == NULL || data == NULL || data_len == 0) {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.data = audio_calloc(1, data_len);
    if (msg.data == NULL) {
        return -1;
    }
    memcpy(msg.data, data, data_len);

    msg.msg_id = DUER_BLE_MSG_DATA;
    msg.data_len = data_len;
    msg.handle = handle;
    ESP_LOGD(TAG, "duer_ble_data_post %d", handle);
    if (xQueueSend(s_msg_queue, &msg, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Post failed");
        return -1;
    }

    return 0;
}

static int duer_ble_data_task_init(void)
{
    int ret = 0;

    if (s_msg_queue != NULL) {
        return -1;
    }

    s_msg_queue = xQueueCreate(10, sizeof(duer_ble_msg_t));
    if (s_msg_queue == NULL) {
        ESP_LOGE(TAG, "ble msg queue create error.");
        return -1;
    }

    ret = xTaskCreate(duer_ble_data_handle_task, "duer_ble_data", 1024 * 5, NULL, 5, NULL);
    if (ret != 1) {
        ESP_LOGI(TAG, "xTaskCreate, ret %d", ret);
        vQueueDelete(s_msg_queue);
        s_msg_queue = NULL;
        return -1;
    }

    return 0;
}

static int duer_send_empty_event(int msg_id)
{
    if (s_msg_queue) {
        duer_ble_msg_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_id = msg_id;
        if (xQueueSend(s_msg_queue, &msg, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Post failed");
            return -1;
        }
    }

    return 0;
}

static int duer_ble_data_task_deinit(void)
{
    return duer_send_empty_event(DUER_BLE_MSG_EXIT);
}

static void duer_on_ble_connect(void)
{
    duer_send_empty_event(DUER_BLE_MSG_CONNECT);
    esp_ble_gap_stop_advertising();
}

static void duer_on_ble_disconnect(void)
{
    duer_send_empty_event(DUER_BLE_MSG_DISCONNECT);
}

static void gatts_ble_wifi_cfg_profile_event_handler(
    esp_gatts_cb_event_t event,
    esp_gatt_if_t gatts_if,
    esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            ESP_LOGI(TAG, "[%s] ESP_GATTS_REG_EVT: %d", __func__, gatts_if);
            esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name(duer_get_device_name());
            if (set_dev_name_ret) {
                ESP_LOGI(TAG, "[%s] set device name failed, error code = %x", __func__, set_dev_name_ret);
            }

            // config adv data
            esp_err_t ret = esp_ble_gap_config_adv_data(&s_adv_data);
            if (ret) {
                ESP_LOGI(TAG, "[%s] config adv data failed, error code = %x", __func__, ret);
            }
            s_adv_config_done |= ADV_CONFIG_FLAG;

            // config scan rssvonse data
            ret = esp_ble_gap_config_adv_data(&s_scan_rsp_data);
            if (ret) {
                ESP_LOGI(TAG, "[%s] config scan raw scan rsp data failed, error code = %x", __func__, ret);
            }
            s_adv_config_done |= SCAN_RSP_CONFIG_FLAG;

            esp_err_t create_attr_ret = esp_ble_gatts_create_attr_tab(s_ble_gatt_db, gatts_if, BLE_IDX_NB, 0);
            if (create_attr_ret) {
                ESP_LOGI(TAG, "[%s] create attr table failed, error code = %x", __func__, create_attr_ret);
            }
        } break;

        case ESP_GATTS_UNREG_EVT:
            duer_ble_data_task_deinit();
            s_sg_gatts_if = 0xff;
            break;
        case ESP_GATTS_CONNECT_EVT:
            ESP_LOGI(TAG, "[%s] ESP_GATTS_CONNECT_EVT: %d", __func__, gatts_if);
            s_sg_conn_id = param->connect.conn_id;
            duer_on_ble_connect();
            break;

        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            ESP_LOGI(TAG, "[%s] ESP_GATTS_CREAT_ATTR_TAB_EVT, status %d, handles 0x%x", __func__,
                param->add_attr_tab.status, param->add_attr_tab.handles[0]);

            if (param->add_attr_tab.status != ESP_GATT_OK) {
                ESP_LOGI(TAG, "[%s] create attribute table failed, error code=0x%x", __func__, param->add_attr_tab.status);
            } else if (param->add_attr_tab.num_handle != BLE_IDX_NB) {
                ESP_LOGI(TAG, "[%s] create attribute table abnormally, num_handle (%d) doesn't equal to BLE_IDX_NB(%d)", __func__, param->add_attr_tab.num_handle, BLE_IDX_NB);
            } else {
                s_primary_char_handle = param->add_attr_tab.handles[BLE_IDX_TX_VAL];
                s_second_char_handle = param->add_attr_tab.handles[BLE_IDX_RX_VAL];
                s_service_handle = param->add_attr_tab.handles[BLE_IDX_SVC];
                ESP_LOGW(TAG, "pri %d, sec %d, ser %d", s_primary_char_handle, s_second_char_handle, s_service_handle);
                esp_ble_gatts_start_service(s_service_handle);
            }
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "[%s] ESP_GATTS_DISCONNECT_EVT, reason = 0x%x", __func__, param->disconnect.reason);
            s_sg_conn_id = 0xffff;
            duer_on_ble_disconnect();
            break;
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(TAG, "[%s] ESP_GATTS_READ_EVT, conn_id %d, trans_id %d, handle %d", __func__,
                (int)param->read.conn_id, (int)param->read.trans_id, (int)param->read.handle);
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = param->read.handle;
            rsp.attr_value.len = 1;
            rsp.attr_value.value[0] = 0;
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            break;

        case ESP_GATTS_WRITE_EVT:
            ESP_LOGD(TAG, "ESP_GATTS_WRITE_EVT %d", param->write.is_prep);
            if (param->write.is_prep) {
                // the data length of gattc write  must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
                ESP_LOGI(TAG, "[%s] GATT_WRITE_EVT, handle = %d, value len = %d, value :", __func__, param->write.handle, param->write.len);
                esp_gatt_status_t status = ESP_GATT_OK;
                if (s_prepare_buf == NULL) {
                    s_prepare_buf = audio_calloc(1, PREPARE_BUF_MAX_SIZE);
                    if (s_prepare_buf == NULL) {
                        ESP_LOGI(TAG, "calloc s_prepare_buf failed");
                        status = ESP_GATT_NO_RESOURCES;
                    }
                } else {
                    if (param->write.offset > PREPARE_BUF_MAX_SIZE) {
                        status = ESP_GATT_INVALID_OFFSET;
                    } else if ((param->write.offset + param->write.len) > PREPARE_BUF_MAX_SIZE) {
                        status = ESP_GATT_INVALID_ATTR_LEN;
                    }
                }

                if (s_prepare_buf) {
                    memcpy(s_prepare_buf + param->write.offset, param->write.value, param->write.len);
                    s_prepare_buf_len += param->write.len;
                    s_prepare_handle = param->write.handle;
                }

                if (param->write.need_rsp) {
                    esp_gatt_rsp_t rsp;
                    memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
                    rsp.attr_value.handle = param->write.handle;
                    rsp.attr_value.len = param->write.len;
                    rsp.attr_value.offset = param->write.offset;
                    memcpy(rsp.attr_value.value, param->write.value, param->write.len);

                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, &rsp);
                }
                break;
            } else {
                ESP_LOGD(TAG, "normal write, len %d, offset %d", param->write.len, param->write.offset);
                duer_ble_data_post(param->write.value, param->write.len, param->write.handle);
                if (param->write.need_rsp) {
                    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id,
                        ESP_GATT_OK, NULL);
                }
            }
            break;
        case ESP_GATTS_EXEC_WRITE_EVT:
            // the length of gattc prepare write data must be less than GATTS_DEMO_CHAR_VAL_LEN_MAX.
            ESP_LOGW(TAG, "ESP_GATTS_EXEC_WRITE_EVT");
            esp_ble_gatts_send_response(gatts_if, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, NULL);
            duer_ble_data_post(s_prepare_buf, s_prepare_buf_len, s_prepare_handle);
            if (s_prepare_buf) {
                audio_free(s_prepare_buf);
                s_prepare_buf = NULL;
            }
            break;
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
            s_ble_mtu = param->mtu.mtu;
            break;
        case ESP_GATTS_CONF_EVT:
            xEventGroupSetBits(s_send_done, BIT0);
            break;
        case ESP_GATTS_START_EVT:
        case ESP_GATTS_STOP_EVT:
        case ESP_GATTS_OPEN_EVT:
        case ESP_GATTS_CANCEL_OPEN_EVT:
        case ESP_GATTS_CLOSE_EVT:
        case ESP_GATTS_LISTEN_EVT:
        case ESP_GATTS_CONGEST_EVT:
        case ESP_GATTS_DELETE_EVT:
        default:
            ESP_LOGI(TAG, "[%s] event %d", __func__, event);
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            if (param->reg.app_id != ESP_BLE_WIFI_CFG_APP_ID) {
                ESP_LOGI(TAG, "[%s] register, app_id %04x, it should be %04x", __func__,
                    param->reg.app_id, ESP_BLE_WIFI_CFG_APP_ID);
                return;
            }
            s_sg_gatts_if = gatts_if;
        } else {
            ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d",
                param->reg.app_id,
                param->reg.status);
            return;
        }
    }

    /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
    if (gatts_if == ESP_GATT_IF_NONE || gatts_if == s_sg_gatts_if) {
        gatts_ble_wifi_cfg_profile_event_handler(event, gatts_if, param);
    }
}

static void esp_ble_wifi_cfg_gap_event_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGD(TAG, "[%s] event %d", __func__, event);
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            s_adv_config_done &= (~ADV_CONFIG_FLAG);
            if (s_adv_config_done == 0) {
                ESP_LOGI(TAG, "esp_ble_gap_start_advertising");
                esp_ble_gap_start_advertising(&s_adv_params);
            }
            break;
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            s_adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (s_adv_config_done == 0) {
                ESP_LOGI(TAG, "esp_ble_gap_start_advertising");
                esp_ble_gap_start_advertising(&s_adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "[%s] event ESP_GAP_BLE_ADV_START_COMPLETE_EVT", __func__);
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "[%s] event ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT", __func__);
            break;
        default:
            ESP_LOGI(TAG, "[%s] event %d", __func__, event);
            break;
    }
}

int duer_ble_send_data(uint8_t *data, uint32_t data_len, uint16_t attr_id)
{
    int offset = 0;
    int send_len = 0;
    int max_data_len = 0;

    ESP_LOGD(TAG, "duer_ble_send_data %d", attr_id);

    if (attr_id == 0) {
        attr_id = s_second_char_handle;
    }

    max_data_len = s_ble_mtu - MTU_RESERVED_SIZE;

    while (offset < data_len) {
        send_len = (data_len - offset) > max_data_len ? max_data_len : (data_len - offset);
        DUER_DUMPD("data", data + offset, send_len);
        esp_ble_gatts_send_indicate(s_sg_gatts_if, s_sg_conn_id, attr_id, send_len, data + offset, true);
        offset += send_len;
        if (xEventGroupWaitBits(s_send_done, BIT0, true, true, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGI(TAG, "wait failed");
        }
    }

    return 0;
}

int duer_wifi_cfg_ble_host_init(duer_ble_wifi_cfg_callbacks_t *callbacks)
{
    if (s_blecfg_alive ||
        callbacks == NULL ||
        callbacks->on_ble_connect_status == NULL ||
        callbacks->on_ble_recv_data == NULL) {
        return -1;
    }
    s_blecfg_alive = true;
    memcpy(&s_callbacks, callbacks, sizeof(s_callbacks));

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_ble_wifi_cfg_gap_event_cb));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(ESP_BLE_WIFI_CFG_APP_ID));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(BLE_WIFI_CFG_GATTC_MTU_SIZE));

    ESP_ERROR_CHECK(duer_ble_data_task_init());

    if (!s_send_done) {
        s_send_done = xEventGroupCreate();
    }

    return 0;
}

int duer_wifi_cfg_ble_host_deinit(void)
{
    if (s_blecfg_alive) {
        if (s_sg_conn_id != 0xffff) {
            esp_ble_gap_disconnect(NULL);
            s_sg_conn_id = 0xffff;
        }
        esp_ble_gap_stop_advertising();

        if (s_service_handle != 0xffff) {
            esp_ble_gatts_stop_service(s_service_handle);
            esp_ble_gatts_delete_service(s_service_handle);
            s_sg_conn_id = 0xffff;
        }

        if (s_sg_gatts_if != 0xff) {
            esp_ble_gatts_app_unregister(s_sg_gatts_if);
            s_sg_gatts_if = 0xff;
        }
        duer_ble_data_task_deinit();
        s_blecfg_alive = false;
    }
    return 0;
}

#endif /* CONFIG_BT_BLE_ENABLED */