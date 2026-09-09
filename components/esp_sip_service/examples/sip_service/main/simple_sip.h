/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Run one P2P SIP call attempt
 *
 *         Links dummy media to one SRC_SINK SIP service and calls the configured
 *         peer. The default loopback peer does not answer, so success means the
 *         stack entered the calling state and sent the INVITE.
 *
 * @param[in]  run_ms  How long to keep an answered call up
 *
 * @return
 *       - ESP_OK  The outgoing call entered the calling state
 *       - Others  Error from service creation, linking, start or call
 */
esp_err_t simple_sip_p2p_call(uint32_t run_ms);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
