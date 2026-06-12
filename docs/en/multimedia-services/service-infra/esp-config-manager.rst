ESP Config Manager
==================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`Config Manager <https://components.espressif.com/components/espressif/esp_config_manager>`__ persists configuration for embedded devices with primary and backup slots, CRC checks, and default-value merging, so settings remain recoverable after power loss, interrupted writes, and structure changes. :doc:`/multimedia-services/peripheral/esp-wifi-service` uses it to store Wi-Fi profiles. Other services that need persistent configuration can reuse it.

Feature List
------------

- Primary/backup dual-slot storage: each configuration group maintains two copies, ``ESP_CONFIG_SLOT_PRIMARY`` and ``ESP_CONFIG_SLOT_BACKUP``
- Record header verification: \ ``magic`` / ``schema_version`` / ``payload_len`` / ``crc32``\ ; if any of these checks fails during loading, the slot is deemed invalid
- Automatic load fallback: if primary fails, backup is tried; if both fail, it falls back to default values
- Primary slot repair on sync: when backup takes effect, it is automatically copied back to primary, with no extra calls required
- Default value merging: a built-in "prefix overwrite" strategy is provided, and a custom ``merge_fn`` is also supported for version migration
- Pluggable storage backends: three built-in adapters for NVS, filesystem (SPIFFS/FATFS/LittleFS, etc.), and raw flash partitions are provided, and a custom ``esp_config_storage_ops_t`` can also be implemented
- Optional encryption hooks: \ ``encrypt`` / ``decrypt`` callbacks; data is stored in plaintext when not configured

Technical Deep Dive
--------------------

Storage Structure and Verification
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each slot stores a binary blob consisting of a 16-byte private record header (\ ``magic``\ , \ ``schema_version``\ , \ ``payload_len``\ , \ ``crc32``\ ) plus a variable-length payload, where \ ``crc32`` covers ``schema_version + payload_len + payload``\ . During loading, if either the magic or the CRC check fails, the slot is considered invalid, triggering the next-level fallback.

Load Fallback and Primary Slot Repair
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:cpp:func:`esp_config_manager_load` tries the three sources, primary, backup, and default values, in sequence, and reports the actual source that was hit as well as whether a repair occurred through :cpp:type:`esp_config_load_info_t`:

.. only:: html

   .. mermaid::

      flowchart TD
          start[load] --> primary{primary check}
          primary -- pass --> done[return primary data]
          primary -- fail --> backup{backup check}
          backup -- pass --> repair[copy backup to primary] --> done2[return backup data]
          backup -- fail --> defaults[merge defaults and write back to both slots] --> done3[return default values]

When both primary and backup are invalid, the component generates the runtime configuration from default values (optionally processed through ``merge_fn``\ ) and writes it back to both slots, so that the next load can hit primary directly.

Default Value Merging
^^^^^^^^^^^^^^^^^^^^^^

When ``merge_fn`` is not provided, the built-in strategy is "prefix overwrite": the output buffer is first filled with ``default_config``\ , and then overwritten with the first ``min(loaded_len, default_size)`` bytes of the loaded payload. This means that newly added trailing fields in the structure retain their default values when the old data is shorter than the new structure; however, if the old and new ``sizeof`` are the same, padding bytes in the old data will also overwrite the new fields as-is. It is recommended to use a ``packed`` layout for persisted structures, or to provide a custom ``merge_fn`` for explicit migration.

.. code:: c

    typedef struct __attribute__((packed)) {
        uint32_t volume;
        uint8_t  mode;
        uint8_t  reserve[3];
    } app_cfg_t;

    static const app_cfg_t s_defaults = { .volume = 50, .mode = 1 };

    esp_config_storage_t storage = NULL;
    esp_config_storage_nvs_t nvs_ctx = {
        .nvs_namespace = "app_cfg",
        .key_primary   = "main",
        .key_backup    = "bak",
    };
    esp_config_storage_init_nvs(&nvs_ctx, &storage);

    esp_config_manager_cfg_t cfg = {
        .storage        = storage,
        .default_config = &s_defaults,
        .default_size   = sizeof(s_defaults),
        .schema_version = 1,
    };

    esp_config_manager_handle_t handle = NULL;
    esp_config_manager_init(&cfg, &handle);

    app_cfg_t runtime_cfg;
    esp_config_load_info_t info;
    esp_config_manager_load(handle, &runtime_cfg, &info);

    runtime_cfg.volume = (runtime_cfg.volume + 5) % 101;
    esp_config_manager_save(handle, &runtime_cfg, sizeof(runtime_cfg));

Storage Backend Extension
^^^^^^^^^^^^^^^^^^^^^^^^^^

The underlying persistence is described by :cpp:type:`esp_config_storage_ops_t`, which requires implementing only the two mandatory callbacks ``read`` / ``write`` and one optional ``erase``\ . The three built-in adapters for NVS, filesystem, and raw flash are created via :cpp:func:`esp_config_storage_init_nvs`, :cpp:func:`esp_config_storage_init_fs`, and :cpp:func:`esp_config_storage_init_flash` respectively; for a custom medium (external EEPROM, remote KV store, etc.), implement a set of ``ops`` and bind it via :cpp:func:`esp_config_storage_init`. The handle obtained from either approach is passed to the manager as ``esp_config_manager_cfg_t.storage``.

.. important::

    ``esp_config_storage_init()`` does not copy the passed-in ``ctx``\ ; the caller must ensure it remains valid until :cpp:func:`esp_config_storage_deinit` is called. The lifetime of the storage handle must also cover the Config Manager handle that uses it; the destruction order is to call ``esp_config_manager_deinit()`` first, then ``esp_config_storage_deinit()``\ .

Encryption Support
^^^^^^^^^^^^^^^^^^^

Inject ``encrypt`` / ``decrypt`` callbacks through :cpp:type:`esp_config_crypto_ops_t`: when saving, the record header and payload are packed first and then encrypted before being written; when loading, decryption is performed first, followed by record header/CRC verification. Data is stored in plaintext when ``crypto`` is not configured; \ ``crypto_extra_size`` indicates the number of extra bytes the ciphertext occupies relative to the plaintext record, as determined by the encryption algorithm, and must be set to ``0`` when encryption is not used.

Application Examples
---------------------

- ``examples/config_manager_example/main/config_manager_example.c`` demonstrates multiple configuration groups, filesystem/flash backends, encryption hooks, and a self-test flow.

FAQ
------

**Q1: If both primary and backup are corrupted, will the configuration be lost?**

No, it will not be silently lost. The component regenerates the configuration from default values and writes it back to both slots, so that subsequent loads will hit primary normally. The caller can check :cpp:type:`esp_config_load_info_t::source` to determine whether this load took the default-value path, and decide accordingly whether additional prompts or re-guiding the user through configuration are needed.

API Reference
--------------

.. include-build-file:: inc/esp_config_manager.inc

.. include-build-file:: inc/esp_config_storage.inc
