/*  Console example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "audio_element.h"
#include "audio_mem.h"
#include "audio_hal.h"
#include "audio_common.h"

#include "fatfs_stream.h"
#include "raw_stream.h"
#include "i2s_stream.h"
#include "wav_decoder.h"
#include "wav_encoder.h"
#include "mp3_decoder.h"
#include "http_stream.h"

#include "esp_audio.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "periph_button.h"
#include "periph_console.h"
#include "aac_decoder.h"

static const char *TAG = "CONSOLE_EXAMPLE";
static esp_audio_handle_t player;

int _http_stream_event_handle(http_stream_event_msg_t *msg)
{
    if (msg->event_id == HTTP_STREAM_RESOLVE_ALL_TRACKS) {
        return ESP_OK;
    }

    if (msg->event_id == HTTP_STREAM_FINISH_TRACK) {
        return http_stream_next_track(msg->el);
    }
    if (msg->event_id == HTTP_STREAM_FINISH_PLAYLIST) {
        return http_stream_restart(msg->el);
    }
    return ESP_OK;
}

static esp_err_t cli_play(esp_periph_handle_t periph, int argc, char *argv[])
{
    ESP_LOGI(TAG, "app_audio play");
    const char *uri[] = {
        "file://sdcard/test.mp3",
        "file://sdcard/test.wav",
        "file://sdcard/test.aac",
        "file://sdcard/test.m4a",
        "file://sdcard/test.ts",
        "http://dl.espressif.com/dl/audio/adf_music.mp3",
    };

    char *str = NULL;
    if (argv[0] && argc) {
        if (isdigit((int)argv[0][0])) {
            int index = atoi(argv[0]);
            if (index >= sizeof(uri) / sizeof(char *)) {
                ESP_LOGE(TAG, "Out of range, index:%d", index);
                return ESP_ERR_INVALID_ARG;
            }
            str = (char *)uri[index];
            ESP_LOGI(TAG, "play index= %d, URI:%s", index, str);
        } else {
            ESP_LOGI(TAG, "play URI:%s", argv[0]);
            str = argv[0];
        }
    }
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, str, 0);
    return ESP_OK;
}

static esp_err_t cli_pause(esp_periph_handle_t periph, int argc, char *argv[])
{
    esp_audio_pause(player);
    ESP_LOGI(TAG, "app_audio paused");
    return ESP_OK;
}

static esp_err_t cli_resume(esp_periph_handle_t periph, int argc, char *argv[])
{
    esp_audio_resume(player);
    ESP_LOGI(TAG, "app_audio resume");
    return ESP_OK;
}

static esp_err_t cli_stop(esp_periph_handle_t periph, int argc, char *argv[])
{
    esp_audio_stop(player, 0);
    ESP_LOGI(TAG, "app_audio stop");
    return ESP_OK;
}

static esp_err_t cli_set_vol(esp_periph_handle_t periph, int argc, char *argv[])
{
    int cur_vol = 0;
    if (argc == 1) {
        cur_vol = atoi(argv[0]);
    } else {
        ESP_LOGE(TAG, "Invalid volume parameter");
        return ESP_ERR_INVALID_ARG;
    }
    int prev_vol = 0;
    esp_audio_vol_get(player, &prev_vol);
    esp_audio_vol_set(player, cur_vol);
    ESP_LOGI(TAG, "Volume setting changed from %d to %d", prev_vol, cur_vol);
    return ESP_OK;
}

static esp_err_t cli_get_vol(esp_periph_handle_t periph, int argc, char *argv[])
{
    int vol = 0;
    esp_audio_vol_get(player, &vol);
    ESP_LOGI(TAG, "Current volume is %d", vol);
    return ESP_OK;
}

static esp_err_t get_pos(esp_periph_handle_t periph, int argc, char *argv[])
{
    int pos = 0;
    esp_audio_time_get(player, &pos);
    ESP_LOGI(TAG, "Current time position is %d second", pos / 1000);
    return ESP_OK;
}

static esp_err_t wifi_set(esp_periph_handle_t periph, int argc, char *argv[])
{
    wifi_config_t w_config = {0};
    switch (argc) {
        case 2:
            memcpy(w_config.sta.password, argv[1], strlen(argv[1]));
        case 1:
            memcpy(w_config.sta.ssid, argv[0], strlen(argv[0]));
            esp_wifi_disconnect();
            ESP_LOGI(TAG, "Connecting Wi-Fi, SSID:\"%s\" PASSWORD:\"%s\"", w_config.sta.ssid, w_config.sta.password);
            esp_wifi_set_config(WIFI_IF_STA, &w_config);
            esp_wifi_connect();
            break;
        default:
            ESP_LOGE(TAG, "Invalid SSID or PASSWORD");
            return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t wifi_info(esp_periph_handle_t periph, int argc, char *argv[])
{
    wifi_config_t w_config = {0};
    wifi_ap_record_t ap_info = {0};
    esp_err_t ret  = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_ERR_WIFI_NOT_CONNECT) {
        ESP_LOGI(TAG, "Not connected Wi-Fi");
        return ESP_OK;
    }
    if (ESP_OK == esp_wifi_get_config(WIFI_IF_STA, &w_config)) {
        ESP_LOGI(TAG, "Connected Wi-Fi, SSID:\"%s\"", w_config.sta.ssid);
    } else {
        ESP_LOGE(TAG, "esp_wifi_get_config failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t sys_reset(esp_periph_handle_t periph, int argc, char *argv[])
{
    esp_restart();
    return ESP_OK;
}

static esp_err_t show_free_mem(esp_periph_handle_t periph, int argc, char *argv[])
{
    AUDIO_MEM_SHOW(TAG);
    return ESP_OK;
}

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
static esp_err_t run_time_stats(esp_periph_handle_t periph, int argc, char *argv[])
{
    static char buf[1024];
    vTaskGetRunTimeStats(buf);
    printf("Run Time Stats:\nTask Name    Time    Percent\n%s\n", buf);
    return ESP_OK;
}
#endif

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static esp_err_t task_list(esp_periph_handle_t periph, int argc, char *argv[])
{
    static char buf[1024];
    vTaskList(buf);
    printf("Task List:\nTask Name    Status   Prio    HWM    Task Number\n%s\n", buf);
    return ESP_OK;
}
#endif

const periph_console_cmd_t cli_cmd[] = {
    /* ======================== Esp_audio ======================== */
    { .cmd = "play",        .id = 1, .help = "Play music",               .func = cli_play },
    { .cmd = "pause",       .id = 2, .help = "Pause",                    .func = cli_pause },
    { .cmd = "resume",      .id = 3, .help = "Resume",                   .func = cli_resume },
    { .cmd = "stop",        .id = 3, .help = "Stop player",              .func = cli_stop },
    { .cmd = "setvol",      .id = 4, .help = "Set volume",               .func = cli_set_vol },
    { .cmd = "getvol",      .id = 5, .help = "Get volume",               .func = cli_get_vol },
    { .cmd = "getpos",      .id = 6, .help = "Get position by seconds",  .func = get_pos },

    /* ======================== Wi-Fi ======================== */
    { .cmd = "join",        .id = 20, .help = "Join WiFi AP as a station",      .func = wifi_set },
    { .cmd = "wifi",        .id = 21, .help = "Get connected AP information",   .func = wifi_info },

    /* ======================== System ======================== */
    { .cmd = "reboot",      .id = 30, .help = "Reboot system",            .func = sys_reset },
    { .cmd = "free",        .id = 31, .help = "Get system free memory",   .func = show_free_mem },
