/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "esp_tongyi.h"
#include "esp_tongyi_protocol.h"

static char *g_started_dialog;
static char *g_state;
static char *g_asr;
static int g_speech_started;
static int g_responding_started;
static int g_responding_ended;
static int g_task_failed;
static int g_error;

static char *dup_or_null(const char *s)
{
    return s == NULL ? NULL : strdup(s);
}

static void reset_parse_sink(void)
{
    free(g_started_dialog);
    free(g_state);
    free(g_asr);
    g_started_dialog = NULL;
    g_state = NULL;
    g_asr = NULL;
    g_speech_started = 0;
    g_responding_started = 0;
    g_responding_ended = 0;
    g_task_failed = 0;
    g_error = 0;
}

static void on_started(void *ctx, const char *dialog_id)
{
    (void)ctx;
    free(g_started_dialog);
    g_started_dialog = dup_or_null(dialog_id);
}

static void on_state(void *ctx, const char *state_str)
{
    (void)ctx;
    free(g_state);
    g_state = dup_or_null(state_str);
}

static void on_speech_started(void *ctx)
{
    (void)ctx;
    g_speech_started++;
}

static void on_responding_started(void *ctx)
{
    (void)ctx;
    g_responding_started++;
}

static void on_responding_ended(void *ctx)
{
    (void)ctx;
    g_responding_ended++;
}

static void on_speech_content(void *ctx, const char *asr_text)
{
    (void)ctx;
    free(g_asr);
    g_asr = dup_or_null(asr_text);
}

static void on_responding_content(void *ctx, const char *raw_message)
{
    (void)ctx;
    (void)raw_message;
}

static void on_error(void *ctx, const char *raw_message)
{
    (void)ctx;
    (void)raw_message;
    g_error++;
}

static void on_task_failed(void *ctx, const char *raw_message)
{
    (void)ctx;
    (void)raw_message;
    g_task_failed++;
}

static esp_tongyi_proto_parse_handler_t make_handler(void)
{
    esp_tongyi_proto_parse_handler_t h = {
        .on_started = on_started,
        .on_dialog_state_changed = on_state,
        .on_speech_started = on_speech_started,
        .on_responding_started = on_responding_started,
        .on_responding_ended = on_responding_ended,
        .on_speech_content = on_speech_content,
        .on_responding_content = on_responding_content,
        .on_error = on_error,
        .on_task_failed = on_task_failed,
    };
    return h;
}

TEST_CASE("parse rejects null arguments", "[esp_tongyi]")
{
    esp_tongyi_proto_parse_handler_t h = make_handler();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_tongyi_proto_parse_message(NULL, &h));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_tongyi_proto_parse_message("{}", NULL));
}

TEST_CASE("parse rejects illegal JSON", "[esp_tongyi]")
{
    esp_tongyi_proto_parse_handler_t h = make_handler();
    TEST_ASSERT_EQUAL(ESP_FAIL, esp_tongyi_proto_parse_message("{not json", &h));
}

TEST_CASE("parse Started and dialog events", "[esp_tongyi]")
{
    reset_parse_sink();
    esp_tongyi_proto_parse_handler_t h = make_handler();
    const char *started =
        "{\"header\":{\"event\":\"result-generated\"},"
        "\"payload\":{\"output\":{\"event\":\"Started\",\"dialog_id\":\"dlg-1\"}}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_tongyi_proto_parse_message(started, &h));
    TEST_ASSERT_EQUAL_STRING("dlg-1", g_started_dialog);

    const char *state =
        "{\"header\":{\"event\":\"result-generated\"},"
        "\"payload\":{\"output\":{\"event\":\"DialogStateChanged\",\"state\":\"Listening\"}}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_tongyi_proto_parse_message(state, &h));
    TEST_ASSERT_EQUAL_STRING("Listening", g_state);

    const char *speech =
        "{\"header\":{\"event\":\"result-generated\"},"
        "\"payload\":{\"output\":{\"event\":\"SpeechContent\",\"text\":\"hello\"}}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_tongyi_proto_parse_message(speech, &h));
    TEST_ASSERT_EQUAL_STRING("hello", g_asr);

    const char *failed = "{\"header\":{\"event\":\"task-failed\"},\"payload\":{}}";
    TEST_ASSERT_EQUAL(ESP_OK, esp_tongyi_proto_parse_message(failed, &h));
    TEST_ASSERT_EQUAL(1, g_task_failed);
    reset_parse_sink();
}

