/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "esp_ota_service.h"
#include "esp_ota_service_source_ble.h"
#include "esp_ota_service_target_app.h"
#include "esp_ota_service_checker_app.h"

#include "esp_ota_service_ble_example.h"

#define EVT_OTA_DONE  (BIT0)
#define EVT_OTA_FAIL  (BIT1)
#define EVT_OTA_SKIP  (BIT2)

static const char *TAG = "OTA_BLE";

static EventGroupHandle_t s_ota_evt;

static void on_ota_adf_event(const adf_event_t *event, void *ctx);

esp_err_t esp_ota_service_ble_example_confirm_running_image(void)
{
    esp_err_t confirm_ret = esp_ota_service_confirm_update();
    if (confirm_ret == ESP_OK) {
        ESP_LOGI(TAG, "Confirm_running_image: image was PENDING_VERIFY - confirmed successfully");
        return ESP_OK;
    }
    if (confirm_ret == ESP_ERR_INVALID_STATE) {
        /* Image is not in PENDING_VERIFY state (e.g. already confirmed or a
         * factory image). This is not an error; nothing to confirm. */
        ESP_LOGI(TAG, "Confirm_running_image: image is not pending verify, nothing to do");
        return ESP_OK;
    }
    ESP_LOGW(TAG, "Confirm_running_image failed: esp_ota_service_confirm_update returned %s",
             esp_err_to_name(confirm_ret));
    return confirm_ret;
}

static void on_ota_adf_event(const adf_event_t *event, void *ctx)
{
    (void)ctx;
    if (event == NULL || event->payload == NULL || event->payload_len < sizeof(esp_ota_service_event_t)) {
        return;
    }
    const esp_ota_service_event_t *evt = (const esp_ota_service_event_t *)event->payload;
    switch (evt->id) {
        case ESP_OTA_SERVICE_EVT_SESSION_BEGIN:
            ESP_LOGI(TAG, "[OTA] Session started");
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK:
            if (evt->error == ESP_OK && !evt->ver_check.upgrade_available) {
                ESP_LOGW(TAG, "[OTA] Firmware is not newer - skipping");
                xEventGroupSetBits(s_ota_evt, EVT_OTA_SKIP);
            }
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_BEGIN:
            ESP_LOGI(TAG, "[OTA] Waiting for APP to send firmware ...");
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_PROGRESS:
            ESP_LOGI(TAG, "[OTA] Progress: %" PRIu32 " / %" PRIu32 " bytes",
                     evt->progress.bytes_written, evt->progress.total_bytes);
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_END:
            switch (evt->item_end.status) {
                case ESP_OTA_SERVICE_ITEM_STATUS_OK:
                    ESP_LOGI(TAG, "[OTA] APP OTA write succeeded");
                    break;
                case ESP_OTA_SERVICE_ITEM_STATUS_SKIPPED:
                    ESP_LOGW(TAG, "[OTA] Item skipped (reason=%d)", (int)evt->item_end.reason);
                    xEventGroupSetBits(s_ota_evt, EVT_OTA_SKIP);
                    break;
                case ESP_OTA_SERVICE_ITEM_STATUS_FAILED:
                    ESP_LOGE(TAG, "[OTA] Failed: %s (reason=%d)", esp_err_to_name(evt->error),
                             (int)evt->item_end.reason);
                    xEventGroupSetBits(s_ota_evt, EVT_OTA_FAIL);
                    break;
            }
            break;
        case ESP_OTA_SERVICE_EVT_SESSION_END:
            if (evt->session_end.aborted || evt->session_end.failed_count > 0) {
                xEventGroupSetBits(s_ota_evt, EVT_OTA_DONE | EVT_OTA_FAIL);
            } else {
                xEventGroupSetBits(s_ota_evt, EVT_OTA_DONE);
            }
            break;
        case ESP_OTA_SERVICE_EVT_MAX:
            break;
    }
}

