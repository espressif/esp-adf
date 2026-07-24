# 音视频录制与实时显示

- [English Version](./README.md)
- 例程难度：⭐⭐⭐

## 例程简介

本例程演示一个带本地屏幕预览的交互式音视频录制应用。例程可采集摄像头画面和音频输入，在 LCD 上持续实时显示画面，并在用户点击屏幕上的录制按钮时开始或停止将音视频内容录制为 MP4 文件并保存到 microSD 卡。

- 上电后持续显示 LCD 实时预览
- 屏幕叠加录制或停止按钮和录制计时
- microSD 卡按 1 分钟分片保存录制生成的 MP4 文件

### 典型场景

- 带本地预览的监控摄像头和视频门铃

### 运行机制

```mermaid
flowchart LR
  CAM[Camera] --> VSRC[esp_capture video source]
  MIC[Audio ADC] --> ASRC[esp_capture audio source]
  VSRC --> REC[Record sink]
  ASRC --> REC
  VSRC --> DISP[Display sink RGB565]
  REC --> VENC[Video encoder MJPEG]
  REC --> AENC[Audio encoder AAC]
  VENC --> MUX[MP4 muxer]
  AENC --> MUX
  MUX --> SD["/sdcard/audio_video_record_<session>_<slice>.mp4"]
  DISP --> LCD[LCD live display]
  TOUCH[Touch panel] --> LCD
```

录制链路使用一个输出通道进行音视频编码、MP4 封装和写卡，显示链路使用另一个输出通道获取视频帧、绘制轻量级录制界面并刷新 LCD。例程将录制相关线程与显示任务分别绑定到不同 CPU 核，降低实时显示对录制链路的影响。

例程按芯片提供不同默认录像参数，当前支持 `ESP32-S3`、`ESP32-S31` 和 `ESP32-P4` 三个平台。

### 源码结构

本例程应用源码位于 `main/`，按功能拆分为多个文件；默认参数集中在 `av_rec_config.h`。

调用关系：`app_main` → `av_rec_board` → `av_rec_capture`（`sink0` MP4 录制控制）→ `av_rec_display`（`sink1` LCD 显示、触摸处理与界面绘制）。

```text
av_record_live_display/
├── main/
│   ├── app_main.c                 # 入口：app_main，启动交互式实时显示会话
│   ├── av_rec_config.h            # 宏、sys 类型、模块对外 API
│   ├── av_rec_board.c             # 板级设备初始化、LCD 和触摸设置、P4 显示完成回调
│   ├── av_rec_capture.c           # esp_capture、sink、MP4 分片、录制 start/stop、线程核绑定
│   └── av_rec_display.c           # LCD 实时取帧、居中显示、触摸 UI 与 FPS 统计
├── sdkconfig.defaults             # 通用 sdkconfig
├── sdkconfig.defaults.esp32s3     # S3 摄像头/PSRAM 等
├── sdkconfig.defaults.esp32p4     # P4 摄像头/PSRAM 等
├── partitions.csv
└── pytest_av_record_live_display.py
```

| 文件 | 职责 |
|------|------|
| `app_main.c` | 注册编解码器与 muxer，并启动交互式实时显示会话 |
| `av_rec_board.c` | 初始化 `display_lcd`、`lcd_touch`、`fs_sdcard`、`camera`、`audio_adc` 等 |
| `av_rec_capture.c` | 创建音视频源，配置 record/display sink 与 MP4 muxer，提供录制 start/stop 与分片文件路径 |
| `av_rec_display.c` | 在独立任务中从 display sink 取帧、绘制界面并处理触摸 |
| `av_rec_config.h` | 分辨率、码率、分片等可调宏（见下文「项目配置」） |

## 环境配置

### 硬件要求

- Camera
- Audio ADC 或麦克风
- LCD 模块
- microSD 卡

### 默认 IDF 分支

本例程支持 IDF release/v5.4 (>= v5.4.3) 与 release/v5.5 (>= v5.5.2) 分支。

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境；若已配置可跳过本段，直接进入工程目录。若未配置，请在 ESP-IDF 根目录运行以下脚本完成环境设置，完整步骤请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/index.html)。

```bash
./install.sh
. ./export.sh
```

