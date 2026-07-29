/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_gmf_err.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_video_color_convert.h"
#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31
#include "esp_gmf_video_ppa.h"
#else
#include "esp_gmf_video_crop.h"
#include "esp_gmf_video_scale.h"
#endif  /* CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31 */
#include "impl/esp_capture_text_overlay.h"
#include "esp_video_render.h"
#include "widget/esp_vui_image_widget.h"
#include "av_rec_config.h"

static const char *TAG = "AV_REC_DISPLAY";

typedef struct {
    av_record_live_display_sys_t *sys;
    TaskHandle_t                  done_task;
    esp_err_t                     result;
} display_task_arg_t;

#define UI_COLOR_RED    COLOR_RGB565_RED
#define UI_COLOR_WHITE  COLOR_RGB565_WHITE
#define UI_COLOR_BLUE   COLOR_RGB565_BLUE

#if CONFIG_ESP_PAINTER_BASIC_FONT_32
#define UI_FONT_SIZE  (32)
#elif CONFIG_ESP_PAINTER_BASIC_FONT_24
#define UI_FONT_SIZE  (24)
#elif CONFIG_ESP_PAINTER_BASIC_FONT_20
#define UI_FONT_SIZE  (20)
#elif CONFIG_ESP_PAINTER_BASIC_FONT_16
#define UI_FONT_SIZE  (16)
#else
#define UI_FONT_SIZE  (12)
#endif  /* CONFIG_ESP_PAINTER_BASIC_FONT_32 */

#define UI_BADGE_PAD_X      (12)
#define UI_BADGE_TEXT_Y     (6)
#define UI_TIMER_X          (8)
#define UI_TIMER_Y          (8)
#define UI_TIMER_WIDTH      (((5 * UI_FONT_SIZE) / 2) + (2 * UI_BADGE_PAD_X))
#define UI_TIMER_HEIGHT     (UI_FONT_SIZE + 20)
#define UI_TIMER_TEXT_X     (UI_BADGE_PAD_X)
#define UI_TIMER_TEXT_Y     (UI_BADGE_TEXT_Y)
#define UI_FPS_WIDTH        (((6 * UI_FONT_SIZE) / 2) + (2 * UI_BADGE_PAD_X))
#define UI_FPS_HEIGHT       (UI_FONT_SIZE + 20)
#define UI_FPS_TEXT_X       (UI_BADGE_PAD_X)
#define UI_FPS_TEXT_Y       (UI_BADGE_TEXT_Y)
#define UI_BTN_RADIUS       (32)
#define UI_BTN_REGION_SIZE  (72)
#define UI_BTN_MARGIN_Y     (52)
#define UI_BTN_STOP_SIZE    (22)
#define UI_BTN_DOT_RADIUS   (11)
#define UI_RENDER_FB_NUM    (2)

typedef struct {
    bool  touch_down;
    int   last_touch_x;
    int   last_touch_y;
    int   button_cx;
    int   button_cy;
    int   button_radius;
    int   x_offset;
    int   y_offset;
    int   panel_width;
    int   panel_height;
    bool  touch_available;
} display_ui_state_t;

typedef struct {
    esp_gmf_pool_handle_t             pool;
    esp_video_render_handle_t         render;
    esp_video_render_stream_handle_t  video_stream;
    esp_video_render_stream_handle_t  ui_stream;
    esp_vui_overlay_handle_t          overlay;
    esp_vui_container_handle_t        btn_container;
    esp_vui_widget_t                 *btn_idle;
    esp_vui_widget_t                 *btn_recording;
    esp_vui_container_handle_t        timer_container;
    esp_vui_container_handle_t        fps_container;
    esp_vui_widget_t                 *timer_badge;
    esp_vui_widget_t                 *fps_badge;
    esp_video_render_img_t            img_idle;
    esp_video_render_img_t            img_recording;
    esp_video_render_format_t         out_format;
} display_render_ctx_t;

static void ui_init_state(const av_record_live_display_sys_t *sys, display_ui_state_t *ui_state)
{
    ui_state->touch_available = sys->touch_handle != NULL;
    ui_state->button_radius = UI_BTN_RADIUS;
    ui_state->panel_width = sys->lcd_cfg ? (int)sys->lcd_cfg->lcd_width : (int)sys->display_info.width;
    ui_state->panel_height = sys->lcd_cfg ? (int)sys->lcd_cfg->lcd_height : (int)sys->display_info.height;
    ui_state->x_offset = ui_state->panel_width > (int)sys->display_info.width ?
                         (ui_state->panel_width - (int)sys->display_info.width) / 2 : 0;
    ui_state->y_offset = ui_state->panel_height > (int)sys->display_info.height ?
                         (ui_state->panel_height - (int)sys->display_info.height) / 2 : 0;
    /* Button is drawn in panel coordinates by video_render. */
    ui_state->button_cx = ui_state->x_offset + (int)sys->display_info.width / 2;
    ui_state->button_cy = ui_state->y_offset + (int)sys->display_info.height - UI_BTN_MARGIN_Y;
}

