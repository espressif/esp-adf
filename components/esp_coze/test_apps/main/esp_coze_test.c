/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "unity.h"

#include "esp_coze_chat.h"
#include "esp_coze_tts.h"
#include "esp_coze_asr.h"
#include "esp_coze_jwt.h"

/* Placeholder credentials. The test suite never opens a WebSocket session, so the
 * values below are only forwarded to the SDK's argument-validation code paths. */
#define COZE_TEST_BOT_ID        "7480******************"
#define COZE_TEST_ACCESS_TOKEN  "pat_******************"
#define COZE_TEST_VOICE_ID      "7426720361733144585"
#define COZE_TEST_USER_ID       "test_user"

static const char *TAG = "COZE_TEST";

#define DEVICE_PUBLIC_KEY  "ZomHzG_rqYph5eRjuy786tghyO6LyfSCF84_uGrKBqw"

static const char device_private_pem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQChkZmcn8YJgscI\n"
    "lTNbTJhZwoUwuD/eqUyoKuSTiOzrdV5uW2rwr5ehTjHVpvW+XwFfRQjaZMxa6/as\n"
    "iPk3oLEjYIMH48Y0zoFkjojDBHQWBJxMnYNs2GLzkVctJYPafjUZE+2CxvnFzP/4\n"
    "HRh7g0vhdHEchLoJaGk2F0Ue1cXXmCrLcMohwPGnt2lMLUo9YoFWASzgdrHBVzX7\n"
    "5DoCSgS1qPK/i0QhUkYX6eZ+FyM+MtJsBJcz/03HDo94VIZuYG2AG5qAJDVvUNfK\n"
    "oA2+oAWD1awMpEChVrcz/JmaXWAB/SQFQ9lMqQrr6FTGUs2/3FT0iJpqQbtqzIYx\n"
    "6tQBel8zAgMBAAECggEAPIMdPtGCaPZ/mAHVecjeuKOHod8oILtU2Bk0geNDJM7R\n"
    "HtEDZQDG8CWquYPuPy4zA90Fu1IMTbNZsHYerQx0u7S+bpqLFiUPxjsyEqRc3BA+\n"
    "/9zwp7gFm4bgWSrXTuJ8/Zb+mj/utXwZlHJFRKFArsCfSpiVWsetvBIME8fp8NxW\n"
    "6lQoEWWlQ6X85lvhShizNNOJF5O0i64vu5sYM4WO/6i6ifsRcBtOjzX0ovEEVgxt\n"
    "GY38TH9BnRhJXEyZ7YTl9wIkIahnfrgcfTuCvpCkPodB71uq1Cu0W8V5A7OvRwd2\n"
    "tzYH4oACefgub08qQJZB7VEHrqnEUtu5TcrdlwG7BQKBgQC+//CLD0yCAceevrO2\n"
    "PTt5XdHFHjHPyfmyQVsrPi0Lb0rdTK52JRbibrcoYFxUAHCOMKZQYK2MDkFWICGr\n"
    "CZLEbaxhKz/fD/L6alSBFeezhGlnVYbeyIlzvg/ruuZ/GbTQt1fxUf7M7E/QH9NO\n"
    "aTwrlSWlXSEPqI1NZkMcAhks1wKBgQDYjZsOiDlIbLDuc/S5ZW3nAvowZwxxCnWg\n"
    "9F/k3n3+ti5VwBEMIi11dXjvfbDjWU5RqlqkuohmyaQsa9006OwZc7iEPo/QcdIc\n"
    "a2hp5COAjxiTjoWLxo4LTCUwZc0G/9B96ai6YW3W3uMJ6jISk2YcOa3CJxu6OSzh\n"
    "M8hZWFCZBQKBgF32aW7v8tKOb5b/+EcyLn0Rk64moZi4o1d3YZOPffd/I9Rk8fr7\n"
    "WhvBHegGJ8XHhZfeBSPLu/UH2kq1efq7tfFehwwyi7SAEyfwgWwTy637+SK156jX\n"
    "/Q7stMZiZxymF9cKK4Bztyc3JjicP4b2rHxAXonQnAApCoLGSUORILN3AoGAe7jz\n"
    "xOq5Z5KJx8LCKAY2M0z1KiBF8HOcXvgfrYAcliD5+g4A72C7fic+j/3MySel4myk\n"
    "2wajla5QuPxrZqZI9gWyfwPLYLkW5RvMjOtGCVY4IV3FGOW5E+VOPgd7iysusDGG\n"
    "L+4oNiIjru956jkCls+xYYK8ibnO1V+jcMBPVA0CgYEAqufoMXmoxn0vZ8eaVWjg\n"
    "DTp6IDPB8hKfmw+A/Ll1+2WZqRjA7QwgIBV6aTflGa0Q3ybz67ECF14SFF2rj7vX\n"
    "NdA7e3IMhFv+9ln0Gg2qoSNdqH7fb+CVZwb0f9CJcs65ZaKClmQwtGGTk1c+cq7Y\n"
    "hg/YuNckD3PFGiJiR+6Rjas=\n"
    "-----END PRIVATE KEY-----\n";

