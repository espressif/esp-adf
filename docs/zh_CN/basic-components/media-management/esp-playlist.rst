ESP Playlist
============

:link_to_translation:`en:[English]`

简介
----------

`ESP Playlist <https://components.espressif.com/components/espressif/esp_playlist>`__ 是面向乐鑫多媒体应用的媒体库与播放列表管理组件，由媒体库与播放列表两个独立模块组成。媒体库记录设备上有哪些媒体文件的名称与地址，播放列表记录播放顺序与当前播放位置。组件提供目录扫描、媒体库持久化、播放列表 JSON 导出与导入、多实例管理，以及顺序、单曲循环、列表循环和随机播放等模式，适用于本地音乐播放器与有声内容设备。

功能清单
----------

- 目录扫描：按递归深度、扩展名和自定义回调过滤指定目录下的媒体文件，批量加入媒体库
- 媒体库持久化：文件系统存储时数据落盘，重启后可重新加载；RAM 存储时仅在运行期有效
- 播放列表构建：从媒体库按条件过滤导入，或从 JSON 文件/内存缓冲区加载
- 播放列表导出：生成 JSON 格式，可保存到文件、NVS 或用于网络下发
- 多实例管理：可同时创建多个媒体库和播放列表句柄，独立维护各自状态
- 条件过滤：按 ``name``\ 、\ ``url``\ 、\ ``id`` 等字段做精确匹配、包含匹配或前缀匹配，并支持 AND/OR 组合
- 播放模式：不循环、单曲循环、列表循环、随机播放，配合当前曲目导航接口切换

技术拆解
----------

媒体库（esp_media_db）
^^^^^^^^^^^^^^^^^^^^^^^^^^

媒体库是媒体文件的目录索引，只保存 ``name`` 和 ``url``\ ，不解析媒体内容本身。:cpp:func:`esp_media_db_init` 创建句柄时通过 :cpp:type:`esp_media_db_cfg_t` 的 ``storage_type`` 选择存储方式：\ :cpp:type:`esp_db_storage_type_t` 取值 ``ESP_DB_STORAGE_FS`` 表示数据落盘到 ``storage_path`` 指向的文件系统三文件数据库，重启后可用 :cpp:func:`esp_media_db_load` 重新加载；取值 ``ESP_DB_STORAGE_RAM`` 表示数据只保存在堆内存中，适合无需持久化、频繁重建的场景。

.. code:: c

    esp_media_db_cfg_t db_cfg = {
        .storage_type = ESP_DB_STORAGE_FS,
        .storage_path = "/sdcard/__playlist",
    };
    esp_media_db_handle_t media_db = NULL;
    esp_media_db_init(&db_cfg, &media_db);
    esp_media_db_load(media_db);  /* storage_path 下已有数据库文件时先加载 */

收录文件有两种方式：:cpp:func:`esp_media_db_scan` 按 :cpp:type:`esp_media_db_scan_cfg_t` 中的 ``path``\ 、\ ``scan_depth``\ 、\ ``file_extensions`` 递归扫描目录，可选 ``filter_cb`` 对扫描到的每个 URL 做二次判定；:cpp:func:`esp_media_db_add` 则直接写入调用方已知的 ``name`` / ``url`` 条目。 ``skip_duplicate`` 只作用于 scan：媒体库为空时始终正常加入；媒体库已有条目时，\ ``skip_duplicate = true`` 表示不比较 URL、全部加入，\ ``skip_duplicate = false`` 表示按 URL 跳过已存在的条目。对已有内容的媒体库做增量扫描，通常设置 ``skip_duplicate = false`` 以避免重复条目。:cpp:func:`esp_media_db_add` 始终按 URL 跳过已存在的条目，没有 ``skip_duplicate`` 参数。

:cpp:func:`esp_media_db_remove` 按 URL 删除指定条目；:cpp:func:`esp_media_db_clean` 只清空当前进程内的媒体库视图，不删除文件系统中的数据库文件，需要恢复时再次调用 :cpp:func:`esp_media_db_load` 即可；:cpp:func:`esp_media_db_get_count` 返回当前条目数。

播放列表（esp_playlist）
^^^^^^^^^^^^^^^^^^^^^^^^^^

播放列表在 RAM 中维护一个有序条目序列和当前索引，由 :cpp:func:`esp_playlist_new` 创建空实例。填充播放列表有两条独立路径，行为不同：