static void ui_get_timer_region(esp_capture_rgn_t *rgn)
{
    rgn->x = UI_TIMER_X;
    rgn->y = UI_TIMER_Y;
    rgn->width = UI_TIMER_WIDTH;
    rgn->height = UI_TIMER_HEIGHT;
}

static void ui_get_fps_region(const av_record_live_display_sys_t *sys, esp_capture_rgn_t *rgn)
{
    rgn->x = (int)sys->display_info.width - UI_TIMER_X - UI_FPS_WIDTH;
    rgn->y = UI_TIMER_Y;
    rgn->width = UI_FPS_WIDTH;
    rgn->height = UI_FPS_HEIGHT;
}

static void destroy_overlay_handle(esp_capture_overlay_if_t **overlay)
{
    if (overlay == NULL || *overlay == NULL) {
        return;
    }
    (*overlay)->close(*overlay);
    free(*overlay);
    *overlay = NULL;
}

/* The overlay is used as a standalone text canvas: it is never attached to a capture sink, so its
 * pixels cannot reach the record path. `esp_video_render` blends the canvas onto the LCD instead. */
static esp_capture_overlay_if_t *create_overlay(const esp_capture_rgn_t *rgn)
{
    esp_capture_rgn_t overlay_rgn = *rgn;
    esp_capture_overlay_if_t *overlay = esp_capture_new_text_overlay(&overlay_rgn);
    if (overlay == NULL) {
        ESP_LOGE(TAG, "Failed to allocate overlay");
        return NULL;
    }
    if (overlay->open(overlay) != ESP_CAPTURE_ERR_OK) {
        ESP_LOGE(TAG, "Failed to open overlay");
        free(overlay);
        return NULL;
    }
    return overlay;
}

/* Widgets reference the canvas memory directly, so compose must be told the content changed. */
static void badge_mark_updated(esp_vui_container_handle_t container)
{
    if (container == NULL) {
        return;
    }
    esp_vui_container_compose_lock(container);
    esp_video_render_err_t ret = esp_vui_container_notify_compose_changed(container, NULL, true);
    esp_vui_container_compose_unlock(container);
    if (ret != ESP_VIDEO_RENDER_ERR_OK) {
        ESP_LOGW(TAG, "Failed to mark badge dirty, ret=%d", ret);
    }
}

static esp_capture_err_t overlay_clear_all(esp_capture_overlay_if_t *overlay, uint16_t width, uint16_t height, uint16_t color)
{
    esp_capture_rgn_t rgn = {
        .x = 0,
        .y = 0,
        .width = width,
        .height = height,
    };
    return esp_capture_text_overlay_clear(overlay, &rgn, color);
}

static esp_err_t hide_timer_overlay(av_record_live_display_sys_t *sys, display_render_ctx_t *ctx)
{
    if (sys->timer_overlay == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx && ctx->timer_badge) {
        esp_video_render_err_t ret = esp_vui_widget_set_visible(ctx->timer_badge, false);
        ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, return ESP_FAIL, "Failed to hide timer badge");
    }
    sys->last_timer_sec = UINT32_MAX;
    sys->last_recording = false;
    return ESP_OK;
}

