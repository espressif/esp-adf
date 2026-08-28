/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include "media_lib_adapter.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "unity.h"
#include "unity_test_utils_memory.h"
#include "esp_heap_trace.h"
#include "esp_log.h"

static const char *TAG = "APP_MAIN";

#define ARRAY_SIZE(a)           (sizeof(a) / sizeof((a)[0]))
#define MAX_LEAK_TRACE_RECORDS  600

#define TEST_MEMORY_LEAK_THRESHOLD  40960

extern void esp_rtsp_service_ut_force_link(void);

static esp_netif_t *s_ap_netif;

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

static void init_softap(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        TEST_ESP_OK(nvs_flash_erase());
        TEST_ESP_OK(nvs_flash_init());
    } else {
        TEST_ESP_OK(ret);
    }

    ret = esp_netif_init();
    TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE);
    ret = esp_event_loop_create_default();
    TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE);
    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        TEST_ASSERT_NOT_NULL(s_ap_netif);
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    TEST_ESP_OK(esp_wifi_init(&init_cfg));
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "rtsp_ut",
            .ssid_len = 7,
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    TEST_ESP_OK(esp_wifi_set_mode(WIFI_MODE_AP));
    TEST_ESP_OK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    TEST_ESP_OK(esp_wifi_start());
}

static void deinit_softap(void)
{
    esp_wifi_stop();
    esp_wifi_deinit();
    if (s_ap_netif != NULL) {
        esp_netif_destroy_default_wifi(s_ap_netif);
        s_ap_netif = NULL;
    }
    esp_event_loop_delete_default();
    esp_netif_deinit();
    nvs_flash_deinit();
}

static void trace_for_leak(bool start)
{
#if CONFIG_HEAP_TRACING_STANDALONE
    static heap_trace_record_t *trace_record;
    static bool started = false;
    static size_t free_internal_at_start;
    if (trace_record == NULL) {
        trace_record = heap_caps_malloc(MAX_LEAK_TRACE_RECORDS * sizeof(heap_trace_record_t),
                                        MALLOC_CAP_SPIRAM);
        if (trace_record == NULL) {
            ESP_LOGE(TAG, "No memory to start heap trace");
            return;
        }
        heap_trace_init_standalone(trace_record, MAX_LEAK_TRACE_RECORDS);
    }
    if (start) {
        if (!started) {
            free_internal_at_start = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            heap_trace_start(HEAP_TRACE_LEAKS);
            started = true;
            ESP_LOGW(TAG, "Heap leak tracing started (free internal=%u)",
                     (unsigned)free_internal_at_start);
        }
    } else if (started) {
        heap_trace_stop();
        size_t free_internal_at_end = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGW(TAG, "Heap free internal: start=%u end=%u delta=%d",
                 (unsigned)free_internal_at_start,
                 (unsigned)free_internal_at_end,
                 (int)free_internal_at_end - (int)free_internal_at_start);
        ESP_LOGW(TAG, "Note: still-held I2S DMA from board ADC may show as false leaks; "
                      "repeat run_all with_trace — growing residual means a real leak");
        heap_trace_dump();
        started = false;
        ESP_LOGW(TAG, "Heap leak tracing dumped");
    }
#else
    if (start) {
        ESP_LOGW(TAG, "Heap tracing disabled; enable CONFIG_HEAP_TRACING_STANDALONE");
    }
#endif  /* CONFIG_HEAP_TRACING_STANDALONE */
}

void app_main(void)
{
    media_lib_add_default_adapter();
    esp_rtsp_service_ut_force_link();
    init_softap();

    printf("Running esp_rtsp_service unit tests\n");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[esp_rtsp_service]", false);
#if CONFIG_HEAP_TRACING_STANDALONE
    trace_for_leak(true);
    unity_run_tests_by_tag("[esp_rtsp_service]", false);
    trace_for_leak(false);
#endif
    UNITY_END();
    deinit_softap();
}
