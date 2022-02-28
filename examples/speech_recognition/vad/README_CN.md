# 语音活动检测 (VAD) 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了从麦克风读取环境声音数据，经过 VAD 处理分析，最后 VAD 输出是噪声还是人声的判定结果。

管道的数据流向，如下所示：

```
mic ---> codec_chip ---> i2s_stream ---> filter ---> raw ---> vad_process ---> output
```

## 环境配置


### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载


### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
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

- 例程开始运行后，程序自动开始检测周围的背景环境声音，打印如下：

```c
I (0) cpu_start: App cpu up.
I (367) heap_init: Initializing. RAM available for dynamic allocation:
I (374) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (380) heap_init: At 3FFB3B50 len 0002C4B0 (177 KiB): DRAM
I (386) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (393) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (399) heap_init: At 4008C240 len 00013DC0 (79 KiB): IRAM
I (405) cpu_start: Pro cpu start user code
I (424) spi_flash: detected chip: gd
I (424) spi_flash: flash io: dio
W (425) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (434) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (445) EXAMPLE-VAD: [ 1 ] Start codec chip
I (475) EXAMPLE-VAD: [ 2 ] Create audio pipeline for recording
I (475) EXAMPLE-VAD: [2.1] Create i2s stream to read audio data from codec chip
I (505) EXAMPLE-VAD: [2.2] Create filter to resample audio data
I (505) EXAMPLE-VAD: [2.3] Create raw to receive data
I (505) EXAMPLE-VAD: [ 3 ] Register all elements to audio pipeline
I (505) EXAMPLE-VAD: [ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[VAD]
I (515) EXAMPLE-VAD: [ 5 ] Start audio_pipeline
I (555) EXAMPLE-VAD: [ 6 ] Initialize VAD handle

```

- 如果此时周围有人说话，程序检测到语音信号，打印出 `Speech detected`，如下：

```c
I (445) EXAMPLE-VAD: [ 1 ] Start codec chip
I (475) EXAMPLE-VAD: [ 2 ] Create audio pipeline for recording
I (475) EXAMPLE-VAD: [2.1] Create i2s stream to read audio data from codec chip
I (505) EXAMPLE-VAD: [2.2] Create filter to resample audio data
I (505) EXAMPLE-VAD: [2.3] Create raw to receive data
I (505) EXAMPLE-VAD: [ 3 ] Register all elements to audio pipeline
I (505) EXAMPLE-VAD: [ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[VAD]
I (515) EXAMPLE-VAD: [ 5 ] Start audio_pipeline
I (555) EXAMPLE-VAD: [ 6 ] Initialize VAD handle
I (7835) EXAMPLE-VAD: Speech detected
I (7865) EXAMPLE-VAD: Speech detected
I (7895) EXAMPLE-VAD: Speech detected
I (7925) EXAMPLE-VAD: Speech detected
I (7955) EXAMPLE-VAD: Speech detected
I (7985) EXAMPLE-VAD: Speech detected
I (8015) EXAMPLE-VAD: Speech detected
I (8045) EXAMPLE-VAD: Speech detected
I (8065) EXAMPLE-VAD: Speech detected
I (8105) EXAMPLE-VAD: Speech detected
I (8135) EXAMPLE-VAD: Speech detected
I (8165) EXAMPLE-VAD: Speech detected
I (8195) EXAMPLE-VAD: Speech detected
I (8215) EXAMPLE-VAD: Speech detected
I (8245) EXAMPLE-VAD: Speech detected
I (8285) EXAMPLE-VAD: Speech detected
I (8315) EXAMPLE-VAD: Speech detected
I (8345) EXAMPLE-VAD: Speech detected
I (8375) EXAMPLE-VAD: Speech detected
I (8395) EXAMPLE-VAD: Speech detected
I (8425) EXAMPLE-VAD: Speech detected
I (8465) EXAMPLE-VAD: Speech detected
I (8495) EXAMPLE-VAD: Speech detected
I (8525) EXAMPLE-VAD: Speech detected
I (8545) EXAMPLE-VAD: Speech detected

```


### 日志输出

以下为本例程的完整日志。