下面是简略步骤：

- 进入本例程工程目录：

```bash
cd adf_examples/recorder/av_record_live_display
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

为获得最佳性能，本工程为部分开发板提供了板级配置覆盖。选择开发板时，请通过 -c 指定 overlay 根目录，Board Manager 将根据开发板名称自动发现并应用对应的 amend 配置：

```bash
idf.py bmgr -b esp32_p4_function_ev_board -c board_overlays
```

首次执行 `idf.py bmgr` 时，组件会根据本工程 `main/idf_component.yml` 中声明的 `espressif/esp_board_manager` 依赖自动下载，并在 `components/gen_bmgr_codes/` 下生成板级代码。

> [!NOTE]
> 如果切换开发板，请重新执行 `idf.py bmgr -b <board_name|index>`，必要时 `idf.py fullclean` 后再编译。
> 所选板型须包含 `camera`、`audio_adc`、`display_lcd`、`fs_sdcard` 等设备，否则例程无法正常运行。
> 自定义开发板请参考 [创建开发板指南](https://docs.espressif.com/projects/esp-board-manager/zh_CN/latest/create-board/index.html)。
> `esp_board_manager` 更多信息请参考 [ESP_BOARD_MANAGER 入门指南](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README_CN.md)。

### 项目配置

可调宏定义见「源码结构」中的 `main/av_rec_config.h`。本例程可通过以下宏调整默认行为：

- `DEFAULT_SLICE_DURATION_MS`：MP4 文件分片时长
- `FILE_RAM_CACHE_SIZE`：MP4 muxer 写卡 RAM 缓存大小，默认 `8 * 1024`
- `RECORD_WIDTH`、`RECORD_HEIGHT`、`RECORD_FPS`：录像参数
- `DISPLAY_FPS`：LCD 实时显示帧率
- `RECORD_BITRATE`：录像码率
- `REC_AUDIO_SAMPLE_RATE`、`REC_AUDIO_CHANNEL`、`REC_AUDIO_BITS`：音频录制参数

录像参数默认按芯片区分：

- `ESP32-S3`：`320x240`，`14 fps`
- `ESP32-S31`：`640x480`，`25 fps`
- `ESP32-P4`：`1024x600`，`30 fps`

当 LCD 分辨率大于摄像头画面时，预览图像会居中显示。例如 `ESP32-S31-Korvo-1` 会在 `800x480` 屏幕上水平居中显示 `640x480` 预览画面，而 `ESP32-P4` 的采集和显示均为 `1024x600`，无需额外居中处理。

上述默认值在选择开发板后生效；更换开发板后请重新执行 `idf.py bmgr -b <board_name>` 并完整编译。

### 编译与烧录

- 编译示例程序

```bash
idf.py build
```

- 烧录程序并运行 monitor 工具来查看串口输出（将 `PORT` 替换为端口名称）：

```bash
idf.py -p PORT flash monitor
```

- 退出调试界面使用 `Ctrl-]`

## 如何使用例程

### 功能和用法

- 上电后例程初始化摄像头、音频输入、LCD 和 SD 卡等板级资源。
- LCD 会持续显示摄像头画面，并通过软件方式叠加一个简单的录制界面，无需使用 LVGL。
- 点击预览画面底部中央的圆形按钮开始录制，再次点击可停止录制。
- 录制过程中，左上角红色标签显示当前录制时长，格式为 `MM:SS`。
- 录制文件保存为 `/sdcard/audio_video_record_<session>_<slice>.mp4`，默认每 1 分钟分片一次。
- 屏幕上的计时和按钮仅显示在 LCD 预览上，不会写入录制生成的 MP4 文件。

### 日志输出

正常流程依次为设备初始化、启动采集、界面就绪以及持续实时显示。关键日志包括 `[ 1 ]`～`[ 4 ]`、`Interactive UI ready`、`Recording started`、`Recording stopped`、`Record file` 和 `Display fps` 等。以下为代表性日志片段：

```text
I (27) boot: ESP-IDF v5.5.3-dirty 2nd stage bootloader
I (28) boot: compile time Jul 28 2026 14:42:48
I (28) boot: Multicore bootloader
I (29) boot: chip revision: v3.2
I (31) boot: efuse block revision: v1.2
I (35) qio_mode: Enabling default flash chip QIO
I (39) boot.esp32p4: SPI Speed      : 80MHz
I (43) boot.esp32p4: SPI Mode       : QIO
I (46) boot.esp32p4: SPI Flash Size : 16MB
I (50) boot: Enabling RNG early entropy source...
I (55) boot: Partition Table:
I (57) boot: ## Label            Usage          Type ST Offset   Length
I (64) boot:  0 nvs              WiFi data        01 02 0000b000 00004000
I (70) boot:  1 factory          factory app      00 00 00010000 00300000
I (77) boot: End of partition table
I (80) esp_image: segment 0: paddr=00010020 vaddr=401d0020 size=86cd4h (552148) map
I (145) esp_image: segment 1: paddr=00096cfc vaddr=30100000 size=00144h (   324) load
I (147) esp_image: segment 2: paddr=00096e48 vaddr=30100150 size=00048h (    72) load
I (150) esp_image: segment 3: paddr=00096e98 vaddr=4ff20000 size=09180h ( 37248) load
I (162) esp_image: segment 4: paddr=000a0020 vaddr=40000020 size=1c0768h (1836904) map
I (355) esp_image: segment 5: paddr=00260790 vaddr=4ff29180 size=106e0h ( 67296) load
I (365) esp_image: segment 6: paddr=00270e78 vaddr=4ff39880 size=05834h ( 22580) load
I (369) esp_image: segment 7: paddr=002766b4 vaddr=50108080 size=00020h (    32) load
I (375) boot: Loaded app from partition at offset 0x10000
I (376) boot: Disabling RNG early entropy source...
I (391) hex_psram: vendor id    : 0x0d (AP)
I (392) hex_psram: Latency      : 0x01 (Fixed)
I (392) hex_psram: DriveStr.    : 0x00 (25 Ohm)
I (393) hex_psram: dev id       : 0x03 (generation 4)
I (397) hex_psram: density      : 0x07 (256 Mbit)
I (402) hex_psram: good-die     : 0x06 (Pass)
I (406) hex_psram: SRF          : 0x02 (Slow Refresh)
I (410) hex_psram: BurstType    : 0x00 ( Wrap)
I (415) hex_psram: BurstLen     : 0x03 (2048 Byte)
I (419) hex_psram: BitMode      : 0x01 (X16 Mode)
I (423) hex_psram: Readlatency  : 0x06 (18 cycles@Fixed)
I (429) hex_psram: DriveStrength: 0x00 (1/1)
I (433) MSPI Timing: Enter psram timing tuning
I (589) esp_psram: Found 32MB PSRAM device
I (589) esp_psram: Speed: 250MHz
I (590) hex_psram: psram CS IO is dedicated
I (590) cpu_start: Multicore app
I (1152) esp_psram: SPI SRAM memory test OK
I (1161) cpu_start: GPIO 38 and 37 are used as console UART I/O pins
I (1162) cpu_start: Pro cpu start user code
I (1162) cpu_start: cpu freq: 400000000 Hz
I (1164) app_init: Application information:
I (1168) app_init: Project name:     av_record_live_display
I (1173) app_init: App version:      v2.7-258-g6dd6a9e88-dirty
I (1179) app_init: Compile time:     Jul 28 2026 14:42:39
I (1184) app_init: ELF file SHA256:  56a606c8a...
I (1188) app_init: ESP-IDF:          v5.5.3-dirty
I (1193) efuse_init: Min chip rev:     v3.1
I (1196) efuse_init: Max chip rev:     v3.99
I (1201) efuse_init: Chip rev:         v3.2
I (1204) heap_init: Initializing. RAM available for dynamic allocation:
I (1211) heap_init: At 4FF41E10 len 000791B0 (484 KiB): RETENT_RAM
I (1217) heap_init: At 4FFBAFC0 len 00004BF0 (18 KiB): RAM
I (1222) heap_init: At 501080A0 len 00007F60 (31 KiB): RTCRAM
I (1227) heap_init: At 30100198 len 00001E68 (7 KiB): TCM
I (1233) esp_psram: Adding pool of 32768K of PSRAM memory to heap allocator
I (1240) spi_flash: detected chip: gd
I (1243) spi_flash: flash io: qio
I (1246) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (1252) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (1259) main_task: Started on CPU0
I (1263) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1270) main_task: Calling app_main()
I (1274) PERIPH_LDO: LDO initialize success
I (1277) AV_REC_BOARD: [ 1 ] Initialize display, storage, camera and audio ADC
I (1284) DEV_DISPLAY_LCD: Initializing LCD display: display_lcd, chip: ek79007, sub_type: dsi
I (1293) DEV_DISPLAY_LCD_SUB_DSI: Initializing DSI LCD display: display_lcd, chip: ek79007
I (1301) BOARD_PERIPH: Reuse periph: ldo_mipi, ref_count=2
I (1307) PERIPH_DSI: MIPI DSI bus initialize success
I (1310) ek79007: version: 1.0.4
E (1482) lcd_panel: esp_lcd_panel_swap_xy(50): swap_xy is not supported by this panel
W (1482) DEV_DISPLAY_LCD: Failed to swap LCD panel XY: ESP_ERR_NOT_SUPPORTED
E (1485) lcd_panel: esp_lcd_panel_disp_on_off(71): disp_on_off is not supported by this panel
I (1493) DEV_DISPLAY_LCD: Successfully initialized LCD display: display_lcd (sub_type: dsi), panel: 0x4ffbb5c8, io: 0x4ffbb57c
I (1505) BOARD_MANAGER: Device display_lcd initialized
I (1510) PERIPH_I2C: I2C master bus initialized successfully
I (1515) GT911: I2C address initialization procedure skipped - using default GT9xx setup
I (1524) GT911: TouchPad_ID:0x39,0x31,0x31
I (1526) GT911: TouchPad_Config_Version:89
I (1530) DEV_LCD_TOUCH_SUB_I2C: Successfully initialized LCD touch: lcd_touch, addr: 0xba, touch:0x4825a30c, io:0x4ffbbb70
I (1541) DEV_LCD_TOUCH: Successfully initialized LCD touch: lcd_touch, chip: gt911, sub_type: i2c
I (1550) BOARD_MANAGER: Device lcd_touch initialized
W (1554) ldo: The voltage value 0 is out of the recommended range [500, 2700]
I (1561) DEV_FS_FAT_SUB_SDMMC: slot_config: cd=-1, wp=-1, clk=0, cmd=0, d0=0, d1=0, d2=0, d3=0, d4=0, d5=0, d6=0, d7=0, width=4, flags=0x1
Name: SC32G
Type: SDHC
Speed: 40.00 MHz (limit: 40.00 MHz)
Size: 30436MB
CSD: ver=2, sector_size=512, capacity=62333952 read_bl_len=9
SSR: bus_width=4
I (1764) DEV_FS_FAT: Filesystem mounted, base path: /sdcard
I (1770) BOARD_MANAGER: Device fs_sdcard initialized
I (1774) DEV_CAMERA_SUB_CSI: Initializing CSI camera...
I (1779) BOARD_PERIPH: Reuse periph: i2c_master, ref_count=2
I (1785) BOARD_PERIPH: Reuse periph: ldo_mipi, ref_count=3
I (1791) sc2336: Detected Camera sensor PID=0xcb3a
I (1868) DEV_CAMERA_SUB_CSI: CSI camera initialized successfully, dev_path: /dev/video0
I (1868) DEV_CAMERA: Successfully initialized camera device: camera, sub_type: csi, dev_path: /dev/video0
I (1874) BOARD_MANAGER: Device camera initialized
I (1879) PERIPH_I2S: I2S[0] STD, RX, ws: 10, bclk: 12, dout: 9, din: 11
I (1885) PERIPH_I2S: I2S[0] initialize success: 0x4826273c
I (1890) DEV_AUDIO_CODEC: ADC over I2S is enabled
I (1894) BOARD_PERIPH: Reuse periph: i2c_master, ref_count=3
I (1905) ES8311: Work in Slave mode
I (1908) DEV_AUDIO_CODEC: Successfully initialized codec: audio_adc
I (1909) DEV_AUDIO_CODEC: Create esp_codec_dev success, dev:0x4ffbecc4, chip:es8311
I (1916) BOARD_MANAGER: Device audio_adc initialized
I (1921) BOARD_DEVICE: Device handle audio_adc found, Handle: 0x4ffbdea8 TO: 0x4ffbdea8
I (1929) BOARD_DEVICE: Device handle display_lcd found, Handle: 0x4ffbb510 TO: 0x4ffbb510
I (1937) BOARD_DEVICE: Device display_lcd config found: 0x4020cf50 (size: 124)
I (1944) BOARD_DEVICE: Device handle lcd_touch found, Handle: 0x4ffbba20 TO: 0x4ffbba20
I (1951) AV_REC_BOARD: LCD touch ready
I (1955) AV_REC_BOARD: Display sink format=0x4c424752 size=1024x600 fps=30
I (1961) AV_REC_MAIN: [ 2 ] Register audio/video encoders and MP4 muxer
I (1968) AV_REC_MAIN: [ 3 ] Build capture system and dual sinks
I (1973) BOARD_DEVICE: Device handle audio_adc found, Handle: 0x4ffbdea8 TO: 0x4ffbdea8
I (1981) BOARD_DEVICE: Device handle camera found, Handle: 0x4ffbbfd4 TO: 0x4ffbbfd4
I (1989) VENC_EL: Create vid_enc-0x482636d8
I (1992) OVERLAY_MIXER: Create video overlay, vid_overlay-0x482637a8
I (1999) AV_REC_MAIN: [ 4 ] Start capture and interactive live display
W (2005) CAPTURE_MUXER: Muxer type 540299341 does not support streaming
I (2012) GMF_VID_PIPE: Build pipe nego for format rgb565 1024x600 30 fps
I (2018) V4L2_SRC: Success to open camera
I (2021) V4L2_SRC: Best match 1024x600
I (2025) OVERLAY_MIXER: Create video overlay, vid_overlay-0x48269c4c
I (2031) VENC_EL: Create vid_enc-0x4826a0f8
I (2035) VENC_EL: Create vid_enc-0x4826a27c
I (2039) VID_PIPE_NEGO: Start to nego for input format rgb565 1024x600 30fps
I (2045) V4L2_SRC: Best match 1024x600
I (2049) VID_PIPE_NEGO: Set path 0 in rgb565 out h264
I (2054) VID_SRC: Info 1024x600 30fps
I (2057) VIDEO_COMM: Video info for vid_ppa-0x48269f04 format:rgb565 1024x600 30fps
I (2064) VID_PIPE_NEGO: Success to negotiate 0 format:rgb565 1024x600 30fps
I (2071) VIDEO_COMM: Video info for vid_enc-0x4826a27c format:rgb565 1024x600 30fps
I (2078) VID_PIPE_NEGO: Success to negotiate 1 format:rgb565 1024x600 30fps
I (2088) VIDEO_COMM: Video info for vid_overlay-0x48269c4c format:rgb565 1024x600 30fps
I (2093) AUD_PIPE_NEGO: Negotiate return 0 src_format:541934416 sample_rate:48000

