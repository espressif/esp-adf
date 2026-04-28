/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"

esp_err_t playlist_bench_storage_init(void);
void playlist_bench_storage_deinit(void);

/** SD mount point from esp_board_manager (e.g. /sdcard). Valid after init. */
const char *playlist_bench_mount_point(void);
/** Scan directory: {mount_point}/music */
const char *playlist_bench_scan_dir(void);
/** Media DB storage_path: {mount_point}/__playlist */
const char *playlist_bench_db_dir(void);
/** Playlist JSON path: {mount_point}/__playlist/playlist.json */
const char *playlist_bench_playlist_file(void);
