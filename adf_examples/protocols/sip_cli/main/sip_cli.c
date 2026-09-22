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

#include "esp_sip_service.h"

#include "sip_cli.h"
#include "sip_example.h"
#include "sip_session.h"
#include "sip_settings.h"

#define SIP_CLI_SERVER_MAX_LEN   (64)
#define SIP_CLI_TEXT_MAX_LEN     ESP_SIP_SERVICE_BODY_MAX
#define SIP_CLI_CMDLINE_MAX_LEN  (ESP_SIP_SERVICE_BODY_MAX + 256)

static const char *TAG = "SIP_CLI";

static esp_cli_service_t *s_cli;

static void print_usage(void)
{
    printf("Usage:\n");
    printf("  sip start [-u <user>] [-w <password>] [-s <server[:port]>] [-t udp|tcp|tls]\n");
    printf("            [-a g711a|g711u|opus|none] [-v h264|mjpeg|none] [--p2p]\n");
    printf("            [--srtp off|prefer|required] [--port <local>] [--res <WxH>] [--fps <n>]\n");
    printf("  sip stop\n");
    printf("  sip call [user]\n");
    printf("  sip answer\n");
    printf("  sip bye\n");
    printf("  sip dtmf <digit>\n");
    printf("  sip msg [-t <uri>] <text...>\n");
    printf("  sip auto <on|off>\n");
    printf("  sip info\n");
    printf("Omitting an account option uses the menuconfig default.\n");
    printf("Starting only registers the device; media flows once a call is answered.\n");
    printf("--p2p skips registration and calls the peer directly, which needs '-s <peer ip>'\n");
    printf("on both sides. The protocol stack then listens on the peer port from -s, so\n");
    printf("both sides have to agree on one port and --port is ignored.\n");
}

/* SIP negotiates one codec for both directions, and payload types 8 and 0 pin
   G.711 to 8 kHz mono, so the sample rate follows the codec choice. */
static bool parse_audio_codec(const char *name, sip_session_opts_t *opts)
{
    if (strcmp(name, "g711a") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_G711A;
        opts->sample_rate = SIP_AUDIO_SAMPLE_RATE;
    } else if (strcmp(name, "g711u") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_G711U;
        opts->sample_rate = SIP_AUDIO_SAMPLE_RATE;
    } else if (strcmp(name, "opus") == 0) {
        opts->audio_codec = ESP_CAPTURE_FMT_ID_OPUS;
        opts->sample_rate = SIP_OPUS_SAMPLE_RATE;
    } else if (strcmp(name, "none") == 0) {
        opts->audio_codec = 0;
    } else {
        printf("Unknown audio codec '%s'; use g711a, g711u, opus or none\n", name);
        return false;
    }
    return true;
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

static bool parse_srtp_mode(const char *name, esp_rtc_srtp_mode_t *out_mode)
{
    if (strcmp(name, "off") == 0) {
        *out_mode = ESP_RTC_SRTP_OFF;
    } else if (strcmp(name, "prefer") == 0) {
        *out_mode = ESP_RTC_SRTP_PREFER;
    } else if (strcmp(name, "required") == 0) {
        *out_mode = ESP_RTC_SRTP_REQUIRED;
    } else {
        printf("Unknown SRTP mode '%s'; use off, prefer or required\n", name);
        return false;
    }
    return true;
}

static bool parse_resolution(const char *value, sip_session_opts_t *opts)
{
    unsigned width = 0;
    unsigned height = 0;
    if (sscanf(value, "%ux%u", &width, &height) != 2 || width == 0 || height == 0) {
        printf("Bad resolution '%s'; expected <width>x<height>, for example 320x240\n", value);
        return false;
    }
    opts->width = (uint16_t)width;
    opts->height = (uint16_t)height;
    return true;
}

/* "192.168.1.10:5080" -> server plus port; a bare host keeps the default port */
static void parse_server(const char *value, sip_session_opts_t *opts, char *server, size_t server_len)
{
    snprintf(server, server_len, "%s", value);
    char *colon = strrchr(server, ':');
    if (colon != NULL) {
        *colon = '\0';
        opts->server_port = (uint16_t)strtoul(colon + 1, NULL, 10);
    }
    opts->server = server;
}

static bool parse_start_options(int argc, char **argv, sip_session_opts_t *opts,
                                char *server, size_t server_len)
{
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;
        bool needs_value = (strcmp(arg, "--p2p") != 0);

        if (needs_value && value == NULL) {
            printf("Option '%s' needs a value\n", arg);
            return false;
        }

        if (strcmp(arg, "-u") == 0) {
            opts->user = value;
            i++;
        } else if (strcmp(arg, "-w") == 0) {
            opts->password = (strcmp(value, "-") == 0) ? "" : value;
            i++;
        } else if (strcmp(arg, "-s") == 0) {
            parse_server(value, opts, server, server_len);
            i++;
        } else if (strcmp(arg, "-t") == 0) {
            if (strcmp(value, "udp") != 0 && strcmp(value, "tcp") != 0 && strcmp(value, "tls") != 0) {
                printf("Unknown transport '%s'; use udp, tcp or tls\n", value);
                return false;
            }
            opts->transport = value;
            i++;
        } else if (strcmp(arg, "-a") == 0) {
            if (!parse_audio_codec(value, opts)) {
                return false;
            }
            i++;
        } else if (strcmp(arg, "-v") == 0) {
            if (!parse_video_codec(value, &opts->video_codec)) {
                return false;
            }
            i++;
        } else if (strcmp(arg, "--srtp") == 0) {
            if (!parse_srtp_mode(value, &opts->srtp_mode)) {
                return false;
            }
            i++;
        } else if (strcmp(arg, "--port") == 0) {
            opts->local_port = (uint16_t)strtoul(value, NULL, 10);
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
        } else if (strcmp(arg, "--p2p") == 0) {
            opts->p2p_mode = true;
        } else {
            printf("Unknown option '%s'\n", arg);
            return false;
        }
    }
    return true;
}

