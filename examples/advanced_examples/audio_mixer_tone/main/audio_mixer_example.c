/*   multi-channel mixing example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_netif.h"

#include "audio_mem.h"
#include "audio_thread.h"
#include "audio_element.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "periph_console.h"
#include "audio_sys.h"
#include "i2s_stream.h"
#include "raw_stream.h"
#include "fatfs_stream.h"
#include "http_stream.h"
#include "board.h"
#include "esp_decoder.h"
#include "esp_audio.h"

#include "audio_mixer.h"
#include "audio_mixer_pipeline.h"

#if __has_include("audio_tone_uri.h")
#include "audio_tone_uri.h"
#else
#error "please refer the README, and then make the tone file"
#endif

static const char *TAG = "example.mixer";

#define SAMPLERATE           (48000)
#define CHANNELS             (2)
#define BITS                 (16)
#define IN_SOURCE_FILE_NUM   (5)
#define TRANSMIT_TIME_MS     (500)
#define RECORD_EVENT_BIT     BIT0
#define AUDIO_PLAY_START_BIT BIT1
#define AUDIO_PLAY_STOP_BIT  BIT2

#define DEFAULT_SOURCE_GAIN_DB (-6)

static audio_element_handle_t  raw_writer_h;
static esp_audio_handle_t      player;
static esp_periph_set_handle_t set;
static void                   *audio_mixer_handle;
static audio_hal_handle_t      audio_hal;
static EventGroupHandle_t      record_event_group;
static EventGroupHandle_t      audio_event_group;
static ringbuf_handle_t        record_rb;
static void                   *mixer;
static int                     audio_player_slot = 1;


struct mixer_pipe_s {
    mixer_pipepine_handle_t mixer_pipe;
    int                     slot;
    char                    url[256];
    int                     used;
    float                   src_gain;
};

static struct mixer_pipe_s mixer_pipes[IN_SOURCE_FILE_NUM];

static void mixer_event_callback(mixer_pip_event_t event, mixer_pipepine_handle_t mix_pip_handle, int slot, void *user_data)
{
    ESP_LOGI(TAG, "mix_pip_handle: %p, event: %d, slot: %d", mix_pip_handle, event, slot);
}

static esp_err_t cli_play_mixer(esp_periph_handle_t periph, int argc, char *argv[])
{
    if (argc < 2) {
        ESP_LOGE(TAG, "Usage: pmixer <uri> <slot>");
        return ESP_FAIL;
    }
    int slot = atoi(argv[1]);
    if (slot <= 0 || slot > IN_SOURCE_FILE_NUM) {
        ESP_LOGE(TAG, "Slot must be in [1, %d], beacuse slot 0 occupied by record", IN_SOURCE_FILE_NUM - 1);
        return ESP_FAIL;
    }
    if (mixer_pipes[slot].used == 1) {
        ESP_LOGE(TAG, "Slot %d is already occupied", slot);
        return ESP_FAIL;
    }
    mixer_pipes[slot].used = 1;
    memset(&mixer_pipes[slot].url[0], 0, sizeof(mixer_pipes[slot].url));
    memcpy(&mixer_pipes[slot].url, argv[0], strlen(argv[0]));

    audio_mixer_pip_cfg_t pip_config = AUDIO_MIXER_PIPELINE_CFG_DEFAULT();
    pip_config.mixer = audio_mixer_handle;
    pip_config.url = argv[0];
    pip_config.slot = slot;
    pip_config.resample_rate = SAMPLERATE;
    pip_config.resample_channel = CHANNELS;
    mixer_pipeline_create(&pip_config, &mixer_pipes[slot].mixer_pipe);
    mixer_pipeline_regiser_event_callback(mixer_pipes[slot].mixer_pipe, mixer_event_callback, NULL);
    mixer_pipepine_run(mixer_pipes[slot].mixer_pipe);
    return ESP_OK;
}

static esp_err_t cli_replay_mixer(esp_periph_handle_t periph, int argc, char *argv[])
{
    if (argc < 1) {
        ESP_LOGE(TAG, "Usage: rmixer <slot>");
        return ESP_FAIL;
    }
    int slot = atoi(argv[0]);
    if (slot < 0 || slot > IN_SOURCE_FILE_NUM) {
        ESP_LOGE(TAG, "Slot must be in [1, %d]", IN_SOURCE_FILE_NUM);
        return ESP_FAIL;
    }
    if (mixer_pipes[slot].used == 0) {
        ESP_LOGE(TAG, "Please use cli_play_mixer to play first");
        return ESP_FAIL;
    }

    mixer_pipepine_restart_with_uri(mixer_pipes[slot].mixer_pipe, mixer_pipes[slot].url);
    return ESP_OK;
}

static esp_err_t cli_stop_mixer(esp_periph_handle_t periph, int argc, char *argv[])
{
    if (argc < 1) {
        ESP_LOGE(TAG, "Usage: stop_mixer paramsters");
        return ESP_FAIL;
    }
    mixer_pipepine_stop(mixer_pipes[atoi(argv[0])].mixer_pipe);
    return ESP_OK;
}

static esp_err_t cli_destory_mixer(esp_periph_handle_t periph, int argc, char *argv[])
{
    if (argc < 1) {
        ESP_LOGE(TAG, "Usage: stop_mixer paramsters");
        return ESP_FAIL;
    }
    mixer_pipes[atoi(argv[0])].used = 0;
    mixer_pipeline_destroy(mixer_pipes[atoi(argv[0])].mixer_pipe);
    mixer_pipes[atoi(argv[0])].mixer_pipe = NULL;
    return ESP_OK;
}

static esp_err_t cli_mixer_gain_set(esp_periph_handle_t periph, int argc, char *argv[])
{
    if (argc < 2) {
        ESP_LOGE(TAG, "Usage: mixer gain <slot> <destination  gain>");
        return ESP_FAIL;
    }
    int slot = atoi(argv[0]);
    if (slot < 0 || slot > IN_SOURCE_FILE_NUM) {
        ESP_LOGE(TAG, "Slot must be in [0, %d]", IN_SOURCE_FILE_NUM);
        return ESP_FAIL;
    }
    char *endptr = NULL;
    float dst_gain = strtof((argv[1]), &endptr);
    if (*endptr == '\0') {
        if (dst_gain > 0) {
            ESP_LOGW(TAG, "Suggest setting a gain value not greater than 0");
        }
        audio_mixer_set_gain(mixer, slot, DEFAULT_SOURCE_GAIN_DB, dst_gain);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Invlaid gain destition parameter");
    return ESP_FAIL;
}


static esp_err_t recorder_fn(esp_periph_handle_t periph, int argc, char *argv[])
{
    if (argc < 1) {
        ESP_LOGE(TAG, "Usage:  1 is start, 0 is stop, please input paramester");
        return ESP_FAIL;
    }
    int r = atoi(argv[0]);
    if (r == 1) {
        ESP_LOGI(TAG, "Start record");
        audio_mixer_reset_slot(mixer, 0);
        xEventGroupSetBits(record_event_group, RECORD_EVENT_BIT);
    } else {
        ESP_LOGI(TAG, "Stop record");
        xEventGroupClearBits(record_event_group, RECORD_EVENT_BIT);
    }
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
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
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

static esp_err_t cli_set_vol(esp_periph_handle_t periph, int argc, char *argv[])
{
    int cur_vol = 0;
    if (argc == 1) {
        cur_vol = atoi(argv[0]);
    } else {
        ESP_LOGE(TAG, "Invalid volume parameter");
        return ESP_ERR_INVALID_ARG;
    }
    audio_hal_set_volume(audio_hal, cur_vol);
    return ESP_OK;
}

static esp_err_t cli_stop(esp_periph_handle_t periph, int argc, char *argv[])
{
    esp_audio_stop(player, 0);
    ESP_LOGI(TAG, "app_audio stop");
    return ESP_OK;
}

static esp_err_t cli_play(esp_periph_handle_t periph, int argc, char *argv[])
{
    ESP_LOGI(TAG, "app_audio play");
    int byte_pos = 0;
    if (argc < 2) {
        ESP_LOGE(TAG, "Invalid play parameter");
        return ESP_ERR_INVALID_ARG;
    }
    char *url = argv[0];
    audio_player_slot = atoi(argv[1]);
    if (audio_player_slot <= 0 || audio_player_slot > IN_SOURCE_FILE_NUM) {
        ESP_LOGE(TAG, "Slot must be in [1, %d], beacuse slot 0 occupied by record", IN_SOURCE_FILE_NUM - 1);
        return ESP_ERR_INVALID_ARG;
    }
    mixer_pipes[audio_player_slot].used = true;
    if (argv[2]) {
        byte_pos = atoi(argv[2]);
    }
    // check url format
    if (strcmp(url + strlen(url) - strlen("#raw"), "#raw") != 0) {
        ESP_LOGE(TAG, "Invalid url format, the suffix needs to have '#raw'.  Example: file://sdcard/test.mp3#raw");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "url: %s, slot: %d, pos: %d", url, audio_player_slot, byte_pos);
    esp_audio_play(player, AUDIO_CODEC_TYPE_DECODER, url, byte_pos);
    return ESP_OK;
}

const periph_console_cmd_t cli_cmd[] = {
    /* ======================== Mixer Pipiline ======================== */
    { .cmd = "play",    .id = 1, .help = "Play music, cmd:\"play [url] [slot] [byte_pos]\",\n\
        \te.g. 1.\"play url slot\"; 2. play with ipostion,\"play url slot postition\"\n\tNote: The '#raw' suffix need to be add to the actual backend mahine after the URL\n"
        "\t\t play file://sdcard/test.mp3#raw 1\n\t\t play file://sdcard/test.mp3#raw 1 20\n",               .func = cli_play
    },
    { .cmd = "stop",    .id = 2,  .help = "Stop player",                                                 .func = cli_stop },


    { .cmd = "pmixer",  .id = 10, .help = "Play audio on one slot. It can play multiple channels simultaneously using different slots.\n" \
    "    example: pmixer /sdcard/test.mp3 1\n             pmixer flash://tone/1_wozai.mp3 2\n             pmixer https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3 3", .func = cli_play_mixer },
    { .cmd = "rpmixer", .id = 11, .help = "Replays the audio from pmixer. Ensures that pmixer is in a stopped or finished state before replaying.\n    example: rpmixer 1\n", .func = cli_replay_mixer },
    { .cmd = "smixer",  .id = 12, .help = "Stop pmixer, temporarily halting audio playback.\n    example: smixer 1\n", .func = cli_stop_mixer },
    { .cmd = "dmixer",  .id = 13, .help = "Destroys pmixer, allowing the freed slots to be used by other channels after destruction.\n    example: dmixer 1\n", .func = cli_destory_mixer },
    { .cmd = "gmixer",  .id = 14, .help = "Set the gain value for the output of the corresponding slot.\n    example: gmixer 1 -3\\n", .func = cli_mixer_gain_set },

    { .cmd = "setvol",  .id = 15, .help = "Set volume", .func = cli_set_vol },
    { .cmd = "record",  .id = 16, .help = "Start or stop recording. 1 is start, 0 is stop\n    example: record 1\n", .func = recorder_fn },

    /* ======================== Wi-Fi ======================== */
    { .cmd = "join",   .id = 17, .help = "Join Wi-Fi AP as a station", .func = wifi_set },
    { .cmd = "wifi",   .id = 18, .help = "Get connected AP information", .func = wifi_info },

    /* ======================== System ======================== */
    { .cmd = "free",   .id = 20, .help = "Get system free memory", .func = show_free_mem },
