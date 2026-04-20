# Config Manager

- [![组件注册](https://components.espressif.com/components/espressif/esp_config_manager/badge.svg)](https://components.espressif.com/components/espressif/esp_config_manager)
- [English version](./README.md)

## 概要

`esp_config_manager` 是一个为嵌入式设备的配置数据提供“主备双副本 + CRC 校验 + 默认值合并”管理能力的模块，核心功能是让配置数据在掉电、写入中断、结构扩展等场景下仍然可使用、可恢复、可进化。

主要能力如下：

- 主/备双槽存储（Primary + Backup）
- 记录头校验（magic / schema_version / payload_len / crc32）
- 自动加载回退（primary -> backup -> defaults）
- 默认值合并（支持自定义 `merge_fn`，用于版本迁移）
- 主槽同步修复（backup 生效后自动回拷）
- 可插拔存储后端（NVS / 文件系统 / 原始 flash / 用户自定义）
- 可选加密钩子（`encrypt` / `decrypt`）

## 存储结构

逻辑上每组配置维护两个槽位：

- `ESP_CONFIG_SLOT_PRIMARY`：主副本
- `ESP_CONFIG_SLOT_BACKUP`：备份副本

每个槽位保存一个二进制 blob，数据格式如下（加密前）：

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

其中 `crc32` 覆盖范围为：`schema_version + payload_len + payload`。加载时若 magic 或 CRC 不通过，会判定该槽位无效。

## 默认值使用

初始化时通过 `esp_config_manager_cfg_t` 传入 `default_config` 和 `default_size`。当主备都不可用时，组件会：

1. 使用默认值生成运行时配置（可经过 `merge_fn`）
2. 将该配置重新写回 primary 和 backup

若未提供 `merge_fn`，内建策略是“前缀覆盖”：

- 先将输出缓冲区填充为 `default_config`
- 再将已加载 payload 的前 `min(loaded_len, default_size)` 字节覆盖到输出

这意味着结构体扩展时，新增尾字段通常能保持默认值。建议对持久化结构使用 `packed` 布局，或提供自定义 `merge_fn` 做显式迁移，避免旧版本 padding 影响新字段。

## 备份逻辑

`esp_config_manager_load()` 的回退策略如下：

1. 先读 `primary`
2. `primary` 缺失或 CRC/magic 失败时，尝试 `backup`
3. 两者都无效时，回退 `defaults`

当 `backup` 成功但 `primary` 失败时：

- `info.primary_repair_scheduled` 会置位
- 组件会在 `esp_config_manager_load()` 返回前完成主槽修复（`backup -> primary`）

可通过 `esp_config_load_info_t::source` 区分本次加载来源：

- `ESP_CONFIG_LOAD_SOURCE_PRIMARY`
- `ESP_CONFIG_LOAD_SOURCE_BACKUP`
- `ESP_CONFIG_LOAD_SOURCE_DEFAULTS`

## 存储扩展

底层持久化由 `esp_config_storage_ops_t` 描述，需实现 3 个回调：

- `read(ctx, slot, buf, inout_len)`
- `write(ctx, slot, buf, len)`
- `erase(ctx, slot)`（可选）

使用 `esp_config_storage_init(ops, ctx, &handle)` 绑定实现与上下文（**不会**拷贝 `ctx`，须在 `esp_config_storage_deinit(handle)` 之前保持有效），将 `handle` 作为 `esp_config_manager_cfg_t.storage` 传入。在销毁配置管理器之后调用 `esp_config_storage_deinit(handle)` 释放句柄（存储句柄生命周期须覆盖管理器句柄）。

组件已内置三种适配器：

- NVS：`esp_config_storage_nvs_ops`
- 文件系统（SPIFFS/FATFS/LittleFS 等）：`esp_config_storage_fs_ops`
- 原始分区 flash：`esp_config_storage_flash_ops`

你可以为任意介质（如外部 EEPROM、远端 KV）实现自己的 `ops`，通过 `esp_config_storage_init()` 得到句柄，再传入 `esp_config_manager_cfg_t.storage`。

## 加密支持

可通过 `esp_config_crypto_ops_t` 注入加解密流程：

- 保存时：先打包 header+payload，再调用 `encrypt()` 后写入存储
- 加载时：先读出 blob，再调用 `decrypt()`，最后做 header/CRC 解析

未配置 `crypto` 时，按明文存储。`encrypt/decrypt` 由用户提供，因此可对接任意算法或硬件安全模块。

> 注意：`crypto_extra_size` 表示密文相对「打包后的明文记录」（16 字节头 + 最长 `default_size` 的 payload）可能多占的字节数，组件据此与内部算出的明文长度一起分配缓冲区。未使用 `encrypt` 时须设为 `0`。

## 使用方法

下面给出最小接入示例（以 NVS 后端为例）：

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
        .merge_fn = NULL,    // 使用默认合并策略
        .merge_ctx = NULL,
        .crypto = NULL,             // 明文存储
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

更多用法（多配置组、FS/Flash 后端、加密钩子、自测流程）可参考示例：

- `examples/simple/main/config_manager_example.c`