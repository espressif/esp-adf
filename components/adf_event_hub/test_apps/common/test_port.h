/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "unity.h"
#include "adf_event_hub_port.h"
#include "adf_event_hub.h"

#define TEST_STACK_SZ  (4 * 1024)
#define TEST_TAG       "[adf_event_hub]"
