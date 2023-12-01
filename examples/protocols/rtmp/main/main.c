/* RTMP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_debug_helpers.h"
#include "freertos/xtensa_context.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "esp_peripherals.h"
#include "periph_wifi.h"
#include "esp_wifi.h"
#include "board.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "rtmp_src_app.h"
#include "rtmp_push_app.h"
#include "rtmp_server_app.h"
#include "audio_mem.h"
#include "rtmp_app_setting.h"
#include "audio_sys.h"
#include "esp_console.h"
#include "esp_idf_version.h"
#include "esp_err.h"

/**
 * @brief Setup test duration for RTMP applications (unit ms)
 */
#define RTMP_APP_RUN_DURATION (6000000)
#define TAG "RTMP_APP"

typedef enum {
    RTMP_APP_TYPE_PUSHER = 1,
    RTMP_APP_TYPE_SRC = 2,
    RTMP_APP_TYPE_SERVER = 4,
} rtmp_app_type_t;

static int app_running = 0;
static char *rtmp_server_url = NULL;
static esp_periph_set_handle_t set;
static esp_periph_handle_t wifi_handle;
static bool wifi_connected = false;
static rtmp_app_type_t rtmp_app_type = RTMP_APP_TYPE_PUSHER;

#define SERVER_URL (rtmp_server_url ? rtmp_server_url : CONFIG_RTMP_SERVER_URI)

static int connect_sta(const char *ssid, const char *psw)
{
    periph_wifi_cfg_t wifi_cfg = {0};
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    strncpy((char *)wifi_cfg.wifi_config.sta.ssid, ssid ? ssid : CONFIG_WIFI_SSID, sizeof(wifi_cfg.wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.wifi_config.sta.password, psw ? psw : CONFIG_WIFI_PASSWORD, sizeof(wifi_cfg.wifi_config.sta.password) - 1);
    if (wifi_handle) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg.wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
    } else {
        wifi_handle = periph_wifi_init(&wifi_cfg);
        esp_periph_init(wifi_handle);
        ESP_LOGI(TAG, "Waiting for wifi connected");
    }
    periph_wifi_wait_for_connected(wifi_handle, 6000 / portTICK_PERIOD_MS);
    if (periph_wifi_is_connected(wifi_handle) == PERIPH_WIFI_CONNECTED) {
        ESP_LOGI(TAG, "Wifi connected OK");
        wifi_connected = true;
    } else {
        wifi_connected = false;
        ESP_LOGI(TAG, "Wifi connect fail, please check your settings");
    }
    return 0;
}

static int mount_sdcard(esp_periph_set_handle_t set)
{
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    return 0;
}

static void app_thread(void *arg)
{
    rtmp_app_type_t app_type = (rtmp_app_type_t) (uint32_t) arg;
    if (app_type == RTMP_APP_TYPE_PUSHER) {
        rtmp_push_app_run(SERVER_URL , RTMP_APP_RUN_DURATION);
        app_running--;
    } else if (app_type == RTMP_APP_TYPE_SRC) {
        rtmp_src_app_run(SERVER_URL, RTMP_APP_RUN_DURATION);
        app_running--;
    } else if (app_type == RTMP_APP_TYPE_SERVER) {
        rtmp_server_app_run(SERVER_URL, RTMP_APP_RUN_DURATION + 1000);
        app_running--;
    }
    media_lib_thread_destroy(NULL);
}

static void start_app()
{
    rtmp_app_type_t app_type = RTMP_APP_TYPE_PUSHER;
#ifdef CONFIG_RTMP_APP_MODE_PUSHER
    app_type = RTMP_APP_TYPE_PUSHER;
#endif
#ifdef CONFIG_RTMP_APP_MODE_SOURCE
    app_type = RTMP_APP_TYPE_SRC;
#endif
#ifdef CONFIG_RTMP_APP_MODE_PUSHER_SERVER
    app_type = RTMP_APP_TYPE_PUSHER | RTMP_APP_TYPE_SERVER;
#endif
    app_type = rtmp_app_type ? rtmp_app_type : app_type;
    if (app_running == 0) {
        media_lib_thread_handle_t thread_handle;
        audio_mem_print(TAG, __LINE__, __func__);
        if (app_type & RTMP_APP_TYPE_SERVER) {
            app_running++;
            media_lib_thread_create(&thread_handle, "server", app_thread, (void *) RTMP_APP_TYPE_SERVER, 5 * 1024, 10,
                                    0);
            // Waiting for server started
            media_lib_thread_sleep(500);
        }
        if (app_type & RTMP_APP_TYPE_PUSHER) {
            app_running++;
            media_lib_thread_create(&thread_handle, "pusher", app_thread, (void *) RTMP_APP_TYPE_PUSHER, 8 * 1024, 10,
                                    0);
        }
        if (app_type & RTMP_APP_TYPE_SRC) {
            app_running++;
            media_lib_thread_create(&thread_handle, "source", app_thread, (void *) RTMP_APP_TYPE_SRC, 5 * 1024, 10, 0);
        }
        media_lib_thread_sleep(5000);
        audio_mem_print(TAG, __LINE__, __func__);
    } else {
        ESP_LOGI(TAG, "Application still running, please wait!");
    }
}

