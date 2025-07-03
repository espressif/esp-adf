/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_gmf_oal_thread.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "avi_player.h"
#include "esp_dual_lcd.h"
#include "esp_dual_display.h"

/* Constants */
#define LCD_DRAW_TASK_STACK_SIZE  4096
#define PLAYER_TASK_STACK_SIZE    4096
#define AVI_PLAYER_STACK_SIZE     3072
#define AVI_PLAYER_BUFFER_SIZE    (16 * 1024)
#define QUEUE_TIMEOUT_MS          1000
#define FPS_REPORT_INTERVAL_US    1000000
#define LCD_WIDTH                 BSP_LCD_H_RES
#define LCD_HEIGHT                BSP_LCD_V_RES
#define TASK_STACK_IN_PSRAM       (true) // If you read file from flash, must set this to false

/* Notification bits */
#define PLAYER_LEFT_END_BIT   (1 << 0)
#define PLAYER_RIGHT_END_BIT  (1 << 1)
#define PLAYER_ERROR_BIT      (1 << 2)

typedef struct {
    uint8_t       *active_buf;
    uint8_t       *decode_buf[2];
    QueueHandle_t  frame_queue;
    int64_t        last_time;
    uint32_t       frame_count;
    TaskHandle_t   task_handle;
    uint32_t       notify_bits;
    void          *panel;
} avi_player_data_t;

static const char *TAG        = "DUAL_EYE_PLAYER";

#if TASK_STACK_IN_PSRAM
static const char *AVI_FILE_L = "/sdcard/avi/angry_l.avi";
static const char *AVI_FILE_R = "/sdcard/avi/angry_r.avi";
#else
static const char *AVI_FILE_L = "/littlefs/angry_l.avi";
static const char *AVI_FILE_R = "/littlefs/angry_r.avi";
#endif

// Both eyes share the same spi bus, so we need to use a mutex to protect the panel
static SemaphoreHandle_t s_panel_mutex = NULL;

/* Function declarations */
static void  report_fps(avi_player_data_t *player_data);
static void  free_jpeg_buffers(avi_player_data_t *player_data);
static void  cleanup_task(esp_gmf_oal_thread_t task);
static void  cleanup_player_data(avi_player_data_t *player_data);
static void  cleanup_lcd_panel(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io);
static esp_err_t    init_player_data(esp_lcd_panel_handle_t panel, avi_player_data_t **player_data);
static esp_err_t    allocate_jpeg_buffers(avi_player_data_t *player_data, size_t width, size_t height);
static jpeg_error_t esp_jpeg_decode_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf, int *out_len);

static void avi_video_frame(frame_data_t *data, void *arg)
{
    avi_player_data_t *player_data = (avi_player_data_t *)arg;
    if (!player_data || !data) {
        ESP_LOGE(TAG, "Invalid arguments");
        return;
    }

    // Allocate JPEG buffers if not already allocated
    if (allocate_jpeg_buffers(player_data, data->video_info.width, data->video_info.height) != ESP_OK) {
        if (player_data->task_handle) {
            xTaskNotify(player_data->task_handle, PLAYER_ERROR_BIT, eSetBits);
        }
        return;
    }

    // Switch between the two buffers
    if (player_data->active_buf == player_data->decode_buf[0]) {
        player_data->active_buf = player_data->decode_buf[1];
    } else {
        player_data->active_buf = player_data->decode_buf[0];
    }

    // Decode JPEG data
    int color_size = 0;
    jpeg_error_t ret = esp_jpeg_decode_one_picture(data->data, data->data_bytes,
                                                   player_data->active_buf, &color_size);
    if (ret != JPEG_ERR_OK) {
        ESP_LOGE(TAG, "JPEG decode failed with error: %d", ret);
        if (player_data->task_handle) {
            xTaskNotify(player_data->task_handle, PLAYER_ERROR_BIT, eSetBits);
        }
        return;
    }

    // Adjust color data pointer for height > LCD_HEIGHT
    uint8_t *color_data = player_data->active_buf;
    if (data->video_info.height > LCD_HEIGHT) {
        color_data += (data->video_info.height - LCD_HEIGHT) * data->video_info.width;
    }

    // Send color data to display queue
    if (xQueueSend(player_data->frame_queue, &color_data, pdMS_TO_TICKS(QUEUE_TIMEOUT_MS)) != pdPASS) {
        ESP_LOGE(TAG, "Failed to send data to display queue");
        if (player_data->task_handle) {
            xTaskNotify(player_data->task_handle, PLAYER_ERROR_BIT, eSetBits);
        }
        return;
    }

    // Report FPS
    report_fps(player_data);
}

