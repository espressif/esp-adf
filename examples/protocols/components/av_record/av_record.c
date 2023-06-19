/*  AV Record module to record video and audio data

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <esp_timer.h>
#include "esp_log.h"
#include "data_queue.h"
#include "media_lib_os.h"
#include "media_lib_err.h"
#include "av_record.h"
#include "esp_jpeg_enc.h"
#include "esp_g711.h"
#include "esp_aac_enc.h"
#include "esp_idf_version.h"
#include "esp_h264_enc.h"

#define AUDIO_FRAME_DURATION          (16)
// Add buffer queue so that when writing takes a long time, it does not block fetch data from camera and I2S
#define RECORD_Q_BUFFER_SIZE          (200 * 1024)
#define RECORD_AV_MAX_LATENCY         (1000) // unit ms
#define VIDEO_ENCODE_MAX_FRAME_SIZE   (80 * 1024)
#define VIDEO_RESYNC_FRAME_TOLERANCE  (4)
#define TAG                         "AV Record"

#define LOG_ON_ERR(ret, fmt, ...)        \
    if (ret != 0) {                      \
        ESP_LOGE(TAG, fmt, __VA_ARGS__); \
    }

typedef struct {
    bool     is_video;
    uint32_t pts;
} write_q_t;

typedef struct {
    av_record_cfg_t     record_cfg;
    bool                encode_video;
    void               *video_enc;
    void               *aud_enc;
    data_queue_t       *stream_buffer_q;
    bool                write_running;
    bool                audio_recording;
    bool                video_recording;
    bool                video_reached;
    bool                audio_reached;
    bool                stopping;
    record_src_handle_t video_src_handle;
    record_src_handle_t audio_src_handle;
    uint32_t            audio_frames;
    uint32_t            video_frames;
    uint32_t            last_video_pts;
} av_record_t;

static av_record_t av_record;

static uint32_t get_cur_time()
{
    return (uint32_t) (esp_timer_get_time() / 1000);
}

void av_record_get_video_size(av_record_video_quality_t quality, uint16_t *width, uint16_t *height)
{
    switch (quality) {
        case AV_RECORD_VIDEO_QUALITY_QVGA:
        default:
            *width = 320;
            *height = 240;
            break;
        case AV_RECORD_VIDEO_QUALITY_HVGA:
            *width = 480;
            *height = 320;
            break;
        case AV_RECORD_VIDEO_QUALITY_VGA:
            *width = 640;
            *height = 480;
            break;
        case AV_RECORD_VIDEO_QUALITY_800X480:
            *width = 800;
            *height = 480;
            break;
        case AV_RECORD_VIDEO_QUALITY_XVGA:
            *width = 1024;
            *height = 768;
            break;
        case AV_RECORD_VIDEO_QUALITY_HD:
            *width = 1280;
            *height = 720;
            break;
        case AV_RECORD_VIDEO_QUALITY_FHD:
            *width = 1920;
            *height = 1080;
            break;
    }
}

static uint8_t get_audio_sample_size()
{
    return (av_record.record_cfg.audio_channel * 2);
}

static uint32_t get_audio_frame_size(uint32_t duration)
{
    uint32_t sample_size = get_audio_sample_size();
    // align to sample size
    return (duration * sample_size * av_record.record_cfg.audio_sample_rate / 1000) / sample_size * sample_size;
}

static uint32_t get_audio_pts()
{
    if (av_record.record_cfg.audio_sample_rate == 0) {
        return 0;
    }
    return (uint32_t) ((uint64_t) av_record.audio_frames * 1000 / av_record.record_cfg.audio_sample_rate);
}

static uint32_t get_video_pts()
{
    if (av_record.record_cfg.video_fps == 0) {
        return 0;
    }
    return (uint32_t) ((uint64_t) av_record.video_frames * 1000 / av_record.record_cfg.video_fps);
}

static bool start_frame_synced()
{
    if (av_record.audio_recording && av_record.video_recording) {
        return av_record.audio_reached && av_record.video_reached;
    }
    return true;
}

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0))

static int start_video_encoder()
{
    uint16_t width = 0, height = 0;
    av_record_get_video_size(av_record.record_cfg.video_quality, &width, &height);
    if (av_record.record_cfg.video_fmt == AV_RECORD_VIDEO_FMT_MJPEG) {
        jpeg_enc_info_t info = DEFAULT_JPEG_ENC_CONFIG();
        info.width = width;
        info.height = height;
        info.src_type = JPEG_RAW_TYPE_YCbYCr;
        info.subsampling = JPEG_SUB_SAMPLE_YUV420;
        info.quality = 40;
        av_record.video_enc = jpeg_enc_open(&info);
        if (av_record.video_enc == NULL) {
            ESP_LOGE(TAG, "Fail to create jpeg encoder");
            return ESP_MEDIA_ERR_NOT_SUPPORT;
        }
    } else if (av_record.record_cfg.video_fmt == AV_RECORD_VIDEO_FMT_H264) {
        esp_h264_enc_cfg_t cfg = {
            .pic_type = ESP_H264_RAW_FMT_YUV422,
            .width = width,
            .height = height,
            .fps = 30,
            .gop_size = av_record.record_cfg.video_fps * 3,
            .target_bitrate = 400000,
        };
        int ret = esp_h264_enc_open(&cfg, (esp_h264_enc_t*)&av_record.video_enc);
        if (av_record.video_enc == NULL) {
            ESP_LOGE(TAG, "Fail to create h264 encoder %d", ret);
            return ESP_MEDIA_ERR_NOT_SUPPORT;
        }
    }
    return 0;
}

static int video_encoder_process(uint8_t *raw, int raw_size, uint8_t *pic, int pic_limit, int *pic_size, uint32_t *pts)
{
    int ret = -1;
    if (av_record.video_enc) {
        switch (av_record.record_cfg.video_fmt) {
            case AV_RECORD_VIDEO_FMT_MJPEG:
            return jpeg_enc_process(av_record.video_enc, raw, raw_size, pic, pic_limit, pic_size);

            case AV_RECORD_VIDEO_FMT_H264: {
                esp_h264_enc_frame_t out_frame = { 0 };
                esp_h264_raw_frame_t in_frame = {
                    .pts = *pts,
                    .raw_data.buffer = raw,
                    .raw_data.len = raw_size,
                };
                ret = esp_h264_enc_process((esp_h264_enc_t)av_record.video_enc, &in_frame, &out_frame);
                if (ret == 0) {
                    *pic_size = 0;
                    if (out_frame.layer_num && out_frame.layer_data) {
                        int total_size = 0, i;
                        for (i = 0; i < out_frame.layer_num; i++) {
                            total_size += out_frame.layer_data[i].len;
                        }
                        *pts = out_frame.pts;
                        if (total_size > pic_limit) {
                            ESP_LOGI(TAG, "Frame size exceed limit %d > %d", total_size, pic_limit);
                        } else {
                            int fill_size = 0;
                            for (i = 0; i < out_frame.layer_num; i++) {
                                memcpy(pic + fill_size, out_frame.layer_data[i].buffer, out_frame.layer_data[i].len);
                                fill_size += out_frame.layer_data[i].len;
                            }
                            *pic_size = fill_size;
                        }
                        return 0;
                    }
                }
                return ret;
            }
            default:
            break;            
        }
    }
    return ret;
}

static void stop_video_encoder()
{
    if (av_record.video_enc) {
        switch (av_record.record_cfg.video_fmt) {
            case AV_RECORD_VIDEO_FMT_MJPEG:
            jpeg_enc_close(av_record.video_enc);
            break;
            case AV_RECORD_VIDEO_FMT_H264:
            esp_h264_enc_close((esp_h264_enc_t)av_record.video_enc);
            break;
            default:
            break;            
        }
        av_record.video_enc = NULL;
    }
}

#else

// JPEG encoder library only support on new IDF 4.4
static int start_video_encoder()
{
    ESP_LOGE(TAG, "Please update IDF to v4.4 or higher version");
    return -1;
}

static int video_encoder_process(uint8_t *raw, int raw_size, uint8_t *pic, int pic_limit, int *pic_size, uint32_t* pts)
{
    return -1;
}

static void stop_video_encoder()
{
}
#endif

static int start_video_recorder()
{
    av_record_cfg_t *cfg = &av_record.record_cfg;
    av_record.encode_video = false;
    if (cfg->video_fmt != AV_RECORD_VIDEO_FMT_MJPEG || cfg->sw_jpeg_enc) {
        av_record.encode_video = true;
    }
    record_src_video_cfg_t video_cfg = {
        .fps = cfg->video_fps,
        .video_fmt = av_record.encode_video ? RECORD_SRC_VIDEO_FMT_YUV422 : RECORD_SRC_VIDEO_FMT_JPEG,
    };
    video_cfg.reuse_i2c = (av_record.record_cfg.audio_fmt != AV_RECORD_AUDIO_FMT_NONE);
    av_record_get_video_size(cfg->video_quality, &video_cfg.width, &video_cfg.height);
    av_record.video_src_handle =
        record_src_open(av_record.record_cfg.video_src, &video_cfg, sizeof(record_src_video_cfg_t));
    if (av_record.video_src_handle == NULL) {
        ESP_LOGE(TAG, "Fail to open video source");
        return ESP_MEDIA_ERR_NOT_SUPPORT;
    }
    int ret = 0;
    if (av_record.encode_video) {
        ret = start_video_encoder();
    }
    if (ret != 0) {
        record_src_close(av_record.video_src_handle);
        av_record.video_src_handle = NULL;
        return ret;
    }
    return 0;
}

static void stop_video_recorder()
{
    stop_video_encoder();
    if (av_record.video_src_handle) {
        record_src_close(av_record.video_src_handle);
        av_record.video_src_handle = NULL;
    }
}

static int start_audio_encoder()
{
    int ret = 0;
    switch (av_record.record_cfg.audio_fmt) {
        case AV_RECORD_AUDIO_FMT_AAC: {
            esp_aac_enc_config_t aac_cfg = {
                .sample_rate = av_record.record_cfg.audio_sample_rate,
                .channel = av_record.record_cfg.audio_channel,
                .bit = 16,
                .bit_rate = 80000,
                .adts_used = 1,
            };
            ret = esp_aac_enc_open(&aac_cfg, &av_record.aud_enc);
            break;
        }
        default:
            break;
    }
    return ret;
}

static void stop_audio_encoder()
{
    switch (av_record.record_cfg.audio_fmt) {
        case AV_RECORD_AUDIO_FMT_AAC: {
            esp_aac_enc_close(av_record.aud_enc);
            break;
        }
        default:
            break;
    }
}

static void stop_audio_recorder()
{
    if (av_record.audio_src_handle) {
        record_src_close(av_record.audio_src_handle);
        av_record.audio_src_handle = NULL;
    }
    stop_audio_encoder();
}

static int start_audio_recorder()
{
    if (av_record.record_cfg.audio_sample_rate == 0 || av_record.record_cfg.audio_channel == 0) {
        return ESP_MEDIA_ERR_INVALID_ARG;
    }
    record_src_audio_cfg_t aud_cfg = {
        .sample_rate = av_record.record_cfg.audio_sample_rate,
        .channel = av_record.record_cfg.audio_channel,
        .bits_per_sample = 16,
    };
    av_record.audio_src_handle =
        record_src_open(av_record.record_cfg.audio_src, &aud_cfg, sizeof(record_src_audio_cfg_t));
    if (av_record.audio_src_handle == NULL) {
        return ESP_MEDIA_ERR_NOT_SUPPORT;
    }
    if (start_audio_encoder() != 0) {
        stop_audio_recorder();
        return ESP_MEDIA_ERR_NOT_SUPPORT;
    }
    return ESP_MEDIA_ERR_OK;
}

static void *get_q_data(uint32_t size)
{
    void *buffer = data_queue_get_buffer(av_record.stream_buffer_q, sizeof(write_q_t) + size);
    if (buffer) {
        return (uint8_t *) buffer + sizeof(write_q_t);
    }
    return NULL;
}

static int send_q_data(uint32_t size)
{
    if (size) {
        size += sizeof(write_q_t);
    }
    return data_queue_send_buffer(av_record.stream_buffer_q, size);
}

static void fill_q_header(void *data, uint32_t size, bool is_video, uint32_t pts)
{
    write_q_t *q = (write_q_t *) (data - sizeof(write_q_t));
    q->is_video = is_video;
    q->pts = pts;
}

static void *read_q_data(write_q_t **h, int *size)
{
    void *buffer = NULL;
    data_queue_read_lock(av_record.stream_buffer_q, &buffer, size);
    if (buffer) {
        write_q_t *q = (write_q_t *) buffer;
        *h = q;
        *size -= sizeof(write_q_t);
        return (uint8_t *) buffer + sizeof(write_q_t);
    }
    return NULL;
}

static void read_q_release(int size)
{
    data_queue_read_unlock(av_record.stream_buffer_q);
}

static void drop_all_data()
{
    int q_num = 0, q_size = 0;
    data_queue_query(av_record.stream_buffer_q, &q_num, &q_size);
    ESP_LOGI(TAG, "Drop for latency high q: %d", q_num);
    while (q_num-- > 0) {
        int size = 0;
        write_q_t *h = NULL;
        void *buffer = read_q_data(&h, &size);
        if (buffer == NULL) {
            break;
        }
        read_q_release(size);
    }
}

static bool check_latency(uint32_t cur_pts, bool drop)
{
    if (av_record_get_pts() > cur_pts + RECORD_AV_MAX_LATENCY) {
        drop = true;
    }
    return drop;
}

static void write_thread(void *arg)
{
    uint32_t write_time = 0;
    uint32_t write_size = 0;
    while (!av_record.stopping) {
        int size = 0;
        write_q_t *h = NULL;
        void *buffer = read_q_data(&h, &size);
        if (buffer == NULL) {
            break;
        }
        if (size > 0) {
            if (check_latency(h->pts, false)) {
                read_q_release(size);
                continue;
            }
            av_record_data_t record_data = {
                .data = buffer,
                .size = size,
                .pts = h->pts,
            };
            if (h->is_video) {
                record_data.stream_type = AV_RECORD_STREAM_TYPE_VIDEO;
            } else {
                record_data.stream_type = AV_RECORD_STREAM_TYPE_AUDIO;
            }
            if (av_record.record_cfg.data_cb) {
                uint32_t start_time = get_cur_time();
                int ret = av_record.record_cfg.data_cb(&record_data, av_record.record_cfg.ctx);
                write_time += get_cur_time() - start_time;
                write_size += size;
                if (write_time >= 2000) {
                    ESP_LOGD(TAG, "Upload speed %d\n", (int)(write_size * 1000 / write_time));
                    write_time = write_size = 0;
                }
                if (ret != 0) {
                    ESP_LOGE(TAG, "Fail to do data_cb ret %d", ret);
                    read_q_release(size);
                    drop_all_data();
                    break;
                }
            }
        }
        read_q_release(size);
    }
    av_record.write_running = 0;
    ESP_LOGI(TAG, "Write thread quited");
    media_lib_thread_destroy(NULL);
}

static void audio_record_encode_get_frame_size(uint32_t *frame_size, uint32_t *out_size)
{
    if (av_record.record_cfg.audio_fmt == AV_RECORD_AUDIO_FMT_AAC) {
        esp_aac_enc_info_t aac_info = {0};
        esp_aac_enc_get_info(av_record.aud_enc, &aac_info);
        *frame_size = aac_info.inbuf_size;
        *out_size = aac_info.outbuf_size;
    }
}

static int audio_record_encode_data(uint8_t *src, int size, uint8_t *dst, int dst_size)
{
    if (av_record.record_cfg.audio_fmt == AV_RECORD_AUDIO_FMT_AAC) {
        int ret = esp_aac_enc_process(av_record.aud_enc, src, size, dst, &dst_size);
        size = (ret == 0) ? dst_size : 0;
    }
    else if (av_record.record_cfg.audio_fmt == AV_RECORD_AUDIO_FMT_G711A) {
        int16_t *pcm_data = (int16_t *) src;
        size >>= 1;
        uint8_t *end = dst + size;
        while (dst < end) {
            *(dst++) = esp_g711a_encode(*(pcm_data++));
        }
    } else if (av_record.record_cfg.audio_fmt == AV_RECORD_AUDIO_FMT_G711U) {
        int16_t *pcm_data = (int16_t *) src;
        size >>= 1;
        uint8_t *end = dst + size;
        while (dst < end) {
            *(dst++) = esp_g711u_encode(*(pcm_data++));
        }
    }
    return size;
}

static void audio_record_thread(void *arg)
{
    uint32_t audio_frame_size = get_audio_frame_size(AUDIO_FRAME_DURATION);
    uint8_t sample_size = get_audio_sample_size();
    uint32_t q_size = audio_frame_size;
    if (av_record.record_cfg.audio_fmt != AV_RECORD_AUDIO_FMT_PCM) {
        audio_record_encode_get_frame_size(&audio_frame_size, &q_size);
    }
    uint8_t *raw_audio = (uint8_t *) media_lib_malloc(audio_frame_size + 16);
    uint8_t* aligned_raw = (uint8_t*)(((uint32_t)raw_audio + 15) & ~(0xF));
    while (!av_record.stopping) {
        int ret = 0;
        if (raw_audio == NULL) {
            break;
        }
        record_src_frame_data_t frame_data = {
            .data = aligned_raw,
            .size = audio_frame_size,
        };
        ret = record_src_read_frame(av_record.audio_src_handle, &frame_data, 100000);
        if (ret != 0) {
            ESP_LOGE(TAG, "Fail to read audio data ret %d", ret);
            break;
        }
        av_record.audio_reached = true;
        if (start_frame_synced() == false) {
            continue;
        }
        uint8_t *buffer = (uint8_t *) get_q_data(q_size);
        if (buffer == NULL) {
            break;
        }
        uint32_t aud_pts = get_audio_pts();
        if (av_record.record_cfg.audio_fmt != AV_RECORD_AUDIO_FMT_PCM) {
            int enc_size = audio_record_encode_data(aligned_raw, frame_data.size, buffer, q_size);
            if (enc_size <= 0) {
                send_q_data(0);
                av_record.audio_frames += frame_data.size / sample_size;
                continue;
            }
            fill_q_header(buffer, enc_size, 0, aud_pts);
            send_q_data(enc_size);
        } else {
            fill_q_header(buffer, frame_data.size, 0, aud_pts);
            memcpy(buffer, aligned_raw, frame_data.size);
            send_q_data(frame_data.size);
        }
        av_record.audio_frames += frame_data.size / sample_size;
    }
    if (raw_audio) {
        media_lib_free(raw_audio);
    }
    av_record.audio_recording = false;
    ESP_LOGI(TAG, "Audio record thread quitted");
    media_lib_thread_destroy(NULL);
}

static void video_record_thread(void *arg)
{
    uint32_t video_start = get_cur_time();
    uint32_t fetch_time = get_cur_time();
    uint32_t resync_time = VIDEO_RESYNC_FRAME_TOLERANCE*1000/av_record.record_cfg.video_fps;
    while (!av_record.stopping) {
        record_src_frame_data_t frame_data;
        memset(&frame_data, 0, sizeof(record_src_frame_data_t));
        int ret = record_src_read_frame(av_record.video_src_handle, &frame_data, MEDIA_LIB_MAX_LOCK_TIME);
        if (ret != 0) {
            ESP_LOGE(TAG, "Fail to read video frame");
            break;
        }
        av_record.video_reached = true;
        uint32_t vid_pts = get_video_pts();
        uint32_t cur_pts;
        // When audio running, use audio pts
        if (av_record.audio_recording) {
            cur_pts = get_audio_pts();
        } else {
            cur_pts = get_cur_time() - video_start;
        }
        // Video drop data according real time or audio pts
        if (start_frame_synced() == false || cur_pts < vid_pts) {
            record_src_unlock_frame(av_record.video_src_handle);
            continue;
        }
        // TODO need cfg to enable this feature
        if (vid_pts + resync_time < cur_pts) {
            vid_pts = cur_pts;
        }
        int pic_size = av_record.encode_video ? VIDEO_ENCODE_MAX_FRAME_SIZE : frame_data.size;
        uint8_t *buffer = (uint8_t *) get_q_data(pic_size);
        if (buffer == NULL) {
            ESP_LOGE(TAG, "Fail to get size %d", pic_size);
            record_src_unlock_frame(av_record.video_src_handle);
            break;
        }
        if (av_record.encode_video) {
            int enc_size = pic_size;
            ret = video_encoder_process(frame_data.data, frame_data.size, buffer, pic_size, &enc_size, &vid_pts);
            if (ret != 0) {
                ESP_LOGE(TAG, "Encode error ret %d", ret);
                send_q_data(0);
                record_src_unlock_frame(av_record.video_src_handle);
                continue;
            }
            fill_q_header(buffer, enc_size, 1, vid_pts);
            send_q_data(enc_size);
        } else {
            memcpy(buffer, frame_data.data, frame_data.size);
            fill_q_header(buffer, frame_data.size, 1, vid_pts);
            send_q_data(frame_data.size);
        }
        av_record.video_frames++;
        record_src_unlock_frame(av_record.video_src_handle);
        if (vid_pts >= av_record.last_video_pts + 2000) {
            int q_num = 0, q_size = 0;
            uint32_t elapse = get_cur_time() - fetch_time;
            data_queue_query(av_record.stream_buffer_q, &q_num, &q_size);
            ESP_LOGI(TAG, "s:%d fps:%d vpts:%d apts:%d q:%d/%d", frame_data.size,
                     (int)(av_record.record_cfg.video_fps * (vid_pts - av_record.last_video_pts) / elapse), 
                     (int)vid_pts, (int)cur_pts,
                     q_num, q_size);
            fetch_time = get_cur_time();
            av_record.last_video_pts = vid_pts;
        }
    }
    av_record.video_recording = false;
    ESP_LOGI(TAG, "Video record thread quitted");
    media_lib_thread_destroy(NULL);
}

int av_record_start(av_record_cfg_t *cfg)
{
    av_record.record_cfg = *cfg;
    cfg = &av_record.record_cfg;
    do {
        av_record.stream_buffer_q = data_queue_init(RECORD_Q_BUFFER_SIZE);
        av_record.stopping = false;
        if (cfg->audio_fmt != AV_RECORD_AUDIO_FMT_NONE) {
            if (start_audio_recorder() != 0) {
                cfg->audio_fmt = AV_RECORD_AUDIO_FMT_NONE;
            }
        }
        if (cfg->video_fmt != AV_RECORD_VIDEO_FMT_NONE) {
            if (start_video_recorder() != 0) {
                cfg->video_fmt = AV_RECORD_VIDEO_FMT_NONE;
            }
        }
        if (cfg->video_fmt == AV_RECORD_VIDEO_FMT_NONE && cfg->audio_fmt == AV_RECORD_AUDIO_FMT_NONE) {
            break;
        }
        av_record.write_running = true;
        media_lib_thread_handle_t thread_handle;
        if (media_lib_thread_create(&thread_handle, "writer", write_thread, NULL, 4 * 1024, 15, 0) != ESP_OK) {
            av_record.write_running = false;
            break;
        }
        if (cfg->video_fmt != AV_RECORD_VIDEO_FMT_NONE) {
            av_record.video_recording = true;
            if (media_lib_thread_create(&thread_handle, "v_record", video_record_thread, NULL, 12 * 1024, 15, 1) !=
                ESP_OK) {
                av_record.video_recording = false;
                break;
            }
        }
        if (cfg->audio_fmt != AV_RECORD_AUDIO_FMT_NONE) {
            av_record.audio_recording = true;
            if (media_lib_thread_create(&thread_handle, "a_record", audio_record_thread, NULL, 4 * 1024, 15, 0) !=
                ESP_OK) {
                av_record.audio_recording = false;
                break;
            }
        }
        ESP_LOGI(TAG, "Start record OK");
        return 0;
    } while (0);
    av_record_stop();
    return ESP_MEDIA_ERR_FAIL;
}

int av_record_stop()
{
    av_record.stopping = true;
    ESP_LOGI(TAG, "Stopping av record");
    if (av_record.stream_buffer_q) {
        data_queue_wakeup(av_record.stream_buffer_q);
        ESP_LOGI(TAG, "Wakeup queue done");
    }
    while (av_record.write_running || av_record.audio_recording || av_record.video_recording) {
        media_lib_thread_sleep(10);
    }
    if (av_record.stream_buffer_q) {
        data_queue_deinit(av_record.stream_buffer_q);
        av_record.stream_buffer_q = NULL;
        ESP_LOGI(TAG, "data q deinit done");
    }
    stop_audio_recorder();
    stop_video_recorder();
    av_record.audio_frames = av_record.video_frames = 0;
    av_record.last_video_pts = 0;
    av_record.stopping = false;
    av_record.audio_reached = av_record.video_reached = false;
    ESP_LOGI(TAG, "Stop av record done");
    return 0;
}

bool av_record_running()
{
    return (av_record.write_running && (av_record.audio_recording || av_record.video_recording));
}

uint32_t av_record_get_pts()
{
    uint32_t pts = get_audio_pts();
    uint32_t video_pts = get_video_pts();
    if (video_pts > pts) {
        pts = video_pts;
    }
    return pts;
}

const char* av_record_get_afmt_str(av_record_audio_fmt_t afmt)
{
    switch (afmt) {
    case AV_RECORD_AUDIO_FMT_PCM:
        return "pcm";
    case AV_RECORD_AUDIO_FMT_G711A:
        return "g711a";
    case AV_RECORD_AUDIO_FMT_G711U:
        return "g711u";
    case AV_RECORD_AUDIO_FMT_AAC:
        return "aac";
    default:
        return "undef";
    }
}

const char* av_record_get_vfmt_str(av_record_video_fmt_t vfmt)
{
    switch (vfmt) {
    case AV_RECORD_VIDEO_FMT_MJPEG:
        return "mjpeg";
    case AV_RECORD_VIDEO_FMT_H264:
        return "h264";
    default:
        return "undef";
    }
}
