/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_gmf_oal_thread.h"
#include "lv_demos.h"
#include "storage.h"
#include "esp_dual_lcd.h"
#include "esp_dual_display.h"

/* Constants */
#define UI_TASK_STACK_SIZE    8192
#define UI_TASK_PRIORITY      3
#define UI_TASK_CORE          0
#define DISPLAY_DELAY_MS      20000
#define MAX_FILE_PATH_LEN     50
#define DEFAULT_IMAGE_WIDTH   128
#define DEFAULT_IMAGE_HEIGHT  128

/* File path prefixes */
#define SDCARD_PREFIX      "/sdcard"
#define LITTLEFS_PREFIX    "/littlefs"
#define MMAP_PREFIX        "/mmap/"
#define PSRAM_PREFIX       "/psram"
#define LVGL_DRIVE_LETTER  "D:"

/* Storage methods */
typedef enum {
    STORAGE_METHOD_NONE  = -1,
    STORAGE_METHOD_PSRAM = 0,
    STORAGE_METHOD_SDCARD,
    STORAGE_METHOD_LITTLEFS,
    STORAGE_METHOD_MMAP,
    STORAGE_METHOD_MAX,
} storage_method_t;

/* UI context structure */
typedef struct {
    lv_obj_t      *gif_left;
    lv_obj_t      *gif_right;
    lv_disp_t     *disp_left;
    lv_disp_t     *disp_right;
    lv_img_dsc_t  *image[2];
    struct {
        uint8_t  *data;
        uint32_t  size;
    } psram[2];
} dual_ui_context_t;

static const char *TAG = "DUAL_EYE_UI";

// Play GIFs from different storage methods
const char *files[STORAGE_METHOD_MAX][2] = {
    {PSRAM_PREFIX SDCARD_PREFIX "/eye_gif/startup.gif", PSRAM_PREFIX SDCARD_PREFIX "/eye_gif/sleep6.gif"},
    {SDCARD_PREFIX "/eye_gif/startup.gif", SDCARD_PREFIX "/eye_gif/sleep6.gif"},
    {LITTLEFS_PREFIX "/blink_100.gif", LITTLEFS_PREFIX "/love.gif"},
    {MMAP_PREFIX "blink_128.gif", MMAP_PREFIX "flicker.gif"},
};

static dual_ui_context_t g_ui_ctx = {0};

static storage_method_t get_storage_method(const char *file)
{
    if (!file) {
        return STORAGE_METHOD_NONE;
    }

    if (strncmp(file, SDCARD_PREFIX, strlen(SDCARD_PREFIX)) == 0) {
        return STORAGE_METHOD_SDCARD;
    } else if (strncmp(file, LITTLEFS_PREFIX, strlen(LITTLEFS_PREFIX)) == 0) {
        return STORAGE_METHOD_LITTLEFS;
    } else if (strncmp(file, MMAP_PREFIX, strlen(MMAP_PREFIX)) == 0) {
        return STORAGE_METHOD_MMAP;
    } else if (strncmp(file, PSRAM_PREFIX, strlen(PSRAM_PREFIX)) == 0) {
        return STORAGE_METHOD_PSRAM;
    }
    return STORAGE_METHOD_NONE;
}

