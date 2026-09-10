/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_cli_service.h"
#include "esp_console.h"
#include "esp_service.h"

#include "rtmp_cli.h"
#include "rtmp_example.h"
#include "rtmp_session.h"
#include "rtmp_settings.h"

#define RTMP_CLI_NAME_MAX_LEN  (64)

static const char *TAG = "RTMP_CLI";

static esp_cli_service_t *s_cli;

static void print_usage(void)
{
    printf("Usage:\n");
    printf("  rtmp server [-p <port>] [--app <name>] [--stream <name>] [--max-clients <n>]\n");
    printf("  rtmp push [url] [-v h264|mjpeg|none] [-a aac|pcm|g711a|g711u|none]\n");
    printf("            [--res <WxH>] [--fps <n>] [--bitrate <bps>] [--chunk <bytes>] [--insecure]\n");
    printf("  rtmp pull [url] [--cache <bytes>] [--chunk <bytes>] [--insecure]\n");
    printf("  rtmp live [-p <port>] [--app <name>] [--stream <name>] [media options]\n");
    printf("  rtmp loopback [same options as live; video defaults to mjpeg]\n");
    printf("  rtmp stop\n");
    printf("  rtmp info\n");
    printf("The server relays only, so 'live' pairs it with the publisher to serve the camera,\n");
    printf("and 'loopback' adds the player to run the whole chain on the board alone.\n");
    printf("Omitting url uses the menuconfig default; g711a/g711u force 8 kHz mono.\n");
    printf("push/live default to h264 so a PC player can read the stream; loopback defaults\n");
    printf("to mjpeg so the on-board LCD can keep up. Pass -v h264 on loopback to share it.\n");
    printf("Each start restarts just its own slot, so 'rtmp push' can be repointed without\n");
    printf("dropping a running server.\n");
}

static bool parse_video_codec(const char *name, esp_media_codec_fourcc_t *out_codec)
{
    if (strcmp(name, "h264") == 0) {
        *out_codec = ESP_CAPTURE_FMT_ID_H264;
    } else if (strcmp(name, "mjpeg") == 0) {
        *out_codec = ESP_CAPTURE_FMT_ID_MJPEG;
    } else if (strcmp(name, "none") == 0) {
        *out_codec = 0;
    } else {
        printf("Unknown video codec '%s'; use h264, mjpeg or none\n", name);
        return false;
    }
    return true;
}

/* G.711 is defined only at 8 kHz mono, so the sample rate follows the codec
   choice instead of being a separate option. */
static bool parse_audio_codec(const char *name, rtmp_session_opts_t *opts)
{
    if (strcmp(name, "aac") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_AAC;
        opts->sample_rate = RTMP_AUDIO_SAMPLE_RATE;
    } else if (strcmp(name, "pcm") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_PCM;
        opts->sample_rate = RTMP_AUDIO_SAMPLE_RATE;
    } else if (strcmp(name, "g711a") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_G711A;
        opts->sample_rate = RTMP_G711_SAMPLE_RATE;
    } else if (strcmp(name, "g711u") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_G711U;
        opts->sample_rate = RTMP_G711_SAMPLE_RATE;
    } else if (strcmp(name, "none") == 0) {
        opts->audio_codec = 0;
    } else {
        printf("Unknown audio codec '%s'; use aac, pcm, g711a, g711u or none\n", name);
        return false;
    }
    return true;
}

static bool parse_resolution(const char *value, rtmp_session_opts_t *opts)
{
    unsigned width = 0;
    unsigned height = 0;
    if (sscanf(value, "%ux%u", &width, &height) != 2 || width == 0 || height == 0) {
        printf("Bad resolution '%s'; expected <width>x<height>, for example 1280x720\n", value);
        return false;
    }
    opts->width = (uint16_t)width;
    opts->height = (uint16_t)height;
    return true;
}

static bool is_flag(const char *arg)
{
    return strcmp(arg, "--insecure") == 0 || strcmp(arg, "--no-audio") == 0 ||
           strcmp(arg, "--no-video") == 0;
}

static bool parse_options(int argc, char **argv, int first, rtmp_session_opts_t *opts,
                          char *app, size_t app_len, char *stream, size_t stream_len)
{
    for (int i = first; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;

        if (!is_flag(arg) && value == NULL) {
            printf("Option '%s' needs a value\n", arg);
            return false;
        }

        if (strcmp(arg, "-p") == 0) {
            opts->port = (uint16_t)strtoul(value, NULL, 10);
            i++;
        } else if (strcmp(arg, "--app") == 0) {
            snprintf(app, app_len, "%s", value);
            i++;
        } else if (strcmp(arg, "--stream") == 0) {
            snprintf(stream, stream_len, "%s", value);
            i++;
        } else if (strcmp(arg, "--max-clients") == 0) {
            opts->max_clients = (uint8_t)strtoul(value, NULL, 10);
            i++;
        } else if (strcmp(arg, "--chunk") == 0) {
            opts->chunk_size = (uint32_t)strtoul(value, NULL, 10);
            i++;
        } else if (strcmp(arg, "-v") == 0) {
            if (!parse_video_codec(value, &opts->video_codec)) {
                return false;
            }
            i++;
        } else if (strcmp(arg, "-a") == 0) {
            if (!parse_audio_codec(value, opts)) {
                return false;
            }
            i++;
        } else if (strcmp(arg, "--res") == 0) {
            if (!parse_resolution(value, opts)) {
                return false;
            }
            i++;
        } else if (strcmp(arg, "--fps") == 0) {
            opts->fps = (uint16_t)strtoul(value, NULL, 10);
            i++;
        } else if (strcmp(arg, "--bitrate") == 0) {
            opts->bitrate = (uint32_t)strtoul(value, NULL, 10);
            i++;
        } else if (strcmp(arg, "--cache") == 0) {
            opts->video_cache = (uint32_t)strtoul(value, NULL, 10);
            i++;
        } else if (strcmp(arg, "--insecure") == 0) {
            opts->tls_insecure = true;
        } else if (strcmp(arg, "--no-audio") == 0) {
            opts->audio_codec = 0;
        } else if (strcmp(arg, "--no-video") == 0) {
            opts->video_codec = 0;
        } else {
            printf("Unknown option '%s'\n", arg);
            return false;
        }
    }
    return true;
}

