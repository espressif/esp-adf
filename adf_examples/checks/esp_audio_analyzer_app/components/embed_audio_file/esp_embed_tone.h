/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

/**
 * @brief  Structure for embedding tone information
 */
typedef struct {
    const uint8_t  *address;  /*!< Pointer to the embedded tone data */
    int             size;     /*!< Size of the tone data in bytes */
} esp_embed_tone_t;

/**
 * @brief  External reference to embedded tone data: embed_0dBFS_1KHz_16K.flac
 */
extern const uint8_t embed_0dBFS_1KHz_16K_flac[] asm("_binary_embed_0dBFS_1KHz_16K_flac_start");

/**
 * @brief  External reference to embedded tone data: embed_Silence_16K.flac
 */
extern const uint8_t embed_Silence_16K_flac[] asm("_binary_embed_Silence_16K_flac_start");

/**
 * @brief  External reference to embedded tone data: embed_Stepped_sweep_61pt_16KHz.flac
 */
extern const uint8_t embed_Stepped_sweep_61pt_16KHz_flac[] asm("_binary_embed_Stepped_sweep_61pt_16KHz_flac_start");

/**
 * @brief  Array of embedded tone information
 */
esp_embed_tone_t g_esp_embed_tone[] = {
    [0] = {
        .address = embed_0dBFS_1KHz_16K_flac,
        .size    = 76843,
    },
    [1] = {
        .address = embed_Silence_16K_flac,
        .size    = 41980,
    },
    [2] = {
        .address = embed_Stepped_sweep_61pt_16KHz_flac,
        .size    = 393109,
    },
};

/**
 * @brief  Enumeration for tone URLs
 */
enum esp_embed_tone_index {
    ESP_EMBED_TONE_EMBED_0DBFS_1KHZ_16K_FLAC           = 0,
    ESP_EMBED_TONE_EMBED_SILENCE_16K_FLAC              = 1,
    ESP_EMBED_TONE_EMBED_STEPPED_SWEEP_61PT_16KHZ_FLAC = 2,
    ESP_EMBED_TONE_URL_MAX                             = 3
};

/**
 * @brief  Array of tone URLs
 */
const char *esp_embed_tone_url[] = {
    "embed://tone/0_embed_0dBFS_1KHz_16K.flac",
    "embed://tone/1_embed_Silence_16K.flac",
    "embed://tone/2_embed_Stepped_sweep_61pt_16KHz.flac",
};