static lv_obj_t *create_gif_object(lv_disp_t *disp)
{
    if (!disp) {
        ESP_LOGE(TAG, "Display is NULL");
        return NULL;
    }

    // Set screen background
    lv_obj_set_style_bg_opa(lv_disp_get_scr_act(disp), LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *gif = lv_gif_create(lv_disp_get_scr_act(disp));
    if (!gif) {
        ESP_LOGE(TAG, "Failed to create GIF object");
        return NULL;
    }

    // Set object properties
    lv_obj_set_style_bg_opa(gif, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_opa(gif, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_align(gif, LV_ALIGN_CENTER, 0, 0);

    return gif;
}

static esp_err_t init_image_descriptors(void)
{
    for (int i = 0; i < 2; i++) {
        if (g_ui_ctx.image[i] == NULL) {
            g_ui_ctx.image[i] = calloc(1, sizeof(lv_img_dsc_t));
            if (!g_ui_ctx.image[i]) {
                ESP_LOGE(TAG, "Failed to allocate image descriptor %d", i);
                return ESP_ERR_NO_MEM;
            }
            g_ui_ctx.image[i]->header.cf = LV_IMG_CF_RAW;
            g_ui_ctx.image[i]->header.w = DEFAULT_IMAGE_WIDTH;
            g_ui_ctx.image[i]->header.h = DEFAULT_IMAGE_HEIGHT;
        }
    }
    return ESP_OK;
}

static void cleanup_image_descriptors(void)
{
    for (int i = 0; i < 2; i++) {
        if (g_ui_ctx.image[i]) {
            free(g_ui_ctx.image[i]);
            g_ui_ctx.image[i] = NULL;
        }
        if (g_ui_ctx.psram[i].data) {
            free(g_ui_ctx.psram[i].data);
            g_ui_ctx.psram[i].data = NULL;
        }
    }
}

static void cleanup_gif_objects(void)
{
    if (g_ui_ctx.gif_left) {
        lv_obj_del(g_ui_ctx.gif_left);
        g_ui_ctx.gif_left = NULL;
    }
    if (g_ui_ctx.gif_right) {
        lv_obj_del(g_ui_ctx.gif_right);
        g_ui_ctx.gif_right = NULL;
    }
}

static int read_file_to_psram(const char *file, uint8_t **ret_buff, uint32_t *size)
{
    FILE *fp = fopen(file, "rb");
    if (fp == NULL) {
        return ESP_FAIL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return ESP_FAIL;
    }

    *size = ftell(fp);
    uint8_t *buf = malloc(*size);
    if (!buf) {
        fclose(fp);
        return ESP_FAIL;
    }

    fseek(fp, 0, SEEK_SET);
    fread(buf, 1, *size, fp);
    fclose(fp);
    *ret_buff = buf;
    return ESP_OK;
}

static lv_obj_t *play_gif_with_method(const char *file, lv_disp_t *disp)
{
    if (!file || !disp) {
        ESP_LOGE(TAG, "Invalid parameters");
        return NULL;
    }

    storage_method_t method = get_storage_method(file);

    ESP_LOGI(TAG, "Reading file: %s for %s eye", file, disp == g_ui_ctx.disp_left ? "left" : "right");

    lv_obj_t *gif = (disp == g_ui_ctx.disp_left) ? g_ui_ctx.gif_left : g_ui_ctx.gif_right;
    if (!gif) {
        return NULL;
    }

    uint8_t *img_data = NULL;
    uint32_t img_size = 0;
    int pic_num = (disp == g_ui_ctx.disp_left) ? 0 : 1;
    // Handle different storage methods
    switch (method) {
        case STORAGE_METHOD_MMAP: {
            file += strlen(MMAP_PREFIX);
            int index = esp_mmap_assets_get_index_by_name(file);
            if (index < 0) {
                ESP_LOGE(TAG, "Failed to get index for file: %s", file);
                return NULL;
            }
            img_data = (uint8_t *)esp_mmap_assets_get_mem(index);
            img_size = esp_mmap_assets_get_size(index);
            g_ui_ctx.image[pic_num]->data = (uint8_t *)img_data;
            g_ui_ctx.image[pic_num]->data_size = img_size;
            lv_gif_set_src(gif, g_ui_ctx.image[pic_num]);
            return gif;
        }
        case STORAGE_METHOD_PSRAM: {
            img_data = g_ui_ctx.psram[pic_num].data;
            if (img_data == NULL) {
                file += strlen(PSRAM_PREFIX);
                if (read_file_to_psram(file, &img_data, &img_size) != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to get file size for %s", file);
                    return NULL;
                }
                g_ui_ctx.psram[pic_num].data = img_data;
                g_ui_ctx.psram[pic_num].size = img_size;
                g_ui_ctx.image[pic_num]->data = (uint8_t *)img_data;
                g_ui_ctx.image[pic_num]->data_size = img_size;
            } else {
                g_ui_ctx.image[pic_num]->data = g_ui_ctx.psram[pic_num].data;
                g_ui_ctx.image[pic_num]->data_size = g_ui_ctx.psram[pic_num].size;
            }
            lv_gif_set_src(gif, g_ui_ctx.image[pic_num]);
            return gif;
        }
        case STORAGE_METHOD_SDCARD:
        case STORAGE_METHOD_LITTLEFS: {
            // Read from SD card or LittleFS
            char file_path[MAX_FILE_PATH_LEN];
            snprintf(file_path, sizeof(file_path), "%s%s", LVGL_DRIVE_LETTER, file);
            lv_gif_set_src(gif, file_path);
            return gif;
        }
        default:
            break;
    }
    return NULL;
}

static void dual_ui_task(void *arg)
{
    // Initialize display
    esp_dual_display_start(&g_ui_ctx.disp_left, &g_ui_ctx.disp_right);

    // Create GIF object
    lvgl_port_lock(0);
    g_ui_ctx.gif_left = create_gif_object(g_ui_ctx.disp_left);
    g_ui_ctx.gif_right = create_gif_object(g_ui_ctx.disp_right);
    lvgl_port_unlock();

    // Initialize image descriptors for MMAP/PSRAM method
    if (init_image_descriptors() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize image descriptors");
        return;
    }

    while (1) {
        for (int i = 0; i < STORAGE_METHOD_MAX; i++) {
            // Play GIFs
            lvgl_port_lock(0);
            play_gif_with_method(files[i][0], g_ui_ctx.disp_left);
            play_gif_with_method(files[i][1], g_ui_ctx.disp_right);
            lvgl_port_unlock();

            vTaskDelay(pdMS_TO_TICKS(DISPLAY_DELAY_MS));
        }
    }

    // Cleanup
    lvgl_port_lock(0);
    cleanup_gif_objects();
    lvgl_port_unlock();
    cleanup_image_descriptors();
    lvgl_port_remove_disp(g_ui_ctx.disp_left);
    lvgl_port_remove_disp(g_ui_ctx.disp_right);
    lvgl_port_deinit();
    esp_dual_lcd_driver_deinit();

    esp_gmf_oal_thread_delete(NULL);
}

void dual_eye_ui_start(void)
{
    esp_gmf_oal_thread_create(NULL, "dual_ui_task", dual_ui_task, NULL,
                    UI_TASK_STACK_SIZE, UI_TASK_PRIORITY, false, UI_TASK_CORE);
}