static esp_err_t update_timer_overlay(av_record_live_display_sys_t *sys, display_render_ctx_t *ctx, int64_t elapsed_ms)
{
    if (sys->timer_overlay == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t total_seconds = elapsed_ms > 0 ? (uint32_t)(elapsed_ms / 1000) : 0;
    uint32_t minutes = (total_seconds / 60) % 100;
    uint32_t seconds = total_seconds % 60;
    esp_capture_text_overlay_draw_info_t draw_info = {
        .color = UI_COLOR_WHITE,
        .font_size = UI_FONT_SIZE,
        .x = UI_TIMER_TEXT_X,
        .y = UI_TIMER_TEXT_Y,
    };
    esp_capture_err_t ret = esp_capture_text_overlay_draw_start(sys->timer_overlay);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to start timer overlay draw");
    ret = overlay_clear_all(sys->timer_overlay, UI_TIMER_WIDTH, UI_TIMER_HEIGHT, UI_COLOR_RED);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to draw timer background");
    ret = esp_capture_text_overlay_draw_text_fmt(sys->timer_overlay, &draw_info, "%02u:%02u",
                                                 (unsigned)minutes, (unsigned)seconds);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to draw timer text");
    ret = esp_capture_text_overlay_draw_finished(sys->timer_overlay);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to finish timer overlay draw");
    if (ctx && ctx->timer_badge) {
        if (sys->last_recording == false) {
            esp_video_render_err_t vr_ret = esp_vui_widget_set_visible(ctx->timer_badge, true);
            ESP_GMF_CHECK(TAG, vr_ret == ESP_VIDEO_RENDER_ERR_OK, return ESP_FAIL, "Failed to show timer badge");
        }
        badge_mark_updated(ctx->timer_container);
    }
    sys->last_timer_sec = total_seconds;
    sys->last_recording = true;
    return ESP_OK;
}

static esp_err_t update_fps_overlay(av_record_live_display_sys_t *sys, display_render_ctx_t *ctx, uint32_t fps_value)
{
    if (sys->fps_overlay == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t fps = fps_value > 99 ? 99 : fps_value;
    esp_capture_text_overlay_draw_info_t draw_info = {
        .color = UI_COLOR_WHITE,
        .font_size = UI_FONT_SIZE,
        .x = UI_FPS_TEXT_X,
        .y = UI_FPS_TEXT_Y,
    };
    esp_capture_err_t ret = esp_capture_text_overlay_draw_start(sys->fps_overlay);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to start FPS overlay draw");
    ret = overlay_clear_all(sys->fps_overlay, UI_FPS_WIDTH, UI_FPS_HEIGHT, UI_COLOR_BLUE);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to draw FPS background");
    ret = esp_capture_text_overlay_draw_text_fmt(sys->fps_overlay, &draw_info, "FPS:%02u", (unsigned)fps);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to draw FPS text");
    ret = esp_capture_text_overlay_draw_finished(sys->fps_overlay);
    ESP_GMF_CHECK(TAG, ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to finish FPS overlay draw");
    if (ctx) {
        badge_mark_updated(ctx->fps_container);
    }
    sys->last_fps = fps;
    return ESP_OK;
}

static bool ui_hit_record_button(const display_ui_state_t *ui_state, int panel_x, int panel_y)
{
    int dx = panel_x - ui_state->button_cx;
    int dy = panel_y - ui_state->button_cy;
    return (dx * dx) + (dy * dy) <= (ui_state->button_radius * ui_state->button_radius);
}

static bool ui_poll_touch_release(const av_record_live_display_sys_t *sys, display_ui_state_t *ui_state,
                                  int *release_x, int *release_y)
{
    if (!ui_state->touch_available || sys->touch_handle == NULL) {
        return false;
    }
    if (esp_lcd_touch_read_data(sys->touch_handle) != ESP_OK) {
        return false;
    }
    esp_lcd_touch_point_data_t point = {0};
    uint8_t point_count = 0;
    if (esp_lcd_touch_get_data(sys->touch_handle, &point, &point_count, 1) != ESP_OK || point_count == 0) {
        if (!ui_state->touch_down) {
            return false;
        }
        ui_state->touch_down = false;
        *release_x = ui_state->last_touch_x;
        *release_y = ui_state->last_touch_y;
        return true;
    }
    ui_state->last_touch_x = point.x;
    ui_state->last_touch_y = point.y;
    ui_state->touch_down = true;
    return false;
}

static esp_video_render_format_t capture_to_render_format(esp_capture_format_id_t format_id)
{
    if (format_id == ESP_CAPTURE_FMT_ID_RGB565_BE) {
        return ESP_VIDEO_RENDER_FORMAT_RGB565_BE;
    }
    return ESP_VIDEO_RENDER_FORMAT_RGB565;
}

static esp_err_t fill_lcd_backend_cfg(const av_record_live_display_sys_t *sys,
                                      const display_ui_state_t *ui_state,
                                      esp_video_render_lcd_cfg_t *lcd_cfg)
{
    memset(lcd_cfg, 0, sizeof(*lcd_cfg));
    lcd_cfg->fb_num = UI_RENDER_FB_NUM;
    lcd_cfg->width = (uint16_t)ui_state->panel_width;
    lcd_cfg->height = (uint16_t)ui_state->panel_height;
    lcd_cfg->lcd_handle = sys->lcd_handles->panel_handle;
    lcd_cfg->io_handle = sys->lcd_handles->io_handle;
    lcd_cfg->out_format = capture_to_render_format(sys->display_info.format_id);

    const char *sub_type = sys->lcd_cfg && sys->lcd_cfg->sub_type ? sys->lcd_cfg->sub_type : "";
    if (strcmp(sub_type, "spi") == 0) {
        lcd_cfg->lcd_type = ESP_VIDEO_RENDER_LCD_TYPE_DVP;
        lcd_cfg->out_format = ESP_VIDEO_RENDER_FORMAT_RGB565_BE;
    } else if (strcmp(sub_type, "rgb") == 0) {
        lcd_cfg->lcd_type = ESP_VIDEO_RENDER_LCD_TYPE_RGB;
        lcd_cfg->out_format = ESP_VIDEO_RENDER_FORMAT_RGB565;
    } else if (strcmp(sub_type, "i80") == 0) {
        lcd_cfg->lcd_type = ESP_VIDEO_RENDER_LCD_TYPE_I80;
        lcd_cfg->out_format = ESP_VIDEO_RENDER_FORMAT_RGB565;
    } else if (strcmp(sub_type, "dsi") == 0) {
        lcd_cfg->lcd_type = ESP_VIDEO_RENDER_LCD_TYPE_DPI;
        lcd_cfg->out_format = ESP_VIDEO_RENDER_FORMAT_RGB565;
    } else {
        ESP_LOGE(TAG, "Unsupported LCD bus type: %s", sub_type);
        return ESP_ERR_NOT_SUPPORTED;
    }
    return ESP_OK;
}

static inline uint16_t rgb565_color(bool be, uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t color = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    return be ? __builtin_bswap16(color) : color;
}

static void put_px(esp_video_render_img_t *img, int x, int y, uint16_t color)
{
    if (img == NULL || img->data == NULL || x < 0 || y < 0 ||
        x >= img->info.width || y >= img->info.height) {
        return;
    }
    ((uint16_t *)img->data)[y * img->info.width + x] = color;
}

static void fill_img(esp_video_render_img_t *img, uint16_t color)
{
    if (img == NULL || img->data == NULL) {
        return;
    }
    uint16_t *pixels = (uint16_t *)img->data;
    int count = img->info.width * img->info.height;
    for (int i = 0; i < count; i++) {
        pixels[i] = color;
    }
}

static void fill_circle(esp_video_render_img_t *img, int cx, int cy, int radius, uint16_t color)
{
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            int dx = x - cx;
            int dy = y - cy;
            if ((dx * dx) + (dy * dy) <= (radius * radius)) {
                put_px(img, x, y, color);
            }
        }
    }
}

static void fill_rect(esp_video_render_img_t *img, int x0, int y0, int w, int h, uint16_t color)
{
    for (int y = y0; y < y0 + h; y++) {
        for (int x = x0; x < x0 + w; x++) {
            put_px(img, x, y, color);
        }
    }
}

static esp_err_t alloc_button_image(esp_video_render_img_t *img, esp_video_render_format_t format)
{
    memset(img, 0, sizeof(*img));
    img->info.format = format;
    img->info.width = UI_BTN_REGION_SIZE;
    img->info.height = UI_BTN_REGION_SIZE;
    img->size = (uint32_t)(UI_BTN_REGION_SIZE * UI_BTN_REGION_SIZE * 2);
    img->data = (uint8_t *)malloc(img->size);
    ESP_GMF_CHECK(TAG, img->data != NULL, return ESP_ERR_NO_MEM, "Failed to allocate button image");
    return ESP_OK;
}

static void free_button_image(esp_video_render_img_t *img)
{
    if (img == NULL) {
        return;
    }
    free(img->data);
    img->data = NULL;
    img->size = 0;
}

static void draw_record_button_image(esp_video_render_img_t *img, bool recording)
{
    bool be = (img->info.format == ESP_VIDEO_RENDER_FORMAT_RGB565_BE);
    uint16_t trans = rgb565_color(be, 0, 255, 0);
    uint16_t red = rgb565_color(be, 255, 0, 0);
    uint16_t white = rgb565_color(be, 255, 255, 255);
    int center = UI_BTN_REGION_SIZE / 2;

    fill_img(img, trans);
    fill_circle(img, center, center, UI_BTN_RADIUS, red);
    if (recording) {
        int stop_half = UI_BTN_STOP_SIZE / 2;
        fill_rect(img, center - stop_half, center - stop_half, UI_BTN_STOP_SIZE, UI_BTN_STOP_SIZE, white);
    } else {
        fill_circle(img, center, center, UI_BTN_DOT_RADIUS, white);
    }
}

static void update_button_widgets(display_render_ctx_t *ctx, bool recording)
{
    if (ctx->btn_idle == NULL || ctx->btn_recording == NULL) {
        return;
    }
    esp_vui_widget_set_visible(ctx->btn_idle, !recording);
    esp_vui_widget_set_visible(ctx->btn_recording, recording);
}

/* Video render resolves scale/crop/color-convert elements from this pool by capability. */
static esp_err_t create_render_pool(esp_gmf_pool_handle_t *pool)
{
    *pool = NULL;
    ESP_GMF_CHECK(TAG, esp_gmf_pool_init(pool) == ESP_GMF_ERR_OK, return ESP_FAIL, "Failed to init render pool");

    esp_gmf_element_handle_t element = NULL;
#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31
    ESP_GMF_CHECK(TAG, esp_gmf_video_ppa_init(NULL, &element) == ESP_GMF_ERR_OK, goto fail, "Failed to init PPA element");
    ESP_GMF_CHECK(TAG, esp_gmf_pool_register_element(*pool, element, NULL) == ESP_GMF_ERR_OK, goto fail, "Failed to register PPA element");
#else
    esp_imgfx_scale_cfg_t scale_cfg = {
        .filter_type = ESP_IMGFX_SCALE_FILTER_TYPE_BILINEAR,
    };
    ESP_GMF_CHECK(TAG, esp_gmf_video_scale_init(&scale_cfg, &element) == ESP_GMF_ERR_OK, goto fail, "Failed to init scale element");
    ESP_GMF_CHECK(TAG, esp_gmf_pool_register_element(*pool, element, NULL) == ESP_GMF_ERR_OK, goto fail, "Failed to register scale element");

    element = NULL;
    esp_imgfx_crop_cfg_t crop_cfg = {0};
    ESP_GMF_CHECK(TAG, esp_gmf_video_crop_init(&crop_cfg, &element) == ESP_GMF_ERR_OK, goto fail, "Failed to init crop element");
    ESP_GMF_CHECK(TAG, esp_gmf_pool_register_element(*pool, element, NULL) == ESP_GMF_ERR_OK, goto fail, "Failed to register crop element");
#endif  /* CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S31 */

    element = NULL;
    esp_imgfx_color_convert_cfg_t color_convert_cfg = {
        .color_space_std = ESP_IMGFX_COLOR_SPACE_STD_BT601,
    };
    ESP_GMF_CHECK(TAG, esp_gmf_video_color_convert_init(&color_convert_cfg, &element) == ESP_GMF_ERR_OK, goto fail,
                  "Failed to init color convert element");
    ESP_GMF_CHECK(TAG, esp_gmf_pool_register_element(*pool, element, NULL) == ESP_GMF_ERR_OK, goto fail,
                  "Failed to register color convert element");
    return ESP_OK;

fail:
    if (element) {
        esp_gmf_element_deinit(element);
    }
    esp_gmf_pool_deinit(*pool);
    *pool = NULL;
    return ESP_FAIL;
}

static void destroy_display_render(display_render_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    if (ctx->btn_container) {
        esp_vui_container_destroy(ctx->btn_container);
        ctx->btn_container = NULL;
        ctx->btn_idle = NULL;
        ctx->btn_recording = NULL;
    }
    if (ctx->timer_container) {
        esp_vui_container_destroy(ctx->timer_container);
        ctx->timer_container = NULL;
        ctx->timer_badge = NULL;
    }
    if (ctx->fps_container) {
        esp_vui_container_destroy(ctx->fps_container);
        ctx->fps_container = NULL;
        ctx->fps_badge = NULL;
    }
    if (ctx->ui_stream) {
        esp_video_render_stream_close(ctx->ui_stream);
        ctx->ui_stream = NULL;
        ctx->overlay = NULL;
    }
    if (ctx->video_stream) {
        esp_video_render_stream_close(ctx->video_stream);
        ctx->video_stream = NULL;
    }
    if (ctx->render) {
        esp_video_render_destroy(ctx->render);
        ctx->render = NULL;
    }
    if (ctx->pool) {
        esp_gmf_pool_deinit(ctx->pool);
        ctx->pool = NULL;
    }
    free_button_image(&ctx->img_idle);
    free_button_image(&ctx->img_recording);
}

/* The badge widget references the text canvas memory, so the canvas must outlive the widget. */
static esp_err_t create_badge_widget(display_render_ctx_t *ctx, esp_capture_overlay_if_t *canvas,
                                     const esp_capture_rgn_t *rgn, const display_ui_state_t *ui_state,
                                     esp_vui_container_handle_t *container, esp_vui_widget_t **widget)
{
    esp_capture_stream_frame_t frame = {0};
    esp_capture_err_t cap_ret = canvas->acquire_frame(canvas, &frame);
    ESP_GMF_CHECK(TAG, cap_ret == ESP_CAPTURE_ERR_OK, return ESP_FAIL, "Failed to acquire badge canvas");
    esp_video_render_img_t img = {
        .info = {
            /* Text canvas always renders native RGB565; bitblt swaps bytes for big endian panels. */
            .format = ESP_VIDEO_RENDER_FORMAT_RGB565,
            .width = (uint16_t)rgn->width,
            .height = (uint16_t)rgn->height,
        },
        .data = frame.data,
        .size = (uint32_t)frame.size,
    };
    canvas->release_frame(canvas, &frame);

    esp_video_render_frame_info_t container_info = {
        .format = ctx->out_format,
        .width = (uint16_t)rgn->width,
        .height = (uint16_t)rgn->height,
    };
    esp_video_render_pos_t container_pos = {
        .x = (uint16_t)(ui_state->x_offset + rgn->x),
        .y = (uint16_t)(ui_state->y_offset + rgn->y),
    };
    /* Badges are opaque, so no container cache is needed: widgets blend straight to the panel. */
    esp_video_render_err_t ret = esp_vui_container_create(ctx->overlay, &container_info, &container_pos, false, container);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, return ESP_FAIL, "Failed to create badge container");

    esp_video_render_pos_t widget_pos = {.x = 0, .y = 0};
    *widget = esp_vui_image_widget_init(*container, &img, &widget_pos);
    ESP_GMF_CHECK(TAG, *widget != NULL, return ESP_FAIL, "Failed to create badge widget");
    return ESP_OK;
}

