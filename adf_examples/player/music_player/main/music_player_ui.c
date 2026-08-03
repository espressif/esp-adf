/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "music_player_ui.h"

#include <stdio.h>
#include <string.h>

#include "esp_gmf_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lv_adapter.h"
#include "music_player_display.h"
#include "music_player_playback.h"
#include "music_player_config.h"

static const char *TAG = "MUSIC_PLAYER_UI";

#define MUSIC_PLAYER_PLAYLIST_MAX_ITEMS      128

#define MUSIC_PLAYER_COLOR_BG           0x0E0E14
#define MUSIC_PLAYER_COLOR_PANEL        0x181824
#define MUSIC_PLAYER_COLOR_BTN          0x2A2A36
#define MUSIC_PLAYER_COLOR_ART          0x1A1A26
#define MUSIC_PLAYER_COLOR_TEXT         0xF2F2F5
#define MUSIC_PLAYER_COLOR_TEXT_DIM     0xA8A8B8
#define MUSIC_PLAYER_COLOR_TIME         0x8E8E9E
#define MUSIC_PLAYER_COLOR_ACCENT       0xFFD166
#define MUSIC_PLAYER_COLOR_PROGRESS_BG  0x2A2A36

#define UI_CLAMP(v, lo, hi)  ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

typedef struct {
    int  pad_x, pad_y_top, pad_y_bottom;
    int  btn_side, btn_nav, btn_play, art_size;
    int  dialog_w, dialog_h, ctrl_pad, ctrl_radius, font_size;
} music_player_ui_metrics_t;

typedef struct {
    lv_obj_t                  *screen;
    lv_obj_t                  *title_label;
    lv_obj_t                  *meta_label;
    lv_obj_t                  *progress_bar;
    lv_obj_t                  *elapsed_label;
    lv_obj_t                  *duration_label;
    lv_obj_t                  *play_btn;
    lv_obj_t                  *playlist_panel;
    lv_timer_t                *progress_timer;
    music_player_ui_metrics_t  metrics;
    const lv_font_t           *title_font;
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
    esp_lv_adapter_ft_font_handle_t  ft_font_handle;
#endif  /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE */
    bool  playing;
    int   volume;
    char  mode_text[32];
} music_player_ui_ctx_t;

typedef struct {
    const char *title;
    const char *mode_text;
    int         volume;
    bool        playing;
} music_player_ui_update_args_t;

static music_player_ui_ctx_t s_ui = {0};
static bool s_ui_inited = false;

typedef enum {
    MUSIC_PLAYER_UI_BTN_PREV = 1,
    MUSIC_PLAYER_UI_BTN_NEXT,
    MUSIC_PLAYER_UI_BTN_MODE,
    MUSIC_PLAYER_UI_BTN_LIST,
    MUSIC_PLAYER_UI_BTN_VOLUME_DOWN,
    MUSIC_PLAYER_UI_BTN_VOLUME_UP,
} music_player_ui_btn_id_t;

static void ui_metrics_init(music_player_ui_metrics_t *m)
{
    int w = lv_display_get_horizontal_resolution(NULL);
    int h = lv_display_get_vertical_resolution(NULL);
    if (w <= 0) {
        w = 800;
    }
    if (h <= 0) {
        h = 480;
    }
    bool large = (w >= 960 && h >= 560);
    bool mid = (!large && (w >= 700 || h >= 460));
    m->pad_x = large ? 56 : (mid ? 32 : 12);
    m->pad_y_top = large ? 28 : (mid ? 18 : 10);
    m->pad_y_bottom = large ? 26 : (mid ? 16 : 10);
    m->btn_side = large ? 56 : (mid ? 48 : 36);
    m->btn_nav = large ? 60 : (mid ? 52 : 40);
    m->btn_play = large ? 78 : (mid ? 64 : 48);
    m->art_size = large ? 120 : (mid ? 96 : 72);
    m->ctrl_pad = large ? 16 : (mid ? 12 : 8);
    m->ctrl_radius = large ? 24 : (mid ? 20 : 16);
    m->font_size = large ? MUSIC_PLAYER_FONT_SIZE : (mid ? 22 : 16);
    m->dialog_w = UI_CLAMP(large ? 720 : (mid ? 640 : 280), 200, w - m->pad_x * 2);
    m->dialog_h = UI_CLAMP(large ? 420 : (mid ? 360 : 220), 160, h - 24);
}

