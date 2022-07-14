/*  AV muxer module to record video and audio data and mux into media file

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_camera.h"
#include "audio_element.h"
#include "audio_common.h"
#include "i2s_stream.h"
#include "board.h"
#include "data_q.h"
#include "media_lib_os.h"
#include "esp_muxer.h"
#include "mp4_muxer.h"
#include "flv_muxer.h"
#include "ts_muxer.h"
#include "av_muxer.h"

#define CAM_PIN_PWDN  -1 // Power down is not used
#define CAM_PIN_RESET -1 // Software reset will be performed

// When the camera is not enabled, use invalid pin setting
#if !defined(FUNC_LCD_SCREEN_EN) && !defined(FUNC_CAMERA_EN)
#define CAM_PIN_XCLK  -1
#define CAM_PIN_SIOD  -1
#define CAM_PIN_SIOC  -1
#define CAM_PIN_D7    -1
#define CAM_PIN_D6    -1
#define CAM_PIN_D5    -1
#define CAM_PIN_D4    -1
#define CAM_PIN_D3    -1
#define CAM_PIN_D2    -1
#define CAM_PIN_D1    -1
#define CAM_PIN_D0    -1
#define CAM_PIN_VSYNC -1
#define CAM_PIN_HREF  -1
#define CAM_PIN_PCLK  -1
#endif

#define VIDEO_FPS            (15)
#define AUDIO_FRAME_DURATION (16)
// Add buffer queue so that when writing SDCARD takes a long time, it does not block fetch data from camera and I2S
#define RECORD_Q_BUFFER_SIZE (500 * 1024)

#define LOG_ON_ERR(ret, fmt, ...)        \
    if (ret != 0) {                      \
        ESP_LOGE(TAG, fmt, __VA_ARGS__); \
    }

typedef struct {
    bool is_video;
} write_q_t;

typedef union {
    ts_muxer_config_t  ts_cfg;
    flv_muxer_config_t flv_cfg;
    mp4_muxer_config_t mp4_cfg;
} muxer_cfg_t;

static const char* TAG = "AV Muxer";

static int video_frame = 0;
static int audio_frame = 0;
static audio_element_handle_t i2s_reader;
static ringbuf_handle_t pcm_buffer;
static esp_muxer_handle_t muxer = NULL;
static data_q_t* stream_buffer_q;
static av_muxer_cfg_t av_muxer_cfg;
static esp_muxer_type_t muxer_type;
static audio_board_handle_t audio_board;
static bool write_running;
static bool record_running;
static bool muxer_running;
static uint32_t record_duration;
static int audio_stream_idx;
static int video_stream_idx;

static struct timeval start_tv = {0, 0};

static uint32_t get_cur_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (start_tv.tv_sec == 0) {
        start_tv = tv;
    }
    return (uint32_t) ((tv.tv_sec - start_tv.tv_sec) * 1000 + (tv.tv_usec - start_tv.tv_usec) / 1000);
}

static void get_video_size(muxer_video_quality_t quality, uint16_t* width, uint16_t* height)
{
    switch (quality) {
        case MUXER_VIDEO_QUALITY_QVGA:
        default:
            *width = 320;
            *height = 240;
            break;
        case MUXER_VIDEO_QUALITY_HVGA:
            *width = 480;
            *height = 320;
            break;
        case MUXER_VIDEO_QUALITY_VGA:
            *width = 640;
            *height = 480;
            break;
        case MUXER_VIDEO_QUALITY_XVGA:
            *width = 1024;
            *height = 768;
            break;
        case MUXER_VIDEO_QUALITY_HD:
            *width = 1280;
            *height = 720;
            break;
    }
}

framesize_t get_video_quality(muxer_video_quality_t quality)
{
    switch (quality) {
        case MUXER_VIDEO_QUALITY_QVGA:
        default:
            return FRAMESIZE_QVGA;
        case MUXER_VIDEO_QUALITY_HVGA:
            return FRAMESIZE_HVGA;
        case MUXER_VIDEO_QUALITY_VGA:
            return FRAMESIZE_VGA;
        case MUXER_VIDEO_QUALITY_XVGA:
            return FRAMESIZE_XGA;
        case MUXER_VIDEO_QUALITY_HD:
            return FRAMESIZE_HD;
    }
}

static esp_media_err_t muxer_register()
{
    esp_media_err_t ret = 0;
    ret = ts_muxer_register();
    LOG_ON_ERR(ret, "Register ts muxer fail %d\n", ret);
    ret |= mp4_muxer_register();
    LOG_ON_ERR(ret, "Register mp4 muxer fail %d\n", ret);
    ret |= flv_muxer_register();
    LOG_ON_ERR(ret, "Register flv muxer fail %d\n", ret);
    return ret;
}

static uint32_t get_video_pts()
{
    return video_frame * 1000 / VIDEO_FPS;
}

static uint32_t get_audio_pts()
{
    return (uint32_t) ((uint64_t) audio_frame * 1000 / av_muxer_cfg.audio_sample_rate);
}

static uint32_t get_audio_frame_size()
{
    return (AUDIO_FRAME_DURATION * 4 * av_muxer_cfg.audio_sample_rate / 1000) / 4 * 4;
}

static esp_media_err_t muxer_video(void* data, int size)
{
    uint32_t video_pts = get_video_pts();
    esp_media_err_t ret;
    esp_muxer_video_packet_t video_packet = {
        .data = data,
        .len = size,
        .pts = video_pts,
        .dts = 0,
    };
    ret = esp_muxer_add_video_packet(muxer, video_stream_idx, &video_packet);
    video_frame ++;
    return ret;
}

static esp_media_err_t muxer_audio(void* data, int size)
{
    uint32_t audio_pts = get_audio_pts();
    esp_media_err_t ret;
    esp_muxer_audio_packet_t audio_packet = {
        .data = data,
        .len = size,
        .pts = audio_pts,
    };
    ret = esp_muxer_add_audio_packet(muxer, audio_stream_idx, &audio_packet);
    audio_frame += size / 4;
    return ret;
}

static int start_video_recorder()
{
    camera_config_t camera_config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sscb_sda = CAM_PIN_SIOD,
        .pin_sscb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        // XCLK 20 MHz or 10 MHz for OV2640 double FPS (Experimental)
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG, // YUV422, GRAYSCALE, RGB565, JPEG
        .jpeg_quality = 6,              // Within the range of 0-63, a lower number means higher quality
        .fb_count = 2,                  // If more than one, i2s runs in continuous mode. Use only with JPEG
        .grab_mode = CAMERA_GRAB_LATEST,
    };
    if (CAM_PIN_XCLK == -1) {
        ESP_LOGE(TAG, "Camera pin not configured on this platform yet");
        return ESP_FAIL;
    }
    camera_config.frame_size = get_video_quality(av_muxer_cfg.quality);
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }
    return ESP_OK;
}

static int stop_video_recorder()
{
    return esp_camera_deinit();
}

static int start_audio_recorder()
{
    audio_board = audio_board_init();
    if (audio_board == NULL) {
        ESP_LOGE(TAG, "Fail to init audio board");
        return -1;
    }
    audio_hal_ctrl_codec(audio_board->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);
    audio_hal_set_volume(audio_board->audio_hal, 0);
    i2s_stream_cfg_t i2s_file_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_file_cfg.type = AUDIO_STREAM_READER;
    i2s_file_cfg.i2s_config.sample_rate = av_muxer_cfg.audio_sample_rate;
    i2s_reader = i2s_stream_init(&i2s_file_cfg);
    if (i2s_reader == NULL) {
        ESP_LOGE(TAG, "Fail to init i2s stream");
        return -1;
    }
    i2s_zero_dma_buffer(i2s_file_cfg.i2s_port);
    uint32_t size = audio_element_get_output_ringbuf_size(i2s_reader);
    pcm_buffer = rb_create(size, 1);
    if (pcm_buffer == NULL) {
        ESP_LOGE(TAG, "Fail to create ringfifo size %d", size);
        return -1;
    }
    audio_element_set_output_ringbuf(i2s_reader, pcm_buffer);
    audio_element_run(i2s_reader);
    audio_element_resume(i2s_reader, 0, 0);
    return 0;
}

static int stop_audio_recorder()
{
    if (i2s_reader) {
        audio_element_stop(i2s_reader);
        audio_element_deinit(i2s_reader);
        i2s_reader = NULL;
    }
    if (pcm_buffer) {
        rb_destroy(pcm_buffer);
        pcm_buffer = NULL;
    }
    if (audio_board) {
        audio_board_deinit(audio_board);
        audio_board = NULL;
    }
    return 0;
}

static void* get_q_data(bool is_video, uint32_t size)
{
    void* buffer = data_q_get_buffer(stream_buffer_q, sizeof(write_q_t) + size);
    if (buffer) {
        write_q_t* q = (write_q_t*) buffer;
        q->is_video = is_video;
        return (uint8_t*) buffer + sizeof(write_q_t);
    }
    return NULL;
}

static int send_q_data(uint32_t size)
{
    return data_q_send_buffer(stream_buffer_q, sizeof(write_q_t) + size);
}

static void* read_q_data(bool* is_video, int* size)
{
    void* buffer = NULL;
    data_q_read_lock(stream_buffer_q, &buffer, size);
    if (buffer) {
        write_q_t* q = (write_q_t*) buffer;
        *is_video = q->is_video;
        *size -= sizeof(write_q_t);
        return (uint8_t*) buffer + sizeof(write_q_t);
    }
    return NULL;
}

static void read_q_release(int size)
{
    data_q_read_unlock(stream_buffer_q);
}

static void write_thread(void* arg)
{
    while (muxer_running) {
        int size = 0;
        bool is_video = 0;
        void* buffer = read_q_data(&is_video, &size);
        if (buffer == NULL) {
            break;
        }
        if (size > 0) {
            esp_media_err_t ret = ESP_MEDIA_ERR_OK;
            if (is_video) {
                if (video_stream_idx >= 0) {
                    ret = muxer_video(buffer, size);
                }
            } else {
                if (audio_stream_idx >= 0) {
                    ret = muxer_audio(buffer, size);
                }
            }
            if (ret != ESP_MEDIA_ERR_OK) {
                ESP_LOGE(TAG, "Fail to add data for %s", is_video ? "video" : "audio");
                break;
            }
        }
        read_q_release(size);
    }
    write_running = 0;
    ESP_LOGI(TAG, "Muxer write quited");
    media_lib_thread_destroy(NULL);
}

static void record_thread(void* arg)
{
    uint32_t audio_frame_size = get_audio_frame_size();
    uint32_t video_start = get_cur_time();
    uint32_t video_frame_duration = 1000 / VIDEO_FPS;
    int drop_cnt = 0;
    while (muxer_running) {
        if (record_duration != ESP_MUXER_MAX_SLICE_DURATION && av_muxer_get_pts() >= record_duration) {
            ESP_LOGI(TAG, "Recording duration reached");
            break;
        }
        if (av_muxer_cfg.muxer_flag & AV_MUXER_AUDIO_FLAG) {
            while (muxer_running) {
                void* buffer = get_q_data(0, audio_frame_size);
                int readed = rb_read(pcm_buffer, buffer, audio_frame_size, 0);
                if (readed <= 0) {
                    send_q_data(0);
                    break;
                }
                send_q_data(readed);
            }
        }
        if ((av_muxer_cfg.muxer_flag & AV_MUXER_VIDEO_FLAG) == 0) {
            media_lib_thread_sleep(10);
            continue;
        }
        uint32_t real_time = get_cur_time() - video_start;
        if (av_muxer_cfg.muxer_flag & AV_MUXER_AUDIO_FLAG) {
            if (real_time > get_audio_pts()) {
                real_time = get_audio_pts();
            }
        }
        // video drop data according real time or audio pts
        if (get_video_pts() >= real_time + video_frame_duration * 2) {
            camera_fb_t* pic = esp_camera_fb_get();
            esp_camera_fb_return(pic);
            drop_cnt++;
            continue;
        }
        camera_fb_t* pic = esp_camera_fb_get();
        if (pic) {
            void* buffer = get_q_data(1, pic->len);
            if (buffer) {
                memcpy(buffer, pic->buf, pic->len);
                send_q_data(pic->len);
            }
            esp_camera_fb_return(pic);
            int q_num = 0, q_size = 0;
            data_q_query(stream_buffer_q, &q_num, &q_size);
            if ((video_frame % VIDEO_FPS) == 0) {
                ESP_LOGI(TAG, "Pic %zu vpts:%d apts:%d real:%d %d/%d drop:%d", pic->len, get_video_pts(),
                         get_audio_pts(), real_time, q_num, q_size, drop_cnt);
                drop_cnt = 0;
            }
        }
    }
    record_running = false;
    ESP_LOGI(TAG, "Record Finished");
    media_lib_thread_destroy(NULL);
}

static int _muxer_data_reached(esp_muxer_data_info_t* data, void* ctx)
{
    if (av_muxer_cfg.data_cb) {
        av_muxer_cfg.data_cb(data->data, data->size, av_muxer_cfg.ctx);
    }
    return 0;
}

static int av_muxer_url_pattern(char* file_name, int len, int slice_idx)
{
    switch (muxer_type) {
        case ESP_MUXER_TYPE_TS:
            snprintf(file_name, len, "/sdcard/slice_%d.ts", slice_idx);
            break;
        case ESP_MUXER_TYPE_MP4:
            snprintf(file_name, len, "/sdcard/slice_%d.mp4", slice_idx);
            break;
        case ESP_MUXER_TYPE_FLV:
            snprintf(file_name, len, "/sdcard/slice_%d.flv", slice_idx);
            break;
        default:
            return -1;
    }
    return 0;
}

int av_muxer_start(av_muxer_cfg_t* cfg)
{
    muxer_cfg_t muxer_cfg = {0};
    esp_muxer_config_t* base_cfg = &muxer_cfg.ts_cfg.base_config;
    base_cfg->data_cb = _muxer_data_reached;
    if ((cfg->data_cb == NULL && cfg->save_to_file == false) || cfg->muxer_flag == 0) {
        ESP_LOGE(TAG, "Bad muxer configuration, filepath or data callback need set");
        return -1;
    }
    base_cfg->url_pattern = NULL;
    if (cfg->save_to_file) {
        base_cfg->url_pattern = av_muxer_url_pattern;
    }
    int cfg_size = 0;
    base_cfg->slice_duration = ESP_MUXER_MAX_SLICE_DURATION;
    record_duration = ESP_MUXER_MAX_SLICE_DURATION;
    if (strcmp(cfg->fmt, "ts") == 0) {
        muxer_type = ESP_MUXER_TYPE_TS;
        cfg_size = sizeof(ts_muxer_config_t);
    } else if (strcmp(cfg->fmt, "flv") == 0) {
        muxer_type = ESP_MUXER_TYPE_FLV;
        cfg_size = sizeof(flv_muxer_config_t);
    } else if (strcmp(cfg->fmt, "mp4") == 0) {
        muxer_type = ESP_MUXER_TYPE_MP4;
        base_cfg->slice_duration = 60000;
        cfg_size = sizeof(mp4_muxer_config_t);
        base_cfg->data_cb = NULL;
    } else {
        ESP_LOGE(TAG, "Not supported muxer format %s", cfg->fmt);
        return -1;
    }
    base_cfg->muxer_type = muxer_type;
    av_muxer_cfg = *cfg;
    cfg = &av_muxer_cfg;
    do {
        esp_media_err_t ret = muxer_register();
        LOG_ON_ERR(ret, "Register muxer fail ret %d\n", ret);
        if (ret != ESP_MEDIA_ERR_OK) {
            break;
        }
        ESP_LOGI(TAG, "Start to open mux\n");
        muxer = esp_muxer_open(base_cfg, cfg_size);
        if (muxer == NULL) {
            break;
        }
        if (cfg->muxer_flag & AV_MUXER_VIDEO_FLAG) {
            esp_muxer_video_stream_info_t video_info = {
                .codec = ESP_MUXER_VDEC_MJPEG,
                .codec_spec_info = NULL,
                .spec_info_len = 0,
                .fps = VIDEO_FPS,
                .min_packet_duration = 30,
            };
            get_video_size(cfg->quality, &video_info.width, &video_info.height);
            ESP_LOGI(TAG, "Got video width:%d height:%d", video_info.width, video_info.height);
            video_stream_idx = -1;
            ret = esp_muxer_add_video_stream(muxer, &video_info, &video_stream_idx);
            if (ret != ESP_MEDIA_ERR_OK) {
                ESP_LOGE(TAG, "Fail to add video stream %d", ret);
                cfg->muxer_flag &= ~AV_MUXER_VIDEO_FLAG;
            }
        }
        if (cfg->muxer_flag & AV_MUXER_AUDIO_FLAG) {
            esp_muxer_audio_stream_info_t audio_info = {
                .bits_per_sample = 16,
                .channel = 2,
                .sample_rate = cfg->audio_sample_rate,
                .codec = ESP_MUXER_ADEC_PCM,
                .min_packet_duration = 16,
            };
            audio_stream_idx = -1;
            ret = esp_muxer_add_audio_stream(muxer, &audio_info, &audio_stream_idx);
            if (ret != ESP_MEDIA_ERR_OK) {
                ESP_LOGE(TAG, "Fail to add audio stream %d", ret);
                cfg->muxer_flag &= ~AV_MUXER_AUDIO_FLAG;
            }
        }
        if (audio_stream_idx < 0 && video_stream_idx < 0) {
            break;
        }
        stream_buffer_q = data_q_init(RECORD_Q_BUFFER_SIZE);
        muxer_running = true;
        video_frame = audio_frame = 0;
        if (cfg->muxer_flag & AV_MUXER_VIDEO_FLAG) {
            ret = start_video_recorder();
            if (ret != 0) {
                break;
            }
        }
        if (cfg->muxer_flag & AV_MUXER_AUDIO_FLAG) {
            ret = start_audio_recorder();
            if (ret != 0) {
                break;
            }
        }
        write_running = true;
        media_lib_thread_handle_t thread_handle;
        if (media_lib_thread_create(&thread_handle, "writer", write_thread, NULL, 3 * 1024, 20, 0) != ESP_OK) {
            write_running = false;
            break;
        }
        record_running = true;
        if (media_lib_thread_create(&thread_handle, "record", record_thread, NULL, 3 * 1024, 20, 0) != ESP_OK) {
            record_running = false;
            break;
        }
        ESP_LOGI(TAG, "Start record return %d", ret);
        return 0;
    } while (0);
    av_muxer_stop();
    return -1;
}

int av_muxer_stop()
{
    muxer_running = false;
    ESP_LOGI(TAG, "Stopping muxer process");
    if (stream_buffer_q) {
        data_q_deinit(stream_buffer_q);
        stream_buffer_q = NULL;
        ESP_LOGI(TAG, "data q deinit done\n");
    }
    while (write_running || record_running) {
        media_lib_thread_sleep(10);
    }
    ESP_LOGI(TAG, "Wait write and record exit done\n");
    if (muxer) {
        esp_muxer_close(muxer);
        muxer = NULL;
    }
    stop_audio_recorder();
    stop_video_recorder();
    ESP_LOGI(TAG, "Stop muxer done");
    return 0;
}

bool av_muxer_running()
{
    return (write_running && record_running);
}

uint32_t av_muxer_get_pts()
{
    uint32_t pts = get_audio_pts();
    uint32_t video_pts = get_video_pts();
    if (video_pts > pts) {
        pts = video_pts;
    }
    return video_pts;
}