static void avi_play_end(void *arg)
{
    avi_player_data_t *player_data = (avi_player_data_t *)arg;
    if (player_data && player_data->task_handle) {
        xTaskNotify(player_data->task_handle, player_data->notify_bits, eSetBits);
    }
}

static void lcd_draw_task(void *arg)
{
    avi_player_data_t *player_data = (avi_player_data_t *)arg;
    while (1) {
        uint8_t *color_data = NULL;
        if (xQueueReceive(player_data->frame_queue, &color_data, pdMS_TO_TICKS(QUEUE_TIMEOUT_MS)) == pdPASS) {
            if (xSemaphoreTake(s_panel_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
                ESP_LOGE(TAG, "Failed to take panel mutex");
                continue;
            }
            esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)player_data->panel, 0, 0,
                                      LCD_WIDTH, LCD_HEIGHT, color_data);
            xSemaphoreGive(s_panel_mutex);
        } else {
            ESP_LOGW(TAG, "Timeout waiting for receive data in LCD draw task");
        }
    }
    esp_gmf_oal_thread_delete(NULL);
}

static void player_task(void *arg)
{
    esp_lcd_panel_io_handle_t io_left = NULL;
    esp_lcd_panel_io_handle_t io_right = NULL;
    esp_lcd_panel_handle_t panel_left = NULL;
    esp_lcd_panel_handle_t panel_right = NULL;
    avi_player_data_t *data_left = NULL;
    avi_player_data_t *data_right = NULL;
    esp_gmf_oal_thread_t draw_left_handle = NULL;
    esp_gmf_oal_thread_t draw_right_handle = NULL;

    s_panel_mutex = xSemaphoreCreateMutex();
    if (s_panel_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        goto cleanup;
    }

    // Initialize display
    if (esp_dual_lcd_driver_init(&panel_left, &io_left, &panel_right, &io_right) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display");
        goto cleanup;
    }

    if (init_player_data(panel_left, &data_left) != ESP_OK ||
        init_player_data(panel_right, &data_right) != ESP_OK) {
        goto cleanup;
    }

    // Store task handle and notification bits in player data
    TaskHandle_t task_handle = xTaskGetCurrentTaskHandle();
    data_left->task_handle = task_handle;
    data_right->task_handle = task_handle;
    data_left->notify_bits = PLAYER_LEFT_END_BIT;
    data_right->notify_bits = PLAYER_RIGHT_END_BIT;

    // Create LCD draw tasks
    if (esp_gmf_oal_thread_create(&draw_left_handle, "lcd_draw_left", lcd_draw_task, data_left,
                        LCD_DRAW_TASK_STACK_SIZE, 10, true, SPI_ISR_CPU_ID) != ESP_GMF_ERR_OK) {
        goto cleanup;
    }
    if (esp_gmf_oal_thread_create(&draw_right_handle, "lcd_draw_right", lcd_draw_task, data_right,
                        LCD_DRAW_TASK_STACK_SIZE, 10, true, SPI_ISR_CPU_ID) != ESP_GMF_ERR_OK) {
        goto cleanup;
    }

    // Configure AVI player
    avi_player_config_t config = {
        .buffer_size = AVI_PLAYER_BUFFER_SIZE,
        .video_cb = avi_video_frame,
        .avi_play_end_cb = avi_play_end,
        .coreID = SPI_ISR_CPU_ID,
        .priority = 5,
        .user_data = data_left,
        .stack_size = AVI_PLAYER_STACK_SIZE,
        .stack_in_psram = TASK_STACK_IN_PSRAM,
    };

    // Initialize AVI players
    avi_player_handle_t eye_left = NULL;
    avi_player_handle_t eye_right = NULL;

    if (avi_player_init(config, &eye_left) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize left eye player");
        goto cleanup;
    }

    config.user_data = data_right;
    if (avi_player_init(config, &eye_right) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize right eye player");
        goto cleanup;
    }

    while (1) {
        if (avi_player_play_from_file(eye_left, AVI_FILE_L) != ESP_OK ||
                    avi_player_play_from_file(eye_right, AVI_FILE_R) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start AVI playback");
            break;
        }

        for (int i = 0; i < 2; i++) {
            uint32_t value = 0;
            if (xTaskNotifyWait(0, 0xFFFFFFFF, &value, pdMS_TO_TICKS(1000 * 30)) == pdFALSE) {
                ESP_LOGE(TAG, "Timeout waiting for player notification");
                break;
            }
            if (value & PLAYER_ERROR_BIT) {
                ESP_LOGE(TAG, "Player error occurred");
                goto cleanup;
            }
        }
    }

cleanup:
    avi_player_deinit(eye_left);
    avi_player_deinit(eye_right);
    cleanup_task(draw_left_handle);
    cleanup_task(draw_right_handle);
    cleanup_player_data(data_left);
    cleanup_player_data(data_right);
    cleanup_lcd_panel(panel_left, io_left);
    cleanup_lcd_panel(panel_right, io_right);
    if (s_panel_mutex) {
        vSemaphoreDelete(s_panel_mutex);
    }
    ESP_LOGW(TAG, "AVI player finished");
    esp_gmf_oal_thread_delete(NULL);
}

