ESP Config Manager
=========================

:link_to_translation:`en:[English]`

简介
----------

`Config Manager <https://components.espressif.com/components/espressif/esp_config_manager>`__ 为嵌入式设备提供配置持久化：主备双槽存储、CRC 校验与默认值合并，使配置在掉电、写入中断与结构扩展后仍可恢复。:doc:`/multimedia-services/peripheral/esp-wifi-service` 用它保存无线网络配置，其他需要持久化配置的服务也可以直接复用。

功能清单
----------

- 主/备双槽存储：每组配置维护 ``ESP_CONFIG_SLOT_PRIMARY`` 与 ``ESP_CONFIG_SLOT_BACKUP`` 两个副本
- 记录头校验：\ ``magic`` / ``schema_version`` / ``payload_len`` / ``crc32``\ ，加载时任一项不通过即判定该槽位失效
- 自动加载回退：primary 失败尝试 backup，两者都失败则回退到默认值
- 主槽同步修复：backup 生效时自动回拷到 primary，无需额外调用
- 默认值合并：内建“前缀覆盖”策略，也支持自定义 ``merge_fn`` 做版本迁移
- 可插拔存储后端：内置 NVS、文件系统（SPIFFS/FATFS/LittleFS 等）、原始 flash 分区三种适配器，也可实现自定义 ``esp_config_storage_ops_t``
- 可选加密钩子：\ ``encrypt`` / ``decrypt`` 回调，未配置时按明文存储

技术拆解
----------

存储结构与校验
^^^^^^^^^^^^^^^^^^

每个槽位保存一段二进制 blob，由 16 字节私有记录头（\ ``magic``\ 、\ ``schema_version``\ 、\ ``payload_len``\ 、\ ``crc32``\ ）加上变长 payload 组成，\ ``crc32`` 覆盖 ``schema_version + payload_len + payload``\ 。加载时 magic 或 CRC 任一项不通过即认为该槽位无效，从而触发下一级回退。

加载回退与主槽修复
^^^^^^^^^^^^^^^^^^^^^^

:cpp:func:`esp_config_manager_load` 依次尝试 primary、backup、默认值三级来源，并通过 :cpp:type:`esp_config_load_info_t` 报告实际命中的来源与是否发生了修复：

.. only:: html

   .. mermaid::

      flowchart TD
          start[load] --> primary{primary 校验}
          primary -- 通过 --> done[返回 primary 数据]
          primary -- 失败 --> backup{backup 校验}
          backup -- 通过 --> repair[回拷 backup 到 primary] --> done2[返回 backup 数据]
          backup -- 失败 --> defaults[合并默认值并写回两槽] --> done3[返回默认值]

primary 和 backup 都失效时，组件用默认值生成运行时配置（可经过 ``merge_fn``\ ），并重新写回两个槽位，使下一次加载能直接从 primary 命中。

默认值合并
^^^^^^^^^^^^^^

未提供 ``merge_fn`` 时，内建策略是“前缀覆盖”：输出缓冲区先填充为 ``default_config``\ ，再用已加载 payload 的前 ``min(loaded_len, default_size)`` 字节覆盖。这意味着结构体新增的尾部字段在旧数据比新结构短时能保持默认值；但如果新旧 ``sizeof`` 相同，旧数据里的 padding 也会原样覆盖新字段。建议持久化结构使用 ``packed`` 布局，或者提供自定义 ``merge_fn`` 做显式迁移。

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

存储后端扩展
^^^^^^^^^^^^^^^^

底层持久化由 :cpp:type:`esp_config_storage_ops_t` 描述，只需实现 ``read`` / ``write`` 两个必选回调和一个可选的 ``erase``\ 。内置的 NVS、文件系统、原始 flash 三种适配器分别通过 :cpp:func:`esp_config_storage_init_nvs`、:cpp:func:`esp_config_storage_init_fs`、:cpp:func:`esp_config_storage_init_flash` 创建；自定义介质（外部 EEPROM、远端 KV 等）实现一套 ``ops`` 后通过 :cpp:func:`esp_config_storage_init` 绑定即可，两种方式得到的句柄都以 ``esp_config_manager_cfg_t.storage`` 传给管理器。

.. important::

    ``esp_config_storage_init()`` 不会拷贝传入的 ``ctx``\ ，调用方须保证其在 :cpp:func:`esp_config_storage_deinit` 之前保持有效；存储句柄的生命周期也必须覆盖使用它的 Config Manager 句柄，销毁顺序为先 ``esp_config_manager_deinit()`` 再 ``esp_config_storage_deinit()``\ 。

加密支持
^^^^^^^^^^^^

通过 :cpp:type:`esp_config_crypto_ops_t` 注入 ``encrypt`` / ``decrypt`` 回调：保存时先打包记录头和 payload 再加密写入；加载时先解密再做记录头/CRC 校验。未配置 ``crypto`` 时按明文存储；\ ``crypto_extra_size`` 表示密文相对明文记录额外占用的字节数，由加密算法决定，未使用加密时须设为 ``0``\ 。

应用示例
----------

- ``examples/config_manager_example/main/config_manager_example.c`` 演示多配置组、文件系统/flash 后端、加密钩子和自测流程。

FAQ
------

**Q1：primary 和 backup 都损坏时，配置会丢失吗？**

不会静默丢失。组件会用默认值重新生成配置并写回两个槽位，之后的加载会正常命中 primary；调用方可通过 :cpp:type:`esp_config_load_info_t::source` 判断本次是否走了默认值路径，据此决定是否需要额外提示或重新引导用户配置。

API 参考
----------

.. include-build-file:: inc/esp_config_manager.inc

.. include-build-file:: inc/esp_config_storage.inc