static esp_err_t create_display_render(av_record_live_display_sys_t *sys,
                                       const display_ui_state_t *ui_state,
                                       display_render_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));

    esp_video_render_lcd_cfg_t lcd_cfg = {0};
    ESP_GMF_CHECK(TAG, fill_lcd_backend_cfg(sys, ui_state, &lcd_cfg) == ESP_OK, return ESP_FAIL, "Failed to fill LCD backend cfg");
    ctx->out_format = lcd_cfg.out_format;

    ESP_GMF_CHECK(TAG, create_render_pool(&ctx->pool) == ESP_OK, return ESP_FAIL, "Failed to create render pool");

    esp_video_render_cfg_t render_cfg = {
        .pool = ctx->pool,
        .fps = (uint8_t)sys->display_info.fps,
    };
    esp_video_render_err_t ret = esp_video_render_create(&render_cfg, &ctx->render);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to create video render");

    esp_video_render_backend_cfg_t backend_cfg = {
        .ops = esp_video_render_get_lcd_backend(),
        .cfg = &lcd_cfg,
        .cfg_size = sizeof(lcd_cfg),
    };
    ret = esp_video_render_set_display(ctx->render, &backend_cfg);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set LCD backend");

    ret = esp_video_render_set_compose_mode(ctx->render, ESP_VIDEO_RENDER_COMPOSE_MODE_MANUAL);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set manual compose mode");

    esp_video_render_clr_t bg = {.r = 0, .g = 0, .b = 0};
    ret = esp_video_render_set_bg_color(ctx->render, &bg);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set background color");

    esp_video_render_stream_info_t video_info = {
        .info = {
            .format = capture_to_render_format(sys->display_info.format_id),
            .width = sys->display_info.width,
            .height = sys->display_info.height,
            .fps = 0,
        },
        .cached = false,
    };
    ret = esp_video_render_stream_open(ctx->render, &video_info, &ctx->video_stream);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to open video stream");

    esp_video_render_rect_t video_rect = {
        .x = (uint16_t)ui_state->x_offset,
        .y = (uint16_t)ui_state->y_offset,
        .width = sys->display_info.width,
        .height = sys->display_info.height,
    };
    ret = esp_video_render_stream_set_disp_rect(ctx->video_stream, &video_rect);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set video display rect");
    ret = esp_video_render_stream_set_zorder(ctx->video_stream, 0);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set video zorder");

    esp_video_render_stream_info_t ui_info = {
        .info = {
            .format = ctx->out_format,
            .width = (uint16_t)ui_state->panel_width,
            .height = (uint16_t)ui_state->panel_height,
            .fps = 0,
        },
        .cached = false,
    };
    ret = esp_video_render_stream_open(ctx->render, &ui_info, &ctx->ui_stream);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to open UI stream");
    ret = esp_video_render_stream_set_zorder(ctx->ui_stream, 1);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set UI zorder");

    ret = esp_video_render_stream_get_overlay(ctx->ui_stream, &ctx->overlay);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to get UI overlay");

    ESP_GMF_CHECK(TAG, alloc_button_image(&ctx->img_idle, ctx->out_format) == ESP_OK, goto fail, "Failed to alloc idle button");
    ESP_GMF_CHECK(TAG, alloc_button_image(&ctx->img_recording, ctx->out_format) == ESP_OK, goto fail, "Failed to alloc recording button");
    draw_record_button_image(&ctx->img_idle, false);
    draw_record_button_image(&ctx->img_recording, true);

    esp_video_render_frame_info_t container_info = {
        .format = ctx->out_format,
        .width = UI_BTN_REGION_SIZE,
        .height = UI_BTN_REGION_SIZE,
    };
    esp_video_render_pos_t container_pos = {
        .x = (uint16_t)(ui_state->button_cx - (UI_BTN_REGION_SIZE / 2)),
        .y = (uint16_t)(ui_state->button_cy - (UI_BTN_REGION_SIZE / 2)),
    };
    ret = esp_vui_container_create(ctx->overlay, &container_info, &container_pos, true, &ctx->btn_container);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to create button container");

    esp_video_render_clr_t trans = {.r = 0, .g = 255, .b = 0};
    ret = esp_vui_container_set_transparent_color(ctx->btn_container, true, &trans);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set container transparent color");
    ret = esp_vui_container_set_bg_color(ctx->btn_container, &trans);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to fill transparent container background");

    esp_video_render_pos_t widget_pos = {.x = 0, .y = 0};
    ctx->btn_idle = esp_vui_image_widget_init(ctx->btn_container, &ctx->img_idle, &widget_pos);
    ctx->btn_recording = esp_vui_image_widget_init(ctx->btn_container, &ctx->img_recording, &widget_pos);
    ESP_GMF_CHECK(TAG, ctx->btn_idle && ctx->btn_recording, goto fail, "Failed to create button widgets");

    ret = esp_vui_image_widget_set_transparent_color(ctx->btn_idle, true, &trans);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set idle button transparent color");
    ret = esp_vui_image_widget_set_transparent_color(ctx->btn_recording, true, &trans);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set recording button transparent color");

    update_button_widgets(ctx, sys->recording);

    ESP_GMF_CHECK(TAG, sys->timer_overlay && sys->fps_overlay, goto fail, "Badge canvases not created");
    esp_capture_rgn_t timer_rgn = {0};
    esp_capture_rgn_t fps_rgn = {0};
    ui_get_timer_region(&timer_rgn);
    ui_get_fps_region(sys, &fps_rgn);
    ESP_GMF_CHECK(TAG, create_badge_widget(ctx, sys->timer_overlay, &timer_rgn, ui_state,
                                           &ctx->timer_container, &ctx->timer_badge) == ESP_OK,
                  goto fail, "Failed to create timer badge");
    ESP_GMF_CHECK(TAG, create_badge_widget(ctx, sys->fps_overlay, &fps_rgn, ui_state,
                                           &ctx->fps_container, &ctx->fps_badge) == ESP_OK,
                  goto fail, "Failed to create FPS badge");
    ret = esp_vui_widget_set_visible(ctx->timer_badge, sys->recording);
    ESP_GMF_CHECK(TAG, ret == ESP_VIDEO_RENDER_ERR_OK, goto fail, "Failed to set timer badge visibility");
    ESP_GMF_CHECK(TAG, update_fps_overlay(sys, ctx, 0) == ESP_OK, goto fail, "Failed to draw FPS badge");
    return ESP_OK;

