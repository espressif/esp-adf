# 使用重采样的录音和回放例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了 ADF 的重采样 (resample) 使用方法，录音的时候把 48000 Hz、16 位、2 通道的音频数据重采样为 16000 Hz、16 位、1 通道的数据，然后编码为 WAV 格式并写入 microSD 卡中，回放的时候把 microSD 卡中的 16000 Hz、16 位、1 通道的 WAV 文件读出，重采样数据为 48000 Hz、16 位、2 通道，然后通过 I2S 外设把录音数据播放出来。


1. 对于录音过程：

  - 设置 I2S 并以 48000 Hz、16 位、立体声的采样率获取音频。
  - 使用重采样过滤器转换数据为 16000 Hz、16 位、1 通道。
  - 使用 WAV 编码器进行数据编码。
  - 写入 microSD 卡。

  录音的重采样并保存到 microSD 卡的管道如下：

  ```
  mic ---> codec_chip ---> i2s_stream ---> resample_filter ---> wav_encoder ---> fatfs_stream ---> sdcard
  ```

2. 对于录音回放过程：

  - 读取 microSD 卡录制的文件，采样率为 16000 Hz、16 位、1 通道。
  - 用 WAV 解码器解码文件数据。
  - 使用重采样过滤器转换数据为 48000 Hz、16 位、2 通道。
  - 将音频写入 I2S 外设。


  读取 microSD 卡的文件重采样后通过 I2S 播放管道如下：

  ```
  sdcard ---> fatfs_stream ---> wav_decoder ---> resample_filter ---> i2s_stream ---> codec_chip ---> speaker
  ```


## 环境配置


### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


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

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。

## 如何使用例程

### 功能和用法

- 例程开始运行后，等待用户按下 [REC] 录音键录音，打印如下：

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 20:11:50
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 40MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 2MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (68) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00100000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0f4f0 ( 62704) map
I (120) esp_image: segment 1: paddr=0x0001f518 vaddr=0x3ffb0000 size=0x00b00 (  2816) load
I (121) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x323f8 (205816) map
0x400d0020: _stext at ??:?

I (205) esp_image: segment 3: paddr=0x00052420 vaddr=0x3ffb0b00 size=0x016f0 (  5872) load
I (207) esp_image: segment 4: paddr=0x00053b18 vaddr=0x40080000 size=0x0d970 ( 55664) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (243) boot: Loaded app from partition at offset 0x10000
I (243) boot: Disabling RNG early entropy source...
I (244) cpu_start: Pro cpu up.
I (247) cpu_start: Application information:
I (252) cpu_start: Project name:     resample_app
I (258) cpu_start: App version:      v2.2-202-gc3504872-dirty
I (264) cpu_start: Compile time:     Sep 29 2021 20:11:45
I (270) cpu_start: ELF file SHA256:  4250af7904a84fee...
I (276) cpu_start: ESP-IDF:          v4.2.2
I (281) cpu_start: Starting app cpu, entry point is 0x40081970
0x40081970: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (291) heap_init: Initializing. RAM available for dynamic allocation:
I (298) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (304) heap_init: At 3FFB2B68 len 0002D498 (181 KiB): DRAM
I (310) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (317) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (323) heap_init: At 4008D970 len 00012690 (73 KiB): IRAM
I (329) cpu_start: Pro cpu start user code
I (347) spi_flash: detected chip: gd
I (348) spi_flash: flash io: dio
W (348) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (358) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (870) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (890) PERIPH_TOUCH: _touch_init
I (1410) RESAMPLE_EXAMPLE: [1.1] Initialize recorder pipeline
I (1410) RESAMPLE_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (1430) RESAMPLE_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (1430) RESAMPLE_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (1430) I2S: I2S driver already installed
I (1440) RESAMPLE_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (1460) RESAMPLE_EXAMPLE: [ 3 ] Set up  event listener
```

- 当按下音频板上的 [REC] 按键时，本例将进行录音，重采样数据后编码为 WAV 文件，并写入到 microSD 卡中保存，打印如下：

```c
E (25420) RESAMPLE_EXAMPLE: STOP Playback and START [Record]
W (25420) AUDIO_PIPELINE: Without stop, st:1
W (25420) AUDIO_PIPELINE: Without wait stop, st:1
I (25420) RESAMPLE_EXAMPLE: Link audio elements to make recorder pipeline ready
I (25430) RESAMPLE_EXAMPLE: Setup file path to save recorded audio
```

- 当 [REC] 键松开时，例程将读取 microSD 卡中的 WAV 文件，重采样后发送到 I2S 接口播放该录音文件，打印如下：

```c
I (31540) RESAMPLE_EXAMPLE: STOP [Record] and START Playback
W (31540) AUDIO_ELEMENT: IN-[filter_downsample] AEL_IO_ABORT
E (31550) AUDIO_ELEMENT: [filter_downsample] Element already stopped
W (31550) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
W (31560) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
I (31570) RESAMPLE_EXAMPLE: Link audio elements to make playback pipeline ready
I (31570) RESAMPLE_EXAMPLE: Setup file path to read the wav audio to play
W (37070) FATFS_STREAM: No more data, ret:0
```


### 日志输出

以下为本例程的完整日志。

```c
cets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7152
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 20:11:50
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 40MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 2MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (68) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00100000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0f4f0 ( 62704) map
I (120) esp_image: segment 1: paddr=0x0001f518 vaddr=0x3ffb0000 size=0x00b00 (  2816) load
I (121) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x323f8 (205816) map
0x400d0020: _stext at ??:?