/* ------------------------------------------------------------------ */
/* Shared helpers                                                     */
/* ------------------------------------------------------------------ */

static void generate_random_string(char *output, size_t length)
{
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    size_t charset_size = sizeof(charset) - 1;
    for (size_t i = 0; i < length; i++) {
        output[i] = charset[esp_random() % charset_size];
    }
    output[length] = '\0';
}

/** Compact JWS must be three '.'-separated segments (header.payload.signature). */
static int jwt_compact_dot_count(const char *jwt)
{
    int n = 0;
    for (const char *p = jwt; *p != '\0'; p++) {
        if (*p == '.') {
            n++;
        }
    }
    return n;
}

static char *build_full_session_payload(void)
{
    cJSON *payload_json = cJSON_CreateObject();
    if (payload_json == NULL) {
        return NULL;
    }

    char random_str[33] = {0};
    generate_random_string(random_str, 32);

    time_t now = time(NULL);
    cJSON_AddStringToObject(payload_json, "iss", "1234567");
    cJSON_AddStringToObject(payload_json, "aud", "api.coze.cn");
    cJSON_AddNumberToObject(payload_json, "iat", (double)now);
    cJSON_AddNumberToObject(payload_json, "exp", (double)(now + 6000));
    cJSON_AddStringToObject(payload_json, "jti", random_str);
    cJSON_AddStringToObject(payload_json, "session_name", "test");

    cJSON *session_context = cJSON_CreateObject();
    cJSON *device_info = cJSON_CreateObject();
    if (session_context == NULL || device_info == NULL) {
        cJSON_Delete(device_info);
        cJSON_Delete(session_context);
        cJSON_Delete(payload_json);
        return NULL;
    }

    cJSON_AddStringToObject(device_info, "device_id", "1234567");
    cJSON_AddStringToObject(device_info, "custom_consumer", "test");
    cJSON_AddItemToObject(session_context, "device_info", device_info);
    cJSON_AddItemToObject(payload_json, "session_context", session_context);

    char *payload_str = cJSON_PrintUnformatted(payload_json);
    cJSON_Delete(payload_json);
    return payload_str;
}

/** Counters used by callback tests to confirm we never receive any spontaneous
 *  events while we are merely init/deinit'ing (no WebSocket connection). */
typedef struct {
    int  events;
    int  audios;
} cb_counters_t;

static void chat_event_cb(esp_coze_chat_event_t event, void *event_data, void *ctx)
{
    (void)event;
    (void)event_data;
    cb_counters_t *c = (cb_counters_t *)ctx;
    if (c) {
        c->events++;
    }
}

static void chat_audio_cb(const uint8_t *data, int len, void *ctx)
{
    (void)data;
    (void)len;
    cb_counters_t *c = (cb_counters_t *)ctx;
    if (c) {
        c->audios++;
    }
}

static void tts_event_cb(esp_coze_tts_event_t event, void *event_data, void *ctx)
{
    (void)event;
    (void)event_data;
    cb_counters_t *c = (cb_counters_t *)ctx;
    if (c) {
        c->events++;
    }
}

static void tts_audio_cb(const uint8_t *data, int len, void *ctx)
{
    (void)data;
    (void)len;
    cb_counters_t *c = (cb_counters_t *)ctx;
    if (c) {
        c->audios++;
    }
}

static void asr_event_cb(esp_coze_asr_event_t event, void *event_data, void *ctx)
{
    (void)event;
    (void)event_data;
    cb_counters_t *c = (cb_counters_t *)ctx;
    if (c) {
        c->events++;
    }
}

