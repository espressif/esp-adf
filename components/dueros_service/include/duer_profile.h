/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2024 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __DUER_PROFILE_H__
#define __DUER_PROFILE_H__

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load the duer profile
 *
 * @return
 *     - NULL, Fail
 *     - Others, Success
 */
const char *duer_profile_load(void);

/**
 * @brief Free the profile loaded
 *
 * @return
 *     - void
 */
void duer_profile_release(const char *profile);

/**
 * @brief Update the bduss, and clinet id in the profile.
 *
 * @return
 *     - ESP_OK, Success
 *     - Others, Fail
 */
esp_err_t duer_profile_update(const char *bduss, const char *client_id);

/**
 * @brief Get the profile certified state
 *
 * @return
 *     - (0),  Certified
 *     - (1), Profile load failed
 *     - (2), Profile parse failed
 *     - (3), Profile not certified
 */
int32_t duer_profile_certified();

/**
 * @brief Get the 'uuid' of the profile
 *
 * @return
 *     - (0), OK
 *     - (1), Profile load failed
 *     - (2), Profile parse failed
 *     - (3), Profile has no 'uuid'
 */
int32_t duer_profile_get_uuid(char *buf, size_t blen);

#ifdef __cplusplus
}
#endif

#endif //__DUER_PROFILE_H__
