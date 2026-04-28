# Playlist Usage Scenarios

This document explains how applications combine the media catalog (`esp_media_db`) and playlist (`esp_playlist`) using the current public headers (`esp_media_db_types.h`, `esp_media_db.h`, `esp_playlist.h`). It covers only public APIs under `components/esp_playlist/include` and does not rely on internal `epl_*` interfaces.

## Layered Architecture and Object Relationships

The component exposes two main objects:

- `esp_media_db`: Media catalog handle that stores `name` / `url` indexes. Catalogs can be persistent (index survives reboot) or temporary (entries exist only at runtime).
- `esp_playlist`: Playlist handle that maintains playlist name, item order, current index, and repeat mode. Playlists can be built from a catalog or loaded directly from JSON; `save` / `load` use human-readable JSON files, which is a different format from the catalog database.

Relationship overview:

```text
Application / Player
  |
  +-- esp_media_db
  |     +-- Catalog entries: name / url
  |     +-- Persistent catalog: keep media index across reboot
  |     +-- Temporary catalog: keep media index only at runtime
  |
  +-- esp_playlist
        +-- Playlist queue: name / url / order / current index / repeat mode
        +-- Source A: esp_playlist_import_media() reads from esp_media_db
        +-- Source B: esp_playlist_load() / esp_playlist_import_ram() reads playlist JSON
        +-- esp_playlist_save() / esp_playlist_export_ram() writes playlist JSON
```

Playlists have two main sources: batch import from a catalog via `esp_playlist_import_media()`, or restore from a JSON file or RAM buffer via `esp_playlist_load()` or `esp_playlist_import_ram()`. After JSON load, items are self-contained with `name` and `url`; playback info does not require the original catalog handle.

When creating a catalog, choose lifecycle via `esp_media_db_cfg_t::storage_type`: `ESP_DB_STORAGE_FS` for persistent catalogs, `ESP_DB_STORAGE_RAM` for temporary catalogs.

Public type roles:

- `esp_media_db_item_t`: Basic entry passed when writing to the catalog (`name` and `url`).
- `esp_media_filter_t` / `esp_media_filter_item_t`: Filters used when importing from a catalog into a playlist. Built-in fields: `name`, `url`, `id`; other keys require matching extended metadata inside the catalog.
- `esp_playlist_info_t`: Info for navigation and UI (`playlist_name`, `media_name`, `media_url`, `index`). Strings are copied into fixed arrays in the caller-provided struct and truncated if needed.

Recommended teardown order: `esp_playlist_del()` first, then `esp_media_db_deinit()`.

## 1. Initialize Catalog and Import Playlist

Typical flow when a catalog already exists on the device and you want a runtime playlist.

```c
#include "esp_media_db.h"
#include "esp_playlist.h"

esp_media_db_handle_t media_db = NULL;
esp_media_db_cfg_t db_cfg = {
    .storage_type = ESP_DB_STORAGE_FS,
    .storage_path = "/sdcard/__playlist",
};
ESP_ERROR_CHECK(esp_media_db_init(&db_cfg, &media_db));

/* Call load explicitly to restore an existing persistent catalog; on failure the implementation recreates an empty catalog. */
ESP_ERROR_CHECK(esp_media_db_load(media_db));

esp_playlist_handle_t playlist = NULL;
esp_playlist_cfg_t playlist_cfg = {
    .playlist_name = "default",
};
ESP_ERROR_CHECK(esp_playlist_new(&playlist_cfg, &playlist));

/* filter == NULL: append all matching catalog rows; the same media_id may appear more than once in the playlist. */
ESP_ERROR_CHECK(esp_playlist_import_media(playlist, media_db, NULL));
```

Note: `esp_playlist_cfg_t` currently only has `playlist_name` and does not bind a catalog at creation time. The catalog handle is passed to `esp_playlist_import_media()`. For JSON-based playlists, call `esp_playlist_load()` after `esp_playlist_new()` without importing from a catalog first.

## 2. Scan Directories to Build a Catalog

