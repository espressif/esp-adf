/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Compatibility shim for ESP-IDF extensions not in upstream FreeRTOS.
 * This header is force-included via compiler flags in CMakeLists.txt.
 */

#pragma once

#include "FreeRTOS.h"
#include "task.h"

/**
 * xTaskCreatePinnedToCore is an ESP-IDF extension.
 * On POSIX, core affinity is ignored; just forward to xTaskCreate.
 */
#ifndef xTaskCreatePinnedToCore
#define xTaskCreatePinnedToCore(func, name, stack, param, prio, handle, core)  \
    xTaskCreate((func), (name), (stack), (param), (prio), (handle))
#endif  /* xTaskCreatePinnedToCore */
