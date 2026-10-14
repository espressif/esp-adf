// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_BASE_VOLC_BASE_H__
#define __CONV_AI_BASE_VOLC_BASE_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#include "util/volc_list.h"
#include "volc_conv_ai.h"

// TODO: internal or external
typedef enum {
    VOLC_MSG_CONNECTED = 0,        // 成功连接
    VOLC_MSG_DISCONNECTED,         // 断开连接
    VOLC_MSG_USER_JOINED,          // 用户加入
    VOLC_MSG_USER_OFFLINE,            // 用户离开
    VOLC_MSG_APP_ID_INVALID,
    VOLC_MSG_TOKEN_INVALID,
    VOLC_MSG_TOKEN_EXPIRED,
    VOLC_MSG_LICENSE_EXPIRED,
    VOLC_MSG_KEY_FRAME_REQ,          // 关键帧请求
    VOLC_MSG_TARGET_BITRATE_CHANGED, // 目标码率变化
    VOLC_MSG_CONV_STATUS,          // 会话状态
    VOLC_MSG_QUOTA_EXCEEDED,     // 资源耗尽
} volc_msg_e;

typedef struct {
  volc_msg_e code;
  union {
    uint32_t target_bitrate;
    uint32_t conv_status;
    char* msg;
  } data;
} volc_msg_t;

typedef void(*volc_msg_cb)(void* context, volc_msg_t *msg);
typedef void(*volc_data_cb)(void* context, const void* buffer, size_t len, volc_data_info_t *info);


#ifdef __cplusplus
}
#endif
#endif /* __CONV_AI_BASE_VOLC_BASE_H__ */
