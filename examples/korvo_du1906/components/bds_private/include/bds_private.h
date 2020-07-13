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

#ifndef _BDS_PRIVATE_H_
#define _BDS_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef bdsc_asr_params_t* (*wrapper_func_t)(char *sn,
        uint32_t primary_pid, uint32_t assist_pid, char *key,
        uint16_t audio_rate, char *cuid, int backtrack_time,
        uint16_t pam_len, char *pam);
/**
 * @brief      Get bds primary pid
 */
bdsc_asr_params_t *bdsc_asr_params_create_wrapper(
        wrapper_func_t func,
        char *sn,
        uint16_t audio_rate, char *cuid, int backtrack_time,
        uint16_t pam_len, char *pam);


#ifdef __cplusplus
}
#endif

#endif /* _BDS_PRIVATE_H_ */
