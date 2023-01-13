/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2023 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#include "string.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "audio_mem.h"
#include "audio_sys.h"
#include "console.h"

static const char *TAG = "CONSOLE";

#define PROMPT_STR CONFIG_IDF_TARGET

static void initialize_console(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 1024;
    ESP_LOGI(TAG, "Command history enabled");

    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

static int free_mem(int argc, char **argv)
{
#ifdef CONFIG_SPIRAM_BOOT_INIT
    printf("Func:%s, Line:%d, MEM Total:%d Bytes, Inter:%d Bytes, Dram:%d Bytes\r\n", __FUNCTION__, __LINE__, (int)esp_get_free_heap_size(),
           (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
#else
    printf("Func:%s, Line:%d, MEM Total:%d Bytes\r\n", __FUNCTION__, __LINE__, (int)esp_get_free_heap_size());
#endif
    return 0;
}

static int reboot(int argc, char **argv)
{
    esp_restart();
    return 0;
}

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
static int run_time_stats(int argc, char **argv)
{
    audio_sys_get_real_time_stats();
    esp_timer_dump(stdout);
    return 0;
}
#endif

static int tasks_info(int argc, char **argv)
{
    const size_t bytes_per_task = 40; /* see vTaskList description */
    char *task_list_buffer = malloc(uxTaskGetNumberOfTasks() * bytes_per_task);
    if (task_list_buffer == NULL) {
        ESP_LOGE(TAG, "failed to allocate buffer for vTaskList output");
        return 1;
    }
    fputs("Task Name\tStatus\tPrio\tHWM\tTask#", stdout);
#ifdef CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
    fputs("\tAffinity", stdout);
#endif
    fputs("\n", stdout);
    vTaskList(task_list_buffer);
    fputs(task_list_buffer, stdout);
    free(task_list_buffer);
    esp_timer_dump(stdout);

    return 0;
}

static bool wifi_join(const char *ssid, const char *pass)
{
    wifi_config_t wifi_config = { 0 };
    if (ssid) {
        memcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    }
    if (pass) {
        memcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_connect() );

    return 0;
}

/** Arguments used by 'join' function */
static struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} join_args;

static int connect(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &join_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, join_args.end, argv[0]);
        return 1;
    }
    ESP_LOGI(__func__, "Connecting to '%s'",
             join_args.ssid->sval[0]);

    wifi_join(join_args.ssid->sval[0],
                join_args.password->sval[0]);
    return 0;
}

static void register_system_commond()
{
    const esp_console_cmd_t cmd_tasks = {
        .command = "tasks",
        .help = "Get information about running tasks",
        .hint = NULL,
        .func = &tasks_info,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_tasks) );

    const esp_console_cmd_t cmd_free = {
        .command = "mem",
        .help = "Get the current size of free heap memory",
        .hint = NULL,
        .func = &free_mem,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_free) );

    const esp_console_cmd_t cmd_reboot = {
        .command = "reboot",
        .help = "reboot",
        .hint = NULL,
        .func = &reboot,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_reboot) );

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    const esp_console_cmd_t cmd_stat = {
        .command = "stat",
        .help = "Get Run Time Stats",
        .hint = NULL,
        .func = &run_time_stats,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd_stat) );
#endif

    join_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    join_args.password = arg_str0(NULL, NULL, "<pass>", "PSK of AP");
    join_args.end = arg_end(2);

    const esp_console_cmd_t join_cmd = {
        .command = "join",
        .help = "Join WiFi AP as a station",
        .hint = NULL,
        .func = &connect,
        .argtable = &join_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&join_cmd) );
}

void console_init()
{
    initialize_console();
    esp_console_register_help_command();
    register_system_commond();
}
