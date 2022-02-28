# 使用播放列表播放 microSD 卡中 MP3 文件例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

该演示使用音频管道接口播放存储在 microSD 卡上的 MP3 文件。例程首先扫描 microSD 卡中的 MP3 文件，并把扫描结果以播放列 (playlist) 的形式保存在 microSD 卡中。

用户可以开始、停止、暂停、恢复播放和前进到下一首歌曲以及调节音量。播放时，上一音乐文件播放完毕后，应用程序会自动前进到下一首歌曲。


本例程的管道如下图：

```
[sdcard] ---> fatfs_stream ---> mp3_decoder ---> resample ---> i2s_stream ---> [codec_chip]
```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程需要准备一张 microSD 卡并且拷贝一些 MP3 文件到 microSD 中备用。

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

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。

## 如何使用例程

### 功能和用法

- 例程开始运行后，自动扫描 microSD 卡中的 MP3 音乐文件，以播放列表 (playlist) 的形式保存在 microSD 卡中，等待用户按键播放，打印如下：

```c
I (0) cpu_start: App cpu up.
I (398) heap_init: Initializing. RAM available for dynamic allocation:
I (405) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (411) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (417) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (424) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (430) heap_init: At 4008E128 len 00011ED8 (71 KiB): IRAM
I (436) cpu_start: Pro cpu start user code
I (454) spi_flash: detected chip: gd
I (455) spi_flash: flash io: dio
W (455) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (465) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (476) SDCARD_MP3_CONTROL_EXAMPLE: [1.0] Initialize peripherals management
I (486) SDCARD_MP3_CONTROL_EXAMPLE: [1.1] Initialize and start peripherals
W (516) PERIPH_TOUCH: _touch_init
I (996) SDCARD_MP3_CONTROL_EXAMPLE: [1.2] Set up a sdcard playlist and scan sdcard music save to it
I (1776) SDCARD_MP3_CONTROL_EXAMPLE: [ 2 ] Start codec chip
E (1776) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [ 3 ] Create and start input key service
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [4.0] Create audio pipeline for playback
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [4.1] Create i2s stream to write data to codec chip
I (1856) SDCARD_MP3_CONTROL_EXAMPLE: [4.2] Create mp3 decoder to decode mp3 file
I (1856) SDCARD_MP3_CONTROL_EXAMPLE: [4.3] Create resample filter
I (1866) SDCARD_MP3_CONTROL_EXAMPLE: [4.4] Create fatfs stream to read data from sdcard
I (1886) SDCARD_MP3_CONTROL_EXAMPLE: [4.5] Register all elements to audio pipeline
I (1886) SDCARD_MP3_CONTROL_EXAMPLE: [4.6] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->resample-->i2s_stream-->[codec_chip]
I (1896) SDCARD_MP3_CONTROL_EXAMPLE: [5.0] Set up  event listener
I (1896) SDCARD_MP3_CONTROL_EXAMPLE: [5.1] Listen for all pipeline events
W (1906) SDCARD_MP3_CONTROL_EXAMPLE: [ 6 ] Press the keys to control music player:
W (1916) SDCARD_MP3_CONTROL_EXAMPLE:       [Play] to start, pause and resume, [Set] next song.
W (1936) SDCARD_MP3_CONTROL_EXAMPLE:       [Vol-] or [Vol+] to adjust volume.

```

- 按下 [Play] 键开始播放，再次按下 [Play] 键则是暂停播放。按下 [Set] 键是播放下一首，同时可以使用 [Vol-] 和 [Vol+] 减小和增加音量。

```c
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 3
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Play] input key event
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Starting audio pipeline
I (15206) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 2
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Set] input key event
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Stopped, advancing to the next song
W (27376) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (27376) AUDIO_ELEMENT: OUT-[filter] AEL_IO_ABORT
W (27386) MP3_DECODER: output aborted -3
W (27376) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (27396) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/DU1906_蓝牙_测试音频.mp3
I (27526) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=48000, bits=16, ch=2
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 2
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Set] input key event
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Stopped, advancing to the next song
W (37126) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (37126) AUDIO_ELEMENT: OUT-[filter] AEL_IO_ABORT
W (37136) MP3_DECODER: output aborted -3
W (37136) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (37146) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/test_output.mp3
I (37186) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 6
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Vol+] input key event
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Volume set to 79 %
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 6
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Vol+] input key event
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Volume set to 88 %
I (199216) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 4
I (200776) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 4
W (222766) FATFS_STREAM: No more data, ret:0
I (223486) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (223486) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/start_service.mp3
I (223616) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=16000, bits=16, ch=1
W (223626) FATFS_STREAM: No more data, ret:0
I (225556) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (225556) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/dingdong.mp3
I (225616) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=22050, bits=16, ch=1
W (225616) FATFS_STREAM: No more data, ret:0
I (226236) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (226236) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/haode.mp3
I (226466) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=48000, bits=16, ch=1
W (226476) FATFS_STREAM: No more data, ret:0
I (227036) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (227036) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/test.mp3
I (227076) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2

```


