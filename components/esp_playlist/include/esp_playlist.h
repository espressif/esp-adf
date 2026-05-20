/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "esp_media_db.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Opaque handle for a playlist instance.
 */
typedef void *esp_playlist_handle_t;

/** @brief Maximum length of playlist name string (including NUL). Configurable via menuconfig. */
#define ESP_PLAYLIST_NAME_MAX  CONFIG_ESP_PLAYLIST_NAME_MAX

/** @brief Maximum length of media display name in esp_playlist_info_t (including NUL). Configurable via menuconfig. */
#define ESP_PLAYLIST_MEDIA_NAME_MAX  CONFIG_ESP_PLAYLIST_MEDIA_NAME_MAX

/** @brief Maximum length of media URL in esp_playlist_info_t (including NUL). Configurable via menuconfig. */
#define ESP_PLAYLIST_URL_MAX  CONFIG_ESP_PLAYLIST_URL_MAX

/**
 * @brief  Snapshot of one playlist entry for navigation APIs.
 *
 *         Caller supplies the structure; APIs copy strings into fixed buffers.
 *         URLs longer than ESP_PLAYLIST_URL_MAX - 1 are truncated with a trailing NUL.
 */
typedef struct {
    char  playlist_name[ESP_PLAYLIST_NAME_MAX];     /*!< Playlist name */
    char  media_name[ESP_PLAYLIST_MEDIA_NAME_MAX];  /*!< Media display name */
    char  media_url[ESP_PLAYLIST_URL_MAX];          /*!< Media URL */
    int   index;                                    /*!< Zero-based index in the list */
} esp_playlist_info_t;

/**
 * @brief  Repeat mode for esp_playlist_next() and esp_playlist_prev().
 */
typedef enum {
    ESP_PLAYLIST_REPEAT_NONE = 0,  /*!< Stop at list boundary (ESP_ERR_NOT_FOUND) */
    ESP_PLAYLIST_REPEAT_ONE,       /*!< Stay on current index */
    ESP_PLAYLIST_REPEAT_ALL,       /*!< Wrap at list ends */
    ESP_PLAYLIST_REPEAT_SHUFFLE,   /*!< Pick a random index on next/prev */
} esp_playlist_repeat_mode_t;

/**
 * @brief  Playlist creation configuration.
 */
typedef struct {
    const char *playlist_name;  /*!< Non-empty playlist name; copied internally */
} esp_playlist_cfg_t;

/**
 * @brief  Create an empty in-RAM playlist.
 *
 *         Populate with esp_playlist_import_media(), esp_playlist_import_ram(), or
 *         esp_playlist_load(). RAM buffer export/import uses a separate format from the media DB
 *         three-file persistence.
 *
 * @param[in]   cfg     Configuration; playlist_name must be non-NULL and non-empty
 * @param[out]  handle  Receives the new handle on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If cfg, handle, or playlist_name is invalid
 *       - ESP_ERR_NO_MEM       If allocation fails
 */
esp_err_t esp_playlist_new(const esp_playlist_cfg_t *cfg, esp_playlist_handle_t *handle);

/**
 * @brief  Destroy a playlist handle and free its resources.
 *
 * @param[in]  handle  Playlist handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle is NULL
 */
esp_err_t esp_playlist_del(esp_playlist_handle_t handle);

/**
 * @brief  Import media entries from a catalog into the playlist.
 *
 *         Appends matching catalog rows to the playlist (no dedup by media_id).
 *         On failure, entries appended during this call are removed before returning.
 *         Binds the playlist to media_db_handle; a later import from a different handle
 *         returns ESP_ERR_INVALID_STATE.
 *
 * @param[in]  handle           Playlist handle
 * @param[in]  media_db_handle  Source media catalog
 * @param[in]  filter           Optional filter; NULL appends all catalog rows
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If handle or media_db_handle is NULL, or filter is invalid
 *       - ESP_ERR_INVALID_STATE  If the playlist is bound to another media DB
 *       - ESP_ERR_NOT_FOUND      If the catalog is empty or no rows matched the filter
 *       - ESP_ERR_NO_MEM         If allocation fails
 *       - ESP_FAIL               On other internal media lookup errors
 */
