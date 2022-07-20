/* RTMP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
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
#include "esp_idf_version.h"
#include "esp_console.h"
#include "esp_err.h"

/**
 * @brief Setup test duration for RTMP application (unit ms)
 */
#define RTMP_APP_RUN_DURATION (600000)

typedef enum {
    RTMP_APP_TYPE_PUSHER = 1,
    RTMP_APP_TYPE_SRC = 2,
    RTMP_APP_TYPE_SERVER = 4,
} rtmp_app_type_t;

static const char *TAG = "RTMP App";
static int app_running = 0;

static int mount_sdcard(esp_periph_set_handle_t set)
{
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    return 0;
}

static int connect_sta(esp_periph_set_handle_t set)
{
    periph_wifi_cfg_t wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    ESP_LOGI(TAG, "Waiting for wifi connected");
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
    ESP_LOGI(TAG, "Wifi connectd ok");
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    char *ip_addr = ip4addr_ntoa(&ip_info.ip);
    if (ip_addr) {
        ESP_LOGI(TAG, "server ip %s", ip_addr);
    }
    return 0;
}

static void app_thread(void *arg)
{
    rtmp_app_type_t app_type = (rtmp_app_type_t) (uint32_t) arg;
    if (app_type == RTMP_APP_TYPE_PUSHER) {
        rtmp_push_app_run(CONFIG_RTMP_SERVER_URI, RTMP_APP_RUN_DURATION);
        app_running--;
    } else if (app_type == RTMP_APP_TYPE_SRC) {
        rtmp_src_app_run(CONFIG_RTMP_SERVER_URI, RTMP_APP_RUN_DURATION);
        app_running--;
    } else if (app_type == RTMP_APP_TYPE_SERVER) {
        rtmp_server_app_run(CONFIG_RTMP_SERVER_URI, RTMP_APP_RUN_DURATION + 1000);
        app_running--;
    }
    media_lib_thread_destroy(NULL);
}

static void start_app() {
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
    if (app_running == 0) {
        media_lib_thread_handle_t thread_handle;
        audio_mem_print(TAG, __LINE__, __func__);
        if (app_type & RTMP_APP_TYPE_SERVER) {
            app_running++;
            media_lib_thread_create(&thread_handle, "server", app_thread, (void *) RTMP_APP_TYPE_SERVER, 5 * 1024, 10, 0);
            // wait for server started
            media_lib_thread_sleep(500);
        }
        if (app_type & RTMP_APP_TYPE_PUSHER) {
            app_running++;
            media_lib_thread_create(&thread_handle, "pusher", app_thread, (void *) RTMP_APP_TYPE_PUSHER, 5 * 1024, 10, 0);
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
        if (strcmp(argv[i], "sw_jpeg") == 0) {
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
        }
        i++;
    }
    uint16_t width, height;
    av_record_get_video_size(rtmp_setting_get_video_quality(), &width, &height);
    ESP_LOGI(TAG, "Video setting | sw_jpeg:%d quality:%dx%d fps:%d\n", rtmp_setting_get_sw_jpeg(), 
            width, height, rtmp_setting_get_video_fps());
    ESP_LOGI(TAG, "Audio setting | format:%d channel:%d sample_rate:%d\n", rtmp_setting_get_audio_fmt(),
           rtmp_setting_get_audio_channel(), rtmp_setting_get_audio_sample_rate());
    rtmp_setting_set_allow_run(true);
    start_app();
    return 0;
}

static int assert_cli(int argc, char **argv)
{
    *(int *) 0 = 0;
    return 0;
}

static int stop_cli(int argc, char **argv)
{
    rtmp_setting_set_allow_run(false);
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
#if (ESP_IDF_VERSION_MAJOR >= 4) && (ESP_IDF_VERSION_MINOR > 3)
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
            .func = assert_cli,
            .help = "assert system",
            .command = "assert",
        }, 
        {
            .func = setting_cli,
            .help = "setting and start application sw_jpeg %d fps %d quality %d channel %d sample_rate %d afmt %d vfmt %d",
            .command = "set",
        },
        {
            .func = stop_cli,
            .help = "stop running",
            .command = "stop",
        },
        {
            .func = cpu_cli,
            .help = "cpu usage query",
            .command = "i",
        },
    };
    for (int i = 0; i < sizeof(console_cmd)/ sizeof(esp_console_cmd_t); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&console_cmd[i]));
    }
#if (ESP_IDF_VERSION_MAJOR >= 4) && (ESP_IDF_VERSION_MINOR > 3)
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
#endif
    return 0;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    media_lib_add_default_adapter();
    init_console();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);
    connect_sta(set);
    mount_sdcard(set);
}