static int setting_cli(int argc, char **argv)
{
    int i = 1;
    while (i + 1 < argc) {
        if (strcmp(argv[i], "url") == 0) {
            if (rtmp_server_url) {
                free(rtmp_server_url);
            }
            rtmp_server_url = strdup(argv[++i]);
        } else if (strcmp(argv[i], "sw_jpeg") == 0) {
            rtmp_setting_set_sw_jpeg((bool)atoi(argv[++i]));
        } else if (strcmp(argv[i], "quality") == 0) {
            rtmp_setting_set_video_quality((av_record_video_quality_t)atoi(argv[++i]));
        } else if (strcmp(argv[i], "fps") == 0) {
            rtmp_setting_set_video_fps((uint8_t)atoi(argv[++i]));
        } else if (strcmp(argv[i], "channel") == 0) {
            rtmp_setting_set_audio_channel((uint8_t)atoi(argv[++i]));
        } else if (strcmp(argv[i], "sample_rate") == 0) {
            rtmp_setting_set_audio_sample_rate(atoi(argv[++i]));
        } else if (strcmp(argv[i], "afmt") == 0) {
            rtmp_setting_set_audio_fmt((av_record_audio_fmt_t)atoi(argv[++i]));
        } else if (strcmp(argv[i], "vfmt") == 0) {
            rtmp_setting_set_video_fmt((av_record_video_fmt_t)atoi(argv[++i]));
        } else if (strcmp(argv[i], "insecure") == 0) {
            rtmp_setting_set_allow_insecure((bool)atoi(argv[++i]));
        }
        i++;
    }
    uint16_t width, height;
    av_record_get_video_size(rtmp_setting_get_video_quality(), &width, &height);
    ESP_LOGI(TAG, "RTMP server url: %s", SERVER_URL);
    ESP_LOGI(TAG, "Video setting | format:%s sw_jpeg:%d quality:%dx%d fps:%d",
             av_record_get_vfmt_str(rtmp_setting_get_video_fmt()),
             rtmp_setting_get_sw_jpeg(), width, height, rtmp_setting_get_video_fps());
    ESP_LOGI(TAG, "Audio setting | format:%s channel:%d sample_rate:%d",
             av_record_get_afmt_str(rtmp_setting_get_audio_fmt()),
             rtmp_setting_get_audio_channel(), rtmp_setting_get_audio_sample_rate());
    return 0;
}

static int start_cli(int argc, char **argv)
{
    if (wifi_connected == false) {
        connect_sta(NULL, NULL);
        if (wifi_connected == false) {
            ESP_LOGI(TAG, "Please connect to wifi firstly");
            return 0;
        }
    }
    rtmp_setting_set_allow_run(true);
    start_app();
    return 0;
}

static int connect_cli(int argc, char **argv)
{
    if (argc == 3) {
        connect_sta(argv[1], argv[2]);
    } else if (argc == 1) {
        connect_sta(NULL, NULL);
    }
    return 0;
}

static int stop_cli(int argc, char **argv)
{
    rtmp_setting_set_allow_run(false);
    return 0;
}

static int assert_cli(int argc, char **argv)
{
    *(int *) 0 = 0;
    return 0;
}

static const char *get_mode_str(rtmp_app_type_t app_type)
{
    switch ((int)app_type) {
        case RTMP_APP_TYPE_PUSHER:
            return "Pusher";
        case RTMP_APP_TYPE_SRC:
            return "Src";
        case RTMP_APP_TYPE_SERVER:
            return "Server";
        case (RTMP_APP_TYPE_SRC | RTMP_APP_TYPE_PUSHER):
            return "Src + Pusher";
        case (RTMP_APP_TYPE_SRC | RTMP_APP_TYPE_SERVER):
            return "Src + Server";
        case (RTMP_APP_TYPE_PUSHER | RTMP_APP_TYPE_SERVER):
            return "Pusher + Server";
        case (RTMP_APP_TYPE_SRC | RTMP_APP_TYPE_PUSHER | RTMP_APP_TYPE_SERVER):
            return "Pusher + Server + Src";
        default:
            return "Undef";
    }
}

static int mode_cli(int argc, char **argv)
{
    if (argc > 1) {
        rtmp_app_type = (rtmp_app_type_t) atoi(argv[1]);
    }
    ESP_LOGI(TAG, "Working in mode: %s", get_mode_str(rtmp_app_type));
    return 0;
}

static int cpu_cli(int argc, char **argv)
{
    audio_mem_print(TAG, __LINE__, __func__);
    audio_sys_get_real_time_stats();
    return 0;
}

static int init_console()
{
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "esp>";
    // install console REPL environment
#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
#endif
#endif
    esp_console_cmd_t console_cmd[] = {
        {
            .func = connect_cli,
            .help = "Connect to wifi: ssid(%s) psw(%s)",
            .command = "connect",
        },
        {
            .func = setting_cli,
            .help = "Settings: url(%s) vfmt(%d) sw_jpeg(%d) fps(%d) quality(%d) afmt(%d) channel(%d) sample_rate(%d)",
            .command = "set",
        },
        {
            .func = start_cli,
            .help = "Start to run RTMP applications",
            .command = "start",
        },
        {
            .func = stop_cli,
            .help = "Stop RTMP applications",
            .command = "stop",
        },
        {
            .func = mode_cli,
            .help = "Setting RTMP working mode:(%d) 1:(Pusher|default) 2:(Src) 4:(Server) 5:(Pusher+Server) etc",
            .command = "mode",
        },
        {
            .func = cpu_cli,
            .help = "CPU usage",
            .command = "i",
        },
        {
            .func = assert_cli,
            .help = "Assert system to debug system hang issue",
            .command = "assert",
        },
    };
    for (int i = 0; i < sizeof(console_cmd) / sizeof(esp_console_cmd_t); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&console_cmd[i]));
    }
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
#endif
    return 0;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    media_lib_add_default_adapter();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    audio_board_handle_t audio_board = audio_board_init();
    if (audio_board == NULL) {
        ESP_LOGE(TAG, "Fail to init audio board");
        return;
    }
    audio_hal_ctrl_codec(audio_board->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(audio_board->audio_hal, 0);
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);
    init_console();
    mount_sdcard(set);
}
