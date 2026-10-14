// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_SRC_VOLC_DEVICE_MANAGER__
#define __CONV_AI_SRC_VOLC_DEVICE_MANAGER__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HARDWARE_ID "a2:c8:2c:89:6e:46"

#define ERROR_LICENSE_EXHAUSTED 12000130
#define ERROR_LICENSE_EXPIRED   12000140

typedef struct {
    char* instance_id;
    char* product_key;
    char* product_secret;
    char* device_name;
    char* device_secret;
    char* rtc_app_id;
} volc_iot_info_t;

typedef struct {
  char *p_channel_name;
  char *p_uid;
  char *p_token;
} volc_rtc_option_t;

typedef struct {
    volc_rtc_option_t rtc_opt;
    char* task_id; // Task ID for the room
} volc_room_info_t;

int volc_device_register(volc_iot_info_t* info, char** output);
int volc_get_rtc_config(volc_iot_info_t* info, int audio_codec, const char* bot_id, const char* task_id, volc_room_info_t* room_info, const char* params);
char* volc_generate_signature(const char* secret_key, const char* product_key, const char* device_name, int rnd, uint64_t timestamp, int auth_type);
char* volc_generate_signature_ws(const char* secret_key, const char* product_key, const char* device_name, const char* instance_id, int rnd, uint64_t timestamp, int auth_type);

#ifdef __cplusplus
}
#endif
#endif /* __CONV_AI_SRC_VOLC_DEVICE_MANAGER__ */
