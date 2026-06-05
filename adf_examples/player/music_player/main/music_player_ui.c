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
#include "music/lv_demo_music.h"
#include "music/lv_demo_music_main.h"
#include "esp_lv_adapter.h"
#include "music_player_display.h"
#include "music_player_playback.h"
#include "music_player_config.h"

static const char *TAG = "MUSIC_PLAYER_UI";

#define MUSIC_PLAYER_PLAYLIST_DIALOG_WIDTH   720
#define MUSIC_PLAYER_PLAYLIST_DIALOG_HEIGHT  420
#define MUSIC_PLAYER_PLAYLIST_MAX_ITEMS      128
#define MUSIC_PLAYER_COLOR_TEXT              0xF0F0F0
#define MUSIC_PLAYER_COLOR_TEXT_DIM          0xB0B0B0
#define MUSIC_PLAYER_COLOR_ACCENT            0xFFD166

typedef struct {
    lv_obj_t        *title_label;
    lv_obj_t        *mode_label;
    lv_obj_t        *volume_label;
    lv_obj_t        *play_btn;
    lv_obj_t        *playlist_panel;
    const lv_font_t *title_font;
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
    esp_lv_adapter_ft_font_handle_t  ft_font_handle;
#endif  /* CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE */
    bool  playing;
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

static void close_playlist_event_cb(lv_event_t *e)
{
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

    s_ui.playlist_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_ui.playlist_panel, MUSIC_PLAYER_PLAYLIST_DIALOG_WIDTH, MUSIC_PLAYER_PLAYLIST_DIALOG_HEIGHT);
    lv_obj_center(s_ui.playlist_panel);
    lv_obj_set_style_bg_opa(s_ui.playlist_panel, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(s_ui.playlist_panel, lv_color_hex(0x101018), 0);
    lv_obj_set_style_text_color(s_ui.playlist_panel, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT), 0);
    lv_obj_set_style_radius(s_ui.playlist_panel, 16, 0);
    lv_obj_set_style_pad_all(s_ui.playlist_panel, 12, 0);
    lv_obj_set_flex_flow(s_ui.playlist_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_ui.playlist_panel, 8, 0);

    lv_obj_t *header = lv_obj_create(s_ui.playlist_panel);
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
    lv_obj_add_event_cb(close_btn, close_playlist_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_label = lv_label_create(close_btn);
    lv_label_set_text(close_label, LV_SYMBOL_CLOSE);

    lv_obj_t *list = lv_obj_create(s_ui.playlist_panel);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 4, 0);
    lv_obj_set_style_text_color(list, lv_color_hex(MUSIC_PLAYER_COLOR_TEXT), 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

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

static bool is_demo_music_title_box(lv_obj_t *obj)
{
    if (obj == NULL) {
        return false;
    }
    if (lv_obj_get_style_flex_flow(obj, LV_PART_MAIN) != LV_FLEX_FLOW_COLUMN) {
        return false;
    }
    uint32_t cnt = lv_obj_get_child_cnt(obj);
    if (cnt != 3) {
        return false;
    }
    for (uint32_t i = 0; i < cnt; i++) {
        if (!lv_obj_check_type(lv_obj_get_child(obj, i), &lv_label_class)) {
            return false;
        }
    }
    return true;
}

static bool hide_demo_title_box_in_tree(lv_obj_t *root)
{
    if (root == NULL) {
        return false;
    }
    uint32_t cnt = lv_obj_get_child_cnt(root);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(root, i);
        if (is_demo_music_title_box(child)) {
            lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
            return true;
        }
        if (hide_demo_title_box_in_tree(child)) {
            return true;
        }
    }
    return false;
}

static inline void hide_lv_demo_music_title_box(void)
{
    if (hide_demo_title_box_in_tree(lv_screen_active())) {
        ESP_LOGI(TAG, "Hidden lv_demo_music title box");
        return;
    }
    ESP_LOGW(TAG, "lv_demo_music title box not found");
}

static const lv_font_t *load_title_font(void)
{
#if CONFIG_ESP_LVGL_ADAPTER_ENABLE_FREETYPE
    esp_lv_adapter_ft_font_config_t font_cfg = {
        .name = MUSIC_PLAYER_FONT_PATH,
        .size = MUSIC_PLAYER_FONT_SIZE,
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

static void create_overlay_controls(lv_obj_t *parent, const lv_font_t *title_font)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_align(panel, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 6, 0);

    s_ui.title_label = lv_label_create(panel);
    lv_obj_set_width(s_ui.title_label, lv_pct(90));
    lv_label_set_long_mode(s_ui.title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_ui.title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_ui.title_label, title_font, 0);
    lv_label_set_text(s_ui.title_label, "准备播放");

    s_ui.mode_label = lv_label_create(panel);
    lv_obj_set_style_text_font(s_ui.mode_label, title_font, 0);
    lv_label_set_text(s_ui.mode_label, "列表循环");

    s_ui.volume_label = lv_label_create(panel);
    lv_obj_set_style_text_font(s_ui.volume_label, title_font, 0);
    int volume = MUSIC_PLAYER_DEFAULT_VOLUME;
    if (music_player_playback_get_volume(&volume) != ESP_OK) {
        ESP_LOGW(TAG, "Use default playback volume");
    }
    lv_label_set_text_fmt(s_ui.volume_label, "音量 %d%%", volume);

    lv_obj_t *ctrl = lv_obj_create(parent);
    lv_obj_remove_style_all(ctrl);
    lv_obj_set_size(ctrl, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_align(ctrl, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_opa(ctrl, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(ctrl, lv_color_hex(0x202030), 0);
    lv_obj_set_style_radius(ctrl, 16, 0);
    lv_obj_set_style_pad_all(ctrl, 10, 0);
    lv_obj_set_flex_flow(ctrl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *list_btn = lv_button_create(ctrl);
    lv_obj_add_event_cb(list_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)MUSIC_PLAYER_UI_BTN_LIST);
    lv_obj_t *list_label = lv_label_create(list_btn);
    lv_label_set_text(list_label, LV_SYMBOL_LIST);

    lv_obj_t *prev_btn = lv_button_create(ctrl);
    lv_obj_add_event_cb(prev_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)MUSIC_PLAYER_UI_BTN_PREV);
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);

    s_ui.play_btn = lv_button_create(ctrl);
    lv_obj_add_event_cb(s_ui.play_btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *play_label = lv_label_create(s_ui.play_btn);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY);

    lv_obj_t *next_btn = lv_button_create(ctrl);
    lv_obj_add_event_cb(next_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)MUSIC_PLAYER_UI_BTN_NEXT);
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);

    lv_obj_t *vol_down_btn = lv_button_create(ctrl);
    lv_obj_add_event_cb(vol_down_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)MUSIC_PLAYER_UI_BTN_VOLUME_DOWN);
    lv_obj_t *vol_down_label = lv_label_create(vol_down_btn);
    lv_label_set_text(vol_down_label, LV_SYMBOL_MINUS);

    lv_obj_t *vol_up_btn = lv_button_create(ctrl);
    lv_obj_add_event_cb(vol_up_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)MUSIC_PLAYER_UI_BTN_VOLUME_UP);
    lv_obj_t *vol_up_label = lv_label_create(vol_up_btn);
    lv_label_set_text(vol_up_label, LV_SYMBOL_PLUS);

    lv_obj_t *mode_btn = lv_button_create(ctrl);
    lv_obj_add_event_cb(mode_btn, btn_event_cb, LV_EVENT_CLICKED, (void *)MUSIC_PLAYER_UI_BTN_MODE);
    lv_obj_t *mode_btn_label = lv_label_create(mode_btn);
    lv_label_set_text(mode_btn_label, LV_SYMBOL_LOOP);
}