static int sip_start_command(int argc, char **argv)
{
    sip_session_opts_t opts = {0};
    sip_session_opts_default(&opts);

    char server[SIP_CLI_SERVER_MAX_LEN];
    if (!parse_start_options(argc, argv, &opts, server, sizeof(server))) {
        print_usage();
        return 1;
    }

    esp_err_t ret = sip_session_start(&opts);
    if (ret != ESP_OK) {
        printf("sip start failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

/* Joins the remaining words so a message can be typed without quoting */
static int sip_msg_command(int argc, char **argv)
{
    const char *peer = NULL;
    int first = 2;
    if (argc > 3 && strcmp(argv[2], "-t") == 0) {
        peer = argv[3];
        first = 4;
    }
    if (first >= argc) {
        printf("Usage: sip msg [-t <uri>] <text...>\n");
        return 1;
    }

    char text[SIP_CLI_TEXT_MAX_LEN] = {0};
    size_t used = 0;
    bool truncated = false;
    for (int i = first; i < argc; i++) {
        size_t avail = sizeof(text) - used;
        if (avail <= 1) {
            truncated = true;
            break;
        }
        int written = snprintf(text + used, avail, "%s%s", (i > first) ? " " : "", argv[i]);
        if (written <= 0) {
            break;
        }
        if ((size_t)written >= avail) {
            truncated = true;
            used = sizeof(text) - 1;
            break;
        }
        used += (size_t)written;
    }
    if (truncated) {
        printf("Message truncated to %d bytes\n", SIP_CLI_TEXT_MAX_LEN - 1);
    }

    esp_err_t ret = sip_session_send_message(peer, text);
    if (ret != ESP_OK) {
        printf("sip msg failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

static int sip_command(int argc, char **argv)
{
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *sub = argv[1];
    esp_err_t ret = ESP_OK;

    if (strcmp(sub, "info") == 0) {
        sip_session_print_info();
        return 0;
    }
    if (strcmp(sub, "start") == 0) {
        return sip_start_command(argc, argv);
    }
    if (strcmp(sub, "stop") == 0) {
        if (!sip_session_is_active()) {
            printf("No session is running\n");
            return 0;
        }
        ret = sip_session_stop();
    } else if (strcmp(sub, "call") == 0) {
        ret = sip_session_call((argc >= 3) ? argv[2] : SIP_DEFAULT_PEER);
    } else if (strcmp(sub, "answer") == 0) {
        ret = sip_session_answer();
    } else if (strcmp(sub, "bye") == 0) {
        ret = sip_session_hangup();
    } else if (strcmp(sub, "dtmf") == 0) {
        if (argc < 3) {
            printf("Usage: sip dtmf <digit>\n");
            return 1;
        }
        ret = sip_session_send_dtmf(argv[2][0]);
    } else if (strcmp(sub, "msg") == 0) {
        return sip_msg_command(argc, argv);
    } else if (strcmp(sub, "auto") == 0) {
        if (argc < 3 || (strcmp(argv[2], "on") != 0 && strcmp(argv[2], "off") != 0)) {
            printf("Usage: sip auto <on|off>\n");
            return 1;
        }
        sip_session_set_auto_answer(strcmp(argv[2], "on") == 0);
        return 0;
    } else {
        printf("Unknown subcommand '%s'\n", sub);
        print_usage();
        return 1;
    }

    if (ret != ESP_OK) {
        printf("sip %s failed: %s\n", sub, esp_err_to_name(ret));
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

    const char *ssid = SIP_WIFI_SSID;
    const char *password = SIP_WIFI_PASSWORD;
    if (argc >= 2) {
        ssid = argv[1];
    }
    if (argc >= 3) {
        password = (strcmp(argv[2], "-") == 0) ? "" : argv[2];
    }

    esp_err_t ret = sip_example_wifi_connect(ssid, password);
    if (ret != ESP_OK) {
        printf("wifi connect failed: %s\n", esp_err_to_name(ret));
        return 1;
    }
    return 0;
}

esp_err_t sip_cli_start(void)
{
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.base_cfg.name = "sip_cli";
    cfg.prompt = "sip> ";
    cfg.max_cmdline_length = SIP_CLI_CMDLINE_MAX_LEN;
    cfg.task_stack = 8192;

    ESP_RETURN_ON_ERROR(esp_cli_service_create(&cfg, &s_cli), TAG, "Create CLI service");

    const esp_console_cmd_t commands[] = {
        {
            .command = "sip",
            .help = "SIP control: sip <start|stop|call|answer|bye|dtmf|msg|auto|info> [options]",
            .func = sip_command,
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
