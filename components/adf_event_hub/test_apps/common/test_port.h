/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Portable bridge for adf_event_hub test bodies.
 *
 * A single test source is shared between two build targets:
 *   - PC build (FreeRTOS POSIX port + upstream Unity via test_compat.h)
 *   - ESP-IDF target (native FreeRTOS + IDF-flavoured Unity / test_utils)
 *
 * This header normalises the FreeRTOS include prefix, pulls in the correct
 * Unity / TEST_CASE macro, and exposes a common stack size constant for
 * xTaskCreate() calls so test bodies never need an #ifdef.
 */

#pragma once

#ifdef ESP_PLATFORM

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "unity.h"

#define TEST_STACK_SZ  (4 * 1024)

#else  /* POSIX / PC build */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "unity.h"
#include "test_compat.h"

#define TEST_STACK_SZ  (configMINIMAL_STACK_SIZE * 4)

#endif  /* ESP_PLATFORM */

#include "adf_event_hub_port.h"
#include "adf_event_hub.h"

#define TEST_TAG  "[adf_event_hub]"