fail:
    destroy_display_render(ctx);
    return ESP_FAIL;
}

esp_err_t av_rec_display_create_overlays(av_record_live_display_sys_t *sys)
{
    ESP_GMF_CHECK(TAG, sys != NULL, return ESP_ERR_INVALID_ARG, "Invalid system context");
    if (sys->timer_overlay != NULL || sys->fps_overlay != NULL) {
        return ESP_OK;
    }

    esp_capture_rgn_t timer_rgn = {0};
    esp_capture_rgn_t fps_rgn = {0};
    ui_get_timer_region(&timer_rgn);
    ui_get_fps_region(sys, &fps_rgn);

    sys->timer_overlay = create_overlay(&timer_rgn);
    sys->fps_overlay = create_overlay(&fps_rgn);
    ESP_GMF_CHECK(TAG, sys->timer_overlay && sys->fps_overlay, goto fail, "Failed to create overlays");

    sys->last_timer_sec = UINT32_MAX;
    sys->last_fps = UINT32_MAX;
    sys->last_recording = false;
    /* Content is drawn once the render widgets exist, see create_display_render(). */
    return ESP_OK;

fail:
    av_rec_display_destroy_overlays(sys);
    return ESP_FAIL;
}

void av_rec_display_destroy_overlays(av_record_live_display_sys_t *sys)
{
    if (sys == NULL) {
        return;
    }
    destroy_overlay_handle(&sys->timer_overlay);
    destroy_overlay_handle(&sys->fps_overlay);
    sys->last_timer_sec = UINT32_MAX;
    sys->last_fps = UINT32_MAX;
    sys->last_recording = false;
}

