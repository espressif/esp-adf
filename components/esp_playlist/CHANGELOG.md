# Changelog

## v1.0.0-beta1

### Bugfixes

- Preserved existing playlist items when JSON loading or RAM import fails, built exact-sized replacements while releasing consumed JSON nodes, and fixed NULL-array access when replacing an empty list
- Hardened playlist JSON saves with checked writes, flush/sync/close, temporary-file replacement and unlink-and-retry only on `EEXIST`; retained temporary files after rename failures when the destination is gone, and recover them on later load/save calls only when the destination is reported missing (`ENOENT`)
- Bounded playlist JSON load/save sizes with `CONFIG_ESP_PLAYLIST_FILE_SIZE_MAX` (default 256 KiB); oversized saves return `ESP_ERR_INVALID_SIZE` before writing
- Updated navigation cursors only after successful lookup and made `esp_playlist_curr()` re-read catalog items instead of serving stale cached values
- Used `esp_random()` for shuffle, avoided consecutive selection of the current item in multi-item lists, and kept single-item lists playable
- Released the media catalog binding on `esp_playlist_clean()` and successful JSON load/RAM import so a different catalog can be imported
- Rejected filter keys other than `name`, `url` and `id` with `ESP_ERR_INVALID_ARG` in `esp_playlist_import_media()`
- Reopened persisted catalogs after `esp_media_db_clean()` instead of recreating them; refused corrupt or incomplete catalogs without truncating the surviving files
- Cleaned disposable staging files after failed catalog opens and partial files after failed first-time creation
- Installed synced catalog staging files by rename instead of copying over live files; aborted installation on sync failure and recovered interrupted replacements when a missing live file still has its staging copy
- Validated catalog headers, key lengths and file sizes with overflow-safe arithmetic; recomputed live row counts on load and rejected unreadable or invalid row-used flags
- Checked cell ranges, scalar sizes and short reads in catalog reads/lookups; continued past unreadable rows and reported a storage error if none of the readable rows matched
- Propagated URL duplicate-lookup read and allocation failures instead of treating them as missing URLs and inserting new rows
- Bounded row-ID enumeration and returned the actual populated count to prevent heap overflow and uninitialized IDs when stored counts differ
- Stopped reusing deleted row slots so existing playlist entries cannot silently resolve to different media
- Propagated short-write errors and cleared the recorded failure so it does not affect the next successful operation
- Rolled back failed catalog mutations and recovered closed databases inside the library without reopening after unchanged rejected batches
- Saved the catalog once per scan, skipped inaccessible subdirectories with a warning, and reported allocation failures
- Removed every catalog row matching a requested URL, including duplicate entries
- Checked name/URL cell types before string access, fixed catalog-handle/key-cache leaks on error paths, and checked debug allocations
- Allocated the new name before changing RAM storage objects during rename, preserving them on allocation failure

### Changes

- Replaced the unused unsafe squeeze function with threshold-based automatic value reclamation after deletion; preserved IDs and added recoverable paired data/offset installation
- Used a point lookup for single-item imports and a batch-sized borrowed-URL index for batches, scanning the catalog once without retaining copies of its URLs
- Rebuilt staging lazily after successful saves so staging refresh failure cannot retroactively fail an already committed operation
- Unified media-library initialization cleanup and wrapped component code to 120 columns
- Declared the component license and ESP-IDF >=5.4 requirement in the manifest, made cJSON a private dependency, and added the missing `<stdint.h>` include

### Docs

- Documented caller-managed synchronization and the scan filter callback's no-re-entry rule
- Corrected `epl_media_db_get_row_count()` to state that deleted rows are not counted

### Tests

- Added paired-install and preparation-failure compaction regressions, lazy-refresh and private rollback-recovery fault injection, batch collision/order and heap bounds, allocation-failure replacement coverage and two-task caller-lock coverage
- Added default/LFN_NONE CI variants and rejected-temporary-name preservation coverage; test fixtures use valid 8.3 names

### Known Limitations

- Unsupported playlist temporary names now return `ESP_ERR_NOT_SUPPORTED` without touching the old file; there is no unsafe in-place fallback
- Compaction needs temporary disk space and retains row slots to preserve IDs; it reclaims value bytes, not audio content
- A catalog save can return an error after some files have been installed even though a change may have persisted; retrying the public add/remove operations is safe

## v0.5.1~1

### Docs

- Updated example README files to use the latest ESP Board Manager board selection commands and board names

## v0.5.1

### Bugfixes

- Fixed build failure caused by missing `cJSON.h` include path when building `esp_playlist`
- Fixed `esp_playlist` example and test app build issues in CI

## v0.5.0

### Features

- Initial release of `esp_playlist` and `esp_media_db`
- Media catalog with filesystem (FS) or RAM storage; directory scan and catalog add/remove
- In-memory playlist: import from catalog (optional filter), JSON file save/load, RAM export/import
- Playback control: repeat modes, index navigation, item count queries
- Compatible with ESP-IDF v5.5 and v6.0
