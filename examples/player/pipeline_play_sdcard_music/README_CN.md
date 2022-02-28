# 播放 microSD card 中不同格式的音乐

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程介绍了使用 FatFs 元素读取 microSD 卡中不同格式的音乐，然后经过 decoder 元素解码，解码后数据用 I2S 元素输出音乐。

本例支持 MP3、OPUS、OGG、FLAC、AAC、M4A、TS、MP4、AMRNB、AMRWB 音频格式，默认选择 MP3 音乐格式。

例程中引用的音源可以通过 [音频样例或短样例](https://docs.espressif.com/projects/esp-adf/zh-CN/latest/design-guide/audio-samples.html#short-samples) 来获取并下载到 microSD 卡中。

下表列出本例程支持的音乐格式：

|序号|音乐格式|文件名|
|-|-|-|
|1|MP3|test.mp3|
|2|AMRNB|test.amr|
|3|AMRWB|test.Wamr|
|4|OPUS|test.opus|
|5|FLAC|test.flac|
|6|WAV|test.wav|
|7|AAC|test.aac|
|8|M4A|test.m4a|
|9|TS|test.ts|
|10|MP4|test.mp4|

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

## 编译和下载


### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程需要准备一张 microSD 卡，下载 [音频样例或短样例](https://docs.espressif.com/projects/esp-adf/zh-CN/latest/design-guide/audio-samples.html#short-samples) 立体声音源到 microSD 卡中，当然也可以是用户自备的音源。

> 本例中需要播放的文件名是固定的，以 `test` 开头，以格式名后缀结尾。例如 `test.mp3` 这样的文件命名。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程同时需要打开 FatFs 长文件名支持。

```
menuconfig > Component config > FAT Filesystem support > Long filename support
```


### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程


### 功能和用法

- 例程开始运行后，自动播放 microSD 卡中的音乐文件，打印如下：

```c
I (29) SDCARD_MUSIC_EXAMPLE: [ 1 ] Mount sdcard
I (529) SDCARD_MUSIC_EXAMPLE: [ 2 ] Start codec chip
E (529) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (549) SDCARD_MUSIC_EXAMPLE: [3.0] Create audio pipeline for playback
I (549) SDCARD_MUSIC_EXAMPLE: [3.1] Create fatfs stream to read data from sdcard
I (559) SDCARD_MUSIC_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (569) SDCARD_MUSIC_EXAMPLE: [3.3] Create mp3 decoder
I (569) SDCARD_MUSIC_EXAMPLE: [3.4] Register all elements to audio pipeline
I (579) SDCARD_MUSIC_EXAMPLE: [3.5] Link it together [sdcard]-->fatfs_stream-->music_decoder-->i2s_stream-->[codec_chip]
I (589) SDCARD_MUSIC_EXAMPLE: [3.6] Set up uri: /sdcard/test.mp3
I (599) SDCARD_MUSIC_EXAMPLE: [ 4 ] Set up  event listener
I (599) SDCARD_MUSIC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (609) SDCARD_MUSIC_EXAMPLE: [4.2] Listening event from peripherals
I (619) SDCARD_MUSIC_EXAMPLE: [ 5 ] Start audio_pipeline
I (629) SDCARD_MUSIC_EXAMPLE: [ 6 ] Listen for all pipeline events
I (639) SDCARD_MUSIC_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (15869) FATFS_STREAM: No more data, ret:0
W (16579) SDCARD_MUSIC_EXAMPLE: [ * ] Stop event received
I (16579) SDCARD_MUSIC_EXAMPLE: [ 7 ] Stop audio_pipeline
E (16579) AUDIO_ELEMENT: [file] Element already stopped
E (16589) AUDIO_ELEMENT: [dec] Element already stopped
E (16599) AUDIO_ELEMENT: [i2s] Element already stopped
W (16599) AUDIO_PIPELINE: There are no listener registered
W (16609) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (16619) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (16619) AUDIO_ELEMENT: [dec] Element has not create when AUDIO_ELEMENT_TERMINATE
```


### 日志输出

以下为本例程的完整日志。

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0018,len:4
load:0x3fff001c,len:6840
load:0x40078000,len:12072
load:0x40080400,len:6708
entry 0x40080778
I (73) boot: Chip Revision: 3
I (73) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) boot: ESP-IDF v3.3.2-107-g722043f73 2nd stage bootloader
I (40) boot: compile time 17:56:08
I (40) boot: Enabling RNG early entropy source...
I (46) boot: SPI Speed      : 40MHz
I (50) boot: SPI Mode       : DIO
I (54) boot: SPI Flash Size : 8MB
I (58) boot: Partition Table:
I (62) boot: ## Label            Usage          Type ST Offset   Length
I (69) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (84) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (103) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x18028 ( 98344) map
I (146) esp_image: segment 1: paddr=0x00028050 vaddr=0x3ffb0000 size=0x01f64 (  8036) load
I (150) esp_image: segment 2: paddr=0x00029fbc vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (153) esp_image: segment 3: paddr=0x0002a3c4 vaddr=0x40080400 size=0x05c4c ( 23628) load
I (172) esp_image: segment 4: paddr=0x00030018 vaddr=0x400d0018 size=0x2f65c (194140) map
0x400d0018: _flash_cache_start at ??:?