static esp_err_t allocate_jpeg_buffers(avi_player_data_t *player_data, size_t width, size_t height)
{
    if (!player_data) {
        return ESP_ERR_INVALID_ARG;
    }
    if (player_data->active_buf != NULL) {
        // Buffers already allocated
        return ESP_OK;
    }

    for (int i = 0; i < 2; i++) {
        if (player_data->decode_buf[i] == NULL) {
            player_data->decode_buf[i] = jpeg_calloc_align(width * height * 2, 16);
            if (player_data->decode_buf[i] == NULL) {
                ESP_LOGE(TAG, "Failed to allocate JPEG buffer %d", i);
                return ESP_ERR_NO_MEM;
            }
        }
    }
    return ESP_OK;
}

static void free_jpeg_buffers(avi_player_data_t *player_data)
{
    if (player_data) {
        for (int i = 0; i < 2; i++) {
            if (player_data->decode_buf[i]) {
                jpeg_free_align(player_data->decode_buf[i]);
                player_data->decode_buf[i] = NULL;
            }
        }
    }
}

static void report_fps(avi_player_data_t *player_data)
{
    if (!player_data) {
        return;
    }

    int64_t current_time = esp_timer_get_time();
    if (player_data->last_time == 0) {
        player_data->last_time = current_time;
    }

    int64_t elapsed_time = current_time - player_data->last_time;
    player_data->frame_count++;

    if (elapsed_time >= FPS_REPORT_INTERVAL_US) {
        float fps = player_data->frame_count * 1000000.0f / elapsed_time;
        ESP_LOGI(TAG, "FPS(%p): %.1f", player_data->panel, fps);
        player_data->last_time = current_time;
        player_data->frame_count = 0;
    }
}

