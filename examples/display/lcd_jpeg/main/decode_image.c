/* SPI Master example: jpeg decoder.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
The image used for the effect on the LCD in the SPI master example is stored in flash
as a jpeg file. This file contains the decode_image routine, which uses the tiny JPEG
decoder library to decode this JPEG into a format that can be sent to the display.

Keep in mind that the decoder library cannot handle progressive files (will give
``Image decoder: jd_prepare failed (8)`` as an error) so make sure to save in the correct
format if you want to use a different image file.
*/

#include "decode_image.h"
#include "esp_log.h"
#include "esp_jpeg_dec.h"
#include "esp_jpeg_common.h"

// Reference the binary-included jpeg file
extern const uint8_t image_jpg_start[] asm("_binary_image_jpg_start");
extern const uint8_t image_jpg_end[] asm("_binary_image_jpg_end");

const char *TAG = "ImageDec";

int decode_image(uint16_t ***pixels)
{
    // Generate default configuration
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_RAW_TYPE_RGB565_BE;
    // Empty handle to jpeg_decoder
    jpeg_dec_handle_t jpeg_dec = NULL;
    // Create jpeg_dec
    jpeg_dec = jpeg_dec_open(&config);
    // Create io_callback handle
    jpeg_dec_io_t *jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL) {
        return ESP_FAIL;
    }

    // Create out_info handle
    jpeg_dec_header_info_t *out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL) {
        return ESP_FAIL;
    }

    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = (unsigned char *)image_jpg_start;
    jpeg_io->inbuf_len = image_jpg_end - image_jpg_start;

    int ret = 0;
    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret < 0) {
        ESP_LOGE(TAG, "Got an error by jpeg_dec_parse_header, ret:%d", ret);
        return ret;
    }
    // Calloc out_put data buffer and update inbuf ptr and inbuf_len
    int outbuf_len;
    if (config.output_type == JPEG_RAW_TYPE_RGB565_LE
        || config.output_type == JPEG_RAW_TYPE_RGB565_BE) {
        outbuf_len = out_info->height * out_info->width * 2;
    } else if (config.output_type == JPEG_RAW_TYPE_RGB888) {
        outbuf_len = out_info->height * out_info->width * 3;
    } else {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "The image size is %d bytes, width:%d, height:%d", outbuf_len, out_info->width, out_info->height);
    unsigned char *out_buf = jpeg_malloc_align(outbuf_len, 16);
    jpeg_io->outbuf = out_buf;
    int inbuf_consumed = jpeg_io->inbuf_len - jpeg_io->inbuf_remain;
    jpeg_io->inbuf = (unsigned char *)(image_jpg_start + inbuf_consumed);
    jpeg_io->inbuf_len = jpeg_io->inbuf_remain;

    // Start decode jpeg raw data
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret < 0) {
        ESP_LOGE(TAG, "Got an error by jpeg_dec_process, ret:%d", ret);
        return ret;
    }
    *pixels = (uint16_t **)out_buf;
    // Decoder deinitialize
    jpeg_dec_close(jpeg_dec);
    free(out_info);
    free(jpeg_io);
    return ESP_OK;
}