I (240) esp_image: segment 5: paddr=0x0005f67c vaddr=0x4008604c size=0x05940 ( 22848) load
0x4008604c: prvReceiveGeneric at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp_ringbuf/ringbuf.c:969

I (257) boot: Loaded app from partition at offset 0x10000
I (257) boot: Disabling RNG early entropy source...
I (257) cpu_start: Pro cpu up.
I (261) cpu_start: Application information:
I (266) cpu_start: Project name:     play_sdcard_music
I (272) cpu_start: App version:      v2.2-103-g33721b98-dirty
I (278) cpu_start: Compile time:     Apr 27 2021 19:37:04
I (284) cpu_start: ELF file SHA256:  5d6c86d684a92743...
I (290) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (296) cpu_start: Starting app cpu, entry point is 0x40081200
0x40081200: call_start_cpu1 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (307) heap_init: Initializing. RAM available for dynamic allocation:
I (314) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (320) heap_init: At 3FFB30C0 len 0002CF40 (179 KiB): DRAM
I (326) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (332) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (339) heap_init: At 4008B98C len 00014674 (81 KiB): IRAM
I (345) cpu_start: Pro cpu start user code
I (27) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (29) SDCARD_MUSIC_EXAMPLE: [ 1 ] Mount sdcard
I (529) SDCARD_MUSIC_EXAMPLE: [ 2 ] Start codec chip
E (529) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (549) SDCARD_MUSIC_EXAMPLE: [3.0] Create audio pipeline for playback
I (549) SDCARD_MUSIC_EXAMPLE: [3.1] Create fatfs stream to read data from sdcard
I (559) SDCARD_MUSIC_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (569) SDCARD_MUSIC_EXAMPLE: [3.3] Create mp3 decoder
I (569) SDCARD_MUSIC_EXAMPLE: [3.4] Register all elements to audio pipeline
I (579) SDCARD_MUSIC_EXAMPLE: [3.5] Link it together [sdcard]-->fatfs_stream-->music_decoder-->i2s_stream-->[codec_chip]
I (589) SDCARD_MUSIC_EXAMPLE: [3.6] Set up uri: /sdcard/test.mp3
I (599) SDCARD_MUSIC_EXAMPLE: [ 4 ] Set up  event listener
I (599) SDCARD_MUSIC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (609) SDCARD_MUSIC_EXAMPLE: [4.2] Listening event from peripherals
I (619) SDCARD_MUSIC_EXAMPLE: [ 5 ] Start audio_pipeline
I (629) SDCARD_MUSIC_EXAMPLE: [ 6 ] Listen for all pipeline events
I (639) SDCARD_MUSIC_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (15869) FATFS_STREAM: No more data, ret:0
W (16579) SDCARD_MUSIC_EXAMPLE: [ * ] Stop event received
I (16579) SDCARD_MUSIC_EXAMPLE: [ 7 ] Stop audio_pipeline
E (16579) AUDIO_ELEMENT: [file] Element already stopped
E (16589) AUDIO_ELEMENT: [dec] Element already stopped
E (16599) AUDIO_ELEMENT: [i2s] Element already stopped
W (16599) AUDIO_PIPELINE: There are no listener registered
W (16609) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (16619) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (16619) AUDIO_ELEMENT: [dec] Element has not create when AUDIO_ELEMENT_TERMINATE
```


## 故障排除

如果您的日志有如下的错误提示，这是因为在 microSD 卡 中没有找到需要播放的音频文件，请按照上面的 *配置* 一节操作重命名文件。

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
