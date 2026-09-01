/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/* Stream 0 movie URL playlist. */
#define EXAMPLE_TRACK0_NAME  "test.mp4"
#define EXAMPLE_TRACK0_URL   "file:///sdcard/test.mp4"
#define EXAMPLE_TRACK1_NAME  "test1.mp4"
#define EXAMPLE_TRACK1_URL   "file:///sdcard/test1.mp4"
#define EXAMPLE_TRACK2_NAME  "test.mp3"
#define EXAMPLE_TRACK2_URL   "file:///sdcard/test.mp3"

/* Stream 1 TTS PCM. */
#define EXAMPLE_PCM_FILENAME         "test_8000hz_16bit_2ch_10000ms.pcm"
#define EXAMPLE_PCM_SAMPLE_RATE      8000
#define EXAMPLE_PCM_CHANNEL          2
#define EXAMPLE_PCM_BITS_PER_SAMPLE  16
#define EXAMPLE_FEED_TRACK_ID        1
#define EXAMPLE_PCM_FEED_PERIOD_MS   20

#define EXAMPLE_MOVIE_GAIN_PERCENT       70
#define EXAMPLE_MOVIE_DUCK_GAIN_PERCENT  40
#define EXAMPLE_TTS_GAIN_PERCENT         100
#define EXAMPLE_MIXER_TRANSITION_MS      1500

/* Stream 0 elementary-stream feed (start es). README has the ffmpeg commands. */
#define EXAMPLE_AUDIO_ES_PATH  "/sdcard/feed.aac"
#define EXAMPLE_VIDEO_ES_PATH  "/sdcard/feed.mjpeg"

#define EXAMPLE_AUDIO_TRACK_ID  (0)
#define EXAMPLE_VIDEO_TRACK_ID  (1)

#define EXAMPLE_AUDIO_BITS_PER_SAMPLE  (16)
#define EXAMPLE_VIDEO_FPS              (15)

#define EXAMPLE_AUDIO_FRAME_MAX_SIZE  (8 * 1024)
#define EXAMPLE_VIDEO_FRAME_MAX_SIZE  (256 * 1024)

#define EXAMPLE_FEED_LEAD_MS       (300)
#define EXAMPLE_FEED_TASK_STACK    (6144)
#define EXAMPLE_FEED_TASK_PRIO     (5)
#define EXAMPLE_FEED_RETRY_MS      (20)
#define EXAMPLE_FEED_JOIN_POLL_MS  (20)
#define EXAMPLE_FEED_JOIN_RETRIES  (200)

#ifdef __cplusplus
}
#endif  /* __cplusplus */
