/* volc rtc message parse example code

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include "cJSON.h"

typedef enum {
    VOLC_RTC_MESSAGE_TYPE_SUBTITLE      = 1,
    VOLC_RTC_MESSAGE_TYPE_FUNCTION_CALL = 2,
    VOLC_RTC_MESSAGE_TYPE_NONE          = 0,
} volc_rtc_message_type_t;

esp_err_t volc_rtc_message_init(uint32_t msg_type);
esp_err_t volc_rtc_message_process(const uint8_t* message, int size);