`esp_media_db_scan()` adds files under a directory to the catalog. With `skip_duplicate = false`, URL deduplication applies only when the catalog already has entries; with `skip_duplicate = true`, no deduplication is performed. An empty catalog skips deduplication even when `skip_duplicate = false`.

```c
/* Example audio extensions; configure .mp4, .mkv, etc. as needed */
static const char *const media_exts[] = { ".mp3", ".wav", ".aac", ".flac", ".mp4" };

esp_media_db_scan_cfg_t scan_cfg = {
    .skip_duplicate = false,
    .path = "/sdcard/music",
    .scan_depth = 3,
    .file_extensions = media_exts,
    .file_extension_count = sizeof(media_exts) / sizeof(media_exts[0]),
    .filter_cb = NULL,
    .filter_cb_ctx = NULL,
};
ESP_ERROR_CHECK(esp_media_db_scan(media_db, &scan_cfg));
```

Extensions must match file suffixes (e.g. `".mp3"` or `".mp4"`). `file_extensions == NULL` or `file_extension_count == 0` means no extension filter. `filter_cb` can apply extra filtering during scan; return `true` to add the file.

`scan_depth`, `file_extension_count`, and `esp_media_filter_t::item_count` are `uint8_t`; values beyond internal limits return `ESP_ERR_INVALID_ARG`.

## 3. Add Known name / URL Entries

Use cases: network URLs, user temporary queues, HTTP-fetched lists written to a local playback queue, etc.

```c
esp_media_db_item_t items[] = {
    { .name = "media1", .url = "https://example.com/a.mp4" },
    { .name = "media2", .url = "file:/sdcard/music/b.mp3" },
};

ESP_ERROR_CHECK(esp_media_db_add(media_db, items, 2));
```

`esp_media_db_add()` deduplicates by URL. The public API does not return media IDs or expose single-item lookup; playback usually imports via `esp_playlist_import_media()`, then uses `esp_playlist_get_info()` / `next()` / `prev()` for `media_name` and `media_url`.

For a non-persistent catalog:

```c
esp_media_db_cfg_t db_cfg = {
    .storage_type = ESP_DB_STORAGE_RAM,
    .storage_path = "runtime_queue",
};
ESP_ERROR_CHECK(esp_media_db_init(&db_cfg, &media_db));
```

## 4. Clear Current Catalog and Re-import

`esp_media_db_clean()` clears in-memory catalog content only; it does not delete persistent on-disk data. After clean you can `scan` / `add` again, or `load` to restore persisted content.

```c
ESP_ERROR_CHECK(esp_media_db_clean(media_db));

esp_media_db_item_t items[] = {
    { .name = "new_media", .url = "https://example.com/new.mp4" },
};
ESP_ERROR_CHECK(esp_media_db_add(media_db, items, 1));
```

To restore a persisted catalog after clean:

```c
ESP_ERROR_CHECK(esp_media_db_load(media_db));
```

## 5. Filtered Import from Catalog to Playlist

Signature:

```c
esp_err_t esp_playlist_import_media(esp_playlist_handle_t handle,
                                    esp_media_db_handle_t media_db_handle,
                                    const esp_media_filter_t *filter);
```

`filter == NULL` appends all matching rows; each call appends to the end of the playlist without deduplicating by `media_id`. Built-in filter fields:

- `name`: string; `ESP_MEDIA_MATCH_EXACT`, `ESP_MEDIA_MATCH_CONTAINS`, `ESP_MEDIA_MATCH_PREFIX`.
- `url`: string; `EXACT`, `CONTAINS`, `PREFIX`.
- `id`: integer; `ESP_MEDIA_MATCH_EXACT`.

Other keys (`album`, `artist`, `year`, etc.) match only if the catalog stores that extended metadata. Public `esp_media_db_add()` writes only `name` and `url`, so entries created via public APIs are usually filtered on `name` / `url` / `id` only.

### 5.1 Import by Exact URL