#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    { .cmd = "stat",   .id = 21, .help = "Show processor time of all FreeRTOS tasks", .func = run_time_stats },
#endif
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    { .cmd = "tasks",  .id = 22, .help = "Get information about running tasks", .func = task_list },
#endif
    { .cmd = "system", .id = 23, .help = "Get freertos all task states information", .func = task_real_time_states },
};

static void cli_setup_wifi()
{
    ESP_LOGI(TAG, "Start Wi-Fi");
    periph_wifi_cfg_t wifi_cfg = {
        .disable_auto_reconnect = true,
        .wifi_config.sta.ssid = "",
        .wifi_config.sta.password = "",
    };
    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);
    esp_periph_start(set, wifi_handle);
}

static esp_err_t play_read_callback(uint8_t *data, int len, void *ctx)
{
    return raw_stream_read(raw_writer_h, (char *)data, len);
}

static void aduio_play_read_task(void *pv)
{
    char *buffer = audio_calloc(1, 2048);
    assert(buffer);
    while (1) {
        EventBits_t uxBits = xEventGroupWaitBits(audio_event_group, AUDIO_PLAY_START_BIT | AUDIO_PLAY_STOP_BIT, true, pdFALSE, portMAX_DELAY);
        if (uxBits & AUDIO_PLAY_START_BIT) {
            audio_mixer_reset_slot(mixer, audio_player_slot);
            audio_mixer_data_is_ready(mixer);
            audio_mixer_set_read_cb(mixer, audio_player_slot, play_read_callback, NULL);
            mixer_pipes[audio_player_slot].used = true;
        } else if (uxBits & AUDIO_PLAY_STOP_BIT) {
            audio_mixer_set_read_cb(mixer, audio_player_slot, NULL, NULL);
            mixer_pipes[audio_player_slot].used = false;
        }
    }
    vTaskDelete(NULL);
}

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

