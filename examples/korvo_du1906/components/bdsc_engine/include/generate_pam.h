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
#ifndef __GENERATE_PAM_H__
#define __GENERATE_PAM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdio.h"
#include "string.h"

#define     BDSC_MAX_UUID_LEN   37

/**
 * @brief Generate dcs parameters
 *
 * @return
 *      - NULL : fail
 *      - dcs parameters: success
 */
char* generate_dcs_pam_needfree();

/**
 * @brief Generate thirdparty(iot) parameters
 *
 * @param[in] pam_prama    buffer to hold generated parameters
 * @param[in] max_len      length of buffer
 *
 * @return
 *      - 0 : on success
 *      - -1: fail
 */
int generate_asr_thirdparty_pam(char* pam_prama, size_t max_len, int option);

/**
 * @brief Generate auth parameters
 *
 * @param[in] pam_prama    buffer to hold auth parameters
 * @param[in] max_len      length of buffer
 *
 * @return
 *      - 0 : on success
 *      - -1: fail
 */
int generate_auth_pam(char* pam_prama, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif