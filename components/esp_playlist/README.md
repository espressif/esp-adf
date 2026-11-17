# ESP Playlist

[中文版](./README_CN.md)

`esp_playlist` is a **media catalog** and **playlist** management component for Espressif multimedia applications. The catalog stores each media file’s `name` and `url`; the playlist stores playback order and the current playback position. The component provides directory scanning, loading a catalog from storage, custom filters for building playlists, playlist JSON save/load, current-item navigation (`curr` / `next` / `prev`), and several repeat modes.

## Key Features

- **Load an existing catalog** — With `ESP_DB_STORAGE_FS`, call `esp_media_db_load()` at startup to restore entries saved on the filesystem
- **Build a catalog quickly** — `esp_media_db_scan()` filters media under a folder by recursion depth, file extension, and related options
- **Flexible playlists** — `esp_playlist_import_media()` appends from the catalog; `esp_playlist_load()` / `esp_playlist_import_ram()` load a playlist from a file or memory and **replace** the current list
- **Export playlists** — `esp_playlist_save()` / `esp_playlist_export_ram()` produce readable JSON for files, NVS, or network delivery
- **Multiple playlist instances** — Create several playlists for quick switching
- **Custom playlists** — `esp_media_filter_t` supports AND/OR matching on `name`, `url` and `id`; album/artist-based lists may be added in future versions
- **Current item and repeat** — Set the current index; use `next` / `prev` / `curr` / `get_info` for item details; supports no repeat, single-track repeat, list repeat, and shuffle

## Directory Structure

Public headers:

| Header | Description |
|--------|-------------|
| `esp_media_db_types.h` | Catalog storage types, field values, filters, and scan configuration |
| `esp_media_db.h` | Catalog init, scan, load, add/remove, and count APIs |
| `esp_playlist.h` | Playlist create, import, JSON save/load, navigation, and item info APIs |

Repository layout:

```text
components/esp_playlist/
├── include/          Public API
├── src/              Media catalog and playlist implementation
├── private_inc/      Component-internal headers
├── helper/           Internal helper container
├── test_apps/        Unity test application
└── examples/playlist_benchmark/
```

## Architecture and Data Model

The component has two independent modules:

- **`esp_media_db` (catalog)** — Records which media files exist on the device. With `ESP_DB_STORAGE_FS`, the catalog can be saved under `storage_path` and loaded again after reboot; with `ESP_DB_STORAGE_RAM`, data stays in RAM only for flexible switching and fast read/write
- **`esp_playlist` (playlist)** — Keeps playback order and the current item in RAM. Build from the catalog via `import_media`, or restore from `playlist.json` via `load` / `import_ram`; write back with `save` / `export_ram`

```text
Application
  ├─ esp_media_db  ←→  media directory (scan/add); optional VFS catalog files
  └─ esp_playlist (RAM)  ←→  playlist.json; optional import_media from catalog
```

## Quick Start

Typical example:

Use `ESP_DB_STORAGE_FS` on SD card to persist the catalog, scan a directory, import into a playlist, read the current item, and save JSON.

`file_extensions` is configured by the application (e.g. `.mp3`, `.mp4`). `/sdcard/music` is only an example path.

```c
#include "esp_playlist.h"

esp_media_db_handle_t media_db = NULL;
esp_playlist_handle_t playlist = NULL;

const esp_media_db_cfg_t db_cfg = {
    .storage_type = ESP_DB_STORAGE_FS,
    .storage_path = "/sdcard/__playlist",
};
ESP_ERROR_CHECK(esp_media_db_init(&db_cfg, &media_db));
ESP_ERROR_CHECK(esp_media_db_load(media_db));  /* Load first if catalog files already exist under storage_path */

const char *exts[] = {".mp3", ".wav"};
const esp_media_db_scan_cfg_t scan_cfg = {
    .path = "/sdcard/music",
    .scan_depth = 2,
    .skip_duplicate = false,
    .file_extensions = exts,
    .file_extension_count = 2,
};
ESP_ERROR_CHECK(esp_media_db_scan(media_db, &scan_cfg));

ESP_ERROR_CHECK(esp_playlist_new(&(esp_playlist_cfg_t) { .playlist_name = "default" }, &playlist));
ESP_ERROR_CHECK(esp_playlist_import_media(playlist, media_db, NULL));

esp_playlist_info_t info = {0};
ESP_ERROR_CHECK(esp_playlist_set_curr_index(playlist, 0));
ESP_ERROR_CHECK(esp_playlist_curr(playlist, &info));
/* Start playback with info.media_url; show info.media_name in the UI */
/* my_player_play(info.media_url); */
/* my_ui_set_title(info.media_name); */

ESP_ERROR_CHECK(esp_playlist_next(playlist, &info));
/* my_player_play(info.media_url); */

ESP_ERROR_CHECK(esp_playlist_save(playlist, "/sdcard/__playlist/playlist.json"));

ESP_ERROR_CHECK(esp_playlist_del(playlist));
ESP_ERROR_CHECK(esp_media_db_deinit(media_db));
```

## Media Catalog and Playlist Usage

`esp_media_db_init()` creates an empty catalog. Use `esp_media_db_load()` to restore from `storage_path`; `esp_media_db_scan()` and `esp_media_db_add()` also reopen the persisted catalog before adding to it, so they are the calls to use when you need to collect files again.

`storage_type`: `ESP_DB_STORAGE_FS` = persist on the filesystem; `ESP_DB_STORAGE_RAM` = runtime only, not written to disk.

| Operation | Behavior |
|-----------|----------|
| `esp_media_db_load()` | Load catalog from `storage_path`; if no catalog files exist an empty catalog is created; if the files are corrupted an error is returned and the files are left untouched. A save that was interrupted between removing a catalog file and renaming its staging copy is finished here, so the catalog is always one complete state, the old or the new one |
| `esp_media_db_scan()` | Scan a directory and add files matching extensions; with `ESP_DB_STORAGE_FS`, results are written under `storage_path` (see `skip_duplicate` below) |
| `esp_media_db_add()` | Add `name` / `url` entries manually |
| `esp_media_db_remove()` | Remove entries by URL |
| `esp_media_db_clean()` | Release the in-memory catalog view only; does not delete catalog files on disk; the next `add` / `scan` / `load` reopens the persisted catalog |

**`skip_duplicate` (skip URLs already in the catalog during scan):**

| `skip_duplicate` | Empty catalog | Catalog already has entries |
|------------------|---------------|-----------------------------|
| `true` | Add normally | No URL comparison; all scanned items are added |
| `false` | Add normally (no dedup) | Skip URLs already present; add new URLs only |

For incremental scans into a **non-empty** catalog, use `skip_duplicate = false`.

`filter_cb` runs inside `esp_media_db_scan()` while the catalog is being modified; it must not call any `esp_media_db` API on the handle being scanned.

`esp_playlist_new()` creates an empty playlist. `esp_playlist_import_media()` **appends** entries: `filter == NULL` imports all; with `esp_media_filter_t`, apply AND/OR filters (supported keys: `name`, `url`, `id`; any other key is rejected with `ESP_ERR_INVALID_ARG`). **The same media may appear more than once** (no dedup by `media_id`). Public scan/add APIs do not write embedded metadata such as `album` or `artist`.

`esp_playlist_load()` / `esp_playlist_import_ram()` **clear and replace** the current list; they do not append. If the JSON is invalid or memory runs out, the current list is left unchanged. `esp_playlist_save()` / `esp_playlist_export_ram()` export playlist JSON only, not catalog files. `esp_playlist_save()` returns `ESP_ERR_INVALID_SIZE` without writing when the JSON would exceed `CONFIG_ESP_PLAYLIST_FILE_SIZE_MAX`, the limit `esp_playlist_load()` enforces. The buffer returned by `export_ram` must be freed by the caller.

`esp_playlist_save()` writes `<save_path>.tmp`, `fsync()`s it and renames it over `save_path`. On FATFS the old file has to be removed before the rename; if power is lost in between, the next `esp_playlist_load()` / `esp_playlist_save()` of the same path installs the kept temp file. An unsupported temporary filename returns `ESP_ERR_NOT_SUPPORTED` and leaves the original file unchanged; there is no in-place fallback. With `CONFIG_FATFS_LFN_NONE`, use an extensionless 8.3 save path (for example `/sdcard/list`, whose temporary file is `list.tmp`) or enable long filenames. On SPIFFS, shorten the path or increase `CONFIG_SPIFFS_OBJ_NAME_LEN`.

A playlist is bound to the catalog of its first `esp_playlist_import_media()`; importing from a different catalog returns `ESP_ERR_INVALID_STATE` until `esp_playlist_clean()`, `esp_playlist_load()` or `esp_playlist_import_ram()` replaces the list and releases the binding.

