/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Run the on-device smoke tests for player/led/cloud services.
 *
 * @return
 *       - Number  of individual test functions that reported failure.
 *                 0 means all smoke tests passed.
 */
int smoke_test_run(void);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