static void esp_audio_state_task (void *para)
{
    QueueHandle_t que = (QueueHandle_t) para;
    esp_audio_state_t esp_state = {0};
    while (1) {
        xQueueReceive(que, &esp_state, portMAX_DELAY);
        ESP_LOGI(TAG, "esp_auido status:%x,err:%x\n", esp_state.status, esp_state.err_msg);
        if ((esp_state.status == AUDIO_STATUS_FINISHED)
            || (esp_state.status == AUDIO_STATUS_ERROR)
            || (esp_state.status == AUDIO_STATUS_STOPPED)) {
            xEventGroupSetBits(audio_event_group, AUDIO_PLAY_STOP_BIT);
        } else if (esp_state.status == AUDIO_STATUS_RUNNING) {
            xEventGroupSetBits(audio_event_group, AUDIO_PLAY_START_BIT);
        }
    }
    vTaskDelete(NULL);
}

static void cli_setup_player(void)
{
    if (player) {
        return ;
    }
    esp_audio_cfg_t cfg = DEFAULT_ESP_AUDIO_CONFIG();
    cfg.prefer_type = ESP_AUDIO_PREFER_MEM;
    cfg.resample_rate = SAMPLERATE;
    cfg.evt_que = xQueueCreate(3, sizeof(esp_audio_state_t));
    player = esp_audio_create(&cfg);
    xTaskCreate(esp_audio_state_task, "player_task", 4096, cfg.evt_que, 1, NULL);

    // Create readers and add to esp_audio
    fatfs_stream_cfg_t fs_reader = FATFS_STREAM_CFG_DEFAULT();
    fs_reader.type = AUDIO_STREAM_READER;
    esp_audio_input_stream_add(player, fatfs_stream_init(&fs_reader));
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = _http_stream_event_handle;
    http_cfg.type = AUDIO_STREAM_READER;
    http_cfg.enable_playlist_parser = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    esp_audio_input_stream_add(player, http_stream_reader);

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
    audio_element_handle_t mp3_el = esp_decoder_init(&auto_dec_cfg, auto_decode, 10);
    esp_audio_codec_lib_add(player, AUDIO_CODEC_TYPE_DECODER, mp3_el);

    // Create writers and add to esp_audio
    raw_stream_cfg_t raw_writer = RAW_STREAM_CFG_DEFAULT();
    raw_writer.type = AUDIO_STREAM_WRITER;
    raw_writer_h = raw_stream_init(&raw_writer);
    esp_audio_output_stream_add(player, raw_writer_h);

    audio_event_group = xEventGroupCreate();
    assert(audio_event_group);
    xTaskCreate(aduio_play_read_task, "aduio_play_read_task", 4096, NULL, 5, NULL);
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

static esp_err_t write_to_i2s(uint8_t *data, int len, void *ctx)
{
    if (len == 0) {
        ESP_LOGW(TAG, "No mixer data output");
        return 0;
    }
    int ret = 0;
    ret = audio_element_output((audio_element_handle_t)ctx, (char *)data, len);
    return ret;
}

static void record_task(void *arg)
{
    audio_element_handle_t i2s_reader = (audio_element_handle_t)arg;
    char *buffer = (char *)audio_malloc(2048);
    while (1) {
        xEventGroupWaitBits(record_event_group, RECORD_EVENT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        int r_len = audio_element_input(i2s_reader, buffer, 100);
        rb_write(record_rb, buffer, r_len, 1000);
        audio_mixer_data_is_ready(mixer);
    }
}

static esp_err_t i2s_read_callback(uint8_t *data, int len, void *ctx)
{
    return rb_read(record_rb, (char *)data, len, 0);
}

static void record_setup()
{
    record_event_group = xEventGroupCreate();
    assert(record_event_group);
#define DEAFULT_I2S_RECORD_BUFFER_SZ (2048)
    record_rb = rb_create(DEAFULT_I2S_RECORD_BUFFER_SZ, 1);
    assert(record_rb);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.task_stack = -1;
    audio_element_handle_t i2s_reader = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(i2s_reader, SAMPLERATE, BITS, CHANNELS);

    audio_thread_create(NULL, "record_task", record_task, (void *)i2s_reader, 4096, 10, true, 1);
    // Slote 0 will be occupied
    mixer_pipes[0].used = true;
    audio_mixer_set_read_cb(mixer, AUDIO_MIXER_SLOT_0, i2s_read_callback, NULL);
}

static void audio_mixer_new(void **handle)
{
    // output stream
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.task_stack = -1;
    audio_element_handle_t i2s_writer = i2s_stream_init(&i2s_cfg);
    i2s_stream_set_clk(i2s_writer, SAMPLERATE, BITS, CHANNELS);

    audio_mixer_slot_info_t in_source[IN_SOURCE_FILE_NUM] = {
        AUDIO_MIXER_DEFAULT_CHANNEL_INFO_CONF(DEFAULT_SOURCE_GAIN_DB, DEFAULT_SOURCE_GAIN_DB, TRANSMIT_TIME_MS),
        AUDIO_MIXER_DEFAULT_CHANNEL_INFO_CONF(DEFAULT_SOURCE_GAIN_DB, DEFAULT_SOURCE_GAIN_DB, TRANSMIT_TIME_MS),
        AUDIO_MIXER_DEFAULT_CHANNEL_INFO_CONF(DEFAULT_SOURCE_GAIN_DB, DEFAULT_SOURCE_GAIN_DB, TRANSMIT_TIME_MS),
        AUDIO_MIXER_DEFAULT_CHANNEL_INFO_CONF(DEFAULT_SOURCE_GAIN_DB, DEFAULT_SOURCE_GAIN_DB, TRANSMIT_TIME_MS),
        AUDIO_MIXER_DEFAULT_CHANNEL_INFO_CONF(DEFAULT_SOURCE_GAIN_DB, DEFAULT_SOURCE_GAIN_DB, TRANSMIT_TIME_MS),
    };

    audio_mixer_config_t config = AUDIO_MIXER_DEFAULT_CONF(SAMPLERATE, CHANNELS, BITS, IN_SOURCE_FILE_NUM);
    audio_mixer_init(&config, &mixer);
    audio_mixer_configure_in(mixer, in_source, sizeof(in_source) / sizeof(audio_mixer_slot_info_t));
    audio_mixer_configure_out(mixer, write_to_i2s, i2s_writer);
    *handle = mixer;
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("AUDIO_ELEMENT", ESP_LOG_DEBUG);
    esp_log_level_set("AUDIO_PIPELINE", ESP_LOG_WARN);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    set = esp_periph_set_init(&periph_cfg);

    audio_board_handle_t board_handle = audio_board_init();
    audio_hal = board_handle->audio_hal;
    audio_hal_ctrl_codec(audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_board_sdcard_init(set, SD_MODE_1_LINE);
    audio_mixer_new(&audio_mixer_handle);
    cli_setup_wifi();
    cli_setup_player();
    cli_setup_console();
    record_setup();

}