static inline void post_cmd(music_player_cmd_t cmd)
{
    if (music_player_playback_post(cmd) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post playback command: %d", cmd);
    }
}

static inline void post_playlist_cmd(music_player_cmd_t cmd)
{
    bool has_playlist = false;
    if (music_player_playback_has_playlist(&has_playlist) != ESP_OK || !has_playlist) {
        return;
    }
    post_cmd(cmd);
}

static inline void post_play_index_cmd(int index)
{
    bool has_playlist = false;
    if (music_player_playback_has_playlist(&has_playlist) != ESP_OK || !has_playlist) {
        return;
    }
    if (music_player_playback_post_index(MUSIC_PLAYER_CMD_PLAY_INDEX, index) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to post play index command: %d", index);
    }
}

static void format_time_ms(int ms, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) {
        return;
    }
    if (ms < 0) {
        ms = 0;
    }
    int total_sec = ms / 1000;
    int min = total_sec / 60;
    int sec = total_sec % 60;
    snprintf(buf, buf_size, "%d:%02d", min, sec);
}

static void refresh_meta_label(void)
{
    if (s_ui.meta_label == NULL) {
        return;
    }
    const char *mode = (s_ui.mode_text[0] != '\0') ? s_ui.mode_text : "";
    lv_label_set_text_fmt(s_ui.meta_label, "%s · 音量 %d%%", mode, s_ui.volume);
}

static void update_progress_widgets(int elapsed_ms, int duration_ms)
{
    char elapsed_text[16] = {0};
    char duration_text[16] = {0};

    if (duration_ms > 0) {
        int value = (int)(((int64_t)elapsed_ms * 1000) / duration_ms);
        if (value < 0) {
            value = 0;
        } else if (value > 1000) {
            value = 1000;
        }
        if (s_ui.progress_bar != NULL) {
            lv_bar_set_value(s_ui.progress_bar, value, LV_ANIM_OFF);
        }
        format_time_ms(elapsed_ms, elapsed_text, sizeof(elapsed_text));
        format_time_ms(duration_ms, duration_text, sizeof(duration_text));
    } else {
        if (s_ui.progress_bar != NULL) {
            lv_bar_set_value(s_ui.progress_bar, 0, LV_ANIM_OFF);
        }
        format_time_ms(elapsed_ms, elapsed_text, sizeof(elapsed_text));
        snprintf(duration_text, sizeof(duration_text), "--:--");
    }

    if (s_ui.elapsed_label != NULL) {
        lv_label_set_text(s_ui.elapsed_label, elapsed_text);
    }
    if (s_ui.duration_label != NULL) {
        lv_label_set_text(s_ui.duration_label, duration_text);
    }
}

static void progress_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    int elapsed_ms = 0;
    int duration_ms = 0;
    if (music_player_playback_get_progress(&elapsed_ms, &duration_ms) != ESP_OK) {
        return;
    }
    update_progress_widgets(elapsed_ms, duration_ms);
}

static void close_playlist_event_cb(lv_event_t *e)
{
    (void)e;
    if (s_ui.playlist_panel != NULL) {
        lv_obj_delete(s_ui.playlist_panel);
        s_ui.playlist_panel = NULL;
    }
}

static void playlist_row_event_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    post_play_index_cmd(index);
    close_playlist_event_cb(e);
}