I (2100) AUD_PIPE_NEGO: Path mask 1 select sink:0 format 541278529
I (2106) AUD_SRC: Get rate:48000, ch:2, bits:16
lcd 1024x600 format 4c424752
I (2114) VIDEO_OVERLAY: add_region: overlay=0x484c5bfc rgn=0x484cae48 frame 72x72 fmt=1279412050 disp 476-512 72x72 vis=1 alpha=255
I (2124) VIDEO_CONTAINER: create container=0x484cae48 frame 72x72 fmt=1279412050 pos=476,512 with_cache=1
I (2129) VIDEO_COMM: Video info for vid_ppa-0x48269f04 format:rgb565 1024x600 30fps
I (2134) IMG_WIDGET: create img=0x484cd964 size=72x72 pos=0,0 widget.rect 0-0 72x72
I (2141) VIDEO_COMM: Video info for vid_enc-0x4826a27c format:rgb565 1024x600 30fps
I (2149) VIDEO_CONTAINER: add_widget: container=0x484cae48 widget=0x484cd964 rect 0-0 72x72 dirty 0-0 72x72 visible=1
I (2166) IMG_WIDGET: create img=0x484c5428 size=72x72 pos=0,0 widget.rect 0-0 72x72
I (2174) VIDEO_CONTAINER: add_widget: container=0x484cae48 widget=0x484c5428 rect 0-0 72x72 dirty 0-0 72x72 visible=1
I (2184) VIDEO_WIDGET: set_visible: widget=0x484cd964 visible=1 dirty 0-0 72x72
I (2191) VIDEO_WIDGET: set_visible: widget=0x484c5428 visible=0 dirty 0-0 72x72
I (2198) VIDEO_OVERLAY: add_region: overlay=0x484c5bfc rgn=0x484cd9a4 frame 104x52 fmt=1279412050 disp 8-8 104x52 vis=1 alpha=255
I (2209) VIDEO_CONTAINER: create container=0x484cd9a4 frame 104x52 fmt=1279412050 pos=8,8 with_cache=0
I (2218) IMG_WIDGET: create img=0x484cda0c size=104x52 pos=0,0 widget.rect 0-0 104x52
I (2226) VIDEO_CONTAINER: add_widget: container=0x484cd9a4 widget=0x484cda0c rect 0-0 104x52 dirty 0-0 104x52 visible=1
I (2237) VIDEO_OVERLAY: add_region: overlay=0x484c5bfc rgn=0x484cda4c frame 120x52 fmt=1279412050 disp 896-8 120x52 vis=1 alpha=255
I (2248) VIDEO_CONTAINER: create container=0x484cda4c frame 120x52 fmt=1279412050 pos=896,8 with_cache=0
I (2257) IMG_WIDGET: create img=0x484cdab4 size=120x52 pos=0,0 widget.rect 0-0 120x52
I (2265) VIDEO_CONTAINER: add_widget: container=0x484cda4c widget=0x484cdab4 rect 0-0 120x52 dirty 0-0 120x52 visible=1
I (2275) VIDEO_WIDGET: set_visible: widget=0x484cda0c visible=0 dirty 0-0 104x52
I (2283) AV_REC_DISPLAY: Interactive UI ready, touch=yes, offset=(0,0), panel=1024x600
I (2290) AV_REC_DISPLAY: Live display loop started (manual compose)
I (2296) VIDEO_RENDER: Rebuild proc ret 0
I (3116) AV_REC_DISPLAY: Display fps=24.88, frames=25, pts=966
I (4149) AV_REC_DISPLAY: Display fps=30.01, frames=56, pts=2000
I (5182) AV_REC_DISPLAY: Display fps=30.01, frames=87, pts=3033
I (6216) AV_REC_DISPLAY: Display fps=29.98, frames=118, pts=4066
I (7249) AV_REC_DISPLAY: Display fps=30.01, frames=149, pts=5100
I (7273) : ┌───────────────────┬──────────┬─────────────┬─────────┬──────────┬───────────┬────────────┬───────┐
I (7289) : │ Task              │ Core ID  │ Run Time    │ CPU     │ Priority │ Stack HWM │ State      │ Stack │
I (7300) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (7328) : │ IDLE0             │ 0        │ 980827      │  49.04% │ 0        │ 1240      │ Ready      │ Intr  │
I (7339) : │ isp_task          │ 0        │ 16696       │   0.83% │ 11       │ 1712      │ Blocked    │ Intr  │
I (7350) : │ vid_src           │ 0        │ 2133        │   0.11% │ 10       │ 1764      │ Blocked    │ Extr  │
I (7363) : │ sys_monitor       │ 0        │ 344         │   0.02% │ 1        │ 3612      │ Running    │ Extr  │
I (7373) : │ main              │ 0        │ 0           │   0.00% │ 1        │ 668       │ Suspended  │ Intr  │
I (7384) : │ ipc0              │ 0        │ 0           │   0.00% │ 24       │ 708       │ Suspended  │ Intr  │
I (7396) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (7423) : │ IDLE1             │ 1        │ 762492      │  38.13% │ 0        │ 1232      │ Ready      │ Intr  │
I (7434) : │ display           │ 1        │ 234226      │  11.71% │ 5        │ 5512      │ Blocked    │ Intr  │
I (7446) : │ venc_1            │ 1        │ 3282        │   0.16% │ 2        │ 3224      │ Blocked    │ Extr  │
I (7457) : │ ipc1              │ 1        │ 0           │   0.00% │ 24       │ 716       │ Suspended  │ Intr  │
I (7468) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (7496) : │ Tmr Svc           │ 7fffffff │ 0           │   0.00% │ 1        │ 1720      │ Blocked    │ Intr  │
I (7507) : └───────────────────┴──────────┴─────────────┴─────────┴──────────┴───────────┴────────────┴───────┘
I (7534) MONITOR: Func:sys_monitor_task, Line:25, MEM Total:28974928 Bytes, Inter:490463 Bytes, Dram:490463 Bytes

