# ESP Playlist

[English](./README.md)

`esp_playlist` 是面向 Espressif 多媒体应用的**媒体库**与**播放列表**管理组件：媒体库记录每条媒体文件的 `name` 和 `url`；播放列表记录播放顺序和当前播放位置。组件提供目录扫描、从存储加载媒体库、自定义过滤并加入播放列表、播放列表 JSON 格式保存/加载、当前曲目与上一首/下一首切换，以及多种循环模式等能力。

## 主要特性

- **加载已有媒体库** — 使用 `ESP_DB_STORAGE_FS` 时，启动可调用 `esp_media_db_load()` 读回上次保存在文件系统的条目
- **快速创建媒体库** — `esp_media_db_scan()` 可根据递归深度、扩展名等快速过滤指定文件夹下的媒体文件
- **灵活构建播放列表** — `esp_playlist_import_media()` 从媒体库追加条目；`esp_playlist_load()` / `esp_playlist_import_ram()` 可用于从文件或内存中加载播放列表，并覆盖当前列表
- **导出播放列表** — `esp_playlist_save()` / `esp_playlist_export_ram()` 生成可读 JSON 格式，便于保存到文件、NVS 或用于网络下发
- **多个播放列表实例** — 可同时创建多个播放列表，便于快速切换
- **定制化播放列表** — `esp_media_filter_t` 支持对 `name`、`url`、`id` 等字段做 AND/OR 匹配，后期支持根据专辑、歌手等生成播放列表
- **当前曲目与循环** — 设置当前索引，`next` / `prev` / `curr` / `get_info` 获取条目信息；支持不循环、单曲循环、列表循环、随机播放

## 目录结构

组件提供以下公开头文件：

| 头文件 | 说明 |
|--------|------|
| `esp_media_db_types.h` | 媒体库存储类型、字段值、过滤条件和扫描配置 |
| `esp_media_db.h` | 媒体库初始化、扫描、加载、增删和计数接口 |
| `esp_playlist.h` | 播放列表创建、导入、JSON 保存/加载、当前曲目切换和条目信息接口 |

仓库主要目录如下：

```text
components/esp_playlist/
├── include/          公开 API
├── src/              媒体库与播放列表实现
├── private_inc/      组件内部头文件
├── helper/           内部辅助容器
├── test_apps/        Unity 测试应用
└── examples/playlist_benchmark/
```

## 架构与数据模型

本组件包含两个独立模块：

- **`esp_media_db`（媒体库）**：用于记录设备上有哪些媒体文件。使用 `ESP_DB_STORAGE_FS` 时可将媒体数据库保存到 `storage_path` 中，重启后可再次加载；使用 `ESP_DB_STORAGE_RAM` 时仅保存在内存中，便于灵活切换并实现快速读写
- **`esp_playlist`（播放列表）**：在 RAM 中维护播放顺序和当前曲目，可通过 `import_media` 从媒体库生成，或通过 `load` / `import_ram` 从 `playlist.json` 恢复；用 `save` / `export_ram` 写回 JSON

```text
应用
  ├─ esp_media_db  ←→  媒体目录（scan/add）；可选 VFS 库文件
  └─ esp_playlist（RAM）  ←→  playlist.json；可选从媒体库 import_media
```

## 快速开始

下面是常见示例：

使用 `ESP_DB_STORAGE_FS` 在 SD 卡上保存媒体库、扫描目录、导入播放列表、读取当前条目并保存 JSON。

`file_extensions` 由应用配置，可为 `.mp3`、`.mp4` 等常见后缀。`/sdcard/music` 仅为示例路径。

```c
#include "esp_media_db.h"
#include "esp_playlist.h"

esp_media_db_handle_t media_db = NULL;
esp_playlist_handle_t playlist = NULL;

const esp_media_db_cfg_t db_cfg = {
    .storage_type = ESP_DB_STORAGE_FS,
    .storage_path = "/sdcard/__playlist",
};
ESP_ERROR_CHECK(esp_media_db_init(&db_cfg, &media_db));
ESP_ERROR_CHECK(esp_media_db_load(media_db));  /* 若 storage_path 下已有媒体库文件，可先加载 */

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
/* 用 info.media_url 启动播放，用 info.media_name 显示标题 */
/* my_player_play(info.media_url); */
/* my_ui_set_title(info.media_name); */

ESP_ERROR_CHECK(esp_playlist_next(playlist, &info));
/* my_player_play(info.media_url); */

ESP_ERROR_CHECK(esp_playlist_save(playlist, "/sdcard/__playlist/playlist.json"));

ESP_ERROR_CHECK(esp_playlist_del(playlist));
ESP_ERROR_CHECK(esp_media_db_deinit(media_db));
```

## 媒体库与播放列表使用说明

`esp_media_db_init()` 用于创建空媒体库，可使用 `esp_media_db_load()` 从存储路径加载已有媒体库；需要重新收录文件时调用 `esp_media_db_scan()` 或 `esp_media_db_add()`。