esp_err_t esp_playlist_import_media(esp_playlist_handle_t handle, esp_media_db_handle_t media_db_handle,
                                    const esp_media_filter_t *filter);

/**
 * @brief  Replace the in-memory playlist from a JSON file.
 *
 *         Not append semantics. JSON playlist format matches esp_playlist_save(),
 *         esp_playlist_export_ram(), and esp_playlist_import_ram(). Independent of
 *         esp_media_db VFS catalog files.
 *
 * @param[in]  handle     Playlist handle
 * @param[in]  load_path  Path to JSON playlist file
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle or load_path is NULL
 *       - ESP_ERR_NO_MEM       If allocation fails while parsing or building items
 *       - ESP_FAIL             If the file cannot be read or JSON is invalid
 */
esp_err_t esp_playlist_load(esp_playlist_handle_t handle, const char *load_path);

/**
 * @brief  Write the current playlist to a JSON file.
 *
 *         JSON playlist format matches esp_playlist_export_ram(), esp_playlist_load(),
 *         and esp_playlist_import_ram().
 *
 * @param[in]  handle     Playlist handle
 * @param[in]  save_path  Path to JSON playlist file
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle or save_path is NULL
 *       - ESP_ERR_NO_MEM       If JSON allocation fails
 *       - ESP_FAIL             If the file cannot be written
 */
esp_err_t esp_playlist_save(esp_playlist_handle_t handle, const char *save_path);

/**
 * @brief  Replace the in-memory playlist from a JSON buffer in RAM.
 *
 *         Same replace semantics as esp_playlist_load(). The buffer must contain JSON in
 *         the same playlist format as the file read by esp_playlist_load() (and as written
 *         by esp_playlist_save() / esp_playlist_export_ram()). Parsed entries become inline
 *         items with copied name and url strings.
 *
 * @param[in]  handle   Playlist handle
 * @param[in]  buf      JSON playlist text buffer
 * @param[in]  buf_len  Byte length of buf; if 0, buf is treated as a NUL-terminated string
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle or buf is NULL
 *       - ESP_ERR_NO_MEM       If allocation fails while parsing or building items
 *       - ESP_FAIL             If buffer content is invalid or missing required fields
 */
esp_err_t esp_playlist_import_ram(esp_playlist_handle_t handle, const char *buf, size_t buf_len);

/**
 * @brief  Export the current playlist to a heap-allocated JSON buffer.
 *
 *         JSON uses the same playlist format as esp_playlist_save() writes to a file and
 *         as esp_playlist_load() / esp_playlist_import_ram() accept. The returned string is
 *         NUL-terminated. Caller must release it with free().
 *
 * @param[in]   handle   Playlist handle
 * @param[out]  out_buf  Receives the JSON buffer on success; set to NULL on failure
 * @param[out]  out_len  Optional; receives strlen(out_buf); may be NULL
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle or out_buf is NULL
 *       - ESP_ERR_NO_MEM       If allocation fails
 *       - ESP_FAIL             If serialization fails
 */
esp_err_t esp_playlist_export_ram(esp_playlist_handle_t handle, char **out_buf, size_t *out_len);

/**
 * @brief  Clear all items from the in-RAM playlist.
 *
 *         Does not delete JSON or media DB files on disk.
 *
 * @param[in]  handle  Playlist handle
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle is NULL
 */
esp_err_t esp_playlist_clean(esp_playlist_handle_t handle);

/**
 * @brief  Set repeat mode for navigation APIs.
 *
 * @param[in]  handle       Playlist handle
 * @param[in]  repeat_mode  Repeat behavior
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle is NULL or repeat_mode is out of range
 */