```c
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
I (27) boot: compile time 10:29:24
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
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1c268 (115304) map
I (154) esp_image: segment 1: paddr=0x0002c290 vaddr=0x3ffb0000 size=0x021b8 (  8632) load
I (158) esp_image: segment 2: paddr=0x0002e450 vaddr=0x40080000 size=0x01bc8 (  7112) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (164) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x4ff28 (327464) map
0x400d0020: _stext at ??:?

I (294) esp_image: segment 4: paddr=0x0007ff50 vaddr=0x40081bc8 size=0x0a678 ( 42616) load
0x40081bc8: esp_crosscore_int_send at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/crosscore_int.c:109

I (320) boot: Loaded app from partition at offset 0x10000
I (320) boot: Disabling RNG early entropy source...
I (320) cpu_start: Pro cpu up.
I (324) cpu_start: Application information:
I (329) cpu_start: Project name:     example_vad
I (334) cpu_start: App version:      v2.2-195-gf0079c11
I (340) cpu_start: Compile time:     Sep 18 2021 10:29:18
I (346) cpu_start: ELF file SHA256:  1c0b620a29a11e10...
I (352) cpu_start: ESP-IDF:          v4.2.2
I (357) cpu_start: Starting app cpu, entry point is 0x40081960
0x40081960: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (367) heap_init: Initializing. RAM available for dynamic allocation:
I (374) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (380) heap_init: At 3FFB3B50 len 0002C4B0 (177 KiB): DRAM
I (386) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (393) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (399) heap_init: At 4008C240 len 00013DC0 (79 KiB): IRAM
I (405) cpu_start: Pro cpu start user code
I (424) spi_flash: detected chip: gd
I (424) spi_flash: flash io: dio
W (425) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (434) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (445) EXAMPLE-VAD: [ 1 ] Start codec chip
I (475) EXAMPLE-VAD: [ 2 ] Create audio pipeline for recording
I (475) EXAMPLE-VAD: [2.1] Create i2s stream to read audio data from codec chip
I (505) EXAMPLE-VAD: [2.2] Create filter to resample audio data
I (505) EXAMPLE-VAD: [2.3] Create raw to receive data
I (505) EXAMPLE-VAD: [ 3 ] Register all elements to audio pipeline
I (505) EXAMPLE-VAD: [ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[VAD]
I (515) EXAMPLE-VAD: [ 5 ] Start audio_pipeline
I (555) EXAMPLE-VAD: [ 6 ] Initialize VAD handle
I (7835) EXAMPLE-VAD: Speech detected
I (7865) EXAMPLE-VAD: Speech detected
I (7895) EXAMPLE-VAD: Speech detected
I (7925) EXAMPLE-VAD: Speech detected
I (7955) EXAMPLE-VAD: Speech detected
I (7985) EXAMPLE-VAD: Speech detected
I (8015) EXAMPLE-VAD: Speech detected
I (8045) EXAMPLE-VAD: Speech detected
I (8065) EXAMPLE-VAD: Speech detected
I (8105) EXAMPLE-VAD: Speech detected
I (8135) EXAMPLE-VAD: Speech detected
I (8165) EXAMPLE-VAD: Speech detected
I (8195) EXAMPLE-VAD: Speech detected
I (8215) EXAMPLE-VAD: Speech detected
I (8245) EXAMPLE-VAD: Speech detected
I (8285) EXAMPLE-VAD: Speech detected
I (8315) EXAMPLE-VAD: Speech detected
I (8345) EXAMPLE-VAD: Speech detected
I (8375) EXAMPLE-VAD: Speech detected
I (8395) EXAMPLE-VAD: Speech detected
I (8425) EXAMPLE-VAD: Speech detected
I (8465) EXAMPLE-VAD: Speech detected
I (8495) EXAMPLE-VAD: Speech detected
I (8525) EXAMPLE-VAD: Speech detected
I (8545) EXAMPLE-VAD: Speech detected
```

## 故障排除

此应用程序可能会将一些其他与人声频率相似的随机声音报告为语音，例如敲击棋盘、带有某些音调的音乐等。


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
