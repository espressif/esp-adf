/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sdkconfig.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define EXTRACTOR_WIFI_SSID         CONFIG_EXTRACTOR_EXAMPLE_WIFI_SSID
#define EXTRACTOR_WIFI_PASSWORD     CONFIG_EXTRACTOR_EXAMPLE_WIFI_PASSWORD
#define EXTRACTOR_WIFI_WAIT_SEC     CONFIG_EXTRACTOR_EXAMPLE_WIFI_WAIT_SEC
#define EXTRACTOR_TEST_DURATION_MS  (200000)
/** Default play-throughs for the repeat console command */
#define EXTRACTOR_REPEAT_COUNT      (3)
/** Read this long before seeking in the manual-provider seek test */
#define EXTRACTOR_SEEK_AFTER_MS     (5000)
/** Seek target position (ms) used by the manual-provider seek test */
#define EXTRACTOR_SEEK_POSITION_MS  (10000)
/** Extractor output frame pool; must fit largest A/V access unit (default service is 64 KB) */
#define EXTRACTOR_OUT_POOL_SIZE     (100 * 1024)

/** SD card mount point used by board manager FatFS device */
#define EXTRACTOR_SD_MOUNT     "/sdcard"
#define EXTRACTOR_STORAGE_DIR  EXTRACTOR_SD_MOUNT "/video"

/**
 * Test URL series: local MP4 on SD + public HLS HTTP URIs.
 * Place a playable MP4 at EXTRACTOR_URL_SD_MP4 before running file cases.
 */
#define EXTRACTOR_URL_SD_MP4   EXTRACTOR_STORAGE_DIR "/test1.mp4"
#define EXTRACTOR_URL_HLS_AV   "https://playertest.longtailvideo.com/adaptive/oceans_aes/oceans_aes.m3u8"
#define EXTRACTOR_URL_HLS_AAC  "http://open.ls.qingting.fm/live/274/64k.m3u8?format=aac"

typedef struct {
    const char *name;
    const char *url;
} extractor_example_url_t;

static const extractor_example_url_t EXTRACTOR_EXAMPLE_URLS[] = {
    {"hls_aac", EXTRACTOR_URL_HLS_AAC},
    {"sd_mp4", EXTRACTOR_URL_SD_MP4},
    {"hls_av", EXTRACTOR_URL_HLS_AV},
};

#define EXTRACTOR_EXAMPLE_URL_COUNT  (sizeof(EXTRACTOR_EXAMPLE_URLS) / sizeof(EXTRACTOR_EXAMPLE_URLS[0]))

#ifdef __cplusplus
}
#endif  /* __cplusplus */
