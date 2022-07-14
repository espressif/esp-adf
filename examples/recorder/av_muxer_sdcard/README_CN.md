# 录制视频并存储到文件
- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")

## 例程简介

本例程将摄像头输入的视频以及麦克风输入的音频保存到文件中存储，以演示 `esp_muxer` 模块的基础用法。例程中分别录制了 MP4、FLV、TS 三种视频，每种格式总时长由 `MUXER_FILE_DURATION` 指定。视频采用摄像头编码的 JPEG 格式，分辨率默认为 HVGA。音频为小端字节序 PCM 格式，16 比特位宽，2 声道。视频的分辨率和音频的采样率均可以修改。`esp_muxer` 支持的音视频编码格式见下表：
|       |MP4|TS|FLV|
| :-----| :---- | :---- | :---- |
|PCM  |Y|N|Y|
|AAC  |Y|Y|Y|
|MP3  |Y|Y|Y|
|H264 |Y|Y|Y|
|MJPEG|Y|Y|Y|

如需在 PC 上播放录制的视频，请参考 [功能和用法](#功能和用法)。


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中 [例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性) 中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

本例程测试的摄像头型号为 OV3660，其他型号请参考 [esp32-camera](https://github.com/espressif/esp32-camera)。

## 编译和下载


### IDF 默认分支

本例程支持 IDF release/v4.4 及以后的分支，例程默认使用 IDF release/v4.4 分支。

### IDF 分支

IDF release/v4.4 分支切换命令如下：

  ```
  cd $IDF_PATH
  git checkout master
  git pull
  git checkout release/v4.4
  git submodule update --init --recursive
  ```

### 配置

本例程默认选择的开发板是 `ESP32-S3-Korvo-2`。

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)，并在页面左上角选择芯片和版本，查看对应的文档。

## 如何使用例程

### 功能和用法

例程运行结束后，可以直接从 SD 卡获取录制的视频文件 (slice_0.flv, slice_0.ts) 以及 MP4 文件片段 (slice_0.mp4, slice_1.mp4 等)。录制的 MP4 文件可用 PC 播放器播放，如 VLC。由于 TS 和 FLV 尚无对 MJPEG 格式的官方支持，无法在 PC 上使用通用播放器播放。如需播放上述两种文件，请导入 [ffmpeg_mjpeg.patch](ffmpeg_mjpeg.patch) 修改 [FFmpeg](https://github.com/FFmpeg/FFmpeg) 源码，参考 [FFmpeg 官方编译文档](https://trac.ffmpeg.org/wiki/CompilationGuide) 进行编译。编译后，可使用命令 `ffplay slice_0.flv` 播放文件。如使用 Windows 平台，可以直接解压 [ffmpeg_win.7z](ffmpeg_win.7z)，使用编译好的 `ffplay.exe`。

### 日志输出

以下是本例程的完整日志。

```c
I (24) boot: ESP-IDF v4.4-353-gb2cf3d6edc-dirty 2nd stage bootloader
I (25) boot: compile time 12:09:17
I (25) boot: chip revision: 0
I (28) boot.esp32s3: Boot SPI Speed : 80MHz
I (33) boot.esp32s3: SPI Mode       : DIO
I (38) boot.esp32s3: SPI Flash Size : 2MB
I (42) boot: Enabling RNG early entropy source...
I (48) boot: Partition Table:
I (51) boot: ## Label            Usage          Type ST Offset   Length
I (59) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (66) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (73) boot:  2 factory          factory app      00 00 00010000 00100000
I (81) boot: End of partition table
I (85) esp_image: segment 0: paddr=00010020 vaddr=3c080020 size=1e484h (124036) map
I (116) esp_image: segment 1: paddr=0002e4ac vaddr=3fc950a0 size=01b6ch (  7020) load
I (118) esp_image: segment 2: paddr=00030020 vaddr=42000020 size=74d54h (478548) map
I (207) esp_image: segment 3: paddr=000a4d7c vaddr=3fc96c0c size=02190h (  8592) load
I (209) esp_image: segment 4: paddr=000a6f14 vaddr=40374000 size=11094h ( 69780) load
I (228) esp_image: segment 5: paddr=000b7fb0 vaddr=50000000 size=00010h (    16) load
I (236) boot: Loaded app from partition at offset 0x10000
I (236) boot: Disabling RNG early entropy source...
I (249) opi psram: vendor id : 0x0d (AP)
I (249) opi psram: dev id    : 0x02 (generation 3)
I (249) opi psram: density   : 0x03 (64 Mbit)
I (253) opi psram: good-die  : 0x01 (Pass)
I (257) opi psram: Latency   : 0x01 (Fixed)
I (262) opi psram: VCC       : 0x01 (3V)
I (267) opi psram: SRF       : 0x01 (Fast Refresh)
I (272) opi psram: BurstType : 0x01 (Hybrid Wrap)
I (278) opi psram: BurstLen  : 0x01 (32 Byte)
I (283) opi psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (289) opi psram: DriveStrength: 0x00 (1/1)
I (294) spiram: Found 64MBit SPI RAM device
I (298) spiram: SPI RAM mode: sram 40m
I (303) spiram: PSRAM initialized, cache is in normal (1-core) mode.
I (310) cpu_start: Pro cpu up.
I (314) cpu_start: Starting app cpu, entry point is 0x40375494
I (0) cpu_start: App cpu up.
I (1051) spiram: SPI SRAM memory test OK
I (1059) cpu_start: Pro cpu start user code
I (1059) cpu_start: cpu freq: 240000000
I (1060) cpu_start: Application information:
I (1063) cpu_start: Project name:     av_muxer
I (1068) cpu_start: App version:      v2.4-41-g0a31df6d-dirty
I (1074) cpu_start: Compile time:     Jun 22 2022 14:44:23
I (1080) cpu_start: ELF file SHA256:  92ff848c8a861751...
I (1086) cpu_start: ESP-IDF:          v4.4-353-gb2cf3d6edc-dirty
I (1093) heap_init: Initializing. RAM available for dynamic allocation:
I (1100) heap_init: At 3FC9AF40 len 000450C0 (276 KiB): D/IRAM
I (1107) heap_init: At 3FCE0000 len 0000EE34 (59 KiB): STACK/DRAM
I (1114) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (1120) heap_init: At 600FE000 len 00002000 (8 KiB): RTCRAM
I (1126) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (1135) spi_flash: detected chip: gd
I (1139) spi_flash: flash io: dio
W (1143) spi_flash: Detected size(16384k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1156) sleep: Configure to isolate all GPIO pins in sleep state
I (1163) sleep: Enable automatic switching of GPIO sleep configuration
I (1170) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1180) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1190) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1200) SDCARD: Using 1-line SD mode
I (1210) gpio: GPIO[15]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (1220) gpio: GPIO[7]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (1220) gpio: GPIO[4]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (1730) AV Muxer: Start muxer to mp4
I (1730) AV Muxer: Start to open mux
I (1730) AV Muxer: Got video width:480 height:320
I (1760) s3 ll_cam: DMA Channel=4
I (1760) cam_hal: cam init ok
I (1760) sccb: pin_sda 17 pin_scl 18
I (1790) camera: Detected camera at address=0x3c
I (1790) camera: Detected OV3660 camera
I (1790) camera: Camera PID=0x3660 VER=0x00 MIDL=0x00 MIDH=0x00
I (2090) cam_hal: buffer_size: 16384, half_buffer_size: 1024, node_buffer_size: 1024, node_cnt: 16, total_cnt: 30
I (2090) cam_hal: Allocating 30720 Byte frame buffer in PSRAM
I (2090) cam_hal: Allocating 30720 Byte frame buffer in PSRAM
I (2100) cam_hal: cam config ok
I (2110) ov3660: Calculated VCO: 200000000 Hz, PLLCLK: 200000000 Hz, SYSCLK: 50000000 Hz, PCLK: 10000000 Hz
I (2140) DRV8311: ES8311 in Slave mode
I (2150) gpio: GPIO[48]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
W (2150) I2C_BUS: i2c_bus_create:58: I2C bus has been already created, [port:0]
I (2160) ES7210: ES7210 in Slave mode
I (2170) ES7210: Enable ES7210_INPUT_MIC1
I (2170) ES7210: Enable ES7210_INPUT_MIC2
I (2180) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (2190) HAL: Channel style mode 13 samplerate 22050 comformat 1 channelfmt 0 bits 16 chb 16 active 2 total 2
I (2190) HAL: Channel style mode 13 samplerate 22050 comformat 1 channelfmt 0 bits 16 chb 16 active 2 total 2
I (2200) I2S: CFG TX sclk 160000000 mclk 5644800 bclk 705600 mclkdiv 28 bclkdiv 8
I (2210) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2210) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2220) I2S: I2S0, MCLK output by GPIO16
I (2220) ESP32_S3_KORVO_2: I2S0, MCLK output by GPIO0
I (2230) AUDIO_ELEMENT: [iis-0x3d910754] Element task created
I (2240) AUDIO_ELEMENT: [iis] AEL_MSG_CMD_RESUME,state:1
I (2240) AV Muxer: Start record return 0
I (2250) AV Muxer: Start Record success
I (2260) AV Muxer: Pic 5842 vpts:0 apts:0 real:0 1/5846 drop:0
I (2290) AV Muxer: Pic 6755 vpts:0 apts:0 real:0 5/14665 drop:0

```

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