- :cpp:func:`esp_playlist_import_media` 从一个媒体库\ **追加**\ 条目，\ ``filter`` 为 ``NULL`` 时导入全部，带 :cpp:type:`esp_media_filter_t` 时按条件筛选；导入后播放列表绑定该媒体库句柄，之后从不同句柄导入会返回 ``ESP_ERR_INVALID_STATE``\ 。这类条目在播放列表内部只保存到媒体库的引用（DB 条目），导航时需要回查媒体库获取 ``name`` / ``url``
- :cpp:func:`esp_playlist_load` / :cpp:func:`esp_playlist_import_ram` 从 JSON 文件或内存缓冲区\ **清空并替换**\ 当前列表，与媒体库无关；解析出的条目直接携带自己的 ``name`` / ``url``\ （inline 条目），导航时不需要回查媒体库

.. code:: c

    esp_playlist_handle_t playlist = NULL;
    esp_playlist_new(&(esp_playlist_cfg_t) { .playlist_name = "default" }, &playlist);
    esp_playlist_import_media(playlist, media_db, NULL);  /* 导入媒体库全部条目 */

:cpp:func:`esp_playlist_save` / :cpp:func:`esp_playlist_export_ram` 只导出播放列表自身的 JSON（名称、顺序、条目），不涉及媒体库文件；两者与 :cpp:func:`esp_playlist_load` / :cpp:func:`esp_playlist_import_ram` 共用同一种 JSON 格式，可互相读写。\ :cpp:func:`esp_playlist_export_ram` 返回的缓冲区需由调用方 ``free()`` 释放。:cpp:func:`esp_playlist_clean` 清空当前列表条目，不删除磁盘上的 JSON 或媒体库文件。

当前曲目与循环模式
^^^^^^^^^^^^^^^^^^^^^^^^^^

播放列表维护一个从 0 开始的当前索引，默认指向第 0 项。:cpp:func:`esp_playlist_set_curr_index` 可跳转到指定索引；:cpp:func:`esp_playlist_curr`、:cpp:func:`esp_playlist_next`、:cpp:func:`esp_playlist_prev` 和 :cpp:func:`esp_playlist_get_info` 用于读取条目信息并填充到调用方提供的 :cpp:type:`esp_playlist_info_t`。其中 ``get_info`` 按指定 ``index`` 读取且不改变当前索引，其余三个都会更新或依赖当前索引。

``next`` / ``prev`` 的边界行为由 :cpp:func:`esp_playlist_set_repeat_mode` 设置的 :cpp:type:`esp_playlist_repeat_mode_t` 决定：

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - 模式
     - 行为
   * - ``ESP_PLAYLIST_REPEAT_NONE``
     - 到达列表边界后返回 ``ESP_ERR_NOT_FOUND``\ ，不循环
   * - ``ESP_PLAYLIST_REPEAT_ONE``
     - 保持在当前索引，反复读取同一条目
   * - ``ESP_PLAYLIST_REPEAT_ALL``
     - 到达边界后回绕到列表另一端
   * - ``ESP_PLAYLIST_REPEAT_SHUFFLE``
     - 每次调用 ``next`` / ``prev`` 时随机选取一个索引

条目为 DB 条目但绑定的媒体库已失效时，导航接口返回 ``ESP_ERR_INVALID_STATE``\ ；列表为空或到达非循环边界时返回 ``ESP_ERR_NOT_FOUND``\ 。

过滤器（esp_media_filter_t）
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:cpp:type:`esp_media_filter_t` 由若干 :cpp:type:`esp_media_filter_item_t` 条件组成，每个条件指定字段名（如 ``name``\ 、\ ``url``\ 、\ ``id``\ ）、期望值和 :cpp:type:`esp_media_match_type_t` 匹配方式（精确匹配 ``ESP_MEDIA_MATCH_EXACT``\ 、包含匹配 ``ESP_MEDIA_MATCH_CONTAINS``\ 、前缀匹配 ``ESP_MEDIA_MATCH_PREFIX``\ ），并通过 ``match_all`` 决定条件间是 AND 还是 OR 组合。该过滤器同时用于 :cpp:func:`esp_playlist_import_media` 的导入筛选。

.. code:: c

    esp_media_filter_item_t items[] = {
        { .key = "url", .expected = { .type = ESP_DB_FIELD_TYPE_STRING, .value.strv = ".mp3", .size = 5 },
          .match_type = ESP_MEDIA_MATCH_CONTAINS },
    };
    esp_media_filter_t filter = {
        .items = items,
        .item_count = 1,
        .match_all = true,
    };
    esp_playlist_import_media(playlist, media_db, &filter);

