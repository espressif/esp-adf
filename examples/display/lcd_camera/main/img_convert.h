/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#pragma once

#include <stdint.h>
#include "esp_jpeg_common.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Convert YUV to RGB
 *
 * @param[in]   sub_sample  Chroma subsampling factors. Grayscale is un-supported. The others are supported.
 * @param[in]   raw_type    Raw data type. RGB888 and RGB565 are supported. The others are un-supported.
 * @param[in]   yuv_image   Input YUV image data. The buffer must be aligned 16 byte.
 * @param[in]   width       Image width
 * @param[in]   height      Image height
 * @param[out]  rgb_image   Output RGB image data
 *
 * @return
 *       - JPEG_ERR_OK  On success
 *       - Others       Error occurs
 */
jpeg_error_t jpeg_yuv2rgb(jpeg_subsampling_t sub_sample, jpeg_pixel_format_t raw_type, uint8_t *yuv_image, int width, int height, uint8_t *rgb_image);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