### 日志输出

以下为本例程的完整日志。

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 17:17:27
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 2MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x3f734 (259892) map
I (210) esp_image: segment 1: paddr=0x0004f75c vaddr=0x3ffb0000 size=0x008bc (  2236) load
I (211) esp_image: segment 2: paddr=0x00050020 vaddr=0x400d0020 size=0x3b0a8 (241832) map
0x400d0020: _stext at ??:?

I (309) esp_image: segment 3: paddr=0x0008b0d0 vaddr=0x3ffb08bc size=0x0195c (  6492) load
I (312) esp_image: segment 4: paddr=0x0008ca34 vaddr=0x40080000 size=0x0e128 ( 57640) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (348) boot: Loaded app from partition at offset 0x10000
I (348) boot: Disabling RNG early entropy source...
I (349) cpu_start: Pro cpu up.
I (353) cpu_start: Application information:
I (357) cpu_start: Project name:     play_sdcard_mp3_control
I (364) cpu_start: App version:      v2.2-260-gec0ea830
I (370) cpu_start: Compile time:     Nov 30 2021 15:26:59
I (376) cpu_start: ELF file SHA256:  91b340aeb4ef5838...
I (382) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (388) cpu_start: Starting app cpu, entry point is 0x400819bc
0x400819bc: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (398) heap_init: Initializing. RAM available for dynamic allocation:
I (405) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (411) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (417) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (424) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (430) heap_init: At 4008E128 len 00011ED8 (71 KiB): IRAM
I (436) cpu_start: Pro cpu start user code
I (454) spi_flash: detected chip: gd
I (455) spi_flash: flash io: dio
W (455) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (465) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (476) SDCARD_MP3_CONTROL_EXAMPLE: [1.0] Initialize peripherals management
I (486) SDCARD_MP3_CONTROL_EXAMPLE: [1.1] Initialize and start peripherals
W (516) PERIPH_TOUCH: _touch_init
I (996) SDCARD_MP3_CONTROL_EXAMPLE: [1.2] Set up a sdcard playlist and scan sdcard music save to it
I (1776) SDCARD_MP3_CONTROL_EXAMPLE: [ 2 ] Start codec chip
E (1776) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [ 3 ] Create and start input key service
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [4.0] Create audio pipeline for playback
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [4.1] Create i2s stream to write data to codec chip
I (1856) SDCARD_MP3_CONTROL_EXAMPLE: [4.2] Create mp3 decoder to decode mp3 file
I (1856) SDCARD_MP3_CONTROL_EXAMPLE: [4.3] Create resample filter
I (1866) SDCARD_MP3_CONTROL_EXAMPLE: [4.4] Create fatfs stream to read data from sdcard
I (1886) SDCARD_MP3_CONTROL_EXAMPLE: [4.5] Register all elements to audio pipeline
I (1886) SDCARD_MP3_CONTROL_EXAMPLE: [4.6] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->resample-->i2s_stream-->[codec_chip]
I (1896) SDCARD_MP3_CONTROL_EXAMPLE: [5.0] Set up  event listener
I (1896) SDCARD_MP3_CONTROL_EXAMPLE: [5.1] Listen for all pipeline events
W (1906) SDCARD_MP3_CONTROL_EXAMPLE: [ 6 ] Press the keys to control music player:
W (1916) SDCARD_MP3_CONTROL_EXAMPLE:       [Play] to start, pause and resume, [Set] next song.
W (1936) SDCARD_MP3_CONTROL_EXAMPLE:       [Vol-] or [Vol+] to adjust volume.
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 3
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Play] input key event
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Starting audio pipeline
I (15206) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 2
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Set] input key event
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Stopped, advancing to the next song
W (27376) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (27376) AUDIO_ELEMENT: OUT-[filter] AEL_IO_ABORT
W (27386) MP3_DECODER: output aborted -3
W (27376) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (27396) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/DU1906_蓝牙_测试音频.mp3
I (27526) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=48000, bits=16, ch=2
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 2
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Set] input key event
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Stopped, advancing to the next song
W (37126) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (37126) AUDIO_ELEMENT: OUT-[filter] AEL_IO_ABORT
W (37136) MP3_DECODER: output aborted -3
W (37136) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (37146) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/test_output.mp3
I (37186) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 6
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Vol+] input key event
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Volume set to 79 %
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 6
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Vol+] input key event
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Volume set to 88 %
I (199216) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 4
I (200776) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 4
W (222766) FATFS_STREAM: No more data, ret:0
I (223486) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (223486) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/start_service.mp3
I (223616) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=16000, bits=16, ch=1
W (223626) FATFS_STREAM: No more data, ret:0
I (225556) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (225556) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/dingdong.mp3
I (225616) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=22050, bits=16, ch=1
W (225616) FATFS_STREAM: No more data, ret:0
I (226236) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (226236) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/haode.mp3
I (226466) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=48000, bits=16, ch=1
W (226476) FATFS_STREAM: No more data, ret:0
I (227036) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (227036) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/test.mp3
I (227076) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2

Done
```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