#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    { .cmd = "stat",        .id = 32, .help = "Show processor time of all FreeRTOS tasks",      .func = run_time_stats },
#endif
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    { .cmd = "tasks",       .id = 33, .help = "Get information about running tasks",            .func = task_list },
#endif
};

static void esp_audio_state_task (void *para)
{
    QueueHandle_t que = (QueueHandle_t) para;
    esp_audio_state_t esp_state = {0};
    while (1) {
        xQueueReceive(que, &esp_state, portMAX_DELAY);
        ESP_LOGI(TAG, "esp_auido status:%x,err:%x\n", esp_state.status, esp_state.err_msg);
    }
    vTaskDelete(NULL);
}

static void cli_setup_wifi()
{
    ESP_LOGI(TAG, "Start Wi-Fi");
    esp_periph_config_t periph_cfg = { 0 };
    esp_periph_init(&periph_cfg);
    periph_wifi_cfg_t wifi_cfg = {
        .disable_auto_reconnect = true,
        .ssid = "",
        .password = "",
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(wifi_handle);
}

static void cli_setup_sdcard()
{
    ESP_LOGI(TAG, "Start SdCard");
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = SD_CARD_INTR_GPIO, // GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    esp_periph_start(sdcard_handle);

    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "sdcard mounting...");
    }
}

