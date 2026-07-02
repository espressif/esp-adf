# SD 卡音乐播放器

- [English Version](./README.md)
- 例程难度：⭐⭐

## 例程简介

本例程演示一个带触摸屏 UI 的 SD 卡音乐播放器。例程可扫描 microSD 卡中的本地音乐文件，在屏幕上显示播放器界面、歌曲信息和播放控制入口，并通过音频输出设备播放音乐。

### 典型场景

- 带触摸屏的本地音乐播放器
- SD 卡音频文件扫描与列表播放
- 屏幕播放器界面与音频播放状态联动

### 运行机制

1. `esp_board_manager` 初始化 SD 卡、音频 DAC、LCD 与触摸
2. `esp_media_db` 扫描 SD 卡挂载点 `/sdcard` 下的 `.mp3`、`.aac`、`.wav` 文件（扫描深度为 1）
3. `esp_playlist` 导入扫描结果并管理播放模式（单曲循环 / 列表循环 / 随机播放）
4. `esp_audio_simple_player` 异步解码，PCM 通过 `esp_codec_dev_write()` 直接写入 codec 输出
5. `esp_lvgl_adapter` 运行 LVGL 任务，显示 `lv_demo_music` 界面并叠加中文歌名与控制按钮

### 文件结构

```
music_player/
├── assets/                 font.ttf 可放在此目录，编译时打入 assets 分区
├── main/
│   ├── app_main.c              例程入口
│   ├── music_player_config.h   默认音量、字体路径等宏
│   ├── music_player_board.c/h   板级设备初始化
│   ├── music_player_display.c/h LVGL adapter 与显示注册
│   ├── music_player_playback.c/h 播放列表与 simple player 控制
│   ├── music_player_ui.c/h       LVGL music demo 与触摸控制
│   └── idf_component.yml
├── partitions.csv
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── pytest_music_player.py
├── README.md
└── README_CN.md
```

## 环境配置

### 硬件要求

- microSD 卡（FAT 文件系统）
- 扬声器或耳机（连接到开发板音频输出）
- LCD 屏与触摸输入设备

### 默认 IDF 分支

本例程支持 IDF release/v5.4 (>= v5.4.3) 与 release/v5.5 (>= v5.5.2) 分支。

### 软件要求

- 在 microSD 卡挂载点 `/sdcard` 下放入 `.mp3`、`.aac` 或 `.wav` 测试文件（支持中文文件名）
- 如需使用 FreeType 显示中文，将 `font.ttf` 放入 `assets/` 目录；编译时会打入 Flash 的 assets 分区（须确认字体授权可再分发）。未提供时使用内置 CJK 字体

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境；若已配置可跳过本段，直接进入工程目录。若未配置，请在 ESP-IDF 根目录运行以下脚本完成环境设置，完整步骤请根据目标芯片参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/)。

```
./install.sh
. ./export.sh
```

下面是简略步骤：

- 进入本例程工程目录：

```
cd adf_examples/player/music_player
```