/* ------------------------------------------------------------------ */
/* JWT                                                                */
/* ------------------------------------------------------------------ */

TEST_CASE("esp_coze JWT: argument validation", "[esp_coze][jwt]")
{
    const uint8_t *pem = (const uint8_t *)device_private_pem;
    const size_t pem_len = strlen(device_private_pem);
    const char *minimal_payload = "{\"iss\":\"api_test\"}";

    TEST_ASSERT_NULL(esp_coze_jwt_create(NULL, minimal_payload, pem, pem_len));
    TEST_ASSERT_NULL(esp_coze_jwt_create(DEVICE_PUBLIC_KEY, NULL, pem, pem_len));
    TEST_ASSERT_NULL(esp_coze_jwt_create(DEVICE_PUBLIC_KEY, minimal_payload, NULL, pem_len));
    TEST_ASSERT_NULL(esp_coze_jwt_create(DEVICE_PUBLIC_KEY, minimal_payload, pem, 0));
}

TEST_CASE("esp_coze JWT: sequential sign calls", "[esp_coze][jwt]")
{
    const uint8_t *pem = (const uint8_t *)device_private_pem;
    const size_t pem_len = strlen(device_private_pem);

    char *jwt1 = esp_coze_jwt_create(DEVICE_PUBLIC_KEY, "{\"iss\":\"api_test\"}",
                                     pem, pem_len);
    TEST_ASSERT_NOT_NULL_MESSAGE(jwt1, "first JWT creation failed");
    ESP_LOGI(TAG, "sequential jwt1: %s", jwt1);
    free(jwt1);

    char *payload_str = build_full_session_payload();
    TEST_ASSERT_NOT_NULL(payload_str);

    char *jwt2 = esp_coze_jwt_create(DEVICE_PUBLIC_KEY, payload_str, pem, pem_len);
    free(payload_str);

    TEST_ASSERT_NOT_NULL_MESSAGE(jwt2, "second JWT creation failed after a successful first call");
    TEST_ASSERT_EQUAL(2, jwt_compact_dot_count(jwt2));
    ESP_LOGI(TAG, "sequential jwt2: %s", jwt2);
    free(jwt2);
}

TEST_CASE("esp_coze JWT: minimal claims sign successfully", "[esp_coze][jwt]")
{
    char *jwt = esp_coze_jwt_create(DEVICE_PUBLIC_KEY, "{\"iss\":\"api_test\"}",
                                    (const uint8_t *)device_private_pem,
                                    strlen(device_private_pem));
    TEST_ASSERT_NOT_NULL(jwt);
    TEST_ASSERT_EQUAL(2, jwt_compact_dot_count(jwt));
    TEST_ASSERT_GREATER_THAN(32, strlen(jwt));
    ESP_LOGI(TAG, "minimal jwt: %s", jwt);
    free(jwt);
}

TEST_CASE("esp_coze JWT: full session payload", "[esp_coze][jwt]")
{
    char *payload_str = build_full_session_payload();
    TEST_ASSERT_NOT_NULL(payload_str);

    char *jwt = esp_coze_jwt_create(DEVICE_PUBLIC_KEY, payload_str,
                                    (const uint8_t *)device_private_pem,
                                    strlen(device_private_pem));
    free(payload_str);

    TEST_ASSERT_NOT_NULL(jwt);
    TEST_ASSERT_EQUAL(2, jwt_compact_dot_count(jwt));
    ESP_LOGI(TAG, "jwt: %s", jwt);
    free(jwt);
}

/* ------------------------------------------------------------------ */
/* Chat facade                                                        */
/* ------------------------------------------------------------------ */

TEST_CASE("esp_coze chat: invalid args rejected", "[esp_coze][chat]")
{
    esp_coze_chat_handle_t chat = NULL;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_init(NULL, &chat));
    TEST_ASSERT_NULL(chat);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_deinit(NULL));

    {
        esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;  /* missing bot_id */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_init(&cfg, &chat));
    }
    {
        esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;  /* missing access_token */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_init(&cfg, &chat));
    }
    {
        esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;
        cfg.voice_id = NULL;  /* missing voice_id */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_init(&cfg, &chat));
    }
    {
        esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;
        cfg.ws_base_url = NULL;  /* missing ws_base_url */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_init(&cfg, &chat));
    }
}