I (8283) AV_REC_DISPLAY: Display fps=30.01, frames=180, pts=6133
I (9316) AV_REC_DISPLAY: Display fps=29.98, frames=211, pts=7166
I (9583) CAPTURE_MUXER: Enter muxer thread muxing 1
I (9584) AV_REC_CAPTURE: Recording started, session=0
I (9584) VIDEO_COMM: Video info for vid_enc-0x4826a0f8 format:rgb565 1024x600 30fps
I (9584) VIDEO_WIDGET: set_visible: widget=0x484cd964 visible=0 dirty 0-0 72x72
I (9596) VIDEO_WIDGET: set_visible: widget=0x484c5428 visible=1 dirty 0-0 72x72
I (9604) VIDEO_WIDGET: set_visible: widget=0x484cda0c visible=1 dirty 0-0 104x52
I (9584) I2S_IF: Paired data: 0x4ffbfb1c, current mode: record, paired in_enable: 0, paired out_enable: 0
I (9616) BOARD_DEVICE: Device handle fs_sdcard found, Handle: 0x4ffbbbbc TO: 0x4ffbbbbc
I (9620) I2S_IF: STD: RX, data_bit: 16, slot_bit: 16, ws_width: 16, slot_mode: STEREO, slot_mask: 0x3
I (9627) AV_REC_CAPTURE: Record file: /sdcard/audio_video_record_000_000.mp4
I (9636) I2S_IF: STD: RX, sample_rate_hz: 48000, mclk_multiple: 256, clk_src: 0
I (9650) I2S_IF: STD: TX, data_bit: 16, slot_bit: 16, ws_width: 16, slot_mode: STEREO, slot_mask: 0x3
I (9659) I2S_IF: STD: TX, sample_rate_hz: 48000, mclk_multiple: 256, clk_src: 0
I (9680) Adev_Codec: Open codec device OK
I (9680) AUD_SRC: Start to fetch audio src data now
I (9692) ESP_GMF_AENC: Open, type:AAC, acquire in frame: 4096, out frame: 1736
I (10323) AV_REC_DISPLAY: Display fps=27.81, frames=239, pts=8100
I (11325) AV_REC_DISPLAY: Display fps=29.97, frames=269, pts=9100
I (12356) AV_REC_DISPLAY: Display fps=30.04, frames=300, pts=10133
I (13390) AV_REC_DISPLAY: Display fps=29.98, frames=331, pts=11166
I (13544) : ┌───────────────────┬──────────┬─────────────┬─────────┬──────────┬───────────┬────────────┬───────┐
I (13561) : │ Task              │ Core ID  │ Run Time    │ CPU     │ Priority │ Stack HWM │ State      │ Stack │
I (13572) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (13599) : │ IDLE0             │ 0        │ 830112      │  41.51% │ 0        │ 1240      │ Ready      │ Intr  │
I (13611) : │ Muxer             │ 0        │ 93562       │   4.68% │ 5        │ 2088      │ Blocked    │ Extr  │
I (13622) : │ isp_task          │ 0        │ 42555       │   2.13% │ 11       │ 1580      │ Blocked    │ Intr  │
I (13634) : │ venc_0            │ 0        │ 18294       │   0.91% │ 2        │ 1556      │ Blocked    │ Extr  │
I (13645) : │ AUD_SRC           │ 0        │ 9408        │   0.47% │ 15       │ 2328      │ Blocked    │ Extr  │
I (13657) : │ vid_src           │ 0        │ 5500        │   0.28% │ 10       │ 1764      │ Blocked    │ Extr  │
I (13668) : │ sys_monitor       │ 0        │ 569         │   0.03% │ 1        │ 1824      │ Running    │ Extr  │
I (13679) : │ main              │ 0        │ 0           │   0.00% │ 1        │ 668       │ Suspended  │ Intr  │
I (13691) : │ ipc0              │ 0        │ 0           │   0.00% │ 24       │ 708       │ Suspended  │ Intr  │
I (13702) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (13731) : │ IDLE1             │ 1        │ 479796      │  23.99% │ 0        │ 1216      │ Ready      │ Intr  │
I (13741) : │ display           │ 1        │ 288748      │  14.44% │ 5        │ 5380      │ Blocked    │ Intr  │
I (13752) : │ aenc_0            │ 1        │ 222678      │  11.13% │ 2        │ 2504      │ Blocked    │ Extr  │
I (13764) : │ venc_1            │ 1        │ 8778        │   0.44% │ 2        │ 3224      │ Blocked    │ Extr  │
I (13775) : │ ipc1              │ 1        │ 0           │   0.00% │ 24       │ 716       │ Suspended  │ Intr  │
I (13788) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (13814) : │ Tmr Svc           │ 7fffffff │ 0           │   0.00% │ 1        │ 1720      │ Blocked    │ Intr  │
I (13826) : └───────────────────┴──────────┴─────────────┴─────────┴──────────┴───────────┴────────────┴───────┘
I (13853) MONITOR: Func:sys_monitor_task, Line:25, MEM Total:25974964 Bytes, Inter:394991 Bytes, Dram:394991 Bytes