TEST_CASE("build start and directive JSON", "[esp_tongyi]")
{
    esp_tongyi_proto_start_cfg_t cfg = {
        .task_id = "task-1",
        .workspace_id = "ws",
        .app_id = "app",
        .up_type = "AudioOnly",
        .up_mode = "duplex",
        .up_audio_format = "pcm",
        .up_sample_rate = 16000,
        .down_voice = "longanhuan",
        .down_audio_format = "pcm",
        .down_sample_rate = 16000,
    };
    char *start = esp_tongyi_proto_build_start(&cfg, "dlg-2");
    TEST_ASSERT_NOT_NULL(start);
    TEST_ASSERT_NOT_NULL(strstr(start, "\"action\":\"run-task\""));
    TEST_ASSERT_NOT_NULL(strstr(start, "\"directive\":\"Start\""));
    TEST_ASSERT_NOT_NULL(strstr(start, "\"dialog_id\":\"dlg-2\""));
    TEST_ASSERT_NOT_NULL(strstr(start, "\"workspace_id\":\"ws\""));
    TEST_ASSERT_NOT_NULL(strstr(start, "\"user_id\":\"task-1\""));
    free(start);

    char *dir = esp_tongyi_proto_build_directive("task-1", "dlg-2", "HeartBeat");
    TEST_ASSERT_NOT_NULL(dir);
    TEST_ASSERT_NOT_NULL(strstr(dir, "\"action\":\"continue-task\""));
    TEST_ASSERT_NOT_NULL(strstr(dir, "\"directive\":\"HeartBeat\""));
    free(dir);

    char *stop = esp_tongyi_proto_build_stop("task-1", "dlg-2");
    TEST_ASSERT_NOT_NULL(stop);
    TEST_ASSERT_NOT_NULL(strstr(stop, "\"action\":\"finish-task\""));
    TEST_ASSERT_NOT_NULL(strstr(stop, "\"directive\":\"Stop\""));
    free(stop);

    TEST_ASSERT_NULL(esp_tongyi_proto_build_start(NULL, NULL));
    TEST_ASSERT_NULL(esp_tongyi_proto_build_payload_wrap("task-1", "[]"));
}

TEST_CASE("build text and image JSON", "[esp_tongyi]")
{
    char *text = esp_tongyi_proto_build_text("task-1", "dlg-2", "hi", false);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "\"directive\":\"RequestToRespond\""));
    TEST_ASSERT_NOT_NULL(strstr(text, "\"type\":\"prompt\""));
    TEST_ASSERT_NOT_NULL(strstr(text, "\"text\":\"hi\""));
    free(text);

    char *img = esp_tongyi_proto_build_image("task-1", "dlg-2", "QUJD", "what", false);
    TEST_ASSERT_NOT_NULL(img);
    TEST_ASSERT_NOT_NULL(strstr(img, "\"type\":\"base64\""));
    TEST_ASSERT_NOT_NULL(strstr(img, "\"value\":\"QUJD\""));
    TEST_ASSERT_NOT_NULL(strstr(img, "\"text\":\"what\""));
    free(img);

    char *upd = esp_tongyi_proto_build_image("task-1", "dlg-2", "QUJD", NULL, true);
    TEST_ASSERT_NOT_NULL(upd);
    TEST_ASSERT_NOT_NULL(strstr(upd, "\"directive\":\"UpdateInfo\""));
    free(upd);

    char *ch = esp_tongyi_proto_build_video_channel("task-1", "dlg-2");
    TEST_ASSERT_NOT_NULL(ch);
    TEST_ASSERT_NOT_NULL(strstr(ch, "\"voicechat_video_channel\""));
    TEST_ASSERT_NOT_NULL(strstr(ch, "\"action\":\"connect\""));
    free(ch);
}

TEST_CASE("directive name mapping", "[esp_tongyi]")
{
    TEST_ASSERT_EQUAL_STRING("SendSpeech",
                             esp_tongyi_proto_directive_name(ESP_TONGYI_SPEECH_ACTION_SEND));
    TEST_ASSERT_EQUAL_STRING("RequestToSpeak",
                             esp_tongyi_proto_directive_name(ESP_TONGYI_SPEECH_ACTION_REQUEST_TO_SPEAK));
    TEST_ASSERT_NULL(esp_tongyi_proto_directive_name((esp_tongyi_speech_action_t)99));
}
