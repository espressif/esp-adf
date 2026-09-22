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
 * One SIP service instance places a P2P call, so no registrar, account or
 * Wi-Fi credentials are needed.
 *
 * The peer defaults to the device itself over loopback so the demo runs with
 * no second device. Nothing answers that INVITE, which is expected: point
 * SIP_PEER_IP at another SIP endpoint to establish real media.
 *
 * Two instances on one device cannot call each other, because in P2P mode the
 * protocol stack binds the port taken from the URI instead of the configured
 * local port, so both peers would have to share one socket.
 */
#define SIP_LOCAL_IP    "127.0.0.1"
#define SIP_PEER_IP     "127.0.0.1"
#define SIP_PEER_PORT   5062
#define SIP_LOCAL_USER  "1001"
#define SIP_PEER_USER   "1002"

#define SIP_CALL_SETTLE_MS  5000
#define SIP_CALL_RUN_MS     5000

#ifdef __cplusplus
}
#endif  /* __cplusplus */