本示例使用 [ESP Board Manager](https://github.com/espressif/esp-board-manager) 管理板级资源。推荐安装辅助工具 [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) 作为默认入口。

- 在已激活的 ESP-IDF Python 环境下安装（同一环境只需安装一次）：

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # 当提示需要更新时执行此命令
```

- 列出当前可见的开发板：

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

- 选择开发板：

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

### 项目配置

本例程默认行为由 `main/music_player_config.h` 与 `sdkconfig.defaults` 决定，通常无需额外 menuconfig。常用可调项如下：

- `MUSIC_PLAYER_DEFAULT_VOLUME`：默认播放音量（当前为 70）
- `MUSIC_PLAYER_FONT_PATH` / `MUSIC_PLAYER_FONT_SIZE`：FreeType 字体路径与字号（默认 `F:font.ttf`、24）
- `MUSIC_PLAYER_SCAN_DEPTH`：媒体库扫描深度（当前为 1，对应挂载点 `/sdcard`）

如需启用或关闭 FreeType、CJK 内置字体等 LVGL 相关选项，可在 `idf.py menuconfig` 中查看 `Component config` → `ESP LVGL Adapter` 与 `LVGL configuration`。

### 编译与烧录

- 编译示例程序

```
idf.py build
```

- 烧录程序并运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

- 退出调试界面使用 `Ctrl-]`

## 如何使用例程

### 功能和用法

1. 将音乐文件放入 microSD 卡挂载点 `/sdcard` 或其一层子目录（见下文 log 中的 `scan dir: /sdcard`）
2. 上电后例程自动扫描；若找到音乐则播放第一首，否则 UI 显示“未找到音乐”
3. 触摸屏底部控制栏：
   - 上一首 / 播放或暂停 / 下一首
   - 循环模式按钮：依次切换单曲循环、列表循环、随机播放
4. 顶部显示当前歌名（固定宽度省略显示，支持中文）和播放模式
5. 背景使用 LVGL 自带 `lv_demo_music` 界面，进度条为 demo 动效，不绑定真实播放进度
6. 当前曲目播放结束后自动播放下一首（按当前模式）

### 日志输出

以下为 ESP32-P4 Function EV 实机运行时的连续串口 log 片段，包含 `[ 1 ]`～`[ 5 ]` 步骤、`Scanned N tracks` 与 `Play file:` 等关键信息。不同开发板的 LCD、触摸与 codec 初始化日志会随板级设备配置变化。

```text
I (1229) main_task: Calling app_main()
I (1229) MUSIC_PLAYER: [ 1 ] Initialize board peripherals
I (1229) PERIPH_LDO: LDO initialize success
W (1229) ldo: The voltage value 0 is out of the recommended range [500, 2700]
I (1229) DEV_FS_FAT_SUB_SDMMC: slot_config: cd=-1, wp=-1, clk=0, cmd=0, d0=0, d1=0, d2=0, d3=0, d4=0, d5=0, d6=0, d7=0, width=4, flags=0x1
Name: SC32G
Type: SDHC
Speed: 40.00 MHz (limit: 40.00 MHz)
Size: 30436MB
CSD: ver=2, sector_size=512, capacity=62333952 read_bl_len=9
SSR: bus_width=4
I (1276) DEV_FS_FAT: Filesystem mounted, base path: /sdcard
I (1276) BOARD_MANAGER: Device fs_sdcard initialized
I (1277) BOARD_DEVICE: Device handle fs_sdcard found, Handle: 0x4ffbb43c TO: 0x4ffbb43c
I (1278) PERIPH_I2S: I2S[0] STD,  TX, ws: 10, bclk: 12, dout: 9, din: 11
I (1278) PERIPH_I2S: I2S[0] initialize success: 0x4800822c
I (1278) DEV_AUDIO_CODEC: DAC is ENABLED
I (1278) PERIPH_GPIO: Initialize success, pin: 53, set the default level: 0
I (1279) PERIPH_I2C: I2C master bus initialized successfully
I (1284) ES8311: Work in Slave mode
I (1287) DEV_AUDIO_CODEC: Successfully initialized codec: audio_dac
I (1287) DEV_AUDIO_CODEC: Create esp_codec_dev success, dev:0x4ffbc284, chip:es8311
I (1287) BOARD_MANAGER: Device audio_dac initialized
I (1287) BOARD_DEVICE: Device handle audio_dac found, Handle: 0x4ffbb854 TO: 0x4ffbb854
I (1288) I2S_IF: No paired data, current mode: playback
I (1288) I2S_IF: STD: TX, data_bit: 16, slot_bit: 16, ws_width: 16, slot_mode: STEREO, slot_mask: 0x3
I (1289) I2S_IF: STD: TX, sample_rate_hz: 48000, mclk_multiple: 256, clk_src: 0
I (1303) Adev_Codec: Open codec device OK
I (1303) MUSIC_PLAYER_BOARD: SD card mounted at /sdcard, scan dir: /sdcard
I (1304) MUSIC_PLAYER: [ 2 ] Initialize display and LVGL music UI
I (1304) DEV_DISPLAY_LCD: Initializing LCD display: display_lcd, chip: ek79007, sub_type: dsi
I (1304) DEV_DISPLAY_LCD_SUB_DSI: Initializing DSI LCD display: display_lcd, chip: ek79007
I (1304) BOARD_PERIPH: Reuse periph: ldo_mipi, ref_count=2
I (1306) PERIPH_DSI: MIPI DSI bus initialize success
I (1306) ek79007: version: 1.0.4
E (1467) lcd_panel: esp_lcd_panel_swap_xy(50): swap_xy is not supported by this panel
W (1467) DEV_DISPLAY_LCD: Failed to swap LCD panel XY: ESP_ERR_NOT_SUPPORTED
E (1467) lcd_panel: esp_lcd_panel_disp_on_off(71): disp_on_off is not supported by this panel
I (1467) DEV_DISPLAY_LCD: Successfully initialized LCD display: display_lcd (sub_type: dsi), panel: 0x4ffbd65c, io: 0x4ffbd610
I (1468) BOARD_MANAGER: Device display_lcd initialized
I (1468) BOARD_PERIPH: Reuse periph: i2c_master, ref_count=2
I (1468) GT911: I2C address initialization procedure skipped - using default GT9xx setup
I (1470) GT911: TouchPad_ID:0x39,0x31,0x31
I (1470) GT911: TouchPad_Config_Version:89
I (1470) DEV_LCD_TOUCH_SUB_I2C: Successfully initialized LCD touch: lcd_touch, addr: 0xba, touch:0x48134cd4, io:0x4ffbdab8
I (1470) DEV_LCD_TOUCH: Successfully initialized LCD touch: lcd_touch, chip: gt911, sub_type: i2c
I (1471) BOARD_MANAGER: Device lcd_touch initialized
I (1471) BOARD_DEVICE: Device handle display_lcd found, Handle: 0x4ffbbe14 TO: 0x4ffbbe14
I (1471) BOARD_DEVICE: Device display_lcd config found: 0x40232fe0 (size: 124)
I (1471) BOARD_DEVICE: Device handle lcd_touch found, Handle: 0x4ffbda30 TO: 0x4ffbda30
I (1472) esp_lvgl:adapter: LVGL adapter initialized successfully
I (1474) esp_lvgl:bridge_v9: Initializing hardware resources
I (1474) esp_lvgl:bridge_v9: Hardware resources initialized successfully
W (1475) esp_lvgl:touch: LV_USE_GESTURE_RECOGNITION is disabled; only single-point pointer events are available
I (1475) esp_lvgl:touch: Touch input device registered successfully (IRQ mode: disabled)
I (1476) mmap_assets: MMAP Assets [assets] mounted successfully. (Lib: v2.0.0, Bin: v1.0.0)
I (1476) lv_fs: Drive 'F' successfully created, version: 1.0.1
I (1476) esp_lvgl:adapter: File system mounted successfully
I (1476) MUSIC_PLAYER_DISPLAY: LVGL assets FS mounted as F:
I (1476) MUSIC_PLAYER_DISPLAY: Display initialized: 1024x600 (dsi)
I (1532) MUSIC_PLAYER_UI: Hidden lv_demo_music title box
I (1536) MUSIC_PLAYER_UI: Use FreeType font: F:font.ttf
I (1540) esp_lvgl:adapter: LVGL task started successfully
I (1540) MUSIC_PLAYER: [ 3 ] Scan SD card playlist from /sdcard
I (1878) MUSIC_PLAYER_PLAYBACK: Scanned 365 tracks under /sdcard
I (1890) MUSIC_PLAYER: [ 4 ] Start playback controller
I (1891) ASP_POOL: Dest rate:48000
I (1891) ASP_POOL: Dest channels:2
I (1891) ASP_POOL: Dest bits:16
I (1892) MUSIC_PLAYER: [ 5 ] Music player ready
I (1892) MUSIC_PLAYER_PLAYBACK: Play file: /sdcard/test_1.mp3
E (1892) ESP_GMF_PIPELINE: esp_gmf_pipeline.c:536 (esp_gmf_pipeline_stop): Got NULL Pointer
I (1892) AUD_SIMP_PLAYER: Use the tag of io from pool, in_str:io_file
I (1893) ESP_GMF_FILE: Open, dir:1, uri:/sdcard/test_1.mp3
I (1896) ESP_GMF_FILE: File size: 4784128 byte, file position: 0
I (1897) ESP_ES_PARSER: The version of es_parser is v1.0.1
W (1898) ESP_GMF_ASMP_DEC: Not enough memory for out, need:4608, old: 1024, new: 4608
I (7892) : ┌───────────────────┬──────────┬─────────────┬─────────┬──────────┬───────────┬────────────┬───────┐
I (7893) : │ Task              │ Core ID  │ Run Time    │ CPU     │ Priority │ Stack HWM │ State      │ Stack │
I (7893) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (7894) : │ lvgl              │ 0        │ 408646      │  20.43% │ 6        │ 4056      │ Suspended  │ Extr  │
I (7895) : │ IDLE0             │ 0        │ 304097      │  15.20% │ 0        │ 1228      │ Ready      │ Intr  │
I (7895) : │ TSK_0x4ff5bd14    │ 0        │ 271071      │  13.55% │ 10       │ 3840      │ Blocked    │ Intr  │
I (7896) : │ esp_timer         │ 0        │ 15753       │   0.79% │ 22       │ 3764      │ Suspended  │ Intr  │
I (7896) : │ main              │ 0        │ 460         │   0.02% │ 1        │ 1428      │ Running    │ Intr  │
I (7897) : │ ipc0              │ 0        │ 0           │   0.00% │ 24       │ 708       │ Suspended  │ Intr  │
I (7897) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (7898) : │ IDLE1             │ 1        │ 482368      │  24.12% │ 0        │ 1220      │ Ready      │ Intr  │
I (7898) : │ ipc1              │ 1        │ 0           │   0.00% │ 24       │ 716       │ Suspended  │ Intr  │
I (7908) : │ music_ctrl        │ 1        │ 0           │   0.00% │ 5        │ 3640      │ Blocked    │ Intr  │
I (7909) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (7910) : │ swdraw            │ 7fffffff │ 518961      │  25.95% │ 3        │ 14348     │ Ready      │ Intr  │
I (7910) : │ Tmr Svc           │ 7fffffff │ 0           │   0.00% │ 1        │ 1724      │ Blocked    │ Intr  │
I (7911) : └───────────────────┴──────────┴─────────────┴─────────┴──────────┴───────────┴────────────┴───────┘
```

## 故障排除

### 未找到音乐文件

如果日志提示扫描失败或 `No music found`，请确认 microSD 卡已正确挂载，且 `/sdcard` 下存在 `.mp3`、`.aac` 或 `.wav` 文件。

### 播放失败

如果日志出现 `Failed to run player` 或 `Failed to open on read`，请检查文件是否损坏、格式是否受支持，以及 SD 卡接触是否良好。

### 中文显示异常

若 assets 分区未包含 `font.ttf`，例程会回退到内置 CJK 字体。如需更好的中文显示，请在编译前将 `font.ttf` 放入 `assets/` 目录后重新编译并烧录。

## 参考资料

- [ESP Audio Simple Player](https://components.espressif.com/components/espressif/esp_audio_simple_player)
- [ESP Board Manager](https://github.com/espressif/esp-board-manager)
- [ESP LVGL Adapter](https://components.espressif.com/components/espressif/esp_lvgl_adapter)

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