Automatic compaction reclaims deleted and superseded value bytes without renumbering media IDs. Successful deletions trigger it when garbage reaches `CONFIG_ESP_PLAYLIST_COMPACT_THRESHOLD` (default 25%; 0 disables automatic compaction). Data and offsets are synced together before an intent marker is created; interrupted installs finish on the next access/load. `^`-prefixed data/offset files and a `!`-prefixed table name are reserved for this recovery protocol. Row slots remain allocated to keep IDs stable. This does not compress audio files.

Single-item imports use a point lookup; batch imports index the input URLs without copying them, then scan the catalog once without retaining its URL strings. The existing row-ID array still scales with the catalog size. JSON replacement builds an exact-sized new item array and releases consumed JSON nodes; the old list remains intact until the replacement is ready. Failed rollback is recovered inside the library; a rejected batch does not force a second catalog copy. Committed saves rebuild staging lazily, so a later staging-copy failure is not reported as a failed commit.

The bound catalog is borrowed, not owned by the playlist. Delete or unbind the playlist before calling `esp_media_db_deinit()` on its catalog.

## Current Item and Navigation

The playlist has a **current index** (0-based, default 0) for the item being played. `esp_playlist_set_curr_index()` jumps to a given item; `esp_playlist_curr()` / `next()` / `prev()` / `get_info()` read item information.

| API | Behavior |
|-----|----------|
| `esp_playlist_set_repeat_mode()` | Repeat mode (`ESP_PLAYLIST_REPEAT_*`): `NONE` (stop at boundary), `ONE` (single-track repeat), `ALL` (list repeat), `SHUFFLE` (random, never the same item twice in a row; a single-item list keeps returning that item) |
| `esp_playlist_set_curr_index()` | Set current index; out-of-range returns an error |
| `esp_playlist_next()` / `esp_playlist_prev()` | Change current index per repeat mode and fill `esp_playlist_info_t`; if the target item cannot be resolved (e.g. removed from the catalog) an error is returned and the current index is not moved |
| `esp_playlist_curr()` | Read the item at the current index |
| `esp_playlist_get_info()` | Read the item at `index` without changing the current index |
| `esp_playlist_get_count()` | Return the number of playlist entries |

`esp_playlist_curr()` re-reads database-backed items on each call instead of serving a stale cache after a catalog update or deletion.

## Typical Usage Scenarios

| Scenario | Suggested flow |
|----------|----------------|
| Local music on SD card | Mount SD, scan directory, import playlist, save `playlist.json` |
| Fixed URL list on Flash | `esp_media_db_add()` entries, then `import_media` |
| Restore on boot | `esp_media_db_load()` for catalog; `esp_playlist_load()` for playlist JSON |
| NVS / network-delivered list | Store JSON from `export_ram` yourself; restore with `import_ram` |
| Filter by name or URL | Build `esp_media_filter_t` with `ESP_MEDIA_MATCH_EXACT` / `ESP_MEDIA_MATCH_CONTAINS` / `ESP_MEDIA_MATCH_PREFIX` |

## Performance and Example

[`examples/playlist_benchmark/`](examples/playlist_benchmark/) reports average latency of media catalog and playlist APIs on SD card (`esp_board_manager`).

The example covers: probing existing catalog files at startup, catalog scan/load, playlist import and track changes, and JSON save/load.

## Thread Safety

The component does not take any locks. `esp_media_db_handle_t` and `esp_playlist_handle_t` must be used from one task at a time; serialize calls with your own mutex when a player task and a scanner/UI task share the same handle.

## FAQ

**Q: What is the difference between the media catalog and playlist JSON?**

The catalog (VFS files under `storage_path`, e.g. `/sdcard/__playlist`) stores the **set of name/url entries** from scan or manual add. `playlist.json` stores **one playlist**—its name, order, and items—not the catalog file.

**Q: If I only load `playlist.json`, do I still need the catalog?**

No. JSON entries already include `name` and `url`; after load you can use `curr` / `next` / `prev` / `get_info` to get URLs for playback.

**Q: Can I import by album or artist?**

Public APIs do not read album/artist from files, and `esp_playlist_import_media()` accepts only `name`, `url` and `id` as filter keys today — any other key returns `ESP_ERR_INVALID_ARG`. Future versions may parse file metadata and expose it as a filter key.
