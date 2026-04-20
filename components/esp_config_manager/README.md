# Config Manager

- [![Component Registry](https://components.espressif.com/components/espressif/esp_config_manager/badge.svg)](https://components.espressif.com/components/espressif/esp_config_manager)
- [中文版本](./README_CN.md)

## Overview

`esp_config_manager` provides embedded configuration management with **dual-copy redundancy + CRC validation + default-value merge**. Its core role is to keep configuration data **still usable, recoverable, and evolvable** across power loss, interrupted writes, and struct/schema expansion.

Key capabilities:

- Primary/backup dual-slot storage (Primary + Backup)
- Record header validation (`magic / schema_version / payload_len / crc32`)
- Automatic load fallback (`primary -> backup -> defaults`)
- Default merge with custom `merge_fn` support (for migration/evolution)
- Synchronous primary repair (copy backup back to primary)
- Pluggable storage backends (NVS / filesystem / raw flash / custom)
- Optional crypto hooks (`encrypt` / `decrypt`)

## Storage Layout

Each config group maintains two logical slots:

- `ESP_CONFIG_SLOT_PRIMARY`: main copy
- `ESP_CONFIG_SLOT_BACKUP`: redundant copy

Each slot stores one binary blob in this format (before optional encryption):

```text
+--------------------------------------------------------------+
| private record header (16 bytes)                             |
|   - magic          (uint32_t)                                |
|   - schema_version (uint32_t)                                |
|   - payload_len    (uint32_t)                                |
|   - crc32          (uint32_t)                                |
+--------------------------------------------------------------+
| payload (variable length)                                    |
+--------------------------------------------------------------+
```

`crc32` covers `schema_version + payload_len + payload`. If `magic` or the CRC check fails during load, that slot is treated as invalid.

## Default Value Behavior

You provide `default_config` and `default_size` through `esp_config_manager_cfg_t`. If both primary and backup are invalid, the manager will:

1. Build runtime config from defaults (optionally via `merge_fn`)
2. Persist that result back into both primary and backup

If `merge_fn` is `NULL`, the built-in merge strategy is prefix overlay:

- Initialize output with `default_config`
- Overwrite the first `min(loaded_len, default_size)` bytes from loaded payload

This typically preserves newly appended tail fields when structs grow. For persistent structs, prefer `packed` layout or a custom `merge_fn` for explicit migration, to avoid old padding bytes affecting new fields.

## Backup and Recovery Logic

`esp_config_manager_load()` uses this fallback flow:

1. Read `primary`
2. If `primary` is missing or fails CRC/magic, try `backup`
3. If both are invalid, fall back to `defaults`

When `backup` succeeds but `primary` fails:

- `info.primary_repair_scheduled` is set
- Primary repair (`backup -> primary`) is completed before `esp_config_manager_load()` returns

Use `esp_config_load_info_t::source` to identify where the loaded config came from:

- `ESP_CONFIG_LOAD_SOURCE_PRIMARY`
- `ESP_CONFIG_LOAD_SOURCE_BACKUP`
- `ESP_CONFIG_LOAD_SOURCE_DEFAULTS`

## Storage Extension

Low-level persistence is defined by `esp_config_storage_ops_t`, with three callbacks:

- `read(ctx, slot, buf, inout_len)`
- `write(ctx, slot, buf, len)`
- `erase(ctx, slot)` (optional)

Create a bound backend with `esp_config_storage_init(ops, ctx, &handle)` (`ctx` is **not** copied; it must stay valid until `esp_config_storage_deinit(handle)`) and pass `handle` as `esp_config_manager_cfg_t.storage`. Release the handle after the manager is destroyed (the storage handle must outlive the manager handle).

The component already includes three adapters:

- NVS: `esp_config_storage_nvs_ops`
- Filesystem (SPIFFS/FATFS/LittleFS, etc.): `esp_config_storage_fs_ops`
- Raw flash partitions: `esp_config_storage_flash_ops`

You can implement your own `ops` for any medium (e.g. external EEPROM, remote KV), bind them with `esp_config_storage_init()`, and pass the resulting handle through `esp_config_manager_cfg_t.storage`.

## Encryption Support

You can inject encryption/decryption via `esp_config_crypto_ops_t`:

- On save: pack header+payload, then call `encrypt()` before writing
- On load: read blob, then call `decrypt()`, then parse header/CRC

If `crypto` is not provided, data is stored in plaintext. Since `encrypt/decrypt` are user-provided hooks, you can integrate any algorithm or secure element/HSM flow.

> Note: `crypto_extra_size` is the extra bytes ciphertext may need beyond the packed plaintext record (16-byte header + payload of up to `default_size`). Internal buffers use that value plus the computed plaintext size. When `encrypt` is not used, set `crypto_extra_size` to `0`.

## Usage

Minimal integration example (NVS backend):

```c
#include "esp_config_manager.h"
#include "esp_config_storage.h"

typedef struct __attribute__((packed)) {
    uint32_t volume;
    uint8_t  mode;
    uint8_t  reserve[3];
} app_cfg_t;

static const app_cfg_t s_defaults = {
    .volume = 50,
    .mode = 1,
};

static esp_config_storage_nvs_t s_nvs_ctx = {
    .nvs_namespace = "app_cfg",
    .key_primary = "main",
    .key_backup = "bak",
};

void init_and_use_config(void)
{
    esp_config_storage_t storage = NULL;
    ESP_ERROR_CHECK(esp_config_storage_init_nvs(&s_nvs_ctx, &storage));

    esp_config_manager_cfg_t cfg = {
        .storage = storage,
        .default_config = &s_defaults,
        .default_size = sizeof(s_defaults),
        .schema_version = 1,
        .merge_fn = NULL,    // use built-in merge strategy
        .merge_ctx = NULL,
        .crypto = NULL,             // plaintext storage
        .crypto_extra_size = 0,
    };

    esp_config_manager_handle_t handle = NULL;
    ESP_ERROR_CHECK(esp_config_manager_init(&cfg, &handle));

    app_cfg_t runtime_cfg;
    esp_config_load_info_t info;
    ESP_ERROR_CHECK(esp_config_manager_load(handle, &runtime_cfg, &info));

    runtime_cfg.volume = (runtime_cfg.volume + 5) % 101;
    ESP_ERROR_CHECK(esp_config_manager_save(handle, &runtime_cfg, sizeof(runtime_cfg)));

    esp_config_manager_deinit(handle);
    esp_config_storage_deinit(storage);
}
```

For advanced usage (multiple config groups, FS/Flash backends, crypto hooks, self-test flow), see:

- `examples/simple/main/config_manager_example.c`
