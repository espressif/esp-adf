/*
   This code convert YUV data to RGB data.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "img_convert.h"

#define COVERT_YUV_TO_RGB(y, u, v, r, g, b)                 \
    int rdif = 5743 * (v - 128) >> 12;                      \
    int gdif = (1409 * (u - 128) + 2925 * (v - 128)) >> 12; \
    int bdif = 7259 * (u - 128) >> 12;                      \
    r        = BYTECLIP(y + rdif);                          \
    g        = BYTECLIP(y - gdif);                          \
    b        = BYTECLIP(y + bdif);

static inline uint8_t BYTECLIP(int16_t val)
{
    if (val < 0) {
        val = 0;
    }
    if (val > 255) {
        val = 255;
    }
    return (uint8_t)val;
}

static void yuv4442rgb888(int16_t *ybuf, int16_t *ubuf, int16_t *vbuf, int yuv_line_size, int height, int16_t width, uint8_t *rgb_out)
{
    int r, g, b;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            COVERT_YUV_TO_RGB(ybuf[i], ubuf[i], vbuf[i], r, g, b);
            *rgb_out++ = r;
            *rgb_out++ = g;
            *rgb_out++ = b;
        }
        ybuf += yuv_line_size;
        ubuf += yuv_line_size;
        vbuf += yuv_line_size;
    }
}

static void yuv4442rgb565_be(int16_t *ybuf, int16_t *ubuf, int16_t *vbuf, int yuv_line_size, int height, int16_t width, uint8_t *rgb_out)
{
    int r, g, b;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            COVERT_YUV_TO_RGB(ybuf[i], ubuf[i], vbuf[i], r, g, b);
            *rgb_out++ = (r & 0xf8) | (g >> 5);
            *rgb_out++ = ((g & 0x1c) << 3) | (b >> 3);
        }
        ybuf += yuv_line_size;
        ubuf += yuv_line_size;
        vbuf += yuv_line_size;
    }
}

static void yuv4442rgb565_le(int16_t *ybuf, int16_t *ubuf, int16_t *vbuf, int yuv_line_size, int height, int16_t width, uint8_t *rgb_out)
{
    int r, g, b;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            COVERT_YUV_TO_RGB(ybuf[i], ubuf[i], vbuf[i], r, g, b);
            *rgb_out++ = ((g & 0x1c) << 3) | (b >> 3);
            *rgb_out++ = (r & 0xf8) | (g >> 5);
        }
        ybuf += yuv_line_size;
        ubuf += yuv_line_size;
        vbuf += yuv_line_size;
    }
}

jpeg_error_t jpeg_yuv2rgb(jpeg_subsampling_t sub_sample, jpeg_pixel_format_t raw_type, uint8_t *yuv_image, int width, int height, uint8_t *rgb_image)
{
    /* allocate buffer */
    if (width == 0 || height == 0 || yuv_image == NULL || rgb_image == NULL) {
        return JPEG_ERR_INVALID_PARAM;
    }
    int buf_size = (width + 15) & (~0xf);
    int16_t *u_data = NULL;
    int16_t *v_data = NULL;
    int16_t *y_data = heap_caps_malloc_prefer(buf_size * sizeof(int16_t), 2, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (y_data == NULL) {
        goto trans_error;
    }
    u_data = heap_caps_malloc_prefer(buf_size * sizeof(int16_t), 2, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (u_data == NULL) {
        goto trans_error;
    }
    v_data = heap_caps_malloc_prefer(buf_size * sizeof(int16_t), 2, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    if (v_data == NULL) {
        goto trans_error;
    }
    for (size_t row = 0; row < height; row++) {
        /* for one line */
        // first data type conversion
        int col_index = 0;
        switch (sub_sample) {
            case JPEG_SUBSAMPLE_444:
                for (int col = 0; col < (width * 3);) {
                    y_data[col_index] = yuv_image[col++];
                    u_data[col_index] = yuv_image[col++];
                    v_data[col_index] = yuv_image[col++];
                    col_index++;
                }
                yuv_image += width * 3;
                break;
            case JPEG_SUBSAMPLE_422:
                for (int col = 0; col < (width * 2);) {
                    y_data[col_index] = yuv_image[col++];  // y
                    u_data[col_index] = yuv_image[col++];  // u
                    u_data[col_index + 1] = u_data[col_index];
                    y_data[col_index + 1] = yuv_image[col++];  // y
                    v_data[col_index] = yuv_image[col++];      // v
                    v_data[col_index + 1] = v_data[col_index];
                    col_index += 2;
                }
                yuv_image += width * 2;
                break;
            case JPEG_SUBSAMPLE_420:
                for (int col = 0; col < (width * 1.5);) {
                    y_data[col_index] = yuv_image[col++];  // y
                    u_data[col_index] = yuv_image[col++];  // u
                    u_data[col_index + 1] = u_data[col_index];
                    u_data[col_index + 2] = u_data[col_index];
                    u_data[col_index + 3] = u_data[col_index];
                    y_data[col_index + 1] = yuv_image[col++];  // y
                    y_data[col_index + 2] = yuv_image[col++];  // y
                    u_data[col_index + 4] = yuv_image[col++];  // u
                    u_data[col_index + 5] = u_data[col_index];
                    u_data[col_index + 6] = u_data[col_index];
                    u_data[col_index + 7] = u_data[col_index];
                    y_data[col_index + 3] = yuv_image[col++];  // y
                    y_data[col_index + 4] = yuv_image[col++];  // y
                    v_data[col_index] = yuv_image[col++];      // v
                    v_data[col_index + 1] = v_data[col_index];
                    v_data[col_index + 2] = v_data[col_index];
                    v_data[col_index + 3] = v_data[col_index];
                    y_data[col_index + 5] = yuv_image[col++];  // y
                    y_data[col_index + 6] = yuv_image[col++];  // y
                    v_data[col_index + 4] = yuv_image[col++];  // v
                    v_data[col_index + 5] = v_data[col_index];
                    v_data[col_index + 6] = v_data[col_index];
                    v_data[col_index + 7] = v_data[col_index];
                    y_data[col_index + 7] = yuv_image[col++];  // y
                    col_index += 8;
                }
                yuv_image += (int)(width * 1.5);
                break;
            default:
                goto trans_error;
        }
        // second color conversion
        switch (raw_type) {
            case JPEG_PIXEL_FORMAT_RGB888:
                yuv4442rgb888(y_data, u_data, v_data, buf_size, 1, width, rgb_image);
                rgb_image += width * 3;
                break;
            case JPEG_PIXEL_FORMAT_RGB565_BE:
                yuv4442rgb565_be(y_data, u_data, v_data, buf_size, 1, width, rgb_image);
                rgb_image += width << 1;
                break;
            case JPEG_PIXEL_FORMAT_RGB565_LE:
                yuv4442rgb565_le(y_data, u_data, v_data, buf_size, 1, width, rgb_image);
                rgb_image += width << 1;
                break;
            default:
                goto trans_error;
        }
    }
    /* free buffer */
    heap_caps_free(y_data);
    heap_caps_free(u_data);
    heap_caps_free(v_data);
    return JPEG_ERR_OK;
trans_error:
    heap_caps_free(y_data);
    heap_caps_free(u_data);
    heap_caps_free(v_data);
    return JPEG_ERR_FAIL;
}