static void ui_init_cb(void *ctx)
{
    (void)ctx;
    s_ui.playing = false;

    lv_demo_music();
    hide_lv_demo_music_title_box();

    const lv_font_t *title_font = load_title_font();
    s_ui.title_font = title_font;
    create_overlay_controls(lv_screen_active(), title_font);
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
        /* Clean up resources allocated by ui_init_cb (e.g. FreeType font)
         * to prevent resource leak when display_start fails */
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
    if (s_ui.mode_label != NULL) {
        lv_label_set_text(s_ui.mode_label, args->mode_text != NULL ? args->mode_text : "");
    }
    if (s_ui.volume_label != NULL) {
        lv_label_set_text_fmt(s_ui.volume_label, "音量 %d%%", args->volume);
    }
    s_ui.playing = args->playing;
    if (args->playing) {
        lv_demo_music_resume();
        if (s_ui.play_btn != NULL) {
            lv_obj_t *label = lv_obj_get_child(s_ui.play_btn, 0);
            if (label != NULL) {
                lv_label_set_text(label, LV_SYMBOL_PAUSE);
            }
        }
    } else {
        lv_demo_music_pause();
        if (s_ui.play_btn != NULL) {
            lv_obj_t *label = lv_obj_get_child(s_ui.play_btn, 0);
            if (label != NULL) {
                lv_label_set_text(label, LV_SYMBOL_PLAY);
            }
        }
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
    /* Delete LVGL objects that reference the font BEFORE freeing the font
     * to prevent use-after-free in the LVGL render task */
    if (s_ui.title_label != NULL) {
        lv_obj_delete(s_ui.title_label);
        s_ui.title_label = NULL;
    }
    if (s_ui.mode_label != NULL) {
        lv_obj_delete(s_ui.mode_label);
        s_ui.mode_label = NULL;
    }
    if (s_ui.volume_label != NULL) {
        lv_obj_delete(s_ui.volume_label);
        s_ui.volume_label = NULL;
    }
    if (s_ui.play_btn != NULL) {
        lv_obj_delete(s_ui.play_btn);
        s_ui.play_btn = NULL;
    }
    if (s_ui.playlist_panel != NULL) {
        lv_obj_delete(s_ui.playlist_panel);
        s_ui.playlist_panel = NULL;
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
        /* Fallback: free resources directly if display lock fails,
         * otherwise ft_font_handle and playlist_panel would be zeroed
         * without being freed (resource leak). */
        ui_deinit_cb(NULL);
    }
    memset(&s_ui, 0, sizeof(s_ui));
    s_ui_inited = false;
}
