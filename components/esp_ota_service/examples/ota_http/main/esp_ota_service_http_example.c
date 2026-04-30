/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "esp_ota_service.h"
#include "esp_ota_service_source_http.h"
#include "esp_ota_service_target_app.h"
#include "esp_ota_service_checker_manifest.h"
#include "esp_ota_service_verifier_sha256.h"

#include "esp_ota_service_http_example.h"

#define WIFI_SSID                             "esp-Office-2.4G"
#define WIFI_PASSWORD                         "1qazxsw2"
#define ESP_OTA_SERVICE_EXAMPLE_MANIFEST_URL  "http://10.18.20.234:18070/manifest.json"
#define ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL  "http://10.18.20.234:18070/firmware.bin"

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_MAX_RETRY      10

#define EVT_OTA_DONE  (BIT0)
#define EVT_OTA_FAIL  (BIT1)
#define EVT_OTA_SKIP  (BIT2)

static const char *TAG = "OTA_HTTP_EXAMPLE";

static EventGroupHandle_t s_wifi_evt;
static int s_retry_count;

static EventGroupHandle_t s_ota_evt;

static void on_ota_adf_event(const adf_event_t *event, void *ctx);

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "WiFi reconnecting... (%d/%d)", s_retry_count, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_evt, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Wifi_connect failed: connection failed after %d retries (%s)", WIFI_MAX_RETRY,
                     esp_err_to_name(ESP_OTA_SERVICE_HTTP_EX_ERR_WIFI_FAILED));
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "WiFi connected — IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_evt, WIFI_CONNECTED_BIT);
    }
}

esp_err_t esp_ota_service_http_example_wifi_connect(void)
{
    s_retry_count = 0;
    s_wifi_evt = xEventGroupCreate();
    if (!s_wifi_evt) {
        ESP_LOGE(TAG, "Wifi_connect failed: event group create returned NULL (%s)",
                 esp_err_to_name(ESP_OTA_SERVICE_HTTP_EX_ERR_NO_EVENT_GROUP));
        return ESP_OTA_SERVICE_HTTP_EX_ERR_NO_EVENT_GROUP;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi_connect failed: esp_netif_init returned %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_evt);
        s_wifi_evt = NULL;
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi_connect failed: esp_event_loop_create_default returned %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_evt);
        s_wifi_evt = NULL;
        return err;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi_connect failed: esp_wifi_init returned %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_evt);
        s_wifi_evt = NULL;
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi_connect failed: register WIFI_EVENT handler returned %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_evt);
        s_wifi_evt = NULL;
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi_connect failed: register IP_EVENT handler returned %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_evt);
        s_wifi_evt = NULL;
        return err;
    }

    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = WIFI_SSID,
                .password = WIFI_PASSWORD,
            },
    };
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi_connect failed: esp_wifi_set_mode returned %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_evt);
        s_wifi_evt = NULL;
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi_connect failed: esp_wifi_set_config returned %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_evt);
        s_wifi_evt = NULL;
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wifi_connect failed: esp_wifi_start returned %s", esp_err_to_name(err));
        vEventGroupDelete(s_wifi_evt);
        s_wifi_evt = NULL;
        return err;
    }

    EventBits_t bits =
        xEventGroupWaitBits(s_wifi_evt, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }
    if (bits & WIFI_FAIL_BIT) {
        /* Failure already logged in wifi_event_handler() */
        vEventGroupDelete(s_wifi_evt);
        s_wifi_evt = NULL;
        return ESP_OTA_SERVICE_HTTP_EX_ERR_WIFI_FAILED;
    }

    ESP_LOGE(TAG, "Wifi_connect failed: wait for IP timed out (%s)", esp_err_to_name(ESP_OTA_SERVICE_HTTP_EX_ERR_WIFI_TIMEOUT));
    vEventGroupDelete(s_wifi_evt);
    s_wifi_evt = NULL;
    return ESP_OTA_SERVICE_HTTP_EX_ERR_WIFI_TIMEOUT;
}

