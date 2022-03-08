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
#include "board.h"
#include "audio_common.h"
#include "fatfs_stream.h"
#include "raw_stream.h"
#include "i2s_stream.h"
#include "esp_audio.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "periph_button.h"
#include "periph_console.h"
#include "esp_decoder.h"
#include "amr_decoder.h"
#include "flac_decoder.h"
#include "ogg_decoder.h"
#include "opus_decoder.h"
#include "mp3_decoder.h"
#include "wav_decoder.h"
#include "aac_decoder.h"
#include "http_stream.h"
#include "wav_encoder.h"
#include "display_service.h"
#include "led_bar_is31x.h"

#include "sdcard_list.h"
#include "sdcard_scan.h"
#include "audio_sys.h"

#include "audio_idf_version.h"

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
#include "esp_netif.h"
#else
#include "tcpip_adapter.h"
#endif

// #define  ESP_AUDIO_AUTO_PLAY

static const char *TAG = "CONSOLE_EXAMPLE";
static esp_audio_handle_t               player;
static esp_periph_set_handle_t          set;
static playlist_operator_handle_t       playlist;
static xTimerHandle                     tone_stop_tm_handle;
static int                              auto_play_type;

static int _http_stream_event_handle(http_stream_event_msg_t *msg)
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

static void sdcard_url_save_cb(void *user_data, char *url)
{
    playlist_operator_handle_t sdcard_handle = (playlist_operator_handle_t)user_data;
    esp_err_t ret = sdcard_list_save(sdcard_handle, url);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Fail to save sdcard url to sdcard playlist");
    }
}

static esp_err_t cli_play(esp_periph_handle_t periph, int argc, char *argv[])
{
    ESP_LOGI(TAG, "app_audio play");
    char *str = NULL;
    int byte_pos = 0;
    if (argv[0] && argc) {
        if (argv[1]) {
            byte_pos = atoi(argv[1]);
        }
        if (isdigit((int)argv[0][0])) {
            int index = atoi(argv[0]);
            if (index >= sdcard_list_get_url_num(playlist)) {
                ESP_LOGE(TAG, "Out of range, index:%d", index);
                return ESP_ERR_INVALID_ARG;
            }
            sdcard_list_choose(playlist, index, &str);
            ESP_LOGI(TAG, "play index= %d, URI:%s, byte_pos:%d", index, str, byte_pos);
        } else {
            ESP_LOGI(TAG, "play index= %d, URI:%s, byte_pos:%d", -1, argv[0], byte_pos);
            str = argv[0];
        }
    } else {
        sdcard_list_current(playlist, &str);
        if (str == NULL) {
            ESP_LOGE(TAG, "No URI to tone play");
            return ESP_ERR_INVALID_ARG;
        }
    }
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, str, byte_pos);
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

static esp_err_t cli_set_speed(esp_periph_handle_t periph, int argc, char *argv[])
{
    esp_audio_play_speed_t cur_audio_play_speed_idx = ESP_AUDIO_PLAY_SPEED_1_00;
    float cur_audio_play_speed = 1.0;
    if (argc == 1) {
        cur_audio_play_speed_idx = atof(argv[0]);
    } else {
        ESP_LOGE(TAG, "Invalid audio parameter");
        return ESP_ERR_INVALID_ARG;
    }
    float prev_audio_play_speed = 1.0;
    esp_audio_play_speed_t prev_audio_play_speed_idx = ESP_AUDIO_PLAY_SPEED_1_00;
    esp_audio_speed_get(player, &prev_audio_play_speed_idx);
    esp_audio_speed_idx_to_float(player, prev_audio_play_speed_idx, &prev_audio_play_speed);
    esp_audio_speed_set(player, cur_audio_play_speed_idx);
    esp_audio_speed_idx_to_float(player, cur_audio_play_speed_idx, &cur_audio_play_speed);
    ESP_LOGI(TAG, "Audio play speed setting changed from %f to %f", prev_audio_play_speed, cur_audio_play_speed);
    return ESP_OK;
}

