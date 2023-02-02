# Record Video and Store to File
- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example demonstrates how to save video input from the camera and audio input from the microphone to a file for storage. It provides sample codes for basic usage of the `esp_muxer` module. This example records into MP4, FLV, and TS formats separately. Recorded file duration can be controlled by macro `MUXER_FILE_DURATION`. The video is in camera-encoded JPEG format, and the default resolution is HVGA. The default format for recorded audio is 16-bit, 2-channel, and little edian PCM. Both the resolution of the video and the sample rate of the audio are modifiable. Detailed codec support for `esp_muxer` is as follows:
|       |MP4|TS|FLV|
| :-----| :---- | :---- | :---- |
|PCM  |Y|N|Y|
|AAC  |Y|Y|Y|
|MP3  |Y|Y|Y|
|H264 |Y|Y|Y|
|MJPEG|Y|Y|Y|

To play the recorded video on a PC, please refer to [Example Functionality](#example-functionality).


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

The camera model tested in this example is OV3660. For other models, please refer to [esp32-camera](https://github.com/espressif/esp32-camera).

## Build and Flash


### Default IDF Branch

This example supports IDF release/v4.4 and later branches. By default, it runs on IDF release/v4.4.

### IDF Branch

- The command to switch to IDF release/v4.4 branch is as follows:

  ```
  cd $IDF_PATH
  git checkout master
  git pull
  git checkout release/v4.4
  git submodule update --init --recursive
  ```

### Configuration

The default board for this example is `ESP32-S3-Korvo-2`.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

For full steps to configure and build an ESP-IDF project, please go to [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) and select the chip and version in the upper left corner of the page.

## How to Use the Example

### Example Functionality

After the example finishes running, the recorded video (slice_0.flv, slice_0.ts) and MP4 file clips (slice_0.mp4, slice_1.mp4, etc.) can be obtained directly from the SD card. The generated MP4 files can be played on a common PC player like VLC. Since TS and FLV do not provide official support for MJPEG, files in these two formats can't be played directly using a common PC player. Please modify the source code referring to [ffmpeg_mjpeg.patch](ffmpeg_mjpeg.patch) for latest [FFmpeg](https://github.com/FFmpeg/FFmpeg) and follow [FFmpeg Compilation Guide](https://trac.ffmpeg.org/wiki/CompilationGuide) to build. Once built, use the `ffplay slice_0.flv` command line for playback. For Windows systems, you can also unzip [ffmpeg.7z](ffmpeg.7z) to get `ffplay.exe` directly.

### Example Log

A complete log is as follows:

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

## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
