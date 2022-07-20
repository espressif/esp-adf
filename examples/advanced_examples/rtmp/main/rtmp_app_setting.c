/* RTMP application test settings 

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "rtmp_app_setting.h"

// Set default value of basic settings
static bool use_sw_jpg = false;
static av_record_video_quality_t video_quality = AV_RECORD_VIDEO_QUALITY_HD;
static uint8_t video_fps = 10;
static bool allow_running = true;
static uint8_t audio_channel = 1;
static int audio_sample_rate = 11025;
static av_record_audio_fmt_t audio_fmt = AV_RECORD_AUDIO_FMT_AAC;
static av_record_video_fmt_t video_fmt = AV_RECORD_VIDEO_FMT_MJPEG;

#define SET_FUNCTION_CONSTRUCT(func_name, arg_type, dst) \
void func_name(arg_type setting)                         \
{                                                        \
    dst = setting;                                       \
}

#define GET_FUNCTION_CONSTRUCT(func_name, arg_type, from) \
arg_type func_name()                                      \
{                                                         \
    return from;                                          \
} 

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_allow_run, bool, allow_running)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_allow_run, bool, allow_running)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_sw_jpeg, bool, use_sw_jpg)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_sw_jpeg, bool, use_sw_jpg)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_video_fps, uint8_t, video_fps)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_video_fps, uint8_t, video_fps)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_video_quality, av_record_video_quality_t, video_quality)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_video_quality, av_record_video_quality_t, video_quality)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_audio_fmt, av_record_audio_fmt_t, audio_fmt)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_audio_fmt, av_record_audio_fmt_t, audio_fmt)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_video_fmt, av_record_video_fmt_t, video_fmt)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_video_fmt, av_record_video_fmt_t, video_fmt)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_audio_channel, uint8_t, audio_channel)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_audio_channel, uint8_t, audio_channel)

SET_FUNCTION_CONSTRUCT(rtmp_setting_set_audio_sample_rate, int, audio_sample_rate)
GET_FUNCTION_CONSTRUCT(rtmp_setting_get_audio_sample_rate, int, audio_sample_rate)
