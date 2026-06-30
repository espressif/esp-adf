# ESP Playlist 性能压测例程

- [English Version](./README.md)
- 例程难度：⭐

## 例程简介

- 本例程在 **SD 卡**（经 `esp_board_manager` 挂载）上完成性能压测，串口输出各操作的单次平均耗时，格式为 `[perf][*_avg=...]`（微秒）。
- 例程使用 `ESP_DB_STORAGE_FS` 媒体库、目录扫描（可开关 URL 去重）、播放列表 JSON 的保存与加载、从媒体库批量导入，以及 `next` / `prev` / `curr` / `get_info` 等接口。

压测结果与 SoC、SD 卡速度、媒体文件数量及日志级别有关。

### 典型场景

- 评估媒体库与播放列表在真实 SD 存储上的开销。
- 版本发布或重构前后做性能回归对比。
- 评估扫描深度、扩展名过滤、媒体库规模对实际应用的影响。

### 文件结构

```text
playlist_benchmark/
├── CMakeLists.txt
├── partitions_example.csv
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32s3
├── sdkconfig.defaults.esp32p4
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild              说明：SD 由 esp_board_manager 初始化
│   ├── idf_component.yml
│   ├── playlist_benchmark.c           app_main，压测流程编排
│   ├── benchmark_common.c/.h          计时与路径辅助
│   ├── media_db_benchmark.c/.h        媒体库压测项
│   ├── playlist_benchmark_tests.c/.h  播放列表压测项
│   ├── startup_restore_check.c/.h     启动时恢复检查（压测前探测已有 DB/JSON）
│   └── storage.c/.h                   SD 挂载与运行时路径
├── pytest_benchmark.py
├── README.md
└── README_CN.md
```

## 环境配置

### 硬件要求

- **开发板**：推荐 **ESP32-P4 Function EV**（`esp32_p4_function_ev_board`），亦支持其他已在 `esp_board_manager` 中配置 SD 卡的板型。
- **PSRAM**：`sdkconfig.defaults.esp32p4` 默认开启 SPIRAM，建议保持开启。
- **SD 卡**：FAT 格式 microSD；挂载点由板级配置决定（常见为 `/sdcard`）。
- **目录布局**（以挂载点 `{mount}` 为例）：

| 用途 | 路径 |
|------|------|
| 扫描目录 | `{mount}/music` |
| 媒体库 | `{mount}/__playlist` |
| 播放列表 JSON | `{mount}/__playlist/playlist.json` |

### 默认 IDF 分支

本例程支持 **ESP-IDF v5.4** 及以上版本。

### 软件要求

- 在 `{mount}/music` 下放置若干媒体文件（扩展名需与扫描配置一致，示例常用 `.mp3`、`.wav`、`.aac`；`remove` 压测需至少若干 `.mp3`）。
- 扫描失败或条目为 0 时会注入 fallback 网络 URL 条目，仍可继续播放列表压测。

## 编译和下载

### 编译准备

编译前请先完成 ESP-IDF 环境配置；若已配置可跳过。

```bash
./install.sh
. ./export.sh
```

进入本例程目录：

```bash
cd YOUR_ADF_PATH/components/esp_playlist/examples/playlist_benchmark
```

