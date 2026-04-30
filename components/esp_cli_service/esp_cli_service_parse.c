/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "esp_cli_service_internal.h"

esp_err_t cli_parse_i64(const char *s, long long *out)
{
    if (cli_token_is_empty(s) || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    errno = 0;
    char *end = NULL;
    long long value = strtoll(s, &end, 0);
    if (errno != 0 || end == s || *end != '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    *out = value;
    return ESP_OK;
}

esp_err_t cli_parse_double(const char *s, double *out)
{
    if (cli_token_is_empty(s) || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    errno = 0;
    char *end = NULL;
    double value = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    *out = value;
    return ESP_OK;
}

esp_err_t cli_parse_bool(const char *s, bool *out)
{
    if (cli_token_is_empty(s) || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(s, "true") == 0) {
        *out = true;
        return ESP_OK;
    }
    if (strcmp(s, "false") == 0) {
        *out = false;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}