esp_err_t esp_playlist_set_repeat_mode(esp_playlist_handle_t handle, esp_playlist_repeat_mode_t repeat_mode);

/**
 * @brief  Set the current playback index without loading item info.
 *
 *         Invalidates the esp_playlist_curr() cache. Default index after esp_playlist_new()
 *         is 0.
 *
 * @param[in]  handle  Playlist handle
 * @param[in]  index   Zero-based index; must be < item count
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle is NULL or index < 0
 *       - ESP_ERR_NOT_FOUND    If index >= item count
 */
esp_err_t esp_playlist_set_curr_index(esp_playlist_handle_t handle, int index);

/**
 * @brief  Advance to the next item and fill info.
 *
 *         Updates current index according to repeat_mode. REPEAT_ALL wraps at the end;
 *         REPEAT_NONE returns ESP_ERR_NOT_FOUND at the last item.
 *
 * @param[in]   handle  Playlist handle
 * @param[out]  info    Caller buffer; strings are copied on success
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If handle or info is NULL
 *       - ESP_ERR_NOT_FOUND      If the list is empty or the boundary is reached
 *       - ESP_ERR_INVALID_STATE  If a DB item has no bound media catalog
 *       - ESP_FAIL               On internal media lookup failure
 */
esp_err_t esp_playlist_next(esp_playlist_handle_t handle, esp_playlist_info_t *info);

/**
 * @brief  Move to the previous item and fill info.
 *
 *         Updates current index according to repeat_mode. REPEAT_ALL wraps at the start;
 *         REPEAT_NONE returns ESP_ERR_NOT_FOUND at the first item.
 *
 * @param[in]   handle  Playlist handle
 * @param[out]  info    Caller buffer; strings are copied on success
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If handle or info is NULL
 *       - ESP_ERR_NOT_FOUND      If the list is empty or the boundary is reached
 *       - ESP_ERR_INVALID_STATE  If a DB item has no bound media catalog
 *       - ESP_FAIL               On internal media lookup failure
 */
esp_err_t esp_playlist_prev(esp_playlist_handle_t handle, esp_playlist_info_t *info);

/**
 * @brief  Read the current item without changing the index.
 *
 *         Uses an in-RAM cache when valid; otherwise loads from the bound media catalog
 *         or inline JSON items.
 *
 * @param[in]   handle  Playlist handle
 * @param[out]  info    Caller buffer; strings are copied on success
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If handle or info is NULL
 *       - ESP_ERR_NOT_FOUND      If the list is empty
 *       - ESP_ERR_INVALID_STATE  If a DB item has no bound media catalog
 *       - ESP_FAIL               On internal media lookup failure
 */
esp_err_t esp_playlist_curr(esp_playlist_handle_t handle, esp_playlist_info_t *info);

/**
 * @brief  Read one item by index without changing the current playback index.
 *
 * @param[in]   handle  Playlist handle
 * @param[in]   index   Zero-based index; must be < item count
 * @param[out]  info    Caller buffer; strings are copied on success
 *
 * @return
 *       - ESP_OK                 On success
 *       - ESP_ERR_INVALID_ARG    If handle or info is NULL, or index < 0
 *       - ESP_ERR_NOT_FOUND      If index >= item count
 *       - ESP_ERR_INVALID_STATE  If a DB item has no bound media catalog
 *       - ESP_FAIL               On internal media lookup failure
 */
esp_err_t esp_playlist_get_info(esp_playlist_handle_t handle, int index, esp_playlist_info_t *info);

/**
 * @brief  Get the number of items in the playlist.
 *
 * @param[in]   handle  Playlist handle
 * @param[out]  count   Receives item count on success
 *
 * @return
 *       - ESP_OK               On success
 *       - ESP_ERR_INVALID_ARG  If handle or count is NULL
 */
esp_err_t esp_playlist_get_count(esp_playlist_handle_t handle, int *count);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