static esp_err_t cli_get_speed(esp_periph_handle_t periph, int argc, char *argv[])
{
    esp_audio_play_speed_t audio_speed_idx = ESP_AUDIO_PLAY_SPEED_1_00;
    float audio_speed = 1.0;
    esp_audio_speed_get(player, &audio_speed_idx);
    esp_audio_speed_idx_to_float(player, audio_speed_idx, &audio_speed);
    ESP_LOGI(TAG, "Current audio play speed is %f", audio_speed);
    return ESP_OK;
}

static esp_err_t get_pos(esp_periph_handle_t periph, int argc, char *argv[])
{
    int pos = 0;
    esp_audio_time_get(player, &pos);
    ESP_LOGI(TAG, "Current time position is %d ms", pos);
    return ESP_OK;
}

static esp_err_t cli_seek(esp_periph_handle_t periph, int argc, char *argv[])
{
    int pos = 0;
    if (argc == 1) {
        pos = atoi(argv[0]);
    } else {
        ESP_LOGE(TAG, "Invalid parameter");
        return ESP_ERR_INVALID_ARG;
    }
    esp_audio_seek(player, pos);
    ESP_LOGI(TAG, "Seek to %d s to play", pos);
    return ESP_OK;
}

static esp_err_t cli_duration(esp_periph_handle_t periph, int argc, char *argv[])
{
    int t_ms = 0;
    esp_audio_duration_get(player, &t_ms);
    ESP_LOGI(TAG, "The music duration is %d ms", t_ms);
    return ESP_OK;
}

static esp_err_t cli_insert_tone(esp_periph_handle_t periph, int argc, char *argv[])
{
    ESP_LOGI(TAG, "tone play");
    char *str = NULL;
    if (argv[0] && argc) {
        if (isdigit((int)argv[0][0])) {
            int index = atoi(argv[0]);
            if (index >= sdcard_list_get_url_num(playlist)) {
                ESP_LOGE(TAG, "Tone play out of range, index:%d", index);
                return ESP_ERR_INVALID_ARG;
            }
            sdcard_list_choose(playlist, index, &str);
            ESP_LOGI(TAG, "Tone play index= %d, URI:%s", index, str);
        } else {
            ESP_LOGI(TAG, "Tone play index= %d, URI:%s", -1, argv[0]);
            str = argv[0];
        }
    } else {
        ESP_LOGE(TAG, "No URI to tone play");
        return ESP_ERR_INVALID_ARG;
    }
    esp_audio_info_t backup_info = { 0 };
    esp_audio_info_get(player, &backup_info);
    esp_audio_stop(player, TERMINATION_TYPE_NOW);
    esp_audio_media_type_set(player, MEDIA_SRC_TYPE_TONE_SD);
    esp_audio_sync_play(player, str, 0);
    ESP_LOGE(TAG, "Tone play finished\r\n");
    esp_audio_media_type_set(player, MEDIA_SRC_TYPE_MUSIC_SD);
    esp_audio_info_set(player, &backup_info);
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, NULL, backup_info.codec_info.byte_pos);

    return ESP_OK;
}

static void tone_stop_timer_cb(TimerHandle_t xTimer)
{
    ESP_LOGW(TAG, "Tone esp_audio_stop");
    esp_audio_stop(player, TERMINATION_TYPE_NOW);
}

static esp_err_t cli_stop_tone(esp_periph_handle_t periph, int argc, char *argv[])
{
    ESP_LOGI(TAG, "Stop done");
    if (tone_stop_tm_handle == NULL) {
        tone_stop_tm_handle = xTimerCreate("hp_timer0", 3000 / portTICK_RATE_MS, pdFALSE, NULL, tone_stop_timer_cb);
        AUDIO_NULL_CHECK(TAG, tone_stop_tm_handle, return ESP_FAIL;);
    }
    xTimerReset(tone_stop_tm_handle, 1000 / portTICK_RATE_MS);
    return ESP_OK;
}