static void create_playlist_dialog(void)
{
    if (s_ui.playlist_panel != NULL) {
        lv_obj_delete(s_ui.playlist_panel);
        s_ui.playlist_panel = NULL;
        return;
    }

    lv_obj_t *parent = (s_ui.screen != NULL) ? s_ui.screen : lv_screen_active();

    s_ui.playlist_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(s_ui.playlist_panel);
    lv_obj_set_size(s_ui.playlist_panel, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_ui.playlist_panel, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_ui.playlist_panel, LV_OPA_50, 0);
    lv_obj_add_flag(s_ui.playlist_panel, LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_ui.playlist_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_ui.playlist_panel, close_playlist_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *dialog = lv_obj_create(s_ui.playlist_panel);
    lv_obj_set_size(dialog, s_ui.metrics.dialog_w, s_ui.metrics.dialog_h);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(MUSIC_PLAYER_COLOR_PANEL), 0);
    lv_obj_set_style_text_color(dialog, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT), 0);
    lv_obj_set_style_border_width(dialog, 1, 0);
    lv_obj_set_style_border_color(dialog, lv_color_hex(0x2A2A36), 0);
    lv_obj_set_style_radius(dialog, 16, 0);
    lv_obj_set_style_pad_all(dialog, 12, 0);
    lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(dialog, 8, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(dialog, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *header = lv_obj_create(dialog);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_set_style_text_font(title, s_ui.title_font, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT), 0);
    lv_label_set_text(title, "播放列表");

    lv_obj_t *close_btn = lv_button_create(header);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(MUSIC_PLAYER_COLOR_BTN), 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, close_playlist_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_color(close_label, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT), 0);

    lv_obj_t *list = lv_obj_create(dialog);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(list, lv_color_hex(MUSIC_PLAYER_COLOR_PANEL), 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 4, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_text_color(list, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT), 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    int count = 0;
    int current = -1;
    esp_err_t ret = music_player_playback_get_track_count(&count);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Get playlist track count failed: %s", esp_err_to_name(ret));
    }
    ret = music_player_playback_get_current_index(&current);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Get current playlist index failed: %s", esp_err_to_name(ret));
    }
    int show_count = count > MUSIC_PLAYER_PLAYLIST_MAX_ITEMS ? MUSIC_PLAYER_PLAYLIST_MAX_ITEMS : count;
    ESP_LOGI(TAG, "Show playlist: count=%d, current=%d", count, current);
    if (show_count <= 0) {
        lv_obj_t *empty = lv_label_create(list);
        lv_obj_set_style_text_font(empty, s_ui.title_font, 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT), 0);
        lv_label_set_text(empty, "未找到音乐");
        return;
    }

    lv_obj_t *current_row = NULL;
    for (int i = 0; i < show_count; i++) {
        char title_buf[MUSIC_PLAYER_TITLE_MAX] = {0};
        if (music_player_playback_get_track_title(i, title_buf, sizeof(title_buf)) != ESP_OK) {
            continue;
        }
        char row_text[MUSIC_PLAYER_TITLE_MAX + 16] = {0};
        snprintf(row_text, sizeof(row_text), "%c %02d. %s", i == current ? '>' : ' ', i + 1, title_buf);
        lv_obj_t *row_btn = lv_button_create(list);
        lv_obj_set_width(row_btn, lv_pct(100));
        lv_obj_set_style_bg_opa(row_btn, i == current ? LV_OPA_30 : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(row_btn, lv_color_hex(MUSIC_PLAYER_COLOR_ACCENT), 0);
        lv_obj_set_style_border_width(row_btn, 0, 0);
        lv_obj_set_style_shadow_width(row_btn, 0, 0);
        lv_obj_set_style_pad_all(row_btn, 6, 0);
        lv_obj_add_event_cb(row_btn, playlist_row_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        if (i == current) {
            current_row = row_btn;
        }

        lv_obj_t *row = lv_label_create(row_btn);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_text_font(row, s_ui.title_font, 0);
        lv_obj_set_style_text_color(row,
                                    lv_color_hex(i == current ? MUSIC_PLAYER_COLOR_ACCENT : MUSIC_PLAYER_COLOR_TEXT),
                                    0);
        lv_label_set_long_mode(row, LV_LABEL_LONG_DOT);
        lv_label_set_text(row, row_text);
    }

    if (count > show_count) {
        char more_text[48] = {0};
        snprintf(more_text, sizeof(more_text), "还有 %d 首未显示", count - show_count);
        lv_obj_t *more = lv_label_create(list);
        lv_obj_set_style_text_font(more, s_ui.title_font, 0);
        lv_obj_set_style_text_color(more, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT_DIM), 0);
        lv_label_set_text(more, more_text);
    }

    if (current_row != NULL) {
        lv_obj_update_layout(list);
        lv_obj_scroll_to_y(list, lv_obj_get_y(current_row), LV_ANIM_OFF);
    }
}