I (14423) AV_REC_DISPLAY: Display fps=30.01, frames=362, pts=12200
I (15457) AV_REC_DISPLAY: Display fps=29.98, frames=393, pts=13233
I (16490) AV_REC_DISPLAY: Display fps=30.04, frames=424, pts=14266
I (17525) AV_REC_DISPLAY: Display fps=27.05, frames=452, pts=15333
I (18557) AV_REC_DISPLAY: Display fps=30.04, frames=483, pts=16366
I (19590) AV_REC_DISPLAY: Display fps=29.98, frames=514, pts=17400
```

## 故障排除

### Board Manager / 板型配置

- 若未执行 `idf.py bmgr -b` 或缺少 `components/gen_bmgr_codes/`，编译可能失败或设备无法初始化。
- 更换开发板后请重新执行 `idf.py bmgr -b <board_name>`，必要时 `idf.py fullclean` 后再 `idf.py build`。

### LCD 无实时显示画面

- 确认执行 `idf.py bmgr -b` 时选择的板型包含 `display_lcd`（如 `esp32_s3_korvo_2_3`、`esp32_p4_function_ev_board`）。
- 确认开发板的 LCD 屏幕与摄像头可同时正常初始化。

### 未生成 MP4 文件

- 确认 microSD 卡已正确挂载并具备写权限，且需要使能长文件名支持。
- 若日志未出现 `Record file:`，请检查摄像头、Audio ADC、音视频编码器和 MP4 muxer 是否启动成功。

### MP4 文件没有音频

- 确认所选开发板支持并启用了 `AUDIO_ADC` 设备。
- 确认 `CONFIG_ESP_BOARD_DEV_AUDIO_CODEC_SUPPORT` 已启用。
- 确认日志中未出现 `Failed to create audio source` 或 `Failed to init audio ADC`。

### 视频采集启动失败

- 确认摄像头已正确连接并与所选开发板匹配。
- 对 `ESP32-S3`，建议使用仓库中已验证的 `esp32_s3_korvo_2_3` 板级配置。
- 对 `ESP32-P4`，建议使用 `esp32_p4_function_ev_board` 并确保 MIPI 相关硬件连接正常。

### 文件写入速度偏慢或录像卡顿

- 若 SD 卡写入偏慢、出现丢帧、日志中 muxer 相关告警增多，或高分辨率/高码率录制时卡顿，可适当增大 `FILE_RAM_CACHE_SIZE`，例如改为 `16 * 1024` 或 `32 * 1024`。
- 缓存越大通常越有利于平滑写卡，但会占用更多 RAM；调整后需重新编译并确认系统仍有足够可用内存。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
