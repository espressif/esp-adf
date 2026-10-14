// Copyright (2025) Beijing Volcano Engine Technology Ltd.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CONV_AI_SRC_UTIL_VOLC_LOG_H__
#define __CONV_AI_SRC_UTIL_VOLC_LOG_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        VOLC_LOG_LEVEL_ERROR,
        VOLC_LOG_LEVEL_WARN,
        VOLC_LOG_LEVEL_INFO,
        VOLC_LOG_LEVEL_DEBUG,
        VOLC_LOG_LEVEL_VERBOSE,
        VOLC_LOG_LEVEL_NONE, // 不输出日志
    } volc_log_level_e;

#define VOLC_LOG_LEVEL_DEFAULT VOLC_LOG_LEVEL_INFO

// 日志级别颜色定义
#define LOG_COLOR_RESET "\033[0m"
#define LOG_COLOR_RED "\033[31m"
#define LOG_COLOR_GREEN "\033[32m"
#define LOG_COLOR_YELLOW "\033[33m"
#define LOG_COLOR_BLUE "\033[34m"
#define LOG_COLOR_PRUPLE "\033[35m"

#ifndef __FILENAME__
#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
// 通用日志宏
#define LOG(level, tag, color, format, ...)                                                                      \
    do                                                                                                           \
    {                                                                                                            \
        if (level <= VOLC_LOG_LEVEL_DEFAULT)                                                                     \
        {                                                                                                        \
            printf(color "[%s|%s:%d]" format LOG_COLOR_RESET "\n", tag, __FILENAME__, __LINE__, ##__VA_ARGS__); \
            fflush(stdout);                                                                                      \
        }                                                                                                        \
    } while (0)

// 分级日志实现
#define LOGV(format, ...) LOG(VOLC_LOG_LEVEL_VERBOSE, "VRB", LOG_COLOR_PRUPLE, format, ##__VA_ARGS__)
#define LOGD(format, ...) LOG(VOLC_LOG_LEVEL_DEBUG, "DBG", LOG_COLOR_BLUE, format, ##__VA_ARGS__)
#define LOGI(format, ...) LOG(VOLC_LOG_LEVEL_INFO, "INF", LOG_COLOR_GREEN, format, ##__VA_ARGS__)
#define LOGW(format, ...) LOG(VOLC_LOG_LEVEL_WARN, "WRN", LOG_COLOR_YELLOW, format, ##__VA_ARGS__)
#define LOGE(format, ...) LOG(VOLC_LOG_LEVEL_ERROR, "ERR", LOG_COLOR_RED, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif /* __CONV_AI_SRC_UTIL_VOLC_LOG_H__ */