static esp_err_t run_display_loop(av_record_live_display_sys_t *sys)
{
    int64_t last_report_ms = esp_timer_get_time() / 1000;
    uint32_t frame_count = 0;
    uint32_t last_report_frame_count = 0;
    uint32_t expected_frame_size = sys->display_info.width * sys->display_info.height * 2;
    display_ui_state_t ui_state = {0};
    display_render_ctx_t render_ctx = {0};
    ui_init_state(sys, &ui_state);

    ESP_GMF_CHECK(TAG, create_display_render(sys, &ui_state, &render_ctx) == ESP_OK, return ESP_FAIL,
                  "Failed to create video render path");

    esp_capture_stream_frame_t frame = {
        .stream_type = ESP_CAPTURE_STREAM_TYPE_VIDEO,
    };

    ESP_LOGI(TAG, "Interactive UI ready, touch=%s, offset=(%d,%d), panel=%dx%d",
             ui_state.touch_available ? "yes" : "no", ui_state.x_offset, ui_state.y_offset,
             ui_state.panel_width, ui_state.panel_height);
    ESP_LOGI(TAG, "Live display loop started (manual compose)");

    while (true) {
        esp_capture_err_t ret = esp_capture_sink_acquire_frame(sys->display_sink, &frame, false);
        if (ret != ESP_CAPTURE_ERR_OK) {
            ESP_LOGW(TAG, "Failed to acquire display frame, ret=%d", ret);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        ESP_GMF_CHECK(TAG, (uint32_t)frame.size == expected_frame_size, {
            esp_capture_sink_release_frame(sys->display_sink, &frame);
            destroy_display_render(&render_ctx);
            return ESP_FAIL;
        }, "Unexpected display frame size");

        esp_video_render_frame_t vr_frame = {
            .format = capture_to_render_format(sys->display_info.format_id),
            .width = sys->display_info.width,
            .height = sys->display_info.height,
            .data = frame.data,
            .size = (uint32_t)frame.size,
            .pts = frame.pts,
        };
        esp_video_render_err_t vr_ret = esp_video_render_stream_write(render_ctx.video_stream, &vr_frame);
        if (vr_ret != ESP_VIDEO_RENDER_ERR_OK) {
            ESP_LOGW(TAG, "Failed to write video frame, ret=%d", vr_ret);
        } else {
            vr_ret = esp_video_render_compose(render_ctx.render);
            if (vr_ret != ESP_VIDEO_RENDER_ERR_OK) {
                ESP_LOGW(TAG, "Failed to compose display frame, ret=%d", vr_ret);
            }
        }

        uint32_t saved_pts = frame.pts;
        esp_capture_sink_release_frame(sys->display_sink, &frame);
        frame_count++;

        int64_t now_ms = esp_timer_get_time() / 1000;
        if (sys->recording && sys->record_start_ms > 0) {
            int64_t record_elapsed_ms = now_ms - sys->record_start_ms;
            if (record_elapsed_ms < 0) {
                record_elapsed_ms = 0;
            }
            uint32_t current_sec = (uint32_t)(record_elapsed_ms / 1000);
            if (current_sec != sys->last_timer_sec || !sys->last_recording) {
                esp_err_t overlay_ret = update_timer_overlay(sys, &render_ctx, record_elapsed_ms);
                if (overlay_ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to update timer overlay, ret=%d", overlay_ret);
                }
            }
        } else if (sys->last_recording) {
            esp_err_t overlay_ret = hide_timer_overlay(sys, &render_ctx);
            if (overlay_ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to hide timer overlay, ret=%d", overlay_ret);
            }
        }

        int release_x = 0;
        int release_y = 0;
        if (ui_poll_touch_release(sys, &ui_state, &release_x, &release_y) &&
            ui_hit_record_button(&ui_state, release_x, release_y)) {
            esp_err_t record_ret = sys->recording ? av_rec_stop_record(sys) : av_rec_start_record(sys);
            if (record_ret != ESP_OK) {
                ESP_LOGW(TAG, "Record toggle failed, ret=%d", record_ret);
            } else {
                update_button_widgets(&render_ctx, sys->recording);
                esp_err_t overlay_ret = ESP_OK;
                if (sys->recording) {
                    overlay_ret = update_timer_overlay(sys, &render_ctx, 0);
                } else {
                    overlay_ret = hide_timer_overlay(sys, &render_ctx);
                }
                if (overlay_ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to update record status overlay, ret=%d", overlay_ret);
                }
                (void)esp_video_render_compose(render_ctx.render);
            }
        }

        if (now_ms > last_report_ms + 1000) {
            int64_t interval_ms = now_ms - last_report_ms;
            uint32_t interval_frames = frame_count - last_report_frame_count;
            float fps = interval_ms ? (interval_frames * 1000.0f / interval_ms) : 0.0f;
            uint32_t current_fps = interval_ms ? (uint32_t)((interval_frames * 1000U + (uint32_t)(interval_ms / 2)) / (uint32_t)interval_ms) : 0;
            if (current_fps != sys->last_fps) {
                esp_err_t overlay_ret = update_fps_overlay(sys, &render_ctx, current_fps);
                if (overlay_ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to update FPS overlay, ret=%d", overlay_ret);
                }
            }
            ESP_LOGI(TAG, "Display fps=%.2f, frames=%" PRIu32 ", pts=%" PRIu32,
                     fps, frame_count, saved_pts);
            last_report_ms = now_ms;
            last_report_frame_count = frame_count;
        }
    }
}

static void display_task(void *arg)
{
    display_task_arg_t *task_arg = (display_task_arg_t *)arg;
    task_arg->result = run_display_loop(task_arg->sys);
    xTaskNotifyGive(task_arg->done_task);
    vTaskDelete(NULL);
}

esp_err_t av_rec_run_display(av_record_live_display_sys_t *sys)
{
    display_task_arg_t display_arg = {
        .sys = sys,
        .done_task = xTaskGetCurrentTaskHandle(),
        .result = ESP_FAIL,
    };
    BaseType_t task_ret = xTaskCreatePinnedToCore(display_task, "display", 8 * 1024,
                                                  &display_arg, 5, NULL, DISPLAY_CORE_ID);
    ESP_GMF_CHECK(TAG, task_ret == pdPASS, return ESP_FAIL, "Failed to create display task");
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return display_arg.result;
}
