# 使用 ALC 调节 WAV 音量的例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了 ADF 的自动电平控制 (automatic level control, ALC) 的使用方法，它通过检测语音信号的幅值来判断是否超过设定的 Max/Min 值。如果超过出范围，则根据设定增益来调整音频信号的幅值，使其介于一个合理的范围内，从而达到控制音量的目的。

ADF 当前提供了以下两种方法实现 ALC 设置音量。

- 对于使能 `USE_ALONE_ALC` 的管道如下：

  ```
  sdcard ---> fatfs_stream ---> wav_decoder ---> ALC ---> i2s_stream ---> codec_chip ---> speaker
  ```

- 对于未使能 `USE_ALONE_ALC` 的管道如下：

  ```
  sdcard ---> fatfs_stream ---> wav_decoder ---> i2s_stream ---> codec_chip ---> speaker
  ```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载


### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程需要准备一张 microSD 卡，并自备一首 WAV 格式的音频文件，命名为 `test.wav`，然后把 microSD 卡插入开发板备用。

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

程序默认不使能 `USE_ALONE_ALC` 配置：

```c
// #define USE_ALONE_ALC
```

即是通过在 `i2s_stream.h` 函数 `i2s_alc_volume_set` 来控制音量振幅。


- 例程开始运行后，会自动处理音频数据的过高的幅值，打印详见[日志输出](##日志输出)。


### 日志输出
以下为本例程的完整日志。

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 12:05:31
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 2MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00100000
I (90) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0f750 ( 63312) map
I (134) esp_image: segment 1: paddr=0x0001f778 vaddr=0x3ffb0000 size=0x008a0 (  2208) load
I (135) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x2bea8 (179880) map
0x400d0020: _stext at ??:?

I (209) esp_image: segment 3: paddr=0x0004bed0 vaddr=0x3ffb08a0 size=0x01960 (  6496) load
I (212) esp_image: segment 4: paddr=0x0004d838 vaddr=0x40080000 size=0x0daf0 ( 56048) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (248) boot: Loaded app from partition at offset 0x10000
I (248) boot: Disabling RNG early entropy source...
I (248) cpu_start: Pro cpu up.
I (252) cpu_start: Application information:
I (257) cpu_start: Project name:     use_alc_set_volume_app
I (263) cpu_start: App version:      v2.2-202-g020b5c49-dirty
I (270) cpu_start: Compile time:     Sep 30 2021 12:05:25
I (276) cpu_start: ELF file SHA256:  3b7b0211f9ca2f5f...
I (282) cpu_start: ESP-IDF:          v4.2.2
I (287) cpu_start: Starting app cpu, entry point is 0x40081988
0x40081988: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (297) heap_init: Initializing. RAM available for dynamic allocation:
I (304) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (310) heap_init: At 3FFB2B48 len 0002D4B8 (181 KiB): DRAM
I (316) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (322) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (329) heap_init: At 4008DAF0 len 00012510 (73 KiB): IRAM
I (335) cpu_start: Pro cpu start user code
I (353) spi_flash: detected chip: gd
I (354) spi_flash: flash io: dio
W (354) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (364) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (375) USE_ALC_EXAMPLE: [ 1 ] Mount sdcard
I (885) USE_ALC_EXAMPLE: [ 2 ] Start codec chip
E (885) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (905) USE_ALC_EXAMPLE: [3.0] Create audio pipeline for playback
I (905) USE_ALC_EXAMPLE: [3.1] Create fatfs stream to read data from sdcard
I (905) USE_ALC_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (945) USE_ALC_EXAMPLE: [3.3] Create wav decoder to decode wav file
I (945) USE_ALC_EXAMPLE: [3.4] Register all elements to audio pipeline
I (945) USE_ALC_EXAMPLE: [3.5] Link it together [sdcard]-->fatfs_stream-->wav_decoder-->i2s_stream-->[codec_chip]
I (955) USE_ALC_EXAMPLE: [3.6] Set up  uri (file as fatfs_stream, wav as wav decoder, and default output is i2s)
I (965) USE_ALC_EXAMPLE: [ 4 ] Set up  event listener
I (975) USE_ALC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (975) USE_ALC_EXAMPLE: [4.2] Listening event from peripherals
I (985) USE_ALC_EXAMPLE: [ 5 ] Start audio_pipeline
I (1025) USE_ALC_EXAMPLE: [ 6 ] Listen for all pipeline events
I (1035) USE_ALC_EXAMPLE: [ * ] Receive music info from wav decoder, sample_rates=48000, bits=16, ch=2
W (91795) FATFS_STREAM: No more data, ret:0
W (432735) HEADPHONE: Headphone jack inserted

```


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