esp_err_t esp_ota_service_http_example_confirm_if_pending_verify(esp_ota_img_states_t img_state)
{
    if (img_state != ESP_OTA_IMG_PENDING_VERIFY) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "New image pending verify — WiFi OK, marking OTA valid ...");
    esp_err_t confirm_ret = esp_ota_service_confirm_update();
    if (confirm_ret != ESP_OK) {
        ESP_LOGE(TAG, "Confirm_if_pending_verify failed: esp_ota_service_confirm_update returned %s",
                 esp_err_to_name(confirm_ret));
        (void)esp_ota_service_rollback();
        return confirm_ret;
    }

    ESP_LOGI(TAG, "Confirm_if_pending_verify: upgrade OK, rollback cancelled");
    return ESP_OK;
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
                ESP_LOGW(TAG, "[OTA] Server firmware is not newer - skipping (no download)");
                xEventGroupSetBits(s_ota_evt, EVT_OTA_SKIP);
            } else {
                ESP_LOGI(TAG, "[OTA] Ver check done: err=%s upgrade_available=%s size=%" PRIu32,
                         esp_err_to_name(evt->error),
                         evt->ver_check.upgrade_available ? "true" : "false",
                         evt->ver_check.image_size);
            }
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_BEGIN:
            ESP_LOGI(TAG, "[OTA] Downloading from server ...");
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_PROGRESS:
            ESP_LOGI(TAG, "[OTA] Progress: %" PRIu32 " / %" PRIu32 " bytes",
                     evt->progress.bytes_written, evt->progress.total_bytes);
            break;
        case ESP_OTA_SERVICE_EVT_ITEM_END:
            switch (evt->item_end.status) {
                case ESP_OTA_SERVICE_ITEM_STATUS_OK:
                    ESP_LOGI(TAG, "[OTA] Download + write succeeded");
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

esp_err_t esp_ota_service_http_example_run_ota_session(void)
{
    esp_ota_service_cfg_t svc_cfg = ESP_OTA_SERVICE_CFG_DEFAULT();

    esp_ota_service_t *svc = NULL;
    esp_err_t ret = esp_ota_service_create(&svc_cfg, &svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota_session failed: esp_ota_service_create returned %s", esp_err_to_name(ret));
        return ret;
    }

    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.handler = on_ota_adf_event;
    ret = esp_service_event_subscribe((esp_service_t *)svc, &sub);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota_session failed: esp_service_event_subscribe returned %s", esp_err_to_name(ret));
        esp_ota_service_destroy(svc);
        return ret;
    }

    esp_ota_service_source_http_cfg_t hcfg = {
        .timeout_ms = 10000,
        .buf_size = 4096,
    };

    /* --- Step 1: pre-fetch manifest to check version and obtain SHA-256 --- */
    const esp_app_desc_t *app_desc = esp_app_get_description();
    esp_ota_service_checker_manifest_cfg_t mcfg = {
        .manifest_uri = ESP_OTA_SERVICE_EXAMPLE_MANIFEST_URL,
        .current_version = app_desc->version,
        .require_higher_version = true,
        .check_project_name = false,
    };
    esp_ota_service_checker_t *manifest_chk = esp_ota_service_checker_manifest_create(&mcfg);
    if (!manifest_chk) {
        ESP_LOGE(TAG, "Run_ota_session failed: manifest checker create returned NULL");
        esp_ota_service_destroy(svc);
        return ESP_ERR_NO_MEM;
    }
    esp_ota_service_check_result_t chk_result;
    memset(&chk_result, 0, sizeof(chk_result));
    esp_err_t cr = manifest_chk->check(manifest_chk, NULL, NULL, &chk_result);
    manifest_chk->destroy(manifest_chk);
    if (cr != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota_session failed: manifest check returned %s", esp_err_to_name(cr));
        esp_ota_service_destroy(svc);
        return cr;
    }
    if (!chk_result.upgrade_available) {
        ESP_LOGW(TAG, "Run_ota_session: server firmware '%s' is not newer than running '%s' — skipping",
                 chk_result.version, app_desc->version);
        esp_ota_service_destroy(svc);
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Run_ota_session: upgrade available '%s' -> '%s'", app_desc->version, chk_result.version);

    /* --- Step 2: arm SHA-256 verifier if manifest provided a hash --- */
    esp_ota_service_verifier_t *ver = NULL;
    if (chk_result.has_hash) {
        esp_ota_service_verifier_sha256_cfg_t vcfg;
        memcpy(vcfg.expected_hash, chk_result.hash, sizeof(vcfg.expected_hash));
        esp_err_t vr = esp_ota_service_verifier_sha256_create(&vcfg, &ver);
        if (vr != ESP_OK) {
            ESP_LOGW(TAG, "Run_ota_session: SHA-256 verifier create failed (%s) — proceeding without integrity check",
                     esp_err_to_name(vr));
        } else {
            ESP_LOGI(TAG, "Run_ota_session: SHA-256 verifier armed from manifest");
        }
    } else {
        ESP_LOGW(TAG, "Run_ota_session: manifest has no 'sha256' field — integrity check skipped");
    }

    /* --- Step 3: create HTTP source and app target --- */
    esp_ota_service_source_t *src = NULL;
    esp_ota_service_target_t *tgt = NULL;
    esp_err_t ocr = esp_ota_service_source_http_create(&hcfg, &src);
    if (ocr == ESP_OK) {
        esp_ota_service_target_app_cfg_t app_cfg = {.bulk_flash_erase = true};
        ocr = esp_ota_service_target_app_create(&app_cfg, &tgt);
    }
    if (ocr != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota_session failed: OTA component create returned %s", esp_err_to_name(ocr));
        if (src && src->destroy) {
            src->destroy(src);
        }
        if (tgt && tgt->destroy) {
            tgt->destroy(tgt);
        }
        if (ver && ver->destroy) {
            ver->destroy(ver);
        }
        esp_ota_service_destroy(svc);
        return ocr;
    }

    /* checker = NULL: version was confirmed above via manifest pre-fetch;
     * verifier carries the SHA-256 digest for streaming integrity check. */
    esp_ota_upgrade_item_t items[] = {{
        .uri = ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL,
        .partition_label = NULL,
        .source = src,
        .target = tgt,
        .verifier = ver,
        .checker = NULL,
        .skip_on_fail = false,
        .resumable = true,
    }};

    /* From this point on, svc owns src/tgt/ver (per esp_ota_service_set_upgrade_list
     * ownership model). Do NOT call src/tgt/ver->destroy() directly —
     * esp_ota_service_destroy() will release them via the upgrade list. */
    ret = esp_ota_service_set_upgrade_list(svc, items, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota_session failed: esp_ota_service_set_upgrade_list returned %s", esp_err_to_name(ret));
        esp_ota_service_destroy(svc);
        return ret;
    }

    ESP_LOGI(TAG, "Run_ota_session: starting HTTP OTA service: %s", ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL);
    s_ota_evt = xEventGroupCreate();
    if (!s_ota_evt) {
        ESP_LOGE(TAG, "Run_ota_session failed: OTA event group create returned NULL (%s)",
                 esp_err_to_name(ESP_OTA_SERVICE_HTTP_EX_ERR_NO_EVENT_GROUP));
        esp_ota_service_destroy(svc);
        return ESP_OTA_SERVICE_HTTP_EX_ERR_NO_EVENT_GROUP;
    }

    ret = esp_service_start((esp_service_t *)svc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run_ota_session failed: esp_service_start returned %s", esp_err_to_name(ret));
        vEventGroupDelete(s_ota_evt);
        s_ota_evt = NULL;
        esp_ota_service_destroy(svc);
        return ret;
    }

    const TickType_t session_deadline = pdMS_TO_TICKS(600000);
    EventBits_t bits = xEventGroupWaitBits(s_ota_evt, EVT_OTA_DONE | EVT_OTA_FAIL | EVT_OTA_SKIP,
                                           pdFALSE, pdFALSE, session_deadline);

    esp_err_t out = ESP_OK;

    if (bits & EVT_OTA_SKIP) {
        ESP_LOGW(TAG, "Run_ota_session: OTA session skipped (unexpected after check_update)");
        out = ESP_OK;
    } else if ((bits & EVT_OTA_DONE) && !(bits & EVT_OTA_FAIL)) {
        ESP_LOGI(TAG, "Run_ota_session: OTA session finished OK; rebooting from the application");
        esp_ota_service_destroy(svc);
        vEventGroupDelete(s_ota_evt);
        s_ota_evt = NULL;
        esp_restart();
    } else if (bits & EVT_OTA_FAIL) {
        ESP_LOGE(TAG, "Run_ota_session failed: HTTP OTA upgrade failed (%s)",
                 esp_err_to_name(ESP_OTA_SERVICE_HTTP_EX_ERR_OTA_GENERIC));
        out = ESP_OTA_SERVICE_HTTP_EX_ERR_OTA_GENERIC;
    } else {
        ESP_LOGE(TAG, "Run_ota_session failed: HTTP OTA timed out (%s)",
                 esp_err_to_name(ESP_OTA_SERVICE_HTTP_EX_ERR_OTA_TIMEOUT));
        out = ESP_OTA_SERVICE_HTTP_EX_ERR_OTA_TIMEOUT;
    }

    esp_ota_service_destroy(svc);
    vEventGroupDelete(s_ota_evt);
    s_ota_evt = NULL;
    return out;
}