static int rtmp_command(int argc, char **argv)
{
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "stop") == 0) {
        if (!rtmp_session_is_active()) {
            printf("No slot is running\n");
            return 0;
        }
        return (rtmp_session_stop() == ESP_OK) ? 0 : 1;
    }
    if (strcmp(sub, "info") == 0) {
        rtmp_session_print_info();
        return 0;
    }

    bool is_server = (strcmp(sub, "server") == 0);
    bool is_push = (strcmp(sub, "push") == 0);
    bool is_pull = (strcmp(sub, "pull") == 0);
    bool is_live = (strcmp(sub, "live") == 0);
    bool is_loopback = (strcmp(sub, "loopback") == 0);
    if (!is_server && !is_push && !is_pull && !is_live && !is_loopback) {
        printf("Unknown subcommand '%s'\n", sub);
        print_usage();
        return 1;
    }

    rtmp_session_opts_t opts = {0};
    rtmp_session_opts_default(&opts);
    if (is_loopback) {
        opts.video_codec = RTMP_LOOPBACK_VIDEO_CODEC;
    }

    char app[RTMP_CLI_NAME_MAX_LEN];
    char stream[RTMP_CLI_NAME_MAX_LEN];
    snprintf(app, sizeof(app), "%s", RTMP_SERVER_APP);
    snprintf(stream, sizeof(stream), "%s", RTMP_SERVER_STREAM);

    char url[RTMP_URL_MAX_LEN] = {0};
    int first_option = 2;
    if (is_push || is_pull) {
        /* The client roles take an optional URL before the options */
        if (argc >= 3 && argv[2][0] != '-') {
            snprintf(url, sizeof(url), "%s", argv[2]);
            first_option = 3;
        } else {
            snprintf(url, sizeof(url), "%s", is_push ? RTMP_PUSH_URL : RTMP_PULL_URL);
        }
    }

    if (!parse_options(argc, argv, first_option, &opts, app, sizeof(app), stream, sizeof(stream))) {
        print_usage();
        return 1;
    }

    opts.app = app;
    opts.stream = stream;
    opts.url = url;

    esp_err_t ret;
    if (is_server) {
        ret = rtmp_session_start_server(&opts);
    } else if (is_push) {
        ret = rtmp_session_start_push(&opts);
    } else if (is_pull) {
        ret = rtmp_session_start_pull(&opts);
    } else if (is_live) {
        ret = rtmp_session_start_live(&opts);
    } else {
        ret = rtmp_session_start_loopback(&opts);
    }

    if (ret != ESP_OK) {
        printf("rtmp %s failed: %s\n", sub, esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

static int wifi_command(int argc, char **argv)
{
    if (argc > 3) {
        printf("Usage: wifi [ssid] [password]\n");
        printf("  omit both to use the menuconfig defaults; password '-' for an open AP\n");
        return 1;
    }

    const char *ssid = RTMP_WIFI_SSID;
    const char *password = RTMP_WIFI_PASSWORD;
    if (argc >= 2) {
        ssid = argv[1];
    }
    if (argc >= 3) {
        password = (strcmp(argv[2], "-") == 0) ? "" : argv[2];
    }

    esp_err_t ret = rtmp_example_wifi_connect(ssid, password);
    if (ret != ESP_OK) {
        printf("wifi connect failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

esp_err_t rtmp_cli_start(void)
{
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.base_cfg.name = "rtmp_cli";
    cfg.prompt = "rtmp> ";
    cfg.task_stack = 8192;

    ESP_RETURN_ON_ERROR(esp_cli_service_create(&cfg, &s_cli), TAG, "Create CLI service");

    const esp_console_cmd_t commands[] = {
        {
            .command = "rtmp",
            .help = "RTMP control: rtmp <server|push|pull|live|loopback|stop|info> [options]",
            .func = rtmp_command,
        },
        {
            .command = "wifi",
            .help = "Connect to Wi-Fi: wifi [ssid] [password] (defaults from menuconfig)",
            .func = wifi_command,
        },
    };
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        ESP_RETURN_ON_ERROR(esp_cli_service_register_static_command(s_cli, &commands[i]),
                            TAG, "Register command");
    }

    return esp_service_start(ESP_SERVICE_BASE(s_cli));
}