性能
----------

`playlist_benchmark <https://github.com/espressif/esp-adf/tree/master/components/esp_playlist/examples/playlist_benchmark>`__ 例程在 ESP32-P4 Function EV Board（SD 卡、1000 条目）上的实测单次平均耗时如下，实际数值随 SoC、SD 卡速度和媒体数量变化：

.. list-table::
   :header-rows: 1
   :widths: 40 30 30

   * - 操作
     - 场景
     - 平均耗时
   * - ``esp_media_db_scan``\ （冷扫描，不去重）
     - 1000 个文件
     - 664.71 us/item
   * - ``esp_media_db_load``
     - 1000 条目
     - 66.34 us/item
   * - ``esp_playlist_import_media``
     - 1000 条目
     - 176.76 us/item
   * - ``esp_playlist_next`` / ``prev``\ （DB 条目）
     - 单次调用
     - 156.71 / 205.23 us
   * - ``esp_playlist_next`` / ``prev``\ （inline 条目）
     - 单次调用
     - 2.53 / 2.49 us
   * - ``esp_playlist_save``
     - 1000 条目，73045 字节
     - 366.81 us/item
   * - ``esp_playlist_load``\ （JSON）
     - 1000 条目，73045 字节
     - 105.63 us/item

DB 条目导航（\ ``import_media`` 导入）比 inline 条目导航（\ ``load`` / ``import_ram`` 导入）慢约两个数量级，原因是前者每次都要回查媒体库获取 ``name`` / ``url``\ ，后者条目自身已携带完整信息。频繁切换曲目的场景可优先考虑 inline 条目路径。

应用示例
----------

- `playlist_benchmark <https://github.com/espressif/esp-adf/tree/master/components/esp_playlist/examples/playlist_benchmark>`__ 演示媒体库扫描/加载、播放列表导入与导航、JSON 保存/加载的完整流程，并输出各接口的压测耗时

典型场景与对应接口如下：

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - 场景
     - 建议流程
   * - SD 卡本地音乐
     - 挂载 SD 卡，\ ``esp_media_db_scan`` 扫描目录，\ ``esp_playlist_import_media`` 导入，\ ``esp_playlist_save`` 保存播放列表
   * - Flash 固定 URL 列表
     - ``esp_media_db_add`` 写入固定条目后 ``esp_playlist_import_media`` 导入
   * - 开机恢复
     - ``esp_media_db_load`` 恢复媒体库，\ ``esp_playlist_load`` 恢复播放列表 JSON
   * - NVS / 网络下发列表
     - ``esp_playlist_export_ram`` 得到 JSON 后自行存储；恢复时用 ``esp_playlist_import_ram``
   * - 按名称或 URL 筛选
     - 构造 :cpp:type:`esp_media_filter_t`\ ，配合 ``ESP_MEDIA_MATCH_EXACT``\ 、\ ``ESP_MEDIA_MATCH_CONTAINS``\ 、\ ``ESP_MEDIA_MATCH_PREFIX`` 匹配方式

FAQ
------

**Q1：媒体库和播放列表 JSON 有什么区别？**

媒体库（文件系统存储时为 ``storage_path`` 下的数据库文件）保存扫描或手动添加得到的 ``name`` / ``url`` 条目集合，记录设备上有哪些媒体文件。播放列表 JSON 只保存一个播放列表的名称、顺序和条目，记录以什么顺序播放，两者是不同的持久化对象。

**Q2：只加载 playlist.json，还需要媒体库吗？**

不需要。JSON 条目本身已包含 ``name`` 和 ``url``\ ，加载后可直接通过 ``curr`` / ``next`` / ``prev`` / ``get_info`` 取出 URL 播放，这类条目属于 inline 条目，不回查媒体库。

**Q3：能否按专辑或歌手导入？**

当前公开接口不会从媒体文件内解析 ``album``\ 、\ ``artist`` 等元数据。若媒体库条目中已包含这类扩展字段，可以通过 :cpp:type:`esp_media_filter_t` 匹配对应字段实现筛选。

API 参考
----------

.. include-build-file:: inc/esp_playlist.inc

.. include-build-file:: inc/esp_media_db.inc

.. include-build-file:: inc/esp_media_db_types.inc