TEST_CASE("esp_coze chat: init/deinit", "[esp_coze][chat]")
{
    esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
    cfg.bot_id = COZE_TEST_BOT_ID;
    cfg.access_token = COZE_TEST_ACCESS_TOKEN;

    esp_coze_chat_handle_t chat = NULL;
    TEST_ESP_OK(esp_coze_chat_init(&cfg, &chat));
    TEST_ASSERT_NOT_NULL(chat);
    TEST_ESP_OK(esp_coze_chat_deinit(chat));
}

TEST_CASE("esp_coze chat: init with callbacks/deinit", "[esp_coze][chat]")
{
    cb_counters_t cnt = {0};
    esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
    cfg.bot_id = COZE_TEST_BOT_ID;
    cfg.access_token = COZE_TEST_ACCESS_TOKEN;
    cfg.event_callback = chat_event_cb;
    cfg.audio_callback = chat_audio_cb;
    cfg.event_callback_ctx = &cnt;
    cfg.audio_callback_ctx = &cnt;
    cfg.enable_subtitle = true;

    esp_coze_chat_handle_t chat = NULL;
    TEST_ESP_OK(esp_coze_chat_init(&cfg, &chat));
    TEST_ESP_OK(esp_coze_chat_deinit(chat));

    /* No spurious callbacks while we never opened the session. */
    TEST_ASSERT_EQUAL_INT(0, cnt.events);
    TEST_ASSERT_EQUAL_INT(0, cnt.audios);
}

TEST_CASE("esp_coze chat: lifecycle/send APIs reject NULL handle", "[esp_coze][chat]")
{
    char dummy_audio[] = {0x00, 0x01, 0x02, 0x03};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_start(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_stop(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      esp_coze_chat_send_audio_data(NULL, dummy_audio, sizeof(dummy_audio)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_audio_data_clearup(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_send_audio_complete(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_send_audio_cancel(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_update_chat(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_send_customer_data(NULL, "{}"));
}

TEST_CASE("esp_coze chat: setters validate args / round-trip conversation_id",
          "[esp_coze][chat]")
{
    esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
    cfg.bot_id = COZE_TEST_BOT_ID;
    cfg.access_token = COZE_TEST_ACCESS_TOKEN;

    esp_coze_chat_handle_t chat = NULL;
    TEST_ESP_OK(esp_coze_chat_init(&cfg, &chat));

    /* voice id setter */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_set_chat_config_voice_id(NULL, "v"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_set_chat_config_voice_id(chat, NULL));
    TEST_ESP_OK(esp_coze_set_chat_config_voice_id(chat, "7426720361733144585"));

    /* parameters: NULL value is rejected */
    {
        esp_coze_parameters_kv_t bad_kv[] = {
            {.key = (char *)"k", .value = NULL},
            {.key = NULL, .value = NULL},
        };
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_set_chat_config_parameters(chat, bad_kv));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_set_chat_config_parameters(NULL, bad_kv));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_set_chat_config_parameters(chat, NULL));
    }
    /* parameters: ok */
    {
        esp_coze_parameters_kv_t ok_kv[] = {
            {.key = (char *)"k", .value = (char *)"v"},
            {.key = NULL, .value = NULL},
        };
        TEST_ESP_OK(esp_coze_set_chat_config_parameters(chat, ok_kv));
    }

    /* conversation id: not set yet */
    char *conv = NULL;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_get_config_conversation_id(NULL, &conv));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_chat_get_config_conversation_id(chat, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_coze_chat_get_config_conversation_id(chat, &conv));
    TEST_ASSERT_NULL(conv);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_set_chat_config_conversation_id(NULL, "x"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_set_chat_config_conversation_id(chat, NULL));
    TEST_ESP_OK(esp_coze_set_chat_config_conversation_id(chat, "conv_xyz"));

    TEST_ESP_OK(esp_coze_chat_get_config_conversation_id(chat, &conv));
    TEST_ASSERT_NOT_NULL(conv);
    TEST_ASSERT_EQUAL_STRING("conv_xyz", conv);
    free(conv);

    TEST_ESP_OK(esp_coze_chat_deinit(chat));
}

/* ------------------------------------------------------------------ */
/* TTS facade                                                         */
/* ------------------------------------------------------------------ */

TEST_CASE("esp_coze TTS: invalid args rejected", "[esp_coze][tts]")
{
    esp_coze_tts_handle_t tts = NULL;

    /* NULL config / NULL out */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_init(NULL, &tts));
    {
        esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_init(&cfg, NULL));
    }

    /* Required strings */
    {
        esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;  /* missing bot_id */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_init(&cfg, &tts));
    }
    {
        esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;  /* missing access_token */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_init(&cfg, &tts));
    }
    {
        esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;
        cfg.voice_id = NULL;  /* missing voice_id */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_init(&cfg, &tts));
    }
    {
        esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;
        cfg.user_id = NULL;  /* missing user_id */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_init(&cfg, &tts));
    }
    {
        esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;
        cfg.ws_base_url = NULL;  /* missing ws_base_url */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_init(&cfg, &tts));
    }

    /* deinit(NULL) is a tolerated no-op */
    TEST_ESP_OK(esp_coze_tts_deinit(NULL));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_start(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_stop(NULL));
}