static void btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target_obj(e);
    if (btn == s_ui.play_btn) {
        if (s_ui.playing) {
            post_cmd(MUSIC_PLAYER_CMD_PAUSE);
        } else {
            bool has_playlist = false;
            if (music_player_playback_has_playlist(&has_playlist) == ESP_OK && has_playlist) {
                post_cmd(MUSIC_PLAYER_CMD_RESUME);
            }
        }
        return;
    }
    intptr_t id = (intptr_t)lv_event_get_user_data(e);
    switch (id) {
        case MUSIC_PLAYER_UI_BTN_PREV:
            post_playlist_cmd(MUSIC_PLAYER_CMD_PREV);
            break;
        case MUSIC_PLAYER_UI_BTN_NEXT:
            post_playlist_cmd(MUSIC_PLAYER_CMD_NEXT);
            break;
        case MUSIC_PLAYER_UI_BTN_MODE:
            post_playlist_cmd(MUSIC_PLAYER_CMD_TOGGLE_MODE);
            break;
        case MUSIC_PLAYER_UI_BTN_LIST:
            create_playlist_dialog();
            break;
        case MUSIC_PLAYER_UI_BTN_VOLUME_DOWN:
            post_cmd(MUSIC_PLAYER_CMD_VOLUME_DOWN);
            break;
        case MUSIC_PLAYER_UI_BTN_VOLUME_UP:
            post_cmd(MUSIC_PLAYER_CMD_VOLUME_UP);
            break;
        default:
            break;
    }
}

static const lv_font_t *load_title_font(void)
{
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
    esp_lv_adapter_ft_font_config_t font_cfg = {
        .name = MUSIC_PLAYER_FONT_PATH,
        .size = s_ui.metrics.font_size > 0 ? s_ui.metrics.font_size : MUSIC_PLAYER_FONT_SIZE,
        .style = ESP_LV_ADAPTER_FT_FONT_STYLE_NORMAL,
    };
    if (esp_lv_adapter_ft_font_init(&font_cfg, &s_ui.ft_font_handle) == ESP_OK) {
        const lv_font_t *font = esp_lv_adapter_ft_font_get(s_ui.ft_font_handle);
        if (font != NULL) {
            ESP_LOGI(TAG, "Use FreeType font: %s", MUSIC_PLAYER_FONT_PATH);
            return font;
        }
        esp_lv_adapter_ft_font_deinit(s_ui.ft_font_handle);
        s_ui.ft_font_handle = NULL;
    }
    ESP_LOGW(TAG, "FreeType font unavailable, fallback to built-in CJK font");
#endif  /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE */
#if LV_FONT_SOURCE_HAN_SANS_SC_16_CJK
    return &lv_font_source_han_sans_sc_16_cjk;
#else
    return LV_FONT_DEFAULT;
#endif  /* LV_FONT_SOURCE_HAN_SANS_SC_16_CJK */
}

