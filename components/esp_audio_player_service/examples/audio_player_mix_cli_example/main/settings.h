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

/* Stream 0 URL playlist (file:// URLs used by the player). */
#define EXAMPLE_TRACK0_NAME  "test.mp3"
#define EXAMPLE_TRACK0_URL   "file:///sdcard/test.mp3"
#define EXAMPLE_TRACK1_NAME  "test.aac"
#define EXAMPLE_TRACK1_URL   "file:///sdcard/test.aac"
#define EXAMPLE_TRACK2_NAME  "test.wav"
#define EXAMPLE_TRACK2_URL   "file:///sdcard/test.wav"
#define EXAMPLE_TRACK3_NAME  "test.opus"
#define EXAMPLE_TRACK3_URL   "file:///sdcard/test.opus"

/* PCM for the feed stream (16-bit little-endian). */
#define EXAMPLE_PCM_FILENAME         "test_8000hz_16bit_2ch_10000ms.pcm"
#define EXAMPLE_PCM_SAMPLE_RATE      8000
#define EXAMPLE_PCM_CHANNEL          2
#define EXAMPLE_PCM_BITS_PER_SAMPLE  16
#define EXAMPLE_FEED_TRACK_ID        1

#define EXAMPLE_PCM_FEED_PERIOD_MS  20

#define EXAMPLE_URL_GAIN_PERCENT       70
#define EXAMPLE_URL_DUCK_GAIN_PERCENT  40
#define EXAMPLE_LINK_GAIN_PERCENT      100
#define EXAMPLE_FEED_GAIN_PERCENT      100
#define EXAMPLE_MIXER_TRANSITION_MS    1500

#ifdef __cplusplus
}
#endif  /* __cplusplus */
