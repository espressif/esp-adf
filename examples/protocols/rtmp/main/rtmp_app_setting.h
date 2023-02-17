/* RTMP application test settings 

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifndef _RTMP_APP_SETTING_H
#define _RTMP_APP_SETTING_H

#include "av_record.h"
#include "media_lib_tls.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Set allow application to run */
void rtmp_setting_set_allow_run(bool allow_run);

/* Get allow application run flag */
bool rtmp_setting_get_allow_run();

/* Set whether use software jpeg encoder */
void rtmp_setting_set_sw_jpeg(bool use_sw_jpg);

/* Get whether use software jpeg encoder */
bool rtmp_setting_get_sw_jpeg();

/* Set video fps */
void rtmp_setting_set_video_fps(uint8_t fps);

/* Get video fps */
uint8_t rtmp_setting_get_video_fps();

/* Set video format */
void rtmp_setting_set_video_fmt(av_record_video_fmt_t video_format);

/* Get video format */
av_record_video_fmt_t rtmp_setting_get_video_fmt();

/* Set video quality */
void rtmp_setting_set_video_quality(av_record_video_quality_t video_quality);

/* Get video quality */
av_record_video_quality_t rtmp_setting_get_video_quality();

/* Set audio format */
void rtmp_setting_set_audio_fmt(av_record_audio_fmt_t audio_fmt);

/* Get audio format */
av_record_audio_fmt_t rtmp_setting_get_audio_fmt();

/* Set audio channel */
void rtmp_setting_set_audio_channel(uint8_t channel);

/* Get audio channel */
uint8_t rtmp_setting_get_audio_channel();

/* Set audio sample rate */
void rtmp_setting_set_audio_sample_rate(int sample_rate);

/* Get audio sample rate */
int rtmp_setting_get_audio_sample_rate();

/* Skip server CA verify */
void rtmp_setting_set_allow_insecure(bool insecure);

/* Get server SSL setting */
void rtmp_setting_get_server_ssl_cfg(media_lib_tls_server_cfg_t *ssl_cfg);

/* Get client SSL setting */
void rtmp_setting_get_client_ssl_cfg(const char *url, media_lib_tls_cfg_t *ssl_cfg);

#ifdef __cplusplus
}
#endif

#endif
