/*
 * Copyright (c) 2020 Baidu.com, Inc. All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
 * an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 */

#ifndef ESP_SDK_NICODE_HAL_SELF_TEST_H
#define ESP_SDK_NICODE_HAL_SELF_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
typedef void* bds_self_test_handle;

typedef struct pcm_params {
    int channel;
    int sample;
    int bitrate;
} bds_pcm_params;

void bds_switch_self_test(bool enable);

/**
 * 开始录音
 * @param params 录音参数：目前支持 chan 1～3, sample 16000, bit 16
 * @param mini_buffer_size 根据参数计算的帧大小， 指导bds_pcm_read时申请足够空间
 * @return
 */
bds_self_test_handle bds_pcm_open(bds_pcm_params* params, int *mini_buffer_size);

/**
 * read pcm
 * @param handle 打开的handle
 * @param out_buffer 输出音频数据
 * @param out_length 数据长度
 * @return
 */
int bds_pcm_read(bds_self_test_handle handle, uint8_t *out_buffer, int *out_length);

int bds_pcm_close(bds_self_test_handle handle);

#ifdef __cplusplus
}
#endif

#endif //ESP_SDK_NICODE_HAL_SELF_TEST_H