本示例使用 [ESP Board Manager](https://github.com/espressif/esp-board-manager) 管理板级资源。推荐安装辅助工具 [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) 作为默认入口。

在已激活的 ESP-IDF Python 环境下安装（同一环境只需安装一次）：

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # 当提示需要更新时执行此命令
```

列出当前可见的开发板：

```bash
idf.py bmgr -l
```

输出示例：

```text
ℹ️  Board Components:
  espressif/esp_boards:
    [1] esp32_c3_lyra
    [2] esp32_lyrat_4_3
    [3] esp32_lyrat_mini_1_1
    [4] esp32_p4_eye
    [5] esp32_p4_function_ev_board
    [6] esp32_s31_function_coreboard_1
    [7] esp32_s31_korvo_1
    [8] esp32_s3_box_3
    [9] esp32_s3_box_lite
    [10] esp32_s3_korvo_2_3
    [11] esp32_s3_lcd_ev_board
    [12] esp_vocat_1_0
    [13] esp_vocat_1_2
```

以上输出示例基于 `esp_boards` 0.5.2 的开发板列表和排序。不同 `esp_boards` 版本或自定义开发板依赖可能会使列表和序号变化，使用时以 `idf.py bmgr -l` 的实际输出为准。

选择开发板：

```bash
idf.py bmgr -b <board_index|board_name>
```

例如选择 `esp32_p4_function_ev_board`：

```bash
idf.py bmgr -b 5
# 或
idf.py bmgr -b esp32_p4_function_ev_board
```

首次执行 `idf.py bmgr` 时，组件会根据本工程 `main/idf_component.yml` 中声明的 `espressif/esp_board_manager` 依赖自动下载。

> [!NOTE]
> 如果切换为其他 `esp_board_manager` 支持的开发板，请按相同步骤执行并替换板型名称/索引。
> 自定义开发板请参考 [创建开发板指南](https://docs.espressif.com/projects/esp-board-manager/zh_CN/latest/create-board/index.html)。
> `esp_board_manager` 更多信息请参考 [ESP_BOARD_MANAGER 入门指南](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README_CN.md)

> [!IMPORTANT]
> 请先执行 `idf.py bmgr -b ...` 生成板级代码，**不要**在 bmgr 之前单独 `idf.py set-target`，以免与板级目标芯片不一致。

### 项目配置（可选）

一般无需修改 menuconfig；SD 引脚与供电由所选板型决定。若需调整日志级别等，可运行 `idf.py menuconfig`。

### 编译与烧录

```bash
idf.py build
idf.py -p USB0 flash monitor
```

（将 `USB0` 替换为实际串口，如 `/dev/ttyUSB0`。）

退出 monitor：`Ctrl-]`

## 如何使用例程

### 功能和用法

1. 完成上文 **bmgr 选板** 并插入已格式化的 SD 卡。
2. 在 `{mount}/music` 放入测试媒体文件。
3. 编译、烧录并打开串口 monitor。
4. 例程自动执行以下流程（见 `playlist_benchmark.c` 的 `app_main`）：
   1. **存储初始化**：`esp_board_manager` 挂载 SD，并生成 `{mount}/music`、`{mount}/__playlist` 等路径。
   2. **启动恢复检查**（`startup_restore_check.c`）：在清理 DB 目录**之前**探测已有媒体库或 `playlist.json`。
   3. **清理并重建**：删除 `__playlist` 下旧 DB/JSON，创建空目录后开始压测。
   4. **媒体库（扫描阶段）**：冷扫描 → 去重关闭的二次扫描 → `deinit`。
   5. **媒体库（缓存阶段）**：`load` → `get_count` 循环。
   6. **播放列表**：`import_media` → DB 导航 → `save` / `clean` / `load` JSON → inline 导航 → 两次 `import_media`（追加）→ `get_count` 循环。
   7. **媒体库（清理阶段）**：`clean` → `load` → 可选 `remove`（需 `.mp3`）→ 再次 `load`。
   8. **收尾**：打印 `__playlist` 下文件列表 → 卸载 SD；`PLAYLIST_BENCH: Playlist benchmark done`。

串口中每条 `[perf][...]` 表示对应 API 在压测循环内的**平均耗时**。

### 日志输出

- 关注 TAG `PLAYLIST_BENCH`、`STARTUP_RESTORE_CHECK`、`MEDIA_DB_BENCH`、`PLAYLIST_BENCH_TESTS`、`PLAYLIST_BENCH_COMMON`、`PLAYLIST_BENCH_STORAGE`，以及以 `[perf][` 开头的行。
- 完整运行应以 `PLAYLIST_BENCH: Playlist benchmark done` 及 `main_task: Returned from app_main()` 结束。

```text
I (1522) main_task: Calling app_main()
W (1522) ldo: The voltage value 0 is out of the recommended range [500, 2700]
I (1532) DEV_FS_FAT_SUB_SDMMC: slot_config: cd=-1, wp=-1, clk=0, cmd=0, d0=0, d1=0, d2=0, d3=0, d4=0, d5=0, d6=0, d7=0, width=4, flags=0x1
Name: SC32G
Type: SDHC
Speed: 40.00 MHz (limit: 40.00 MHz)
Size: 30436MB
CSD: ver=2, sector_size=512, capacity=62333952 read_bl_len=9
SSR: bus_width=4
I (1712) DEV_FS_FAT: Filesystem mounted, base path: /sdcard
I (1722) BOARD_MANAGER: Device fs_sdcard initialized
I (1722) BOARD_DEVICE: Device handle fs_sdcard found, Handle: 0x4ff3b32c TO: 0x4ff3b32c
I (1732) PLAYLIST_BENCH_STORAGE: SD card mounted at /sdcard (scan=/sdcard/music, db=/sdcard/__playlist)
I (1742) PLAYLIST_BENCH: benchmark log warmup
I (1822) MEDIA_LIB: Media catalog 'esp' loaded successfully.
I (1822) STARTUP_RESTORE_CHECK: Existing media DB found, count=1000
I (1992) STARTUP_RESTORE_CHECK: media_db[0]: playlist=media_probe name=test000 url=file://sdcard/music/test000.aac
I (1992) STARTUP_RESTORE_CHECK: media_db[1]: playlist=media_probe name=test001 url=file://sdcard/music/test001.aac
I (2002) STARTUP_RESTORE_CHECK: media_db[2]: playlist=media_probe name=test002 url=file://sdcard/music/test002.aac
I (2012) STARTUP_RESTORE_CHECK: media_db[3]: playlist=media_probe name=test003 url=file://sdcard/music/test003.aac
I (2022) STARTUP_RESTORE_CHECK: media_db[4]: playlist=media_probe name=test004 url=file://sdcard/music/test004.aac
I (2152) STARTUP_RESTORE_CHECK: Existing playlist found, count=1000
I (2152) STARTUP_RESTORE_CHECK: playlist[0]: playlist=playlist_probe name=test000 url=file://sdcard/music/test000.aac
I (2162) STARTUP_RESTORE_CHECK: playlist[1]: playlist=playlist_probe name=test001 url=file://sdcard/music/test001.aac
I (2172) STARTUP_RESTORE_CHECK: playlist[2]: playlist=playlist_probe name=test002 url=file://sdcard/music/test002.aac
I (2182) STARTUP_RESTORE_CHECK: playlist[3]: playlist=playlist_probe name=test003 url=file://sdcard/music/test003.aac
I (2192) STARTUP_RESTORE_CHECK: playlist[4]: playlist=playlist_probe name=test004 url=file://sdcard/music/test004.aac
I (2202) PLAYLIST_BENCH: cleanup old DB and rebuild
I (2892) MEDIA_DB_BENCH: [perf][media_db_scan_cold_no_dup] path=/sdcard/music items=1000 avg=664.71 us/item
I (2892) PLAYLIST_BENCH: Media count=1000
I (3272) MEDIA_DB_BENCH: [perf][media_db_scan_duplicate] items=1000 avg=374.82 us/item count=1000
I (3352) MEDIA_LIB: Media catalog 'esp' loaded successfully.
I (3352) MEDIA_DB_BENCH: [perf][media_db_load] items=1000 avg=66.34 us/item
I (3352) MEDIA_DB_BENCH: [perf][media_db_get_count_loop] loops=1000 success=1000 avg=0.26 us count=1000
I (3542) PLAYLIST_BENCH_TESTS: [perf][playlist_import_media] items=1000 avg=176.76 us/item
I (3702) PLAYLIST_BENCH_TESTS: [perf][playlist_next_db] loops=1000 success=1000 avg=156.71 us
I (3902) PLAYLIST_BENCH_TESTS: [perf][playlist_prev_db] loops=1000 success=1000 avg=205.23 us
I (4062) PLAYLIST_BENCH_TESTS: [perf][playlist_get_info_db] loops=1000 success=1000 avg=155.39 us
I (4062) PLAYLIST_BENCH_TESTS: [perf][playlist_curr_db] loops=1000 success=1000 avg=0.70 us
I (4432) PLAYLIST_BENCH_TESTS: [perf][playlist_save_db] items=1000 bytes=73045 avg=366.81 us/item
I (4432) PLAYLIST_BENCH_TESTS: [perf][playlist_clean_db] items=1000 avg=0.12 us/item
I (4542) PLAYLIST_BENCH_TESTS: [perf][playlist_load_json] items=1000 bytes=73045 avg=105.63 us/item
I (4542) PLAYLIST_BENCH_TESTS: [perf][playlist_next_inline] loops=1000 success=1000 avg=2.53 us
I (4552) PLAYLIST_BENCH_TESTS: [perf][playlist_prev_inline] loops=1000 success=1000 avg=2.49 us
I (4562) PLAYLIST_BENCH_TESTS: [perf][playlist_get_info_inline] loops=1000 success=1000 avg=1.80 us
I (4572) PLAYLIST_BENCH_TESTS: [perf][playlist_curr_inline] loops=1000 success=1000 avg=0.70 us
I (4582) PLAYLIST_BENCH_TESTS: [perf][playlist_clean_inline] items=1000 avg=4.37 us/item
I (4932) PLAYLIST_BENCH_TESTS: [perf][playlist_import_media_append] items=1000 avg=174.19 us/item count=2000
I (4932) PLAYLIST_BENCH_TESTS: [perf][playlist_get_count] loops=1000 success=1000 avg=0.04 us count=2000
I (4962) MEDIA_DB_BENCH: [perf][media_db_clean] avg=16391 us
I (5052) MEDIA_LIB: Media catalog 'esp' loaded successfully.
I (5052) MEDIA_DB_BENCH: [perf][media_db_load_after_clean] items=1000 avg=94.34 us/item
W (5102) MEDIA_DB_BENCH: Skip remove perf: no removable items collected
I (5182) MEDIA_LIB: Media catalog 'esp' loaded successfully.
I (5182) MEDIA_DB_BENCH: [perf][media_db_load] items=1000 avg=68.77 us/item
I (5202) PLAYLIST_BENCH_COMMON: DB files under /sdcard/__playlist:
I (5202) PLAYLIST_BENCH_COMMON:   esp_mtb.db size=43
I (5202) PLAYLIST_BENCH_COMMON:   esp_mof.db size=21008
I (5202) PLAYLIST_BENCH_COMMON:   esp_mda.db size=40008
I (5212) PLAYLIST_BENCH_COMMON:   playlist.json size=73045
I (5212) BOARD_DEVICE: Deinit device fs_sdcard ref_count: 0 device_handle:0x4ff3b32c
I (5222) BOARD_DEVICE: Device fs_sdcard config found: 0x400ae74c (size: 84)
I (5232) DEV_FS_FAT: Sub device 'sdmmc' deinitialized successfully
I (5232) BOARD_MANAGER: Device fs_sdcard deinitialized
I (5242) PLAYLIST_BENCH_STORAGE: SD card deinitialized
I (5242) PLAYLIST_BENCH: Playlist benchmark done
I (5252) main_task: Returned from app_main()
```

### 参考文献

- 组件说明：[esp_playlist README_CN.md](../../README_CN.md)
- API 场景说明：[playlist_usage_scenarios.md](../../docs/playlist_usage_scenarios.md)

## 故障排除

### SD 卡未挂载或扫描条目为 0

检查：SD 是否 FAT、板型是否与硬件一致（`idf.py bmgr -b`）、`{mount}/music` 下是否有匹配扩展名的文件。扫描失败或条目为 0 时会走 fallback URL，冷扫描 `[perf]` 行可能不出现。

### remove 压测被跳过

若出现 `Skip remove perf: no removable items collected`，说明扫描目录下未收集到 `.mp3`。可改用 `.mp3` 测试文件，或忽略该警告。

### bmgr / 编译错误

确认已安装 `esp-bmgr-assist` 且已执行 `idf.py bmgr -b esp32_p4_function_ev_board`。若更换板型，请重新执行 bmgr 后再 `idf.py build`。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求，请创建 [esp-adf issues](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
