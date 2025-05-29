/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Proprietary
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a JWT (JSON Web Token) for Coze authentication
 *
 * @param[in]  kid             Key ID string
 * @param[in]  payload         JWT payload string
 * @param[in]  privateKey      Private key buffer
 * @param[in]  privateKeySize  Size of the private key buffer
 *
 * @return
 *       - JWT string (Need to be freed by the caller)
 *       - NULL  On failure
 */
char *coze_jwt_create_handler(const char *kid, const char *payload, const uint8_t *privateKey, size_t privateKeySize);

#ifdef __cplusplus
}
#endif
