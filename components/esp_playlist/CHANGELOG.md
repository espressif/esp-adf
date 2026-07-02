# Changelog

## v0.5.1~1

### Docs

- Updated example README files to use the latest ESP Board Manager board selection commands and board names

## v0.5.1

### Bugfix

- Fixed build failure caused by missing `cJSON.h` include path when building `esp_playlist`
- Fixed `esp_playlist` example and test app build issues in CI

## v0.5.0

### Features

- Initial release of `esp_playlist` and `esp_media_db`
- Media catalog with filesystem (FS) or RAM storage; directory scan and catalog add/remove
- In-memory playlist: import from catalog (optional filter), JSON file save/load, RAM export/import
- Playback control: repeat modes, index navigation, item count queries
- Compatible with ESP-IDF v5.5 and v6.0
