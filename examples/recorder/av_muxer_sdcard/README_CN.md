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
I (0) cpu_start: App cpu up.
I (796) spiram: SPI SRAM memory test OK
I (804) cpu_start: Pro cpu start user code
I (805) cpu_start: cpu freq: 240000000
I (805) cpu_start: Application information:
I (807) cpu_start: Project name:     av_muxer
I (812) cpu_start: App version:      v2.4-209-g96e88ce1-dirty
I (819) cpu_start: Compile time:     Feb  7 2023 14:49:43
I (825) cpu_start: ELF file SHA256:  8bd4cf3b5a2d517c...
I (831) cpu_start: ESP-IDF:          v4.4-355-g29f424cff8-dirty
I (837) heap_init: Initializing. RAM available for dynamic allocation:
I (845) heap_init: At 3FCA3DA8 len 0003C258 (240 KiB): D/IRAM
I (851) heap_init: At 3FCE0000 len 0000EE34 (59 KiB): STACK/DRAM
I (858) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (864) heap_init: At 600FE000 len 00002000 (8 KiB): RTCRAM
I (870) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (878) spi_flash: detected chip: gd
I (882) spi_flash: flash io: dio
W (886) spi_flash: Detected size(16384k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (910) sleep: Configure to isolate all GPIO pins in sleep state
I (917) sleep: Enable automatic switching of GPIO sleep configuration
I (935) cpu_start: Starting scheduler on PRO CPU.
I (951) cpu_start: Starting scheduler on APP CPU.
I (971) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1001) SDCARD: Using 1-line SD mode,  base path=/sdcard
I (1021) gpio: GPIO[15]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (1021) gpio: GPIO[7]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (1031) gpio: GPIO[4]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0 
I (1231) SDCARD: CID name SC16G!
I (1541) AV Muxer: Start muxer to mp4
I (1541) AV Muxer: Start to open muxer
I (1551) DRV8311: ES8311 in Slave mode
I (1561) gpio: GPIO[48]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
W (1571) I2C_BUS: i2c_bus_create:58: I2C bus has been already created, [port:0]
I (1581) ES7210: ES7210 in Slave mode
I (1581) ES7210: Enable ES7210_INPUT_MIC1
I (1591) ES7210: Enable ES7210_INPUT_MIC2
I (1591) ES7210: Enable ES7210_INPUT_MIC3
W (1591) ES7210: Enable TDM mode. ES7210_SDP_INTERFACE2_REG12: 2
I (1601) ES7210: config fmt 60
I (1611) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (1631) HAL: Channel style mode 13 samplerate 16000 comformat 1 channelfmt 0 bits 16 chb 16 active 2 total 2
W (1641) I2S: APLL not supported on current chip, use I2S_CLK_D2CLK as default clock source
I (1651) HAL: Channel style mode 13 samplerate 16000 comformat 1 channelfmt 0 bits 16 chb 16 active 2 total 2
I (1661) I2S: CFG TX sclk 160000000 mclk 4096000 bclk 512000 mclkdiv 39 bclkdiv 8
I (1671) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1671) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
E (1681) I2S: pinset ws:45 bck:9 out:8 in:10 mck:16
I (1691) I2S: I2S0, MCLK output by GPIO16
I (1691) ESP32_S3_KORVO_2: I2S0, MCLK output by GPIO0
I (1701) AUDIO_ELEMENT: [iis-0x3d83973c] Element task created
I (1701) AUDIO_ELEMENT: [iis] AEL_MSG_CMD_RESUME,state:1
I (1711) s3 ll_cam: DMA Channel=4
I (1711) cam_hal: cam init ok
I (1741) camera: Detected camera at address=0x3c
I (1741) camera: Detected OV3660 camera
I (1741) camera: Camera PID=0x3660 VER=0x00 MIDL=0x00 MIDH=0x00
I (2041) s3 ll_cam: node_size: 3840, nodes_per_line: 1, lines_per_node: 6
I (2041) s3 ll_cam: dma_half_buffer_min:  3840, dma_half_buffer: 15360, lines_per_half_buffer: 24, dma_buffer_size: 30720
I (2041) cam_hal: buffer_size: 30720, half_buffer_size: 15360, node_buffer_size: 3840, node_cnt: 8, total_cnt: 10
I (2051) cam_hal: Allocating 153600 Byte frame buffer in PSRAM
I (2061) cam_hal: Allocating 153600 Byte frame buffer in PSRAM
I (2091) cam_hal: cam config ok
I (2111) ov3660: Calculated VCO: 320000000 Hz, PLLCLK: 160000000 Hz, SYSCLK: 40000000 Hz, PCLK: 20000000 Hz
I (2121) AV Record: Start record OK
I (2121) AV Muxer: Start muxer success
I (4321) AV Record: s:153600 fps:13 vpts:2000 apts:2048 q:1/2905
I (6301) AV Record: s:153600 fps:15 vpts:4000 apts:4032 q:1/3001

```

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
