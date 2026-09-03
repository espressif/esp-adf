/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_cli_service.h"
#include "esp_console.h"
#include "esp_service.h"

#include "rtsp_cli.h"
#include "rtsp_example.h"
#include "rtsp_session.h"
#include "rtsp_settings.h"

static const char *TAG = "RTSP_CLI";

static esp_cli_service_t *s_cli;

static void print_usage(void)
{
    printf("Usage:\n");
    printf("  rtsp server [url] [-v h264|mjpeg|none]\n");
    printf("              [-a aac|g711a|g711u|none] [--res <WxH>] [--fps <n>] [--bitrate <bps>]\n");
    printf("  rtsp push [url] [-v h264|mjpeg|none] [-a aac|g711a|g711u|none]\n");
    printf("            [--res <WxH>] [--fps <n>] [--bitrate <bps>]\n");
    printf("  rtsp pull [url] [-v h264|mjpeg] [--no-audio] [--no-video]\n");
    printf("            [--cache <bytes>]\n");
    printf("  rtsp stop\n");
    printf("  rtsp info\n");
    printf("Omitting url uses the role default; g711a/g711u force 8 kHz mono.\n");
    printf("Video defaults to mjpeg; bitrate follows the frame size unless --bitrate is set.\n");
    printf("For pull, -v is the codec to expect: it selects the depacketizer before\n");
    printf("the SDP is read, and the wrong one reassembles no frame at all.\n");
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

/* RTP payload types 8 and 0 pin G.711 to 8 kHz mono, so the sample rate
   follows the codec choice instead of being a separate option. */
static bool parse_audio_codec(const char *name, rtsp_session_opts_t *opts)
{
    if (strcmp(name, "aac") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_AAC;
        opts->sample_rate = RTSP_AUDIO_SAMPLE_RATE;
    } else if (strcmp(name, "g711a") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_G711A;
        opts->sample_rate = RTSP_G711_SAMPLE_RATE;
    } else if (strcmp(name, "g711u") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_G711U;
        opts->sample_rate = RTSP_G711_SAMPLE_RATE;
    } else if (strcmp(name, "none") == 0) {
        opts->audio_codec = 0;
    } else {
        printf("Unknown audio codec '%s'; use aac, g711a, g711u or none\n", name);
        return false;
    }
    return true;
}

static bool parse_resolution(const char *value, rtsp_session_opts_t *opts)
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

static bool parse_server_url_port(const char *url, uint16_t *out_port)
{
    static const char prefix[] = "rtsp://";
    if (url == NULL || out_port == NULL || strncmp(url, prefix, sizeof(prefix) - 1) != 0) {
        printf("Bad server URL; expected rtsp://<host>[:port]/<path>\n");
        return false;
    }

    const char *host = url + sizeof(prefix) - 1;
    const char *path = strchr(host, '/');
    const char *host_end = (path != NULL) ? path : host + strlen(host);
    if (host == host_end) {
        printf("Bad server URL '%s'; host is empty\n", url);
        return false;
    }

    const char *colon = memchr(host, ':', (size_t)(host_end - host));
    if (colon == NULL) {
        *out_port = RTSP_SERVER_PORT;
        return true;
    }
    if (colon == host || colon + 1 == host_end) {
        printf("Bad server URL '%s'; invalid port\n", url);
        return false;
    }

    char *end = NULL;
    unsigned long port = strtoul(colon + 1, &end, 10);
    if (end != host_end || port == 0 || port > UINT16_MAX) {
        printf("Bad server URL '%s'; invalid port\n", url);
        return false;
    }
    *out_port = (uint16_t)port;
    return true;
}

static bool parse_options(int argc, char **argv, int first, rtsp_session_opts_t *opts)
{
    for (int i = first; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;
        bool needs_value = (strcmp(arg, "--no-audio") != 0) && (strcmp(arg, "--no-video") != 0);

        if (needs_value && value == NULL) {
            printf("Option '%s' needs a value\n", arg);
            return false;
        }

        if (strcmp(arg, "-t") == 0) {
            if (strcmp(value, "udp") == 0) {
                opts->transport = RTSP_TRANSPORT_UDP;
            } else {
                printf("This example only supports RTP over UDP\n");
                return false;
            }
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

static int rtsp_command(int argc, char **argv)
{
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *sub = argv[1];

    if (strcmp(sub, "stop") == 0) {
        if (!rtsp_session_is_active()) {
            printf("No session is running\n");
            return 0;
        }
        return (rtsp_session_stop() == ESP_OK) ? 0 : 1;
    }
    if (strcmp(sub, "info") == 0) {
        rtsp_session_print_info();
        return 0;
    }

    bool is_server = (strcmp(sub, "server") == 0);
    bool is_push = (strcmp(sub, "push") == 0);
    bool is_pull = (strcmp(sub, "pull") == 0);
    if (!is_server && !is_push && !is_pull) {
        printf("Unknown subcommand '%s'\n", sub);
        print_usage();
        return 1;
    }

    rtsp_session_opts_t opts = {0};
    rtsp_session_opts_default(&opts);

    char url[RTSP_URL_MAX_LEN] = {0};
    if (is_server) {
        snprintf(url, sizeof(url), "rtsp://0.0.0.0:%u%s", RTSP_SERVER_PORT, RTSP_SERVER_PATH);
    } else {
        snprintf(url, sizeof(url), "%s", is_push ? RTSP_PUSH_URL : RTSP_PULL_URL);
    }

    int first_option = 2;
    if (argc >= 3 && argv[2][0] != '-') {
        snprintf(url, sizeof(url), "%s", argv[2]);
        first_option = 3;
    }

    if (!parse_options(argc, argv, first_option, &opts)) {
        print_usage();
        return 1;
    }

    if (is_server && !parse_server_url_port(url, &opts.port)) {
        return 1;
    }
    opts.url = url;

    esp_err_t ret;
    if (is_server) {
        ret = rtsp_session_start_server(&opts);
    } else if (is_push) {
        ret = rtsp_session_start_push(&opts);
    } else {
        ret = rtsp_session_start_pull(&opts);
    }

    if (ret != ESP_OK) {
        printf("rtsp %s failed: %s\n", sub, esp_err_to_name(ret));
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

    const char *ssid = RTSP_WIFI_SSID;
    const char *password = RTSP_WIFI_PASSWORD;
    if (argc >= 2) {
        ssid = argv[1];
    }
    if (argc >= 3) {
        password = (strcmp(argv[2], "-") == 0) ? "" : argv[2];
    }

    esp_err_t ret = rtsp_example_wifi_connect(ssid, password);
    if (ret != ESP_OK) {
        printf("wifi connect failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

esp_err_t rtsp_cli_start(void)
{
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.base_cfg.name = "rtsp_cli";
    cfg.prompt = "rtsp> ";
    cfg.task_stack = 8192;

    ESP_RETURN_ON_ERROR(esp_cli_service_create(&cfg, &s_cli), TAG, "Create CLI service");

    const esp_console_cmd_t commands[] = {
        {
            .command = "rtsp",
            .help = "RTSP control: rtsp <server|push|pull|stop|info> [options]",
            .func = rtsp_command,
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
