# ESP Playlist Benchmark Example

- [中文版](./README_CN.md)
- Basic Example: ⭐

## Introduction

- This example benchmarks performance on an **SD card** mounted via `esp_board_manager`. Serial output reports per-operation average latency as `[perf][*_avg=...]` (microseconds).
- It uses `ESP_DB_STORAGE_FS` for the media catalog, directory scan (optional URL deduplication), playlist JSON save/load, bulk import from catalog, and navigation APIs (`next` / `prev` / `curr` / `get_info`).

Results depend on SoC, SD card speed, media file count, and log level.

### Typical Use Cases

- Measure media DB and playlist API cost on real SD storage.
- Performance regression before/after releases or refactors.
- Study how scan depth, extension filters, and catalog size affect applications.

### File Layout

```text
playlist_benchmark/
├── CMakeLists.txt
├── partitions_example.csv
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32s3
├── sdkconfig.defaults.esp32p4
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild              note: SD init via esp_board_manager
│   ├── idf_component.yml
│   ├── playlist_benchmark.c           app_main orchestration
│   ├── benchmark_common.c/.h          timing and path helpers
│   ├── media_db_benchmark.c/.h        media catalog benchmarks
│   ├── playlist_benchmark_tests.c/.h  playlist benchmarks
│   ├── startup_restore_check.c/.h     startup restore probe
│   └── storage.c/.h                   SD mount and runtime paths
├── pytest_benchmark.py
├── README.md
└── README_CN.md
```

## Environment Setup

### Hardware Requirements

- **Board**: **ESP32-P4 Function EV** (`esp32_p4_function_ev`) is recommended; any board with SD configured in `esp_board_manager` also works.
- **PSRAM**: Enabled by default in `sdkconfig.defaults.esp32p4`; keep it on for heap headroom.
- **SD card**: FAT-formatted microSD; mount point comes from the board profile (often `/sdcard`).
- **Layout** (with mount point `{mount}`):

| Purpose | Path |
|---------|------|
| Scan directory | `{mount}/music` |
| Media catalog | `{mount}/__playlist` |
| Playlist JSON | `{mount}/__playlist/playlist.json` |

### Supported IDF Version

ESP-IDF **v5.4** and later.

### Software Requirements

- Place test media under `{mount}/music` (extensions must match scan config; samples use `.mp3`, `.wav`, `.aac`; `remove` benchmark needs `.mp3` files).
- If scan fails or finds zero items, fallback network URLs are injected so playlist benchmarks can still run.

## Build and Flash

### Prerequisites

```bash
./install.sh
. ./export.sh
```

Enter the example directory:

```bash
cd YOUR_ADF_PATH/components/esp_playlist/examples/playlist_benchmark
```

Install board assist:

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist
```

List supported boards:

```bash
idf.py bmgr -l
```

Select a board (this example uses **ESP32-P4 Function EV**):

```bash
idf.py bmgr -b playlist_bench_p4_ev
```

`playlist_bench_p4_ev` (under `components/playlist_bench_board/boards/`) uses the same SDMMC settings as `esp32_p4_function_ev` but only enables `fs_sdcard`.

> [!NOTE]
> To use another board, run `idf.py bmgr -b <board_name|index>` with the same steps.
> See [ESP Board Manager](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md).

> [!IMPORTANT]
> Run `idf.py bmgr -b ...` **before** building. Do not run `idf.py set-target` alone before bmgr, or the target chip may not match the board profile.

### Project Configuration (Optional)

SD pins and power are defined by the selected board. Use `idf.py menuconfig` only if you need log level or other global options.

### Build and Flash

```bash
idf.py build
idf.py -p USB0 flash monitor
```

Replace `USB0` with your port (e.g. `/dev/ttyUSB0`). Exit monitor with `Ctrl-]`.

## How to Use

### Operation

1. Run **bmgr** board selection and insert a formatted SD card.
2. Put test files under `{mount}/music`.
3. Build, flash, and open the serial monitor.
4. The example runs automatically (see `app_main` in `playlist_benchmark.c`): SD init → startup restore probe → clean `__playlist` → media DB scan/load benchmarks → playlist benchmarks → cleanup → unmount SD → `PLAYLIST_BENCH: Playlist benchmark done`.

Each `[perf][...]` line is the **average** time per API call inside the benchmark loops.

### Log Output

- Watch TAGs `PLAYLIST_BENCH`, `STARTUP_RESTORE_CHECK`, `MEDIA_DB_BENCH`, `PLAYLIST_BENCH_TESTS`, `PLAYLIST_BENCH_COMMON`, `PLAYLIST_BENCH_STORAGE`, and lines starting with `[perf][`.
- A successful run ends with `PLAYLIST_BENCH: Playlist benchmark done` and `main_task: Returned from app_main()`.

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

### References

- Component docs: [esp_playlist README.md](../../README.md)
- API scenarios: [playlist_usage_scenarios.md](../../docs/playlist_usage_scenarios.md)

## Troubleshooting

### SD not mounted or zero scan results

Check FAT formatting, correct `idf.py bmgr -b` board, and media files under `{mount}/music`. Fallback URLs are used when scan fails or count is zero.

### Remove benchmark skipped

`Skip remove perf: no removable items collected` means no `.mp3` files were found for removal. Add `.mp3` files or ignore the warning.

### bmgr / build errors

Install `esp-bmgr-assist` and run `idf.py bmgr -b playlist_bench_p4_ev` before `idf.py build`. Re-run bmgr after changing boards.

## Technical Support

- Forum: [esp32.com](https://esp32.com/viewforum.php?f=20)
- Issues: [esp-adf issues](https://github.com/espressif/esp-adf/issues)