static lv_obj_t *create_icon_button(lv_obj_t *parent, int size, const char *symbol,
                                    music_player_ui_btn_id_t id, bool accent)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_style_radius(btn, accent ? LV_RADIUS_CIRCLE : 16, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(accent ? MUSIC_PLAYER_COLOR_ACCENT : MUSIC_PLAYER_COLOR_BTN), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    if (id == 0) {
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
    } else {
        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)id);
    }

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, symbol);
    lv_obj_set_style_text_color(label, lv_color_hex(accent ? 0x16161C : MUSIC_PLAYER_COLOR_TEXT), 0);
    lv_obj_center(label);
    return btn;
}

static void create_player_screen(const lv_font_t *title_font)
{
    const music_player_ui_metrics_t *m = &s_ui.metrics;

    s_ui.screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_ui.screen);
    lv_obj_set_size(s_ui.screen, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_ui.screen, lv_color_hex(MUSIC_PLAYER_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_ui.screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_top(s_ui.screen, m->pad_y_top, 0);
    lv_obj_set_style_pad_bottom(s_ui.screen, m->pad_y_bottom, 0);
    lv_obj_set_style_pad_left(s_ui.screen, m->pad_x, 0);
    lv_obj_set_style_pad_right(s_ui.screen, m->pad_x, 0);
    lv_obj_set_flex_flow(s_ui.screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_ui.screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_ui.screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Title block */
    lv_obj_t *title_block = lv_obj_create(s_ui.screen);
    lv_obj_remove_style_all(title_block);
    lv_obj_set_width(title_block, lv_pct(100));
    lv_obj_set_height(title_block, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(title_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(title_block, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(title_block, 8, 0);
    lv_obj_set_style_pad_row(title_block, 8, 0);

    s_ui.title_label = lv_label_create(title_block);
    lv_obj_set_width(s_ui.title_label, lv_pct(100));
    lv_label_set_long_mode(s_ui.title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_ui.title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_ui.title_label, title_font, 0);
    lv_obj_set_style_text_color(s_ui.title_label, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT), 0);
    lv_label_set_text(s_ui.title_label, "准备播放");

    s_ui.meta_label = lv_label_create(title_block);
    lv_obj_set_style_text_font(s_ui.meta_label, title_font, 0);
    lv_obj_set_style_text_color(s_ui.meta_label, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT_DIM), 0);
    s_ui.volume = MUSIC_PLAYER_DEFAULT_VOLUME;
    if (music_player_playback_get_volume(&s_ui.volume) != ESP_OK) {
        ESP_LOGW(TAG, "Use default playback volume");
    }
    snprintf(s_ui.mode_text, sizeof(s_ui.mode_text), "%s", "列表循环");
    refresh_meta_label();

    /* Center art */
    lv_obj_t *art_wrap = lv_obj_create(s_ui.screen);
    lv_obj_remove_style_all(art_wrap);
    lv_obj_set_width(art_wrap, lv_pct(100));
    lv_obj_set_flex_grow(art_wrap, 1);
    lv_obj_set_flex_flow(art_wrap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(art_wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(art_wrap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *art = lv_obj_create(art_wrap);
    lv_obj_set_size(art, m->art_size, m->art_size);
    lv_obj_set_style_radius(art, 22, 0);
    lv_obj_set_style_bg_color(art, lv_color_hex(MUSIC_PLAYER_COLOR_ART), 0);
    lv_obj_set_style_bg_opa(art, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(art, 1, 0);
    lv_obj_set_style_border_color(art, lv_color_hex(0x2A2A36), 0);
    lv_obj_set_style_pad_all(art, 0, 0);
    lv_obj_clear_flag(art, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *art_label = lv_label_create(art);
    lv_label_set_text(art_label, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(art_label, lv_color_hex(MUSIC_PLAYER_COLOR_ACCENT), 0);
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(art_label, &lv_font_montserrat_28, 0);
#endif  /* LV_FONT_MONTSERRAT_28 */
    lv_obj_center(art_label);

    /* Progress */
    lv_obj_t *progress_block = lv_obj_create(s_ui.screen);
    lv_obj_remove_style_all(progress_block);
    lv_obj_set_width(progress_block, lv_pct(100));
    lv_obj_set_height(progress_block, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_bottom(progress_block, 14, 0);
    lv_obj_set_flex_flow(progress_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(progress_block, 8, 0);

    s_ui.progress_bar = lv_bar_create(progress_block);
    lv_obj_set_size(s_ui.progress_bar, lv_pct(100), 8);
    lv_bar_set_range(s_ui.progress_bar, 0, 1000);
    lv_bar_set_value(s_ui.progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_ui.progress_bar, lv_color_hex(MUSIC_PLAYER_COLOR_PROGRESS_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ui.progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ui.progress_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ui.progress_bar, lv_color_hex(MUSIC_PLAYER_COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_ui.progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_ui.progress_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    lv_obj_t *time_row = lv_obj_create(progress_block);
    lv_obj_remove_style_all(time_row);
    lv_obj_set_width(time_row, lv_pct(100));
    lv_obj_set_height(time_row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_ui.elapsed_label = lv_label_create(time_row);
    lv_obj_set_style_text_color(s_ui.elapsed_label, lv_color_hex(MUSIC_PLAYER_COLOR_TIME), 0);
    lv_label_set_text(s_ui.elapsed_label, "0:00");

    s_ui.duration_label = lv_label_create(time_row);
    lv_obj_set_style_text_color(s_ui.duration_label, lv_color_hex(MUSIC_PLAYER_COLOR_TIME), 0);
    lv_label_set_text(s_ui.duration_label, "--:--");

    /* Control bar */
    lv_obj_t *ctrl = lv_obj_create(s_ui.screen);
    lv_obj_remove_style_all(ctrl);
    lv_obj_set_width(ctrl, lv_pct(100));
    lv_obj_set_height(ctrl, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(ctrl, lv_color_hex(MUSIC_PLAYER_COLOR_PANEL), 0);
    lv_obj_set_style_bg_opa(ctrl, LV_OPA_80, 0);
    lv_obj_set_style_radius(ctrl, m->ctrl_radius, 0);
    lv_obj_set_style_border_width(ctrl, 1, 0);
    lv_obj_set_style_border_color(ctrl, lv_color_hex(0x2A2A36), 0);
    lv_obj_set_style_pad_all(ctrl, m->ctrl_pad, 0);
    lv_obj_set_flex_flow(ctrl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    create_icon_button(ctrl, m->btn_side, LV_SYMBOL_LIST, MUSIC_PLAYER_UI_BTN_LIST, false);
    create_icon_button(ctrl, m->btn_nav, LV_SYMBOL_PREV, MUSIC_PLAYER_UI_BTN_PREV, false);
    s_ui.play_btn = create_icon_button(ctrl, m->btn_play, LV_SYMBOL_PLAY, 0, true);
    create_icon_button(ctrl, m->btn_nav, LV_SYMBOL_NEXT, MUSIC_PLAYER_UI_BTN_NEXT, false);
    create_icon_button(ctrl, m->btn_side, LV_SYMBOL_MINUS, MUSIC_PLAYER_UI_BTN_VOLUME_DOWN, false);
    create_icon_button(ctrl, m->btn_side, LV_SYMBOL_PLUS, MUSIC_PLAYER_UI_BTN_VOLUME_UP, false);
    create_icon_button(ctrl, m->btn_side, LV_SYMBOL_LOOP, MUSIC_PLAYER_UI_BTN_MODE, false);

    lv_screen_load(s_ui.screen);

    s_ui.progress_timer = lv_timer_create(progress_timer_cb, MUSIC_PLAYER_PROGRESS_POLL_MS, NULL);
}

static void ui_init_cb(void *ctx)
{
    (void)ctx;
    s_ui.playing = false;
    ui_metrics_init(&s_ui.metrics);

    const lv_font_t *title_font = load_title_font();
    s_ui.title_font = title_font;
    create_player_screen(title_font);
}

static void ui_deinit_cb(void *ctx);

esp_err_t music_player_ui_init(QueueHandle_t cmd_queue)
{
    ESP_GMF_CHECK(TAG, cmd_queue != NULL, return ESP_ERR_INVALID_ARG, "Command queue is NULL");
    ESP_GMF_CHECK(TAG, !s_ui_inited, return ESP_ERR_INVALID_STATE, "UI already initialized");
    ESP_GMF_RET_ON_ERROR(TAG, music_player_display_lock_run(ui_init_cb, NULL), return err_rc_,
                         "Failed to init music UI");
    esp_err_t ret = music_player_display_start();
    if (ret != ESP_OK) {
        ui_deinit_cb(NULL);
        return ret;
    }
    s_ui_inited = true;
    return ESP_OK;
}

static void ui_update_cb(void *ctx)
{
    music_player_ui_update_args_t *args = (music_player_ui_update_args_t *)ctx;
    if (s_ui.title_label != NULL) {
        lv_label_set_text(s_ui.title_label, args->title != NULL ? args->title : "");
    }
    if (args->mode_text != NULL) {
        snprintf(s_ui.mode_text, sizeof(s_ui.mode_text), "%s", args->mode_text);
    }
    s_ui.volume = args->volume;
    refresh_meta_label();

    s_ui.playing = args->playing;
    if (s_ui.play_btn != NULL) {
        lv_obj_t *label = lv_obj_get_child(s_ui.play_btn, 0);
        if (label != NULL) {
            lv_label_set_text(label, args->playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        }
    }

    int elapsed_ms = 0;
    int duration_ms = 0;
    if (music_player_playback_get_progress(&elapsed_ms, &duration_ms) == ESP_OK) {
        update_progress_widgets(elapsed_ms, duration_ms);
    }
}

void music_player_ui_update(const char *title, const char *mode_text, int volume, bool playing)
{
    music_player_ui_update_args_t args = {
        .title = title,
        .mode_text = mode_text,
        .volume = volume,
        .playing = playing,
    };
    esp_err_t ret = music_player_display_lock_run(ui_update_cb, &args);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "UI update skipped: display lock failed (%s)", esp_err_to_name(ret));
    }
}

static void ui_deinit_cb(void *ctx)
{
    (void)ctx;
    if (s_ui.progress_timer != NULL) {
        lv_timer_delete(s_ui.progress_timer);
        s_ui.progress_timer = NULL;
    }
    if (s_ui.playlist_panel != NULL) {
        lv_obj_delete(s_ui.playlist_panel);
        s_ui.playlist_panel = NULL;
    }
    if (s_ui.screen != NULL) {
        lv_obj_delete(s_ui.screen);
        s_ui.screen = NULL;
        s_ui.title_label = NULL;
        s_ui.meta_label = NULL;
        s_ui.progress_bar = NULL;
        s_ui.elapsed_label = NULL;
        s_ui.duration_label = NULL;
        s_ui.play_btn = NULL;
    }
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
    if (s_ui.ft_font_handle != NULL) {
        esp_lv_adapter_ft_font_deinit(s_ui.ft_font_handle);
        s_ui.ft_font_handle = NULL;
    }
#endif  /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE */
}

void music_player_ui_deinit(void)
{
    if (!s_ui_inited) {
        return;
    }
    esp_err_t ret = music_player_display_lock_run(ui_deinit_cb, NULL);
    if (ret != ESP_OK) {
        ui_deinit_cb(NULL);
    }
    memset(&s_ui, 0, sizeof(s_ui));
    s_ui_inited = false;
}
