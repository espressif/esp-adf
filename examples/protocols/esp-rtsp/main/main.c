/* RTSP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "string.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "board.h"
#include "console.h"
#include "audio_mem.h"
#include "periph_wifi.h"
#include "algorithm_stream.h"
#include "rtsp_service.h"

#define TAG         "ESP_RTSP_Demo"

#define WIFI_SSID  "myssid"
#define WIFI_PWD   "mypassword"

static esp_rtsp_handle_t esp_rtsp;
static av_stream_handle_t av_stream;

static struct {
    struct arg_str *mode;
    struct arg_str *uri;
    struct arg_end *end;
} rtsp_args;

static void setup_wifi(esp_periph_set_handle_t set)
{
    ESP_ERROR_CHECK(esp_netif_init());
    periph_wifi_cfg_t wifi_cfg = {
        .wifi_config.sta.ssid = WIFI_SSID,
        .wifi_config.sta.password = WIFI_PWD,
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);
}

static int start(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &rtsp_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, rtsp_args.end, argv[0]);
        return 1;
    }

    int mode = atoi(rtsp_args.mode->sval[0]);
    const char *uri = rtsp_args.uri->sval[0];

    esp_rtsp = rtsp_service_start(av_stream, mode, uri);
    return 0;
}

static int stop(int argc, char **argv)
{
    rtsp_service_stop(esp_rtsp);
    return 0;
}

static void register_rtsp_cmd()
{
    rtsp_args.mode   = arg_str1(NULL, NULL, "<mode>", "0: Server 1: Push 2: Play");
    rtsp_args.uri    = arg_str0(NULL, NULL, "<uri>", "rtsp://162.168.10.233:554/live");
    rtsp_args.end    = arg_end(3);
    const esp_console_cmd_t cmd_rtsp_start = {
        .command = "start",
        .help = "Start RTSP Server/Client\r\n",
        .func = &start,
        .argtable = &rtsp_args,
    };
    esp_console_cmd_register(&cmd_rtsp_start);

    const esp_console_cmd_t cmd_rtsp_stop = {
        .command = "stop",
        .help = "Stop RTSP Server/Client",
        .func = &stop,
    };
    esp_console_cmd_register(&cmd_rtsp_stop);
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_ERROR);
    esp_log_level_set("AFE_VC", ESP_LOG_ERROR);
    AUDIO_MEM_SHOW(TAG);

    /* nvs init */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_LOGI(TAG, "[ 1 ] Initialize peripherals management");
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    setup_wifi(set);

    ESP_LOGI(TAG, "[ 2 ] Initialize av stream");
    av_stream_config_t av_stream_config = {
        .algo_mask = ALGORITHM_STREAM_USE_AEC,
        .acodec_samplerate = AUDIO_CODEC_SAMPLE_RATE,
        .acodec_type = AV_ACODEC_G711A,
        .vcodec_type = AV_VCODEC_MJPEG,
        .hal = {
            .uac_en = false,
            .uvc_en = false,
            .video_soft_enc = false,
            .audio_samplerate = AUDIO_HAL_SAMPLE_RATE,
            .audio_framesize = PCM_FRAME_SIZE,
            .video_framesize = RTSP_FRAME_SIZE,
        },
    };
    av_stream = av_stream_init(&av_stream_config);
    AUDIO_NULL_CHECK(TAG, av_stream, return);

    ESP_LOGI(TAG, "[ 3 ] Initialize console command");
    console_init();
    register_rtsp_cmd();
}