I (205) esp_image: segment 3: paddr=0x00052420 vaddr=0x3ffb0b00 size=0x016f0 (  5872) load
I (207) esp_image: segment 4: paddr=0x00053b18 vaddr=0x40080000 size=0x0d970 ( 55664) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (243) boot: Loaded app from partition at offset 0x10000
I (243) boot: Disabling RNG early entropy source...
I (244) cpu_start: Pro cpu up.
I (247) cpu_start: Application information:
I (252) cpu_start: Project name:     resample_app
I (258) cpu_start: App version:      v2.2-202-gc3504872-dirty
I (264) cpu_start: Compile time:     Sep 29 2021 20:11:45
I (270) cpu_start: ELF file SHA256:  4250af7904a84fee...
I (276) cpu_start: ESP-IDF:          v4.2.2
I (281) cpu_start: Starting app cpu, entry point is 0x40081970
0x40081970: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (291) heap_init: Initializing. RAM available for dynamic allocation:
I (298) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (304) heap_init: At 3FFB2B68 len 0002D498 (181 KiB): DRAM
I (310) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (317) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (323) heap_init: At 4008D970 len 00012690 (73 KiB): IRAM
I (329) cpu_start: Pro cpu start user code
I (347) spi_flash: detected chip: gd
I (348) spi_flash: flash io: dio
W (348) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (358) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (870) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (890) PERIPH_TOUCH: _touch_init
I (1410) RESAMPLE_EXAMPLE: [1.1] Initialize recorder pipeline
I (1410) RESAMPLE_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (1430) RESAMPLE_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (1430) RESAMPLE_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (1430) I2S: I2S driver already installed
I (1440) RESAMPLE_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (1460) RESAMPLE_EXAMPLE: [ 3 ] Set up  event listener
E (25420) RESAMPLE_EXAMPLE: STOP Playback and START [Record]
W (25420) AUDIO_PIPELINE: Without stop, st:1
W (25420) AUDIO_PIPELINE: Without wait stop, st:1
I (25420) RESAMPLE_EXAMPLE: Link audio elements to make recorder pipeline ready
I (25430) RESAMPLE_EXAMPLE: Setup file path to save recorded audio
I (31540) RESAMPLE_EXAMPLE: STOP [Record] and START Playback
W (31540) AUDIO_ELEMENT: IN-[filter_downsample] AEL_IO_ABORT
E (31550) AUDIO_ELEMENT: [filter_downsample] Element already stopped
W (31550) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
W (31560) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
I (31570) RESAMPLE_EXAMPLE: Link audio elements to make playback pipeline ready
I (31570) RESAMPLE_EXAMPLE: Setup file path to read the wav audio to play
W (37070) FATFS_STREAM: No more data, ret:0
```


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