```c
esp_db_field_value_t url = {
    .type = ESP_DB_FIELD_TYPE_STRING,
    .value.strv = "https://example.com/a.mp3",
    .size = sizeof("https://example.com/a.mp3"),
};

esp_media_filter_item_t filter_items[] = {
    { .key = "url", .expected = url, .match_type = ESP_MEDIA_MATCH_EXACT },
};

esp_media_filter_t filter = {
    .items = filter_items,
    .item_count = 1,
    .match_all = true,
};

ESP_ERROR_CHECK(esp_playlist_import_media(playlist, media_db, &filter));
```

### 5.2 Import by URL Prefix

```c
esp_db_field_value_t prefix = {
    .type = ESP_DB_FIELD_TYPE_STRING,
    .value.strv = "file:/sdcard/music/album_a/",
    .size = sizeof("file:/sdcard/music/album_a/"),
};

esp_media_filter_item_t filter_items[] = {
    { .key = "url", .expected = prefix, .match_type = ESP_MEDIA_MATCH_PREFIX },
};

esp_media_filter_t filter = {
    .items = filter_items,
    .item_count = 1,
    .match_all = true,
};

ESP_ERROR_CHECK(esp_playlist_import_media(playlist, media_db, &filter));
```

### 5.3 Multi-condition AND / OR

`match_all == true` requires all conditions; `false` matches if any condition matches.

```c
esp_db_field_value_t name_part = {
    .type = ESP_DB_FIELD_TYPE_STRING,
    .value.strv = "live",
    .size = sizeof("live"),
};

esp_db_field_value_t url_prefix = {
    .type = ESP_DB_FIELD_TYPE_STRING,
    .value.strv = "file:/sdcard/music/",
    .size = sizeof("file:/sdcard/music/"),
};

esp_media_filter_item_t filter_items[] = {
    { .key = "name", .expected = name_part, .match_type = ESP_MEDIA_MATCH_CONTAINS },
    { .key = "url", .expected = url_prefix, .match_type = ESP_MEDIA_MATCH_PREFIX },
};

esp_media_filter_t filter = {
    .items = filter_items,
    .item_count = 2,
    .match_all = true,
};

ESP_ERROR_CHECK(esp_playlist_import_media(playlist, media_db, &filter));
```

## 6. Playback Navigation and Item Info

After import, use `get_count`, `get_info`, `curr`, `next`, and `prev`.

```c
int count = 0;
ESP_ERROR_CHECK(esp_playlist_get_count(playlist, &count));

esp_playlist_info_t info = {0};
ESP_ERROR_CHECK(esp_playlist_get_info(playlist, 0, &info));
printf("playlist=%s index=%d name=%s url=%s\n",
       info.playlist_name, info.index, info.media_name, info.media_url);

ESP_ERROR_CHECK(esp_playlist_curr(playlist, &info));
ESP_ERROR_CHECK(esp_playlist_next(playlist, &info));
ESP_ERROR_CHECK(esp_playlist_prev(playlist, &info));
```

Provide `esp_playlist_info_t` on the stack or in static storage; the API copies strings into fixed arrays. Longer internal strings are truncated per `ESP_PLAYLIST_MEDIA_NAME_MAX` / `ESP_PLAYLIST_URL_MAX` with a trailing `'\0'`.

## 7. Repeat Modes

Four repeat modes are supported:

```c
ESP_ERROR_CHECK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_NONE));
ESP_ERROR_CHECK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_ONE));
ESP_ERROR_CHECK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_ALL));
ESP_ERROR_CHECK(esp_playlist_set_repeat_mode(playlist, ESP_PLAYLIST_REPEAT_SHUFFLE));
```

Behavior summary:

- `ESP_PLAYLIST_REPEAT_NONE`: `next` past the end or `prev` before the start returns `ESP_ERR_NOT_FOUND`.
- `ESP_PLAYLIST_REPEAT_ONE`: `next` / `prev` stay on the current item.
- `ESP_PLAYLIST_REPEAT_ALL`: `next` at the tail wraps to the first item; `prev` at the head wraps to the last.
- `ESP_PLAYLIST_REPEAT_SHUFFLE`: picks a random item.

## 8. Playlist Import / Export (File and RAM Buffer)

