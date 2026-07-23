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

#include "esp_audio_dec_default.h"
#include "esp_audio_enc_default.h"
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_cli_service.h"
#include "esp_codec_dev.h"
#include "esp_console.h"
#include "esp_extractor_defaults.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_sys.h"
#include "esp_gmf_oal_thread.h"
#include "esp_log.h"
#include "esp_muxer_default.h"
#include "esp_service.h"

#include "audio_record_cases.h"
#include "audio_record_mcp.h"
#include "audio_record_player.h"
#include "audio_record_scheduler.h"
#include "settings.h"
#include "simple_record.h"

static const char *TAG = "AUDIO_RECORD";

#define RECORD_BG_TASK_NAME        "aud_rec_bg"
#define RECORD_BG_TASK_STACK_SIZE  (12 * 1024)
#define RECORD_BG_TASK_PRIORITY    5

typedef struct {
    char      case_name[32];
    uint32_t  duration_ms;
    bool      verify;
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
    printf("Background record still running; wait for it to finish\n");
    return 1;
}

static void print_cases(void)
{
    printf("Available cases:\n");
    for (uint16_t i = 0; i < audio_record_get_case_count(); i++) {
        const audio_record_case_info_t *info = audio_record_get_case(i);
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

static bool parse_verify_arg(const char *arg)
{
    if (arg == NULL) {
        return false;
    }
    return strcmp(arg, "verify") == 0 ||
           strcmp(arg, "1") == 0 ||
           strcmp(arg, "true") == 0 ||
           strcmp(arg, "yes") == 0;
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

static void record_bg_task(void *arg)
{
    record_job_t *job = (record_job_t *)arg;
    audio_record_case_stats_t stats;
    esp_err_t ret = audio_record_run_case(job->case_name, job->duration_ms, job->verify, &stats);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Record case '%s' failed: %s", job->case_name, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Record case '%s' finished", job->case_name);
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
        printf("Usage: record <case> [duration_ms] [verify|1]\n");
        print_cases();
        return 1;
    }

    uint32_t duration_ms = argc > 2 ? strtoul(argv[2], NULL, 10) : AUDIO_RECORD_DEFAULT_DURATION;
    bool verify = argc > 3 && parse_verify_arg(argv[3]);
    memset(&s_record_job, 0, sizeof(s_record_job));
    strncpy(s_record_job.case_name, argv[1], sizeof(s_record_job.case_name) - 1);
    s_record_job.duration_ms = duration_ms;
    s_record_job.verify = verify;

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

static int run_all_command(int argc, char **argv)
{
    if (resist_if_bg_running() != 0) {
        return 1;
    }
    uint32_t duration_ms = AUDIO_RECORD_DEFAULT_DURATION;
    bool verify = false;
    bool with_trace = argv_has_token(argc, argv, "with_trace");
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "with_trace") == 0) {
            continue;
        }
        if (parse_verify_arg(argv[i])) {
            verify = true;
            continue;
        }
        char *end = NULL;
        unsigned long value = strtoul(argv[i], &end, 10);
        if (end != argv[i] && end != NULL && *end == '\0') {
            duration_ms = (uint32_t)value;
            continue;
        }
        printf("Usage: run_all [duration_ms] [verify|1] [with_trace]\n");
        printf("  First run without with_trace to settle always-held allocations,\n");
        printf("  then run_all <ms> with_trace to dump leaks.\n");
        return 1;
    }
    esp_err_t ret = audio_record_run_all(duration_ms, verify, with_trace);
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
    if (argc < 2 || (strcmp(argv[1], "stream") != 0 &&
                     strcmp(argv[1], "direct") != 0 &&
                     strcmp(argv[1], "ai_direct") != 0 &&
                     strcmp(argv[1], "storage") != 0)) {
        printf("Usage: simple <stream|direct|ai_direct|storage> [duration_ms]\n");
        return 1;
    }
    uint32_t duration_ms = argc > 2 ? strtoul(argv[2], NULL, 10) : AUDIO_RECORD_DEFAULT_DURATION;
    esp_err_t ret;
    if (strcmp(argv[1], "stream") == 0) {
        ret = simple_record_stream(duration_ms);
    } else if (strcmp(argv[1], "direct") == 0) {
        ret = simple_record_direct(duration_ms);
    } else if (strcmp(argv[1], "ai_direct") == 0) {
        ret = simple_record_ai_direct(duration_ms);
    } else {
        ret = simple_record_storage(duration_ms);
    }
    return ret == ESP_OK ? 0 : 1;
}

static int status_command(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("Background record: %s\n", bg_busy() ? "running" : "idle");
    esp_gmf_oal_mem_print("GMF", __LINE__, __func__);
    esp_gmf_oal_sys_get_real_time_stats(2000, false);
    return 0;
}

static esp_err_t start_console(void)
{
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.base_cfg.name = "audio-record-cli";
    cfg.prompt = "audio-record>";

    esp_cli_service_t *cli = NULL;
    ESP_RETURN_ON_ERROR(esp_cli_service_create(&cfg, &cli), TAG, "create CLI");
    const esp_console_cmd_t commands[] = {
        {
            .command = "cases",
            .help = "List audio record cases",
            .func = list_command,
        },
        {
            .command = "record",
            .help = "Run one case in background: record <case> [duration_ms] [verify|1]",
            .func = run_command,
        },
        {
            .command = "run_all",
            .help = "Run every case: run_all [duration_ms] [verify|1] [with_trace]",
            .func = run_all_command,
        },
        {
            .command = "simple",
            .help = "Run a basic walkthrough: simple <stream|direct|ai_direct|storage> [duration_ms]",
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

    dev_audio_codec_handles_t *dac = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_AUDIO_DAC, (void **)&dac) == ESP_OK &&
        dac != NULL && dac->codec_dev != NULL) {
        int ret = esp_codec_dev_set_out_vol(dac->codec_dev, DEFAULT_VOL);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "Failed to set output volume %d: %d", DEFAULT_VOL, ret);
        } else {
            ESP_LOGI(TAG, "Output volume set to %d", DEFAULT_VOL);
        }
    }
    return ESP_OK;
}

void app_main(void)
{
    // esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("RECORD_CASE", ESP_LOG_INFO);

    ESP_ERROR_CHECK(esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_ADC));
    esp_err_t sd_ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD);
    if (sd_ret != ESP_OK) {
        ESP_LOGW(TAG, "SD card is unavailable; storage cases will fail");
    }
    esp_err_t dac_ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_AUDIO_DAC);
    if (dac_ret == ESP_OK) {
        ESP_ERROR_CHECK(audio_record_player_init());
    } else {
        ESP_LOGW(TAG, "Audio DAC is unavailable; AEC reference and verification playback are disabled");
    }
    (void)apply_codec_levels();

    ESP_ERROR_CHECK(esp_audio_enc_register_default());
    ESP_ERROR_CHECK(esp_audio_dec_register_default());
    ESP_ERROR_CHECK(esp_muxer_register_default());
    ESP_ERROR_CHECK(esp_extractor_register_default());
    ESP_ERROR_CHECK(audio_record_scheduler_install());
    ESP_ERROR_CHECK(audio_record_mcp_start());
    ESP_ERROR_CHECK(start_console());

    ESP_LOGI(TAG, "Audio record example is ready");
    ESP_LOGI(TAG, "AUDIO_RECORD_EXAMPLE_READY");
    ESP_LOGI(TAG, "Type 'cases' to list examples, or 'record normal_stream 10000'");
}
