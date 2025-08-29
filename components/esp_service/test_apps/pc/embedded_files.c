/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Provides the _binary_*_start / _binary_*_end symbols that ESP-IDF's
 * EMBED_FILES mechanism normally generates. On PC we define them as C
 * arrays and use asm() to match the exact symbol names the service
 * source files reference via their own asm() extern declarations.
 */

#include <stdint.h>

const uint8_t player_mcp_json_start[] asm("_binary_player_service_mcp_json_start") =
    "[\n"
    "  {\"name\":\"player_service_play\","
    "\"description\":\"Start playback of the audio file\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},\n"
    "  {\"name\":\"player_service_set_volume\","
    "\"description\":\"Set the playback volume\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":"
    "{\"volume\":{\"type\":\"integer\",\"description\":\"Volume level (0-100)\","
    "\"minimum\":0,\"maximum\":100}},\"required\":[\"volume\"]}},\n"
    "  {\"name\":\"player_service_get_volume\","
    "\"description\":\"Get the current playback volume. Returns the volume level (0-100)\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},\n"
    "  {\"name\":\"player_service_get_status\","
    "\"description\":\"Get the current player status. Returns one of: IDLE, PLAYING, PAUSED, STOPPED\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}\n"
    "]";

const uint8_t player_mcp_json_end[] asm("_binary_player_service_mcp_json_end") = "";

const uint8_t led_mcp_json_start[] asm("_binary_led_service_mcp_json_start") =
    "[\n"
    "  {\"name\":\"led_service_on\","
    "\"description\":\"Turn on the LED\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},\n"
    "  {\"name\":\"led_service_off\","
    "\"description\":\"Turn off the LED\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},\n"
    "  {\"name\":\"led_service_blink\","
    "\"description\":\"Set LED to blink mode with specified period\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":"
    "{\"period_ms\":{\"type\":\"integer\",\"description\":\"Blink period in milliseconds\","
    "\"minimum\":0}},\"required\":[\"period_ms\"]}},\n"
    "  {\"name\":\"led_service_set_brightness\","
    "\"description\":\"Set LED brightness level\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":"
    "{\"brightness\":{\"type\":\"integer\",\"description\":\"Brightness level (0-100)\","
    "\"minimum\":0,\"maximum\":100}},\"required\":[\"brightness\"]}},\n"
    "  {\"name\":\"led_service_get_state\","
    "\"description\":\"Get the current LED state. Returns one of: OFF, ON, BLINK\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}\n"
    "]";

const uint8_t led_mcp_json_end[] asm("_binary_led_service_mcp_json_end") = "";

const uint8_t ota_mcp_json_start[] asm("_binary_ota_service_mcp_json_start") =
    "[\n"
    "  {\"name\":\"ota_service_get_version\","
    "\"description\":\"Get the current and latest available firmware version numbers\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},\n"
    "  {\"name\":\"ota_service_get_progress\","
    "\"description\":\"Query the current OTA upgrade progress. Returns partition label, percent complete (0-100), bytes written, and total bytes\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},\n"
    "  {\"name\":\"ota_service_check_update\","
    "\"description\":\"Check whether a firmware upgrade is available. Returns update_available flag and new version number if found\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}}\n"
    "]";

const uint8_t ota_mcp_json_end[] asm("_binary_ota_service_mcp_json_end") = "";

const uint8_t wifi_mcp_json_start[] asm("_binary_wifi_service_mcp_json_start") =
    "[\n"
    "  {\"name\":\"wifi_service_get_status\","
    "\"description\":\"Query the current Wi-Fi connection status. Returns one of: IDLE, SCANNING, CONNECTING, CONNECTED, DISCONNECTED\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},\n"
    "  {\"name\":\"wifi_service_get_ssid\","
    "\"description\":\"Get the SSID of the currently connected (or configured target) Wi-Fi network\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},\n"
    "  {\"name\":\"wifi_service_get_rssi\","
    "\"description\":\"Get the signal strength (RSSI in dBm) of the currently connected Wi-Fi AP. Returns 0 when not connected\","
    "\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},\n"
    "  {\"name\":\"wifi_service_reconnect\","
    "\"description\":\"Disconnect from the current AP and reconnect to a specified Wi-Fi network\","
    "\"inputSchema\":{\"type\":\"object\","
    "\"properties\":{"
    "\"ssid\":{\"type\":\"string\",\"description\":\"SSID of the target Wi-Fi network\"},"
    "\"password\":{\"type\":\"string\",\"description\":\"Password for the target Wi-Fi network\"}"
    "},\"required\":[\"ssid\"]}}\n"
    "]";

const uint8_t wifi_mcp_json_end[] asm("_binary_wifi_service_mcp_json_end") = "";
