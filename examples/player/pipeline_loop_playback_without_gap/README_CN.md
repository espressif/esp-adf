# 无间隙循环播放 SDCard 音频

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_regular.png "中级")


## 例程简介

本例程介绍了使用两路 Pipeline 解码从 microSD 卡中读取名为 “test” 的音乐，并实现无间隙循环播放。这两路 Pipeline 可以解码相同格式音频，也可以解码不同格式音频。本例支持 MP3、WAV、AAC 音频格式，默认选择 MP3 音乐格式。

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

## 编译和下载


### IDF 默认分支

本例程支持 IDF release/v4.2 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

准备一张 microSD 卡，将 [音频样例](https://docs.espressif.com/projects/esp-adf/zh_CN/latest/design-guide/audio-samples.html#short-samples) 立体声音源下载至该 microSD 卡中。用户也可以选择自备音源。

> 本例程默认使用固定名称为 `test` 的音频文件，即 `test.mp3`。请注意修改文件名称为`test`。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程同时需要打开 FatFs 长文件名支持。

```
menuconfig > Component config > FAT Filesystem support > Long filename support
```


### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)，并在页面左上角选择芯片和版本，查看对应的文档。


## 如何使用例程


### 功能和用法

- 例程开始运行后，自动播放 microSD 卡中的音乐文件，打印如下：

```c
I (452) LOOP_PLAYBACK: [ 1 ] Mount sdcard
I (962) LOOP_PLAYBACK: [ 2 ] Start codec chip
E (962) gpio: gpio_install_isr_service(449): GPIO isr service already installed
I (982) LOOP_PLAYBACK: [ 3 ] Create audio pipeline for playback
I (982) LOOP_PLAYBACK:        - Create fatfs_stream_reader[0] to read data from sdcard
I (992) LOOP_PLAYBACK:        - Create music_decoder[0]
I (992) LOOP_PLAYBACK:        - Create fatfs_stream_reader[1] to read data from sdcard
I (1002) LOOP_PLAYBACK:        - Create music_decoder[1]
I (1012) LOOP_PLAYBACK:        - Create i2s_stream_writer to write data to codec chip
I (1022) LOOP_PLAYBACK: [ 4 ] Set up event listener
I (1022) LOOP_PLAYBACK:        - Listening event from all elements of pipeline
I (1032) LOOP_PLAYBACK:        - Listening event from peripherals
I (1042) LOOP_PLAYBACK: [ 5 ] Create the ringbuf and connect decoder to i2s
I (1042) LOOP_PLAYBACK: [ 6 ] Start pipeline_input and i2s_stream_writer
W (1052) AUDIO_THREAD: Make sure selected the `CONFIG_SPIRAM_BOOT_INIT` and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` by `make menuconfig`
I (1072) LOOP_PLAYBACK: [ 7 ] Listen for all pipeline events
I (1082) LOOP_PLAYBACK: [ * ] Receive music info from music_decoder[0], sample_rates=48000, bits=16, ch=2
W (11022) FATFS_STREAM: No more data, ret:0
I (11062) LOOP_PLAYBACK: [ * ] The Pipeline[0] has been finished. Switch to another one
W (11062) AUDIO_THREAD: Make sure selected the `CONFIG_SPIRAM_BOOT_INIT` and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` by `make menuconfig`
W (21012) FATFS_STREAM: No more data, ret:0
I (21052) LOOP_PLAYBACK: [ * ] The Pipeline[1] has been finished. Switch to another one
W (31012) FATFS_STREAM: No more data, ret:0
```


### 日志输出

以下为本例程的完整日志。

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:6624
load:0x40078000,len:15048
load:0x40080400,len:3816
0x40080400: _init at ??:?

entry 0x40080698
I (27) boot: ESP-IDF v4.4.4-278-g3c8bc2213c 2nd stage bootloader
I (27) boot: compile time 20:13:51
I (27) boot: chip revision: v3.0
I (32) boot.esp32: SPI Speed      : 40MHz
I (36) boot.esp32: SPI Mode       : DIO
I (41) boot.esp32: SPI Flash Size : 2MB
I (46) boot: Enabling RNG early entropy source...
I (51) boot: Partition Table:
I (54) boot: ## Label            Usage          Type ST Offset   Length
I (62) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (69) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (77) boot:  2 factory          factory app      00 00 00010000 00100000
I (84) boot: End of partition table
I (88) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=3d460h (250976) map
I (188) esp_image: segment 1: paddr=0004d488 vaddr=3ffb0000 size=01d98h (  7576) load
I (191) esp_image: segment 2: paddr=0004f228 vaddr=40080000 size=00df0h (  3568) load
I (195) esp_image: segment 3: paddr=00050020 vaddr=400d0020 size=301f8h (197112) map
I (273) esp_image: segment 4: paddr=00080220 vaddr=40080df0 size=0eaf0h ( 60144) load
I (306) boot: Loaded app from partition at offset 0x10000
I (306) boot: Disabling RNG early entropy source...
I (317) cpu_start: Pro cpu up.
I (318) cpu_start: Starting app cpu, entry point is 0x400813ac
0x400813ac: call_start_cpu1 at /home/gaowei/esp/esp-adf-github/esp-idf/components/esp_system/port/cpu_start.c:148

I (0) cpu_start: App cpu up.
I (332) cpu_start: Pro cpu start user code
I (332) cpu_start: cpu freq: 160000000
I (332) cpu_start: Application information:
I (336) cpu_start: Project name:     play_sdcard_music
I (342) cpu_start: App version:      v2.5-34-gf5c1ea15-dirty
I (348) cpu_start: Compile time:     Jun  8 2023 21:02:44
I (354) cpu_start: ELF file SHA256:  79581932f13f507a...
I (360) cpu_start: ESP-IDF:          v4.4.4-278-g3c8bc2213c
I (367) cpu_start: Min chip rev:     v0.0
I (371) cpu_start: Max chip rev:     v3.99 
I (376) cpu_start: Chip rev:         v3.0
I (381) heap_init: Initializing. RAM available for dynamic allocation:
I (388) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (394) heap_init: At 3FFB2900 len 0002D700 (181 KiB): DRAM
I (400) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (407) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (413) heap_init: At 4008F8E0 len 00010720 (65 KiB): IRAM
I (421) spi_flash: detected chip: gd
I (424) spi_flash: flash io: dio
W (428) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (442) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (452) LOOP_PLAYBACK: [ 1 ] Mount sdcard
I (962) LOOP_PLAYBACK: [ 2 ] Start codec chip
E (962) gpio: gpio_install_isr_service(449): GPIO isr service already installed
I (982) LOOP_PLAYBACK: [ 3 ] Create audio pipeline for playback
I (982) LOOP_PLAYBACK:        - Create fatfs_stream_reader[0] to read data from sdcard
I (992) LOOP_PLAYBACK:        - Create music_decoder[0]
I (992) LOOP_PLAYBACK:        - Create fatfs_stream_reader[1] to read data from sdcard
I (1002) LOOP_PLAYBACK:        - Create music_decoder[1]
I (1012) LOOP_PLAYBACK:        - Create i2s_stream_writer to write data to codec chip
I (1022) LOOP_PLAYBACK: [ 4 ] Set up event listener
I (1022) LOOP_PLAYBACK:        - Listening event from all elements of pipeline
I (1032) LOOP_PLAYBACK:        - Listening event from peripherals
I (1042) LOOP_PLAYBACK: [ 5 ] Create the ringbuf and connect decoder to i2s
I (1042) LOOP_PLAYBACK: [ 6 ] Start pipeline_input and i2s_stream_writer
W (1052) AUDIO_THREAD: Make sure selected the `CONFIG_SPIRAM_BOOT_INIT` and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` by `make menuconfig`
I (1072) LOOP_PLAYBACK: [ 7 ] Listen for all pipeline events
I (1082) LOOP_PLAYBACK: [ * ] Receive music info from music_decoder[0], sample_rates=48000, bits=16, ch=2
W (11022) FATFS_STREAM: No more data, ret:0
I (11062) LOOP_PLAYBACK: [ * ] The Pipeline[0] has been finished. Switch to another one
W (11062) AUDIO_THREAD: Make sure selected the `CONFIG_SPIRAM_BOOT_INIT` and `CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY` by `make menuconfig`
W (21012) FATFS_STREAM: No more data, ret:0
I (21052) LOOP_PLAYBACK: [ * ] The Pipeline[1] has been finished. Switch to another one
W (31012) FATFS_STREAM: No more data, ret:0
I (31062) LOOP_PLAYBACK: [ * ] The Pipeline[0] has been finished. Switch to another one
W (41022) FATFS_STREAM: No more data, ret:0
I (41062) LOOP_PLAYBACK: [ * ] The Pipeline[1] has been finished. Switch to another one
```


## 故障排除

如果您的日志打印如下错误提示，则表明在 microSD 卡中没有找到需要播放的音频文件。请参照 *配置* 小节重命名文件。

```c
I (608) SDCARD_MUSIC_EXAMPLE: [4.2] Listening event from peripherals
I (618) SDCARD_MUSIC_EXAMPLE: [ 5 ] Start audio_pipeline
E (628) FATFS_STREAM: Failed to open. File name: /sdcard/gs-16b-2c-44100hz.mp3, error message: No such file or directory, line: 116
E (638) AUDIO_ELEMENT: [file] AEL_STATUS_ERROR_OPEN,-1
W (638) AUDIO_ELEMENT: [file] audio_element_on_cmd_error,7
W (648) AUDIO_ELEMENT: IN-[dec] AEL_IO_ABORT
E (648) MP3_DECODER: failed to read audio data (line 118)
W (658) AUDIO_ELEMENT: [dec] AEL_IO_ABORT, -3
W (658) AUDIO_ELEMENT: IN-[i2s] AEL_IO_ABORT
```

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
