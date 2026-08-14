/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"

#include "esp_player_service_playback.h"
#include "esp_playlist.h"

#include "mix_cli.h"
#include "mix_sources.h"

static const char *TAG = "VPM_CLI";

static const esp_media_stream_id_t s_movie_stream = ESP_MEDIA_DEFAULT_STREAM;

static esp_playlist_repeat_mode_t s_repeat_mode = ESP_PLAYLIST_REPEAT_ALL;

static const char *mode_name(esp_playlist_repeat_mode_t mode)
{
    switch (mode) {
        case ESP_PLAYLIST_REPEAT_NONE:
            return "none";
        case ESP_PLAYLIST_REPEAT_ONE:
            return "one";
        case ESP_PLAYLIST_REPEAT_ALL:
            return "all";
        case ESP_PLAYLIST_REPEAT_SHUFFLE:
            return "shuffle";
        default:
            return "unknown";
    }
}

static int fail_if_not_movie(void)
{
    if (mix_sources_require_movie() != ESP_OK) {
        printf("stream 0 is not in movie mode; stop es or link first\n");
        return 1;
    }
    return 0;
}

static int cmd_start(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: start <movie|es|link|tts>\n");
        return 1;
    }
    esp_err_t ret = mix_sources_start(argv[1]);
    if (ret != ESP_OK) {
        printf("start %s failed: %s\n", argv[1], esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

static int cmd_stop(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: stop <movie|es|link|tts>\n");
        return 1;
    }
    esp_err_t ret = mix_sources_stop(argv[1]);
    if (ret != ESP_OK) {
        printf("stop %s failed: %s\n", argv[1], esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    mix_sources_status();
    return 0;
}

static int cmd_play(int argc, char **argv)
{
    if (fail_if_not_movie()) {
        return 1;
    }
    esp_player_service_t *service = mix_sources_service();
    if (service == NULL) {
        printf("player not ready\n");
        return 1;
    }

    if (argc >= 2) {
        int index = atoi(argv[1]);
        esp_err_t ret = esp_player_service_play_index(service, s_movie_stream, index);
        if (ret != ESP_OK) {
            printf("play %d failed: %s\n", index, esp_err_to_name(ret));
            return 1;
        }
        mix_sources_mark_movie();
        printf("Playing index %d\n", index);
        return 0;
    }

    esp_player_state_t st = ESP_PLAYER_STATE_IDLE;
    (void)esp_player_service_get_state(service, s_movie_stream, &st);
    if (st == ESP_PLAYER_STATE_PAUSED) {
        esp_err_t ret = esp_player_service_resume(service, s_movie_stream);
        if (ret != ESP_OK) {
            printf("resume failed: %s\n", esp_err_to_name(ret));
            return 1;
        }
        mix_sources_mark_movie();
        printf("Resumed\n");
        return 0;
    }

    esp_playlist_handle_t pl = mix_sources_playlist();
    esp_playlist_info_t info = {0};
    if (pl == NULL || esp_playlist_curr(pl, &info) != ESP_OK) {
        printf("play failed: no current track\n");
        return 1;
    }
    esp_err_t ret = esp_player_service_play_index(service, s_movie_stream, info.index);
    if (ret != ESP_OK) {
        printf("play failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    mix_sources_mark_movie();
    printf("Playing index %d (%s)\n", info.index, info.media_name);
    return 0;
}

static int cmd_pause(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (fail_if_not_movie()) {
        return 1;
    }
    esp_player_service_t *service = mix_sources_service();
    if (service == NULL) {
        printf("player not ready\n");
        return 1;
    }
    esp_err_t ret = esp_player_service_pause(service, s_movie_stream);
    if (ret != ESP_OK) {
        printf("pause failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    printf("Paused\n");
    return 0;
}

static int cmd_resume(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (fail_if_not_movie()) {
        return 1;
    }
    esp_player_service_t *service = mix_sources_service();
    if (service == NULL) {
        printf("player not ready\n");
        return 1;
    }
    esp_err_t ret = esp_player_service_resume(service, s_movie_stream);
    if (ret != ESP_OK) {
        printf("resume failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    mix_sources_mark_movie();
    printf("Resumed\n");
    return 0;
}

static int cmd_next(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (fail_if_not_movie()) {
        return 1;
    }
    esp_player_service_t *service = mix_sources_service();
    if (service == NULL) {
        printf("player not ready\n");
        return 1;
    }
    esp_err_t ret = esp_player_service_next(service, s_movie_stream);
    if (ret != ESP_OK) {
        printf("next failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    mix_sources_mark_movie();
    esp_playlist_info_t info = {0};
    if (esp_playlist_curr(mix_sources_playlist(), &info) == ESP_OK) {
        printf("Next: [%d] %s\n", info.index, info.media_name);
    }
    return 0;
}

static int cmd_prev(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (fail_if_not_movie()) {
        return 1;
    }
    esp_player_service_t *service = mix_sources_service();
    if (service == NULL) {
        printf("player not ready\n");
        return 1;
    }
    esp_err_t ret = esp_player_service_prev(service, s_movie_stream);
    if (ret != ESP_OK) {
        printf("prev failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    mix_sources_mark_movie();
    esp_playlist_info_t info = {0};
    if (esp_playlist_curr(mix_sources_playlist(), &info) == ESP_OK) {
        printf("Prev: [%d] %s\n", info.index, info.media_name);
    }
    return 0;
}

static int cmd_seek(int argc, char **argv)
{
    if (fail_if_not_movie()) {
        return 1;
    }
    esp_player_service_t *service = mix_sources_service();
    if (service == NULL) {
        printf("player not ready\n");
        return 1;
    }
    if (argc < 2) {
        printf("Usage: seek <milliseconds>\n");
        return 1;
    }
    int pos_ms = atoi(argv[1]);
    if (pos_ms < 0) {
        printf("seek failed: position must not be negative\n");
        return 1;
    }
    esp_err_t ret = esp_player_service_seek(service, s_movie_stream, (uint64_t)pos_ms);
    if (ret != ESP_OK) {
        printf("seek failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    printf("Seeking to %d ms\n", pos_ms);
    return 0;
}

static int cmd_mode(int argc, char **argv)
{
    if (fail_if_not_movie()) {
        return 1;
    }
    esp_player_service_t *service = mix_sources_service();
    if (service == NULL) {
        printf("player not ready\n");
        return 1;
    }
    if (argc < 2) {
        printf("Usage: mode <none|one|all|shuffle>\n");
        printf("Current: %s\n", mode_name(s_repeat_mode));
        return 1;
    }

    esp_playlist_repeat_mode_t mode = ESP_PLAYLIST_REPEAT_ALL;
    if (strcmp(argv[1], "none") == 0) {
        mode = ESP_PLAYLIST_REPEAT_NONE;
    } else if (strcmp(argv[1], "one") == 0) {
        mode = ESP_PLAYLIST_REPEAT_ONE;
    } else if (strcmp(argv[1], "all") == 0) {
        mode = ESP_PLAYLIST_REPEAT_ALL;
    } else if (strcmp(argv[1], "shuffle") == 0) {
        mode = ESP_PLAYLIST_REPEAT_SHUFFLE;
    } else {
        printf("Unknown mode '%s' (use none|one|all|shuffle)\n", argv[1]);
        return 1;
    }

    esp_err_t ret = esp_player_service_set_repeat_mode(service, s_movie_stream, mode);
    if (ret != ESP_OK) {
        printf("mode failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    s_repeat_mode = mode;
    printf("Repeat mode: %s\n", mode_name(mode));
    return 0;
}

static int cmd_list(int argc, char **argv)
{
    int start = 0;
    if (argc >= 2) {
        start = atoi(argv[1]);
    }

    esp_playlist_handle_t pl = mix_sources_playlist();
    if (pl == NULL) {
        printf("playlist not ready\n");
        return 1;
    }

    int count = 0;
    if (esp_playlist_get_count(pl, &count) != ESP_OK) {
        printf("list failed\n");
        return 1;
    }
    if (start < 0 || (count > 0 && start >= count) || (count == 0 && start != 0)) {
        printf("Usage: list [start]\n");
        printf("start %d out of range (0..%d)\n", start, count > 0 ? count - 1 : 0);
        return 1;
    }

    esp_playlist_info_t curr = {0};
    int curr_index = -1;
    if (esp_playlist_curr(pl, &curr) == ESP_OK) {
        curr_index = curr.index;
    }

    printf("index  name\n");
    for (int i = start; i < count; i++) {
        esp_playlist_info_t info = {0};
        if (esp_playlist_get_info(pl, i, &info) != ESP_OK) {
            continue;
        }
        printf("%c %-5d %s\n", (i == curr_index) ? '*' : ' ', i, info.media_name);
    }
    return 0;
}

esp_err_t mix_cli_register_commands(esp_cli_service_t *cli)
{
    if (cli == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_console_cmd_t commands[] = {
        {
            .command = "start",
            .help = "Join a stream: start <movie|es|link|tts> (movie, es and link are exclusive)",
            .func = &cmd_start,
        },
        {
            .command = "stop",
            .help = "Leave a stream: stop <movie|es|link|tts> (movie/es/link all stop stream 0)",
            .func = &cmd_stop,
        },
        {
            .command = "status",
            .help = "Show stream roles, playback state and playlist, ES or link",
            .func = &cmd_status,
        },
        {
            .command = "play",
            .help = "Play movie playlist current item, or play <index>",
            .func = &cmd_play,
        },
        {
            .command = "pause",
            .help = "Pause movie URL playback",
            .func = &cmd_pause,
        },
        {
            .command = "resume",
            .help = "Resume movie URL playback",
            .func = &cmd_resume,
        },
        {
            .command = "next",
            .help = "Play next movie playlist item",
            .func = &cmd_next,
        },
        {
            .command = "prev",
            .help = "Play previous movie playlist item",
            .func = &cmd_prev,
        },
        {
            .command = "seek",
            .help = "Seek within the current movie item: seek <milliseconds>",
            .func = &cmd_seek,
        },
        {
            .command = "mode",
            .help = "Set movie playlist repeat: mode <none|one|all|shuffle>",
            .func = &cmd_mode,
        },
        {
            .command = "list",
            .help = "List movie playlist items from [start] (* = current)",
            .func = &cmd_list,
        },
    };
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        ESP_RETURN_ON_ERROR(esp_cli_service_register_static_command(cli, &commands[i]),
                            TAG, "Register command");
    }

    return ESP_OK;
}