static esp_err_t init_player_data(esp_lcd_panel_handle_t panel, avi_player_data_t **player_data)
{
    if (!player_data || !panel) {
        return ESP_ERR_INVALID_ARG;
    }

    *player_data = calloc(1, sizeof(avi_player_data_t));
    if (!*player_data) {
        ESP_LOGE(TAG, "Failed to allocate player data");
        return ESP_ERR_NO_MEM;
    }

    (*player_data)->panel = panel;
    (*player_data)->frame_queue = xQueueCreate(2, sizeof(uint8_t *));
    if (!(*player_data)->frame_queue) {
        ESP_LOGE(TAG, "Failed to create data queue");
        free(*player_data);
        *player_data = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static void cleanup_player_data(avi_player_data_t *player_data)
{
    if (player_data) {
        free_jpeg_buffers(player_data);
        if (player_data->frame_queue) {
            vQueueDelete(player_data->frame_queue);
        }
        free(player_data);
    }
}

static void cleanup_lcd_panel(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io)
{
    // Delete the panel and IO handles
    if (panel) {
        esp_lcd_panel_del(panel);
    }
    if (io) {
        esp_lcd_panel_io_del(io);
    }
}

static void cleanup_task(esp_gmf_oal_thread_t task)
{
    if (task) {
        esp_gmf_oal_thread_delete(task);
    }
}

static jpeg_error_t esp_jpeg_decode_one_picture(uint8_t *input_buf, int len, uint8_t *output_buf, int *out_len)
{
    jpeg_error_t ret = JPEG_ERR_OK;
    jpeg_dec_io_t *jpeg_io = NULL;
    jpeg_dec_header_info_t *out_info = NULL;

    // Generate default configuration
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_BE;
    config.rotate = JPEG_ROTATE_0D;

    // Create jpeg_dec handle
    jpeg_dec_handle_t jpeg_dec = NULL;
    ret = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK) {
        return ret;
    }

    // Create io_callback handle
    jpeg_io = calloc(1, sizeof(jpeg_dec_io_t));
    if (jpeg_io == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_dec_failed;
    }

    // Create out_info handle
    out_info = calloc(1, sizeof(jpeg_dec_header_info_t));
    if (out_info == NULL) {
        ret = JPEG_ERR_NO_MEM;
        goto jpeg_dec_failed;
    }

    // Set input buffer and buffer len to io_callback
    jpeg_io->inbuf = input_buf;
    jpeg_io->inbuf_len = len;

    // Parse jpeg picture header and get picture for user and decoder
    ret = jpeg_dec_parse_header(jpeg_dec, jpeg_io, out_info);
    if (ret != JPEG_ERR_OK) {
        goto jpeg_dec_failed;
    }

    *out_len = 0;
    // Calloc out_put data buffer and update inbuf ptr and inbuf_len
    if (config.output_type == JPEG_PIXEL_FORMAT_RGB565_LE
        || config.output_type == JPEG_PIXEL_FORMAT_RGB565_BE
        || config.output_type == JPEG_PIXEL_FORMAT_CbYCrY) {
        *out_len = out_info->width * out_info->height * 2;
    } else if (config.output_type == JPEG_PIXEL_FORMAT_RGB888) {
        *out_len = out_info->width * out_info->height * 3;
    } else {
        ret = JPEG_ERR_INVALID_PARAM;
        goto jpeg_dec_failed;
    }
    jpeg_io->outbuf = output_buf;

    // Start decode jpeg
    ret = jpeg_dec_process(jpeg_dec, jpeg_io);
    if (ret != JPEG_ERR_OK) {
        goto jpeg_dec_failed;
    }

    // Decoder deinitialize
jpeg_dec_failed:
    jpeg_dec_close(jpeg_dec);
    if (jpeg_io) {
        free(jpeg_io);
    }
    if (out_info) {
        free(out_info);
    }
    return ret;
}

void avi_player_start(void)
{
    esp_gmf_oal_thread_create(NULL, "player_task", player_task, NULL,
                                PLAYER_TASK_STACK_SIZE, 3, TASK_STACK_IN_PSRAM, 1);
}