static void cli_setup_console()
{
    periph_console_cfg_t console_cfg = {
        .command_num = sizeof(cli_cmd) / sizeof(periph_console_cmd_t),
        .commands = cli_cmd,
    };
    esp_periph_handle_t console_handle = periph_console_init(&console_cfg);
    esp_periph_start(console_handle);
}

static void cli_setup_player(void)
{
    if (player ) {
        return ;
    }
    esp_audio_cfg_t cfg = {
        .in_stream_buf_size = 10 * 1024,
        .out_stream_buf_size = 6 * 1024,
        .evt_que = NULL,
        .resample_rate = 0,
        .hal = NULL,
    };
    audio_hal_codec_config_t audio_hal_codec_cfg =  AUDIO_HAL_ES8388_DEFAULT();
    cfg.hal = audio_hal_init(&audio_hal_codec_cfg, BOARD);
    cfg.evt_que = xQueueCreate(3, sizeof(esp_audio_state_t));
    audio_hal_ctrl_codec(cfg.hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
    player = esp_audio_create(&cfg);
    xTaskCreate(esp_audio_state_task, "player_task", 4096, cfg.evt_que, 1, NULL);

    // Create readers and add to esp_audio
    fatfs_stream_cfg_t fs_reader = FATFS_STREAM_CFG_DEFAULT();
    fs_reader.type = AUDIO_STREAM_READER;
    i2s_stream_cfg_t i2s_reader = I2S_STREAM_CFG_DEFAULT();
    i2s_reader.type = AUDIO_STREAM_READER;
    raw_stream_cfg_t raw_reader = {
        .type = AUDIO_STREAM_READER,
        //.out_rb_size = 20*1024;
    };

    esp_audio_input_stream_add(player, raw_stream_init(&raw_reader));
    esp_audio_input_stream_add(player, fatfs_stream_init(&fs_reader));
    esp_audio_input_stream_add(player, i2s_stream_init(&i2s_reader));
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(player, http_stream_reader);

    // Create writers and add to esp_audio
    fatfs_stream_cfg_t fs_writer = FATFS_STREAM_CFG_DEFAULT();
    fs_writer.type = AUDIO_STREAM_WRITER;

    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.type = AUDIO_STREAM_WRITER;

    raw_stream_cfg_t raw_writer = {
        .type = AUDIO_STREAM_READER,
    };
    esp_audio_output_stream_add(player, i2s_stream_init(&i2s_writer));
    esp_audio_output_stream_add(player, fatfs_stream_init(&fs_writer));
    esp_audio_output_stream_add(player, raw_stream_init(&raw_writer));

    // Add decoders and encoders to esp_audio
    wav_decoder_cfg_t wav_dec_cfg = DEFAULT_WAV_DECODER_CONFIG();
    mp3_decoder_cfg_t mp3_dec_cfg = DEFAULT_MP3_DECODER_CONFIG();
    aac_decoder_cfg_t aac_dec_cfg = DEFAULT_AAC_DECODER_CONFIG();
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, wav_decoder_init(&wav_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, aac_decoder_init(&aac_dec_cfg));
    audio_element_handle_t m4a_dec_cfg = aac_decoder_init(&aac_dec_cfg);
    audio_element_set_tag(m4a_dec_cfg, "m4a");
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, m4a_dec_cfg);

    audio_element_handle_t ts_dec_cfg = aac_decoder_init(&aac_dec_cfg);
    audio_element_set_tag(ts_dec_cfg, "ts");
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, ts_dec_cfg);

    wav_encoder_cfg_t wav_enc_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_ENCODER, wav_encoder_init(&wav_enc_cfg));

    // Set default volume
    esp_audio_vol_set(player, 45);
    AUDIO_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "esp_audio instance is:%p\r\n", player);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_ERROR_CHECK(nvs_flash_init());
    tcpip_adapter_init();
    esp_periph_config_t periph_cfg = { 0 };
    esp_periph_init(&periph_cfg);

    cli_setup_sdcard();
    cli_setup_wifi();
    cli_setup_player();

    cli_setup_console();
}
