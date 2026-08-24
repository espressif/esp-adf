/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_audio_enc_default.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_cli_service.h"
#include "esp_codec_dev.h"
#include "esp_console.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_thread.h"
#include "esp_log.h"
#include "esp_muxer_default.h"
#include "esp_service.h"
#include "esp_video_dec_default.h"
#include "esp_video_enc_default.h"

#include "settings.h"
#include "simple_capture.h"
#include "video_capture_cases.h"
#include "video_capture_mcp.h"
#include "video_capture_scheduler.h"

static const char *TAG = "VIDEO_CAPTURE";

#define RECORD_BG_TASK_NAME        "vid_rec_bg"
#define RECORD_BG_TASK_STACK_SIZE  (12 * 1024)
#define RECORD_BG_TASK_PRIORITY    5

typedef struct {
    char      case_name[32];
    uint32_t  duration_ms;
} record_job_t;

static volatile bool s_bg_running;
static esp_gmf_oal_thread_t s_bg_thread;
static record_job_t s_record_job;

static bool bg_busy(void)
{
    return s_bg_running;
}

static int resist_if_bg_running(void)
{
    if (!bg_busy()) {
        return 0;
    }
    printf("Background capture still running; wait for it to finish\n");
    return 1;
}

static void print_cases(void)
{
    printf("Available cases:\n");
    for (uint16_t i = 0; i < video_capture_get_case_count(); i++) {
        const video_capture_case_info_t *info = video_capture_get_case(i);
        printf("  %-24s %s\n", info->name, info->description);
    }
}

static int list_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    print_cases();
    return 0;
}

static void record_bg_task(void *arg)
{
    record_job_t *job = (record_job_t *)arg;
    video_capture_case_stats_t stats;
    esp_err_t ret = video_capture_run_case(job->case_name, job->duration_ms, &stats);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Capture case '%s' failed: %s", job->case_name, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Capture case '%s' finished", job->case_name);
    }
    s_bg_running = false;
    s_bg_thread = NULL;
    esp_gmf_oal_thread_delete(NULL);
}

static int run_command(int argc, char **argv)
{
    if (resist_if_bg_running() != 0) {
        return 1;
    }
    if (argc < 2) {
        printf("Usage: record <case> [duration_ms]\n");
        print_cases();
        return 1;
    }

    uint32_t duration_ms = argc > 2 ? strtoul(argv[2], NULL, 10) : VIDEO_CAPTURE_DEFAULT_DURATION;
    memset(&s_record_job, 0, sizeof(s_record_job));
    strncpy(s_record_job.case_name, argv[1], sizeof(s_record_job.case_name) - 1);
    s_record_job.duration_ms = duration_ms;

    s_bg_running = true;
    if (esp_gmf_oal_thread_create(&s_bg_thread,
                                  RECORD_BG_TASK_NAME,
                                  record_bg_task,
                                  &s_record_job,
                                  RECORD_BG_TASK_STACK_SIZE,
                                  RECORD_BG_TASK_PRIORITY,
                                  false,
                                  1) != ESP_GMF_ERR_OK) {
        s_bg_running = false;
        s_bg_thread = NULL;
        ESP_LOGE(TAG, "Failed to create background record task");
        return 1;
    }
    ESP_LOGI(TAG, "Started background record for '%s' (%" PRIu32 " ms)",
             s_record_job.case_name, s_record_job.duration_ms);
    return 0;
}

static bool argv_has_token(int argc, char **argv, const char *token)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], token) == 0) {
            return true;
        }
    }
    return false;
}

static int run_all_command(int argc, char **argv)
{
    if (resist_if_bg_running() != 0) {
        return 1;
    }
    uint32_t duration_ms = VIDEO_CAPTURE_DEFAULT_DURATION;
    bool with_trace = argv_has_token(argc, argv, "with_trace");
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "with_trace") == 0) {
            continue;
        }
        char *end = NULL;
        unsigned long value = strtoul(argv[i], &end, 10);
        if (end != argv[i] && end != NULL && *end == '\0') {
            duration_ms = (uint32_t)value;
            break;
        }
        printf("Usage: run_all [duration_ms] [with_trace]\n");
        printf("  First run without with_trace to settle always-held allocations,\n");
        printf("  then run_all <ms> with_trace to dump leaks.\n");
        return 1;
    }
    esp_err_t ret = video_capture_run_all(duration_ms, with_trace);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Run-all stopped: %s", esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

