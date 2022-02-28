# 音频变声例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了 ADF 变声器 (sonic) 的使用方法，例程创建了两个管道，一是录音管道，用于录制声音到 microSD 卡，触发条件是长按 [REC] 录音键，二是播放管道，用于读取并播放 microSD 卡中 `rec.wav` 文件，触发条件是释放长按的 [REC] 录音键。变声器可以通过 [Mode] 按键调整音频倍速和音高变化。

变声器可以通过下列参数来调整音频的输出效果：

- 音频倍速
- 音调

录音保存到 microSD 卡的管道如下：

```
[mic] ---> codec_chip ---> i2s_stream ---> wav_encoder ---> fatfs_stream ---> [sdcard]

```

读取 microSD 卡的 WAV 文件，然后经过变声器处理的播放管道如下：

```
[sdcard] ---> fatfs_stream ---> wav_decoder ---> sonic ---> i2s_stream ---> [codec_chip]

```


## 环境配置

### 硬件要求

本例程支持的开发板在例程与音频开发板的[兼容性表格](../../README.md#compatibility-of-examples-with-espressif-audio-boards)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程需要准备一张 microSD 卡，插入开发板备用，用于保存录音的 WAV 音频文件。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

如果你需要修改录音文件名，本例程建议同时打开 FatFs 长文件名支持。

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

- 例程开始运行后，等待用户长按下 [REC] 录音键录音，打印如下：

```c
ets Jul 29 2019 12:21:46

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
I (27) boot: compile time 15:08:16
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x10584 ( 66948) map
I (137) esp_image: segment 1: paddr=0x000205ac vaddr=0x3ffb0000 size=0x02218 (  8728) load
I (140) esp_image: segment 2: paddr=0x000227cc vaddr=0x40080000 size=0x0d84c ( 55372) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (168) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x2ee44 (192068) map
0x400d0020: _stext at ??:?

I (241) esp_image: segment 4: paddr=0x0005ee6c vaddr=0x4008d84c size=0x0044c (  1100) load
0x4008d84c: spi_flash_common_read_status_16b_rdsr_rdsr2 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/spi_flash/spi_flash_chip_generic.c:427

I (249) boot: Loaded app from partition at offset 0x10000
I (249) boot: Disabling RNG early entropy source...
I (251) cpu_start: Pro cpu up.
I (254) cpu_start: Application information:
I (259) cpu_start: Project name:     sonic_app
I (264) cpu_start: App version:      v2.2-216-gbb375dee-dirty
I (271) cpu_start: Compile time:     Nov  8 2021 15:08:02
I (277) cpu_start: ELF file SHA256:  1878ad41c12b7600...
I (283) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (289) cpu_start: Starting app cpu, entry point is 0x400819b8
0x400819b8: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (299) heap_init: Initializing. RAM available for dynamic allocation:
I (306) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (312) heap_init: At 3FFB2BA0 len 0002D460 (181 KiB): DRAM
I (318) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (325) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (331) heap_init: At 4008DC98 len 00012368 (72 KiB): IRAM
I (337) cpu_start: Pro cpu start user code
I (355) spi_flash: detected chip: gd
I (356) spi_flash: flash io: dio
W (356) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (366) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (878) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (898) PERIPH_TOUCH: _touch_init
I (918) SONIC_EXAMPLE: [1.1] Initialize recorder pipeline
I (918) SONIC_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (958) SONIC_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (958) SONIC_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (958) I2S: I2S driver already installed
I (968) SONIC_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (968) SONIC_EXAMPLE: [ 3 ] Set up  event listener
W (988) SONIC_EXAMPLE: Press [Rec] to start recording

```

- 读取 microSD 卡的 WAV 文件，然后经过变声器处理的播放管道如下：

```c
E (2894608) SONIC_EXAMPLE: Now recording, release [Rec] to STOP
W (2894608) AUDIO_PIPELINE: Without stop, st:1
W (2894608) AUDIO_PIPELINE: Without wait stop, st:1
W (2894608) AUDIO_ELEMENT: [file_reader] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894628) AUDIO_ELEMENT: [wav_decoder] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894628) AUDIO_ELEMENT: [sonic] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894638) AUDIO_ELEMENT: [i2s_writer] Element has not create when AUDIO_ELEMENT_TERMINATE
I (2894648) SONIC_EXAMPLE: Setup file path to save recorded audio

```

- 当释放音频板上的 [REC] 按键时，本例将读取 microSD 卡中保存的 WAV 录音文件，数据经过变声器处理后播放出来，日志如下：

```c
I (2901158) SONIC_EXAMPLE: START Playback
W (2901158) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
E (2901158) AUDIO_ELEMENT: [wav_encoder] Element already stopped
W (2901168) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
I (2901198) SONIC_EXAMPLE: Setup file path to read the wav audio to play
W (2903928) FATFS_STREAM: No more data, ret:0

```

- 本例程还支持使用 [Mode] 键切换音频倍速和音高变化，日志如下：

```c
I (3043078) SONIC_EXAMPLE: The pitch of audio file is changed
I (3044078) SONIC_EXAMPLE: The speed of audio file is changed
I (3047098) SONIC_EXAMPLE: The pitch of audio file is changed
I (3047508) SONIC_EXAMPLE: The speed of audio file is changed
I (3048278) SONIC_EXAMPLE: The pitch of audio file is changed

```



### 日志输出
以下是本例程的完整日志。

```c
ets Jul 29 2019 12:21:46

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
I (27) boot: compile time 15:08:16
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x10584 ( 66948) map
I (137) esp_image: segment 1: paddr=0x000205ac vaddr=0x3ffb0000 size=0x02218 (  8728) load
I (140) esp_image: segment 2: paddr=0x000227cc vaddr=0x40080000 size=0x0d84c ( 55372) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (168) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x2ee44 (192068) map
0x400d0020: _stext at ??:?

I (241) esp_image: segment 4: paddr=0x0005ee6c vaddr=0x4008d84c size=0x0044c (  1100) load
0x4008d84c: spi_flash_common_read_status_16b_rdsr_rdsr2 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/spi_flash/spi_flash_chip_generic.c:427

I (249) boot: Loaded app from partition at offset 0x10000
I (249) boot: Disabling RNG early entropy source...
I (251) cpu_start: Pro cpu up.
I (254) cpu_start: Application information:
I (259) cpu_start: Project name:     sonic_app
I (264) cpu_start: App version:      v2.2-216-gbb375dee-dirty
I (271) cpu_start: Compile time:     Nov  8 2021 15:08:02
I (277) cpu_start: ELF file SHA256:  1878ad41c12b7600...
I (283) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (289) cpu_start: Starting app cpu, entry point is 0x400819b8
0x400819b8: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (299) heap_init: Initializing. RAM available for dynamic allocation:
I (306) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (312) heap_init: At 3FFB2BA0 len 0002D460 (181 KiB): DRAM
I (318) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (325) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (331) heap_init: At 4008DC98 len 00012368 (72 KiB): IRAM
I (337) cpu_start: Pro cpu start user code
I (355) spi_flash: detected chip: gd
I (356) spi_flash: flash io: dio
W (356) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (366) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (878) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (898) PERIPH_TOUCH: _touch_init
I (918) SONIC_EXAMPLE: [1.1] Initialize recorder pipeline
I (918) SONIC_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (958) SONIC_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (958) SONIC_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (958) I2S: I2S driver already installed
I (968) SONIC_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (968) SONIC_EXAMPLE: [ 3 ] Set up  event listener
W (988) SONIC_EXAMPLE: Press [Rec] to start recording
E (2894608) SONIC_EXAMPLE: Now recording, release [Rec] to STOP
W (2894608) AUDIO_PIPELINE: Without stop, st:1
W (2894608) AUDIO_PIPELINE: Without wait stop, st:1
W (2894608) AUDIO_ELEMENT: [file_reader] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894628) AUDIO_ELEMENT: [wav_decoder] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894628) AUDIO_ELEMENT: [sonic] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894638) AUDIO_ELEMENT: [i2s_writer] Element has not create when AUDIO_ELEMENT_TERMINATE
I (2894648) SONIC_EXAMPLE: Setup file path to save recorded audio
I (2901158) SONIC_EXAMPLE: START Playback
W (2901158) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
E (2901158) AUDIO_ELEMENT: [wav_encoder] Element already stopped
W (2901168) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
I (2901198) SONIC_EXAMPLE: Setup file path to read the wav audio to play
W (2903928) FATFS_STREAM: No more data, ret:0
I (3043078) SONIC_EXAMPLE: The pitch of audio file is changed
I (3044078) SONIC_EXAMPLE: The speed of audio file is changed
I (3047098) SONIC_EXAMPLE: The pitch of audio file is changed
I (3047508) SONIC_EXAMPLE: The speed of audio file is changed
I (3048278) SONIC_EXAMPLE: The pitch of audio file is changed

```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
