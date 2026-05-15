# Changelog

## v0.5.0

### Features

- Initial version of `esp_config_manager`
- Dual-slot configuration storage with primary and backup records
- Record validation with magic, schema version, payload length, and CRC32
- Automatic load fallback from primary to backup to defaults
- Default-value merge support for schema evolution and appended fields
- Synchronous primary repair when the backup slot is the valid source
- Pluggable storage backends through `esp_config_storage_ops_t`
- Optional encryption and decryption hooks for stored configuration blobs