esp_err_t esp_ota_service_ble_example_run_ota(void)
{
    esp_ota_service_cfg_t svc_cfg = ESP_OTA_SERVICE_CFG_DEFAULT();
    svc_cfg.stop_timeout_ms = 15000;

    s_ota_evt = xEventGroupCreate();
    if (!s_ota_evt) {
        ESP_LOGE(TAG, "Run_ota failed: event group create returned NULL (%s)",
                 esp_err_to_name(ESP_OTA_SERVICE_BLE_EX_ERR_NO_EVENT_GROUP));
        return ESP_OTA_SERVICE_BLE_EX_ERR_NO_EVENT_GROUP;
    }

    esp_ota_service_t *svc = NULL;
    esp_err_t ret = esp_ota_service_create(&svc_cfg, &svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: esp_ota_service_create returned %s", esp_err_to_name(ret));
        vEventGroupDelete(s_ota_evt);
        s_ota_evt = NULL;
        return ret;
    }

    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.handler = on_ota_adf_event;
    ret = esp_service_event_subscribe((esp_service_t *)svc, &sub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: esp_service_event_subscribe returned %s", esp_err_to_name(ret));
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_ota_evt);
        s_ota_evt = NULL;
        return ret;
    }

    esp_ota_service_source_ble_cfg_t ble_cfg = {
        .ring_buf_size = 0,
        /* Allow generous time for the central (e.g. C6 AT bridge or APP) to
         * scan, connect, exchange MTU, enable indications and finally send
         * CMD_START. The default 30s is too tight for the AT-bridge path. */
        .wait_start_timeout_ms = 180000,
    };

    esp_ota_service_source_t *src = NULL;
    esp_ota_service_target_t *tgt = NULL;
    esp_ota_service_checker_t *chk = NULL;
    esp_err_t ocr = esp_ota_service_source_ble_create(&ble_cfg, &src);
    if (ocr == ESP_OK) {
        ocr = esp_ota_service_target_app_create(NULL, &tgt);
    }
    if (ocr == ESP_OK) {
        /* Strict defaults: require higher semver + matching project_name. */
        chk = esp_ota_service_checker_app_create(NULL);
        if (!chk) {
            ocr = ESP_ERR_NO_MEM;
        }
    }
    if (ocr != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: OTA component create returned %s", esp_err_to_name(ocr));
        if (src && src->destroy) {
            src->destroy(src);
        }
        if (tgt && tgt->destroy) {
            tgt->destroy(tgt);
        }
        if (chk && chk->destroy) {
            chk->destroy(chk);
        }
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_ota_evt);
        s_ota_evt = NULL;
        return ocr;
    }

    esp_ota_upgrade_item_t items[] = {{
        .uri = "ble://",
        .partition_label = NULL,
        .source = src,
        .target = tgt,
        .verifier = NULL,
        .checker = chk,
        .skip_on_fail = false,
        .resumable = false,
    }};

    ret = esp_ota_service_set_upgrade_list(svc, items, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: esp_ota_service_set_upgrade_list returned %s", esp_err_to_name(ret));
        /* On failure, esp_ota_service_set_upgrade_list already takes ownership of
         * source/target/checker and calls their destroy()s — do not free here. */
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_ota_evt);
        s_ota_evt = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "Run_ota: BLE OTA (APP protocol) ready. Use ESP BLE OTA APP to connect and upgrade.");
    ret = esp_service_start((esp_service_t *)svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota failed: esp_service_start returned %s", esp_err_to_name(ret));
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_ota_evt);
        s_ota_evt = NULL;
        return ret;
    }

    EventBits_t bits = xEventGroupWaitBits(s_ota_evt, EVT_OTA_DONE | EVT_OTA_FAIL | EVT_OTA_SKIP, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(1800000));

    esp_err_t out = ESP_OK;

    if (bits & EVT_OTA_SKIP) {
        ESP_LOGI(TAG, "Run_ota: firmware is not newer - already up to date");
        out = ESP_OK;
    } else if ((bits & EVT_OTA_DONE) && !(bits & EVT_OTA_FAIL)) {
        ESP_LOGI(TAG, "Run_ota: OTA succeeded - rebooting ...");
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_ota_evt);
        s_ota_evt = NULL;
        esp_restart();
    } else if (bits & EVT_OTA_FAIL) {
        ESP_LOGE(TAG, "Run_ota failed: BLE OTA (APP) upgrade failed (%s)",
                 esp_err_to_name(ESP_OTA_SERVICE_BLE_EX_ERR_OTA_GENERIC));
        out = ESP_OTA_SERVICE_BLE_EX_ERR_OTA_GENERIC;
    } else {
        ESP_LOGE(TAG, "Run_ota failed: BLE OTA (APP) timed out (%s)",
                 esp_err_to_name(ESP_OTA_SERVICE_BLE_EX_ERR_OTA_WAIT_TIMEOUT));
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_service_stop((esp_service_t *)svc);
        out = ESP_OTA_SERVICE_BLE_EX_ERR_OTA_WAIT_TIMEOUT;
    }

    esp_ota_service_destroy(svc);
    vEventGroupDelete(s_ota_evt);
    s_ota_evt = NULL;
    return out;
}
