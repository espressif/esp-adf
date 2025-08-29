/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * POSIX compatibility shim for esp_timer.h
 * adf_event_hub_port.h already provides esp_timer_get_time() on !ESP_PLATFORM,
 * so this header is intentionally empty to avoid redefinition.
 */

#pragma once