static int simple_command(int argc, char **argv)
{
    if (resist_if_bg_running() != 0) {
        return 1;
    }
    if (argc < 2) {
        printf("Usage: simple <video_only|av_stream|av_storage|av_link|av_ai|"
               "av_dummy_raw|av_dummy_encoded|fullspeed_uvc> [duration_ms]\n");
        return 1;
    }
    uint32_t duration_ms = argc > 2 ? strtoul(argv[2], NULL, 10) : VIDEO_CAPTURE_DEFAULT_DURATION;
    esp_err_t ret;
    if (strcmp(argv[1], "video_only") == 0) {
        ret = simple_capture_video_only(duration_ms);
    } else if (strcmp(argv[1], "av_stream") == 0) {
        ret = simple_capture_av_stream(duration_ms);
    } else if (strcmp(argv[1], "av_storage") == 0) {
        ret = simple_capture_av_storage(duration_ms);
    } else if (strcmp(argv[1], "av_link") == 0) {
        ret = simple_capture_av_link(duration_ms);
    } else if (strcmp(argv[1], "av_ai") == 0) {
        ret = simple_capture_av_ai(duration_ms);
    } else if (strcmp(argv[1], "av_dummy_raw") == 0) {
        ret = simple_capture_av_dummy_raw_storage(duration_ms);
    } else if (strcmp(argv[1], "av_dummy_encoded") == 0) {
        ret = simple_capture_av_dummy_encoded_storage(duration_ms);
    } else if (strcmp(argv[1], "fullspeed_uvc") == 0) {
        ret = simple_capture_fullspeed_uvc(duration_ms);
    } else {
        printf("Usage: simple <video_only|av_stream|av_storage|av_link|av_ai|"
               "av_dummy_raw|av_dummy_encoded|fullspeed_uvc> [duration_ms]\n");
        return 1;
    }
    return ret == ESP_OK ? 0 : 1;
}

static int status_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("Background capture: %s\n", bg_busy() ? "running" : "idle");
    esp_gmf_oal_mem_print("GMF", __LINE__, __func__);
    esp_gmf_oal_sys_get_real_time_stats(2000, false);
    return 0;
}

static esp_err_t start_console(void)
{
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.base_cfg.name = "video-capture-cli";
    cfg.prompt = "video-capture>";

    esp_cli_service_t *cli = NULL;
    ESP_RETURN_ON_ERROR(esp_cli_service_create(&cfg, &cli), TAG, "create CLI");
    const esp_console_cmd_t commands[] = {
        {
            .command = "cases",
            .help = "List video capture cases",
            .func = list_command,
        },
        {
            .command = "record",
            .help = "Run one case in background: record <case> [duration_ms]",
            .func = run_command,
        },
        {
            .command = "run_all",
            .help = "Run every case: run_all [duration_ms] [with_trace]",
            .func = run_all_command,
        },
        {
            .command = "simple",
            .help = "Run a basic walkthrough: simple <video_only|av_stream|av_storage|av_link|av_ai|fullspeed_uvc> [duration_ms]",
            .func = simple_command,
        },
        {
            .command = "i",
            .help = "Query system memory and status",
            .func = status_command,
        },
    };
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        ESP_RETURN_ON_ERROR(esp_cli_service_register_static_command(cli, &commands[i]),
                            TAG, "register command");
    }
    return esp_service_start((esp_service_t *)cli);
}

static esp_err_t apply_codec_levels(void)
{
    dev_audio_codec_handles_t *adc = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_ADC, (void **)&adc) == ESP_OK &&
        adc != NULL && adc->codec_dev != NULL) {
        int ret = esp_codec_dev_set_in_gain(adc->codec_dev, DEFAULT_MIC_GAIN);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "Failed to set mic gain %.1f dB: %d", DEFAULT_MIC_GAIN, ret);
        } else {
            ESP_LOGI(TAG, "Mic gain set to %.1f dB", DEFAULT_MIC_GAIN);
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    // esp_log_level_set("*", ESP_LOG_ERROR);
    esp_log_level_set("VIDEO_CASE", ESP_LOG_INFO);
    esp_log_level_set("SIMPLE_CAPTURE", ESP_LOG_INFO);

#if CONFIG_IDF_TARGET_ESP32P4
    esp_err_t ldo_ret = esp_board_periph_init(ESP_BOARD_PERIPH_NAME_LDO_MIPI);
    if (ldo_ret != ESP_OK) {
        ESP_LOGW(TAG, "LDO MIPI init skipped: %s", esp_err_to_name(ldo_ret));
    }
#endif  /* CONFIG_IDF_TARGET_ESP32P4 */

    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_CAMERA));
    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC));

    esp_err_t sd_ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (sd_ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card is unavailable; storage cases will fail");
    }

    (void)apply_codec_levels();

    ESP_ERROR_CHECK(esp_audio_enc_register_default());
    ESP_ERROR_CHECK(esp_video_enc_register_default());
#if CONFIG_ESP_CAPTURE_ENABLE_VIDEO_DECODER
    ESP_ERROR_CHECK(esp_video_dec_register_default());
#endif  /* CONFIG_ESP_CAPTURE_ENABLE_VIDEO_DECODER */
    ESP_ERROR_CHECK(esp_muxer_register_default());
    ESP_ERROR_CHECK(video_capture_scheduler_install());
    ESP_ERROR_CHECK(video_capture_mcp_start());
    ESP_ERROR_CHECK(start_console());

    ESP_LOGI(TAG, "Video capture example is ready");
    ESP_LOGI(TAG, "VIDEO_CAPTURE_EXAMPLE_READY");
    ESP_LOGI(TAG, "Type 'cases' to list examples, or 'simple video_only 10000'");
}