`esp_playlist_save()` writes the current playlist to a JSON file; `esp_playlist_load()` loads from a JSON file. `esp_playlist_export_ram()` / `esp_playlist_import_ram()` provide the same format in RAM buffers for NVS, network delivery, or environments without a filesystem. This JSON is the playlist file format, not the catalog database.

### 8.1 File Path

```c
ESP_ERROR_CHECK(esp_playlist_save(playlist, "/sdcard/playlist.json"));

esp_playlist_handle_t loaded = NULL;
ESP_ERROR_CHECK(esp_playlist_new(&(esp_playlist_cfg_t) {
    .playlist_name = "loaded",
}, &loaded));
ESP_ERROR_CHECK(esp_playlist_load(loaded, "/sdcard/playlist.json"));
```

### 8.2 RAM Buffer

```c
char *json = NULL;
ESP_ERROR_CHECK(esp_playlist_export_ram(playlist, &json, NULL));
/* e.g. write to NVS, send over socket, or clone to another handle */
ESP_ERROR_CHECK(esp_playlist_import_ram(loaded, json, 0));
free(json);
json = NULL;
```

If the buffer is not NUL-terminated, pass an explicit length:

```c
ESP_ERROR_CHECK(esp_playlist_import_ram(loaded, payload, payload_len));
```

`esp_playlist_load()` and `esp_playlist_import_ram()` use replace semantics: on success they clear existing playlist items and rebuild from the JSON `items` array. Loaded items are inline and self-contained; `name` / `url` do not require the original `esp_media_db_handle_t`.

The string returned by `esp_playlist_export_ram()` must be released with `free()`.

Example JSON shape:

```json
{
  "playlist_name": "default",
  "items": [
    {
      "name": "media1",
      "url": "https://example.com/a.mp4"
    }
  ]
}
```

`esp_playlist_import_media()` imports from the catalog; it does not parse M3U or other external playlist files. To support M3U or server-defined lists, parse externally to `name` / `url`, then call `esp_media_db_add()` and `esp_playlist_import_media()`, or build the JSON above and call `esp_playlist_load()`.

## 9. Remove Catalog Entries

The public API removes catalog entries by `url`. `name` may be kept for application use, but removal matches on `url`:

```c
esp_media_db_item_t items[] = {
    { .name = "media1", .url = "https://example.com/a.mp4" },
};
ESP_ERROR_CHECK(esp_media_db_remove(media_db, items, 1));
```

There is no public API to remove playlist items by index. To **replace** playlist content, call `esp_playlist_clean()` then `import_media()`, or use `load()` / `import_ram()` (replace, not append). Repeated `import_media()` calls append to the end of the list.

## 10. Full Teardown

```c
ESP_ERROR_CHECK(esp_playlist_del(playlist));
ESP_ERROR_CHECK(esp_media_db_deinit(media_db));
```

If you have additional playlists loaded from JSON, call `esp_playlist_del()` on each handle.

## API Summary

| Goal | API |
|------|-----|
| Create catalog handle | `esp_media_db_init` |
| Load existing persistent catalog | `esp_media_db_load` |
| Clear current catalog content | `esp_media_db_clean` |
| Scan directory into catalog | `esp_media_db_scan` |
| Add known name / url | `esp_media_db_add` |
| Remove catalog entries | `esp_media_db_remove` |
| Get catalog item count | `esp_media_db_get_count` |
| Create playlist | `esp_playlist_new` |
| Import playlist from catalog | `esp_playlist_import_media` |
| Save / load playlist JSON (file) | `esp_playlist_save` / `esp_playlist_load` |
| Export / import playlist (RAM buffer) | `esp_playlist_export_ram` / `esp_playlist_import_ram` |
| Clear in-memory playlist items | `esp_playlist_clean` |
| Set repeat mode | `esp_playlist_set_repeat_mode` |
| Playback navigation / item info | `esp_playlist_next` / `prev` / `curr` / `get_info` |
| Get playlist item count | `esp_playlist_get_count` |
| Release resources | `esp_playlist_del` / `esp_media_db_deinit` |