static esp_err_t wifi_set(esp_periph_handle_t periph, int argc, char *argv[])
{
    wifi_config_t w_config = {0};
    switch (argc) {
        case 2:
            memcpy(w_config.sta.password, argv[1], sizeof(w_config.sta.password));
            FALL_THROUGH;
        case 1:
            memcpy(w_config.sta.ssid, argv[0], sizeof(w_config.sta.ssid));
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

static esp_err_t led(esp_periph_handle_t periph, int argc, char *argv[])
{
    static display_service_handle_t disp_led_serv = NULL;
    if (disp_led_serv == NULL) {
        esp_periph_handle_t led_bar = led_bar_is31x_init();
        if (led_bar == NULL) {
            ESP_LOGE(TAG, "led_bar handle create failed, this command only support lyrat-msc board");
            return ESP_FAIL;
        }
        display_service_config_t display = {
            .based_cfg = {
                .task_stack = 0,
                .task_prio  = 0,
                .task_core  = 0,
                .task_func  = NULL,
                .service_start = NULL,
                .service_stop = NULL,
                .service_destroy = NULL,
                .service_ioctl = led_bar_is31x_pattern,
                .service_name = "DISPLAY_LED_BAR",
                .user_data = NULL,
            },
            .instance = led_bar,
        };
        disp_led_serv = display_service_create(&display);
    }
    int cur_vol = 0;
    if (argc == 1) {
        cur_vol = atoi(argv[0]);
    } else {
        ESP_LOGE(TAG, "Invalid volume parameter, %d", argc);
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Set display pattern %d", cur_vol);
    display_service_set_pattern(disp_led_serv, cur_vol, 0);
    return ESP_OK;
}

static esp_err_t playlist_sd_scan(esp_periph_handle_t periph, int argc, char *argv[])
{
    if (argc == 1) {
        if (isdigit((int)argv[0][0])) {
            ESP_LOGE(TAG, "Invalid scan path parameter");
            return ESP_ERR_INVALID_ARG;
        }
        sdcard_list_reset(playlist);
        sdcard_scan(sdcard_url_save_cb, argv[0],
        0, (const char *[]) {"mp3", "m4a", "flac", "ogg", "opus", "amr", "ts", "aac", "wav"}, 9, playlist);
        sdcard_list_show(playlist);
    } else {
        ESP_LOGE(TAG, "Please enter the can path");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t playlist_sd_show(esp_periph_handle_t periph, int argc, char *argv[])
{
    ESP_LOGI(TAG, "There are %d file can be play", sdcard_list_get_url_num(playlist));
    sdcard_list_show(playlist);
    return ESP_OK;
}

static esp_err_t playlist_sd_next(esp_periph_handle_t periph, int argc, char *argv[])
{
    int step = 0;
    if (argc == 1) {
        step = atoi(argv[0]);
    } else {
        ESP_LOGE(TAG, "Invalid next step parameter");
        return ESP_ERR_INVALID_ARG;
    }
    char *url = NULL;
    sdcard_list_next(playlist, step, &url);
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, url, 0);
    return ESP_OK;
}

static esp_err_t playlist_sd_prev(esp_periph_handle_t periph, int argc, char *argv[])
{
    int step = 0;
    if (argc == 1) {
        step = atoi(argv[0]);
    } else {
        ESP_LOGE(TAG, "Invalid previous step parameter");
        return ESP_ERR_INVALID_ARG;
    }
    char *url = NULL;
    sdcard_list_prev(playlist, step, &url);
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, url, 0);
    return ESP_OK;
}

static esp_err_t playlist_set_mode(esp_periph_handle_t periph, int argc, char *argv[])
{
    if (argc == 1) {
        auto_play_type = atoi(argv[0]);
    } else {
        ESP_LOGE(TAG, "Invalid set playlist mode parameter");
        return ESP_ERR_INVALID_ARG;
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

static esp_err_t task_real_time_states(esp_periph_handle_t periph, int argc, char *argv[])
{
    audio_sys_get_real_time_stats();
    return ESP_OK;
}

const periph_console_cmd_t cli_cmd[] = {
    /* ======================== Esp_audio ======================== */
    {
        .cmd = "play",        .id = 1, .help = "Play music, cmd:\"play [index or url] [byte_pos]\",\n\
        \te.g. 1.\"play\"; 2. play with index after scan,\"play index_number\"; 3.play with specific url, \"play url_path\"",               .func = cli_play
    },

    { .cmd = "pause",       .id = 2,  .help = "Pause",                    .func = cli_pause },
    { .cmd = "resume",      .id = 3,  .help = "Resume",                   .func = cli_resume },
    { .cmd = "stop",        .id = 3,  .help = "Stop player",              .func = cli_stop },
    { .cmd = "setvol",      .id = 4,  .help = "Set volume",               .func = cli_set_vol },
    { .cmd = "getvol",      .id = 5,  .help = "Get volume",               .func = cli_get_vol },
    { .cmd = "getpos",      .id = 6,  .help = "Get position by seconds",  .func = get_pos },
    { .cmd = "seek",        .id = 7,  .help = "Seek position by second",  .func = cli_seek },
    { .cmd = "duration",    .id = 8,  .help = "Get music duration",       .func = cli_duration },
    { .cmd = "tone",        .id = 9,  .help = "Insert tone to play",      .func = cli_insert_tone },
    { .cmd = "stone",       .id = 9,  .help = "Stop tone by a timer",     .func = cli_stop_tone },
    { .cmd = "setspeed",    .id = 10, .help = "Set speed",                .func = cli_set_speed },
    { .cmd = "getspeed",    .id = 11, .help = "Get speed",                .func = cli_get_speed },

    /* ======================== Wi-Fi ======================== */
    { .cmd = "join",        .id = 20, .help = "Join Wi-Fi AP as a station",     .func = wifi_set },
    { .cmd = "wifi",        .id = 21, .help = "Get connected AP information",   .func = wifi_info },

    /* ======================== Led bar ======================== */
    { .cmd = "led",         .id = 50,  .help = "Lyrat-MSC led bar pattern",      .func = led },

    /* ======================== Playlist ======================== */
    { .cmd = "scan",        .id = 40, .help = "Scan sdcard music file, cmd: \"scan [path]\",e.g. \"scan /sdcard\"",             .func = playlist_sd_scan },
    { .cmd = "list",        .id = 41, .help = "Show scanned playlist",                                                          .func = playlist_sd_show },
    { .cmd = "next",        .id = 42, .help = "Next x file to play, cmd: \"next [step]\"",                                      .func = playlist_sd_next },
    { .cmd = "prev",        .id = 43, .help = "Previous x file to play, cmd: \"prev [step]\"",                                  .func = playlist_sd_prev },
    { .cmd = "mode",        .id = 44, .help = "Set auto play mode, cmd:\"mode [value]\", 0:once; others: playlist loop all",    .func = playlist_set_mode },

    /* ======================== System ======================== */
    { .cmd = "reboot",      .id = 30, .help = "Reboot system",                                  .func = sys_reset },
    { .cmd = "free",        .id = 31, .help = "Get system free memory",                         .func = show_free_mem },
#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    { .cmd = "stat",        .id = 32, .help = "Show processor time of all FreeRTOS tasks",      .func = run_time_stats },
#endif
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    { .cmd = "tasks",       .id = 33, .help = "Get information about running tasks",            .func = task_list },
#endif
    { .cmd = "system",      .id = 34, .help = "Get freertos all task states information",       .func = task_real_time_states },
};

static void esp_audio_state_task (void *para)
{
    QueueHandle_t que = (QueueHandle_t) para;
    esp_audio_state_t esp_state = {0};
    while (1) {
        xQueueReceive(que, &esp_state, portMAX_DELAY);
        ESP_LOGI(TAG, "esp_auido status:%x,err:%x\n", esp_state.status, esp_state.err_msg);
        if ((esp_state.status == AUDIO_STATUS_FINISHED)
            || (esp_state.status == AUDIO_STATUS_ERROR)) {
            int time = 0;
            int duration = 0;
            esp_audio_time_get(player, &time);
            esp_audio_duration_get(player, &duration);
            ESP_LOGI(TAG, "[ * ] End of time:%d ms, duration:%d ms", time, duration);
            AUDIO_MEM_SHOW(TAG);
            if (auto_play_type) {
                char *url = NULL;
                if (sdcard_list_next(playlist, 1, &url) == ESP_OK) {
                    ESP_LOGI(TAG, "play index= %d, URI:%s, byte_pos:%d", sdcard_list_get_url_id(playlist), url, 0);
                    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, url, 0);
                }
            }
        }
    }
    vTaskDelete(NULL);
}

static void cli_setup_wifi()
{
    ESP_LOGI(TAG, "Start Wi-Fi");
    periph_wifi_cfg_t wifi_cfg = {
        .disable_auto_reconnect = true,
        .ssid = "",
        .password = "",
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
}

static void cli_setup_console()
{
    periph_console_cfg_t console_cfg = {
        .command_num = sizeof(cli_cmd) / sizeof(periph_console_cmd_t),
        .commands = cli_cmd,
        .buffer_size = 384,
    };
    esp_periph_handle_t console_handle = periph_console_init(&console_cfg);
    esp_periph_start(set, console_handle);
}

// Example of initializing esp_audio as an audio player -- START
static void cli_setup_player(void)
{
    if (player ) {
        return ;
    } 
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    audio_board_handle_t board_handle = audio_board_init();
    cfg.vol_handle = board_handle->audio_hal;
    cfg.vol_set = (audio_volume_set)audio_hal_set_volume;
    cfg.vol_get = (audio_volume_get)audio_hal_get_volume;
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.resample_rate = 48000;
    cfg.evt_que = xQueueCreate(3, sizeof(esp_audio_state_t));
    player = esp_audio_create(&cfg);
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    xTaskCreate(esp_audio_state_task, "player_task", 4096, cfg.evt_que, 1, NULL);

    // Create readers and add to esp_audio
    fatfs_stream_cfg_t fs_reader = FATFS_STREAM_CFG_DEFAULT();
    fs_reader.type = AUDIO_STREAM_READER;
    i2s_stream_cfg_t i2s_reader = I2S_STREAM_CFG_DEFAULT();
    i2s_reader.i2s_config.sample_rate = 48000;
    i2s_reader.type = AUDIO_STREAM_READER;
    raw_stream_cfg_t raw_reader = RAW_STREAM_CFG_DEFAULT();
    raw_reader.type = AUDIO_STREAM_READER;

    esp_audio_input_stream_add(player, raw_stream_init(&raw_reader));
    esp_audio_input_stream_add(player, fatfs_stream_init(&fs_reader));
    esp_audio_input_stream_add(player, i2s_stream_init(&i2s_reader));
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(player, http_stream_reader);

    // Add decoders and encoders to esp_audio
#ifdef ESP_AUDIO_AUTO_PLAY
    audio_decoder_t auto_decode[] = {
        DEFAULT_ESP_AMRNB_DECODER_CONFIG(),
        DEFAULT_ESP_AMRWB_DECODER_CONFIG(),
        DEFAULT_ESP_FLAC_DECODER_CONFIG(),
        DEFAULT_ESP_OGG_DECODER_CONFIG(),
        DEFAULT_ESP_OPUS_DECODER_CONFIG(),
        DEFAULT_ESP_MP3_DECODER_CONFIG(),
        DEFAULT_ESP_WAV_DECODER_CONFIG(),
        DEFAULT_ESP_AAC_DECODER_CONFIG(),
        DEFAULT_ESP_M4A_DECODER_CONFIG(),
        DEFAULT_ESP_TS_DECODER_CONFIG(),
    };
    esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, esp_decoder_init(&auto_dec_cfg, auto_decode, 10));
#else
    amr_decoder_cfg_t  amr_dec_cfg  = DEFAULT_AMR_DECODER_CONFIG();
    flac_decoder_cfg_t flac_dec_cfg = DEFAULT_FLAC_DECODER_CONFIG();
    ogg_decoder_cfg_t  ogg_dec_cfg  = DEFAULT_OGG_DECODER_CONFIG();
    opus_decoder_cfg_t opus_dec_cfg = DEFAULT_OPUS_DECODER_CONFIG();
    mp3_decoder_cfg_t  mp3_dec_cfg  = DEFAULT_MP3_DECODER_CONFIG();
    wav_decoder_cfg_t  wav_dec_cfg  = DEFAULT_WAV_DECODER_CONFIG();
    aac_decoder_cfg_t  aac_dec_cfg  = DEFAULT_AAC_DECODER_CONFIG();

    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, amr_decoder_init(&amr_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, flac_decoder_init(&flac_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, ogg_decoder_init(&ogg_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, decoder_opus_init(&opus_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_decoder_init(&mp3_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, wav_decoder_init(&wav_dec_cfg));
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, aac_decoder_init(&aac_dec_cfg));
    audio_element_handle_t m4a_dec_cfg = aac_decoder_init(&aac_dec_cfg);
    audio_element_set_tag(m4a_dec_cfg, "m4a");
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, m4a_dec_cfg);

    audio_element_handle_t ts_dec_cfg = aac_decoder_init(&aac_dec_cfg);
    audio_element_set_tag(ts_dec_cfg, "ts");
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, ts_dec_cfg);

    wav_encoder_cfg_t wav_enc_cfg = DEFAULT_WAV_ENCODER_CONFIG();
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_ENCODER, wav_encoder_init(&wav_enc_cfg));
#endif

    // Create writers and add to esp_audio
    fatfs_stream_cfg_t fs_writer = FATFS_STREAM_CFG_DEFAULT();
    fs_writer.type = AUDIO_STREAM_WRITER;

    i2s_stream_cfg_t i2s_writer = I2S_STREAM_CFG_DEFAULT();
    i2s_writer.i2s_config.sample_rate = 48000;
    i2s_writer.i2s_config.mode = I2S_MODE_MASTER | I2S_MODE_TX;
    i2s_writer.type = AUDIO_STREAM_WRITER;

    raw_stream_cfg_t raw_writer = RAW_STREAM_CFG_DEFAULT();
    raw_writer.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s_h =  i2s_stream_init(&i2s_writer);

    esp_audio_output_stream_add(player, i2s_h);
    esp_audio_output_stream_add(player, fatfs_stream_init(&fs_writer));
    esp_audio_output_stream_add(player, raw_stream_init(&raw_writer));

    // Set default volume
    esp_audio_vol_set(player, 35);
    AUDIO_MEM_SHOW(TAG);
    ESP_LOGI(TAG, "esp_audio instance is:%p\r\n", player);
}
// Example of initializing esp_audio as an audio player -- END

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);

    ESP_ERROR_CHECK(nvs_flash_init());
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0))
    ESP_ERROR_CHECK(esp_netif_init());
#else
    tcpip_adapter_init();
#endif
    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);

    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    cli_setup_wifi();
    cli_setup_player();

    cli_setup_console();
    sdcard_list_create(&playlist);
}
