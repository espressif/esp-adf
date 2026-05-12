/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Build a JWT signed with RS256 (RSA PKCS1-v1.5 + SHA-256).
 *
 *         The header is fixed to `{"alg":"RS256","typ":"JWT","kid":"<kid>"}` and the
 *         payload is taken verbatim from payload. Header and payload are
 *         base64url-encoded (RFC 4648 section 5, no padding); the signature is RSA-PKCS1-v1.5
 *         over SHA-256 of `<base64url(header)>.<base64url(payload)>`.
 *
 * @note  private_key must be PEM-encoded RSA and at least 2048-bit.
 *
 * @param[in]  kid              Key ID written into the JOSE `kid` header
 * @param[in]  payload          NUL-terminated JWT claims JSON string
 * @param[in]  private_key      PEM-encoded RSA private key bytes
 * @param[in]  private_key_len  Length of private_key in bytes
 *
 * @return
 *       - Non-NULL  Newly allocated NUL-terminated JWT; caller must `free()`
 *       - NULL      Invalid arguments, OOM, or key parse/sign failure
 */
char *esp_coze_jwt_create(const char *kid, const char *payload,
                          const uint8_t *private_key, size_t private_key_len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