`storage_type`：`ESP_DB_STORAGE_FS` = 保存到文件系统；`ESP_DB_STORAGE_RAM` = 不写入文件、仅运行期有效。

| 操作 | 行为 |
|------|------|
| `esp_media_db_load()` | 从 `storage_path` 加载已有媒体库；无库文件或加载失败时通常会重建空库并返回 `ESP_OK`，可继续 `scan` / `add` |
| `esp_media_db_scan()` | 扫描目录，将匹配扩展名的文件加入媒体库；`ESP_DB_STORAGE_FS` 时扫描结果会写入 `storage_path`（见下方 `skip_duplicate`） |
| `esp_media_db_add()` | 手动添加 `name`、`url` 条目 |
| `esp_media_db_remove()` | 按 URL 删除条目 |
| `esp_media_db_clean()` | 清空当前进程中的媒体库视图，不删除文件系统中的库文件；要从盘恢复需再调用 `esp_media_db_load()` |

**`skip_duplicate`（扫描时是否跳过已存在的 URL）：**

| `skip_duplicate` | 媒体库为空 | 媒体库已有条目 |
|------------------|------------|----------------|
| `true` | 正常加入 | 不比较 URL，全部加入 |
| `false` | 正常加入（不做去重） | 已存在的 URL 跳过，新 URL 加入 |

向**已有条目**的库做增量扫描时，通常设 `skip_duplicate = false`。

`esp_playlist_new()` 创建空播放列表。`esp_playlist_import_media()` **追加**条目：`filter == NULL` 表示导入全部；带 `esp_media_filter_t` 时按 AND/OR 条件筛选（常用字段 `name`、`url`、`id`）。**同一媒体可多次出现在列表中**（不按 `media_id` 去重）。公开扫描/添加不会写入 `album`、`artist` 等内嵌元数据字段。

`esp_playlist_load()` / `esp_playlist_import_ram()`：**清空并替换**当前列表，不追加。`esp_playlist_save()` / `esp_playlist_export_ram()` 只导出播放列表 JSON，不保存媒体库文件。`export_ram` 返回的缓冲区由调用方 `free()` 释放。

## 当前曲目与切换

播放列表有「当前索引」（从 0 开始，默认 0），表示正在播放第几项。`esp_playlist_set_curr_index()` 可跳到指定项；`esp_playlist_curr()` / `next()` / `prev()` / `get_info()` 用于读取条目信息。

| API | 行为 |
|-----|------|
| `esp_playlist_set_repeat_mode()` | 循环模式（`ESP_PLAYLIST_REPEAT_*`）：`NONE`（到边界停止）、`ONE`（单曲循环）、`ALL`（列表循环）、`SHUFFLE`（随机） |
| `esp_playlist_set_curr_index()` | 设置当前索引；越界返回错误 |
| `esp_playlist_next()` / `esp_playlist_prev()` | 按循环模式切换当前索引，并填充 `esp_playlist_info_t` |
| `esp_playlist_curr()` | 读取当前索引对应条目 |
| `esp_playlist_get_info()` | 读取指定 `index`，不改变当前索引 |
| `esp_playlist_get_count()` | 返回列表条目数 |

## 典型应用场景

| 场景 | 建议流程 |
|------|----------|
| SD 卡本地音乐 | 挂载 SD，扫描目录，导入播放列表，保存 `playlist.json` |
| Flash 固定 URL 列表 | `esp_media_db_add()` 写入条目后 `import_media` |
| 开机恢复 | `esp_media_db_load()` 加载媒体库；`esp_playlist_load()` 加载播放列表 JSON |
| NVS / 网络下发列表 | `export_ram` 得到 JSON 后自行存储；恢复用 `import_ram` |
| 按名称或 URL 筛选 | 构造 `esp_media_filter_t`，使用 `ESP_MEDIA_MATCH_EXACT` / `ESP_MEDIA_MATCH_CONTAINS` / `ESP_MEDIA_MATCH_PREFIX` |

## 性能与示例工程

[`examples/playlist_benchmark/`](examples/playlist_benchmark/) 在 SD 卡（`esp_board_manager`）上输出媒体库与播放列表各 API 的平均耗时。

示例包含：启动时加载已有库文件、媒体库扫描与加载、播放列表导入与切歌、JSON 保存/加载等。

## FAQ

**问：媒体库和播放列表 JSON 有什么区别？**

媒体库（VFS 下为 `storage_path` 中的库文件，示例目录 `/sdcard/__playlist`）保存扫描或手动添加的 **name/url 条目集合**。`playlist.json` 只保存**一个播放列表**的名称、顺序和条目，不是媒体库文件。

**问：只加载 playlist.json，还需要媒体库吗？**

不需要。JSON 里已有 `name` 和 `url`，加载后可直接 `curr` / `next` / `prev` / `get_info` 取 URL 去播放。

**问：能否按 album 或 artist 导入？**

当前公开接口不会从文件内读取 album/artist。若库中有对应扩展字段，可通过 `esp_media_filter_t` 匹配；后续版本可能支持从文件解析元数据后再筛选。