TEST_CASE("esp_coze TTS: init/deinit", "[esp_coze][tts]")
{
    esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
    cfg.bot_id = COZE_TEST_BOT_ID;
    cfg.access_token = COZE_TEST_ACCESS_TOKEN;

    esp_coze_tts_handle_t tts = NULL;
    TEST_ESP_OK(esp_coze_tts_init(&cfg, &tts));
    TEST_ASSERT_NOT_NULL(tts);
    TEST_ASSERT_FALSE(esp_coze_tts_is_connected(tts));
    TEST_ESP_OK(esp_coze_tts_deinit(tts));

    /* is_connected on NULL handle must be safe and return false. */
    TEST_ASSERT_FALSE(esp_coze_tts_is_connected(NULL));
}

TEST_CASE("esp_coze TTS: init with callbacks/deinit", "[esp_coze][tts]")
{
    cb_counters_t cnt = {0};

    esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
    cfg.bot_id = COZE_TEST_BOT_ID;
    cfg.access_token = COZE_TEST_ACCESS_TOKEN;
    cfg.event_callback = tts_event_cb;
    cfg.audio_callback = tts_audio_cb;
    cfg.event_callback_ctx = &cnt;
    cfg.audio_callback_ctx = &cnt;
    cfg.downlink_audio_type = ESP_COZE_AUDIO_TYPE_OPUS;
    cfg.downlink_bitrate = 32000;
    cfg.downlink_frame_size_ms = 60;
    cfg.downlink_speech_rate = 0;

    esp_coze_tts_handle_t tts = NULL;
    TEST_ESP_OK(esp_coze_tts_init(&cfg, &tts));
    TEST_ESP_OK(esp_coze_tts_deinit(tts));

    TEST_ASSERT_EQUAL_INT(0, cnt.events);
    TEST_ASSERT_EQUAL_INT(0, cnt.audios);
}

TEST_CASE("esp_coze TTS: send_text validates args / requires connection",
          "[esp_coze][tts]")
{
    esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
    cfg.bot_id = COZE_TEST_BOT_ID;
    cfg.access_token = COZE_TEST_ACCESS_TOKEN;

    esp_coze_tts_handle_t tts = NULL;
    TEST_ESP_OK(esp_coze_tts_init(&cfg, &tts));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_send_text(NULL, "hi"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_tts_send_text(tts, NULL));

    /* Without esp_coze_tts_start() the WebSocket is not connected; the SDK must
     * refuse the send instead of going through. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_coze_tts_send_text(tts, "hello"));

    TEST_ESP_OK(esp_coze_tts_deinit(tts));
}

/* ------------------------------------------------------------------ */
/* ASR facade                                                         */
/* ------------------------------------------------------------------ */

TEST_CASE("esp_coze ASR: invalid args rejected", "[esp_coze][asr]")
{
    esp_coze_asr_handle_t asr = NULL;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_init(NULL, &asr));
    {
        esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_init(&cfg, NULL));
    }

    {
        esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;  /* missing bot_id */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_init(&cfg, &asr));
    }
    {
        esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;  /* missing access_token */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_init(&cfg, &asr));
    }
    {
        esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
        cfg.bot_id = COZE_TEST_BOT_ID;
        cfg.access_token = COZE_TEST_ACCESS_TOKEN;
        cfg.ws_base_url = NULL;  /* missing ws_base_url */
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_init(&cfg, &asr));
    }

    TEST_ESP_OK(esp_coze_asr_deinit(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_start(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_stop(NULL));
}

TEST_CASE("esp_coze ASR: init/deinit (server VAD default)", "[esp_coze][asr]")
{
    esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
    cfg.bot_id = COZE_TEST_BOT_ID;
    cfg.access_token = COZE_TEST_ACCESS_TOKEN;

    esp_coze_asr_handle_t asr = NULL;
    TEST_ESP_OK(esp_coze_asr_init(&cfg, &asr));
    TEST_ASSERT_NOT_NULL(asr);
    TEST_ASSERT_FALSE(esp_coze_asr_is_connected(asr));
    TEST_ESP_OK(esp_coze_asr_deinit(asr));

    TEST_ASSERT_FALSE(esp_coze_asr_is_connected(NULL));
}

TEST_CASE("esp_coze ASR: init with manual turn detection / callbacks",
          "[esp_coze][asr]")
{
    cb_counters_t cnt = {0};

    esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
    cfg.bot_id = COZE_TEST_BOT_ID;
    cfg.access_token = COZE_TEST_ACCESS_TOKEN;
    cfg.user_id = "test_user";
    cfg.uplink_audio_type = ESP_COZE_AUDIO_TYPE_PCM;
    cfg.turn_detection = ESP_COZE_ASR_TURN_DETECTION_NONE;
    cfg.event_callback = asr_event_cb;
    cfg.event_callback_ctx = &cnt;

    esp_coze_asr_handle_t asr = NULL;
    TEST_ESP_OK(esp_coze_asr_init(&cfg, &asr));
    TEST_ESP_OK(esp_coze_asr_deinit(asr));

    TEST_ASSERT_EQUAL_INT(0, cnt.events);
}

TEST_CASE("esp_coze ASR: send_audio / clear / complete validate args / state",
          "[esp_coze][asr]")
{
    esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
    cfg.bot_id = COZE_TEST_BOT_ID;
    cfg.access_token = COZE_TEST_ACCESS_TOKEN;

    esp_coze_asr_handle_t asr = NULL;
    TEST_ESP_OK(esp_coze_asr_init(&cfg, &asr));

    /* Argument validation */
    uint8_t dummy[16] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_send_audio(NULL, dummy, sizeof(dummy)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_send_audio(asr, NULL, sizeof(dummy)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_send_audio(asr, dummy, 0));

    /* Without start() the WebSocket is not connected; send must refuse. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_coze_asr_send_audio(asr, dummy, sizeof(dummy)));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_send_audio_complete(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_coze_asr_clear_buffer(NULL));

    TEST_ESP_OK(esp_coze_asr_deinit(asr));
}

/* ------------------------------------------------------------------ */
/* Cross-facade smoke: the three modules can coexist                  */
/* ------------------------------------------------------------------ */

TEST_CASE("esp_coze: chat + tts + asr can coexist", "[esp_coze][multi]")
{
    /* Chat */
    esp_coze_chat_config_t cc = ESP_COZE_CHAT_DEFAULT_CONFIG();
    cc.bot_id = COZE_TEST_BOT_ID;
    cc.access_token = COZE_TEST_ACCESS_TOKEN;
    esp_coze_chat_handle_t chat = NULL;
    TEST_ESP_OK(esp_coze_chat_init(&cc, &chat));

    /* TTS */
    esp_coze_tts_config_t tc = ESP_COZE_TTS_DEFAULT_CONFIG();
    tc.bot_id = COZE_TEST_BOT_ID;
    tc.access_token = COZE_TEST_ACCESS_TOKEN;
    esp_coze_tts_handle_t tts = NULL;
    TEST_ESP_OK(esp_coze_tts_init(&tc, &tts));

    /* ASR */
    esp_coze_asr_config_t ac = ESP_COZE_ASR_DEFAULT_CONFIG();
    ac.bot_id = COZE_TEST_BOT_ID;
    ac.access_token = COZE_TEST_ACCESS_TOKEN;
    esp_coze_asr_handle_t asr = NULL;
    TEST_ESP_OK(esp_coze_asr_init(&ac, &asr));

    TEST_ASSERT_FALSE(esp_coze_tts_is_connected(tts));
    TEST_ASSERT_FALSE(esp_coze_asr_is_connected(asr));

    /* Tear down in arbitrary order. */
    TEST_ESP_OK(esp_coze_asr_deinit(asr));
    TEST_ESP_OK(esp_coze_tts_deinit(tts));
    TEST_ESP_OK(esp_coze_chat_deinit(chat));
}
