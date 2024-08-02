
# 多种格式的录音例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介


本例程使用 stream element 和 codec element 组成 pipeline，把音频数据流从 i2s stream 中读取，经过 encoder 编码，最后由 fatfs stream 写入 microSD card 中。 encoder 可以通过 menuconfig 配置为 WAV、OPUS、AMRNB、AMRWB。

此例支持 WAV、OPUS、AMRNB、AMRWB 四种音频编码器。默认选择 WAV 编码器来编码录音，录音文件保存在 microSD card 中。


## 环境配置

### 硬件要求

本例程需要准备一张 microSD card，并预先插入开发板中备用。

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v5.0 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3` ，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程需要打开 FATFS 长文件名支持，例如 `rec.opus` 这样的文件名是超出默认的 `8.3 命名`  规范的。

```
menuconfig > Component config > FAT Filesystem support > Long filename support
```

本例程默认的编码器是 WAV，如果更改为其他的编码器，需要在 `menuconfig` 中选择想要的编码器，例如选择 OPUS 编码器。

```
menuconfig > Example configuration > Encoding type of recording file > opus encoder
```


### 编译和下载
请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.3/esp32/index.html)。

## 如何使用例程

### 功能和用法
例程开始运行后，会打印如下倒计时提示，并提示录音开始了。

```
I (655) RECORDING_TO_SDCARD: [ 5 ] Start audio_pipeline
I (665) RECORDING_TO_SDCARD: [ 6 ] Listen for all pipeline events, record for 10 Seconds
I (1725) RECORDING_TO_SDCARD: [ * ] Recording ... 1
I (2725) RECORDING_TO_SDCARD: [ * ] Recording ... 2
I (3735) RECORDING_TO_SDCARD: [ * ] Recording ... 3
```

例程结束时候打印如下，此时可以取下 microSD card，使用电脑播放器播放 microSD card 里保存的音频，音频文件命名为 `rec.wav` 或者 `rec.opus` 这样的文件。

```
W (10975) RECORDING_TO_SDCARD: [ * ] Stop event received
I (10975) RECORDING_TO_SDCARD: [ 7 ] Stop audio_pipeline
E (10975) AUDIO_ELEMENT: [opus] Element already stopped
E (10975) AUDIO_ELEMENT: [res] Element already stopped
E (10985) AUDIO_ELEMENT: [file] Element already stopped
W (10995) AUDIO_PIPELINE: There are no listener registered
W (10995) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11005) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11015) AUDIO_ELEMENT: [res] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11025) AUDIO_ELEMENT: [opus] Element has not create when AUDIO_ELEMENT_TERMINATE
```


### 日志输出
本例选取完整的从启动到初始化完成的 log，示例如下：

```c
--- idf_monitor on /dev/ttyUSB0 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
I (13) boot: ESP-IDF v3.3.2-107-g722043f73 2nd stage bootloader
I (13) boot: compile time 17:12:08
I (13) boot: Enabling RNG early entropy source...
I (18) boot: SPI Speed      : 40MHz
I (22) boot: SPI Mode       : DIO
I (26) boot: SPI Flash Size : 8MB
I (30) boot: Partition Table:
I (33) boot: ## Label            Usage          Type ST Offset   Length
I (41) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (48) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (56) boot:  2 factory          factory app      00 00 00010000 00100000
I (63) boot: End of partition table
I (67) boot_comm: chip revision: 1, min. application chip revision: 0
I (74) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1bc98 (113816) map
I (123) esp_image: segment 1: paddr=0x0002bcc0 vaddr=0x3ffb0000 size=0x01f7c (  8060) load
I (127) esp_image: segment 2: paddr=0x0002dc44 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (130) esp_image: segment 3: paddr=0x0002e04c vaddr=0x40080400 size=0x01fc4 (  8132) load
I (142) esp_image: segment 4: paddr=0x00030018 vaddr=0x400d0018 size=0x4e4b4 (320692) map
0x400d0018: _flash_cache_start at ??:?

I (260) esp_image: segment 5: paddr=0x0007e4d4 vaddr=0x400823c4 size=0x0910c ( 37132) load
0x400823c4: _xt_coproc_exc at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1019

I (283) boot: Loaded app from partition at offset 0x10000
I (283) boot: Disabling RNG early entropy source...
I (283) cpu_start: Pro cpu up.
I (287) cpu_start: Application information:
I (292) cpu_start: Project name:     recording_to_sdcard
I (298) cpu_start: App version:      v2.2-110-gb3efcc37-dirty
I (304) cpu_start: Compile time:     Apr 22 2021 17:12:07
I (310) cpu_start: ELF file SHA256:  ee3b208c350a5270...
I (316) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (322) cpu_start: Starting app cpu, entry point is 0x400811f8
0x400811f8: call_start_cpu1 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (333) heap_init: Initializing. RAM available for dynamic allocation:
I (340) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (346) heap_init: At 3FFB30D8 len 0002CF28 (179 KiB): DRAM
I (352) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (358) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (365) heap_init: At 4008B4D0 len 00014B30 (82 KiB): IRAM
I (371) cpu_start: Pro cpu start user code
I (54) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (55) RECORDING_TO_SDCARD: [ 1 ] Mount sdcard
I (555) RECORDING_TO_SDCARD: [ 2 ] Start codec chip
W (575) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
I (585) RECORDING_TO_SDCARD: [3.0] Create audio pipeline for recording
I (585) RECORDING_TO_SDCARD: [3.1] Create fatfs stream to write data to sdcard
I (595) RECORDING_TO_SDCARD: [3.2] Create i2s stream to read audio data from codec chip
I (605) RECORDING_TO_SDCARD: [3.3] Create audio encoder to handle data
I (605) RECORDING_TO_SDCARD: [3.4] Register all elements to audio pipeline
I (615) RECORDING_TO_SDCARD: [3.5] Link it together [codec_chip]-->i2s_stream-->audio_encoder-->fatfs_stream-->[sdcard]
I (625) RECORDING_TO_SDCARD: [3.6] Set up  uri
I (635) RECORDING_TO_SDCARD: [ 4 ] Set up  event listener
I (635) RECORDING_TO_SDCARD: [4.1] Listening event from pipeline
I (645) RECORDING_TO_SDCARD: [4.2] Listening event from peripherals
I (655) RECORDING_TO_SDCARD: [ 5 ] Start audio_pipeline
I (665) RECORDING_TO_SDCARD: [ 6 ] Listen for all pipeline events, record for 10 Seconds
I (1725) RECORDING_TO_SDCARD: [ * ] Recording ... 1
I (2725) RECORDING_TO_SDCARD: [ * ] Recording ... 2
I (3735) RECORDING_TO_SDCARD: [ * ] Recording ... 3
I (4735) RECORDING_TO_SDCARD: [ * ] Recording ... 4
I (5735) RECORDING_TO_SDCARD: [ * ] Recording ... 5
I (6735) RECORDING_TO_SDCARD: [ * ] Recording ... 6
I (7745) RECORDING_TO_SDCARD: [ * ] Recording ... 7
I (8885) RECORDING_TO_SDCARD: [ * ] Recording ... 8
I (9885) RECORDING_TO_SDCARD: [ * ] Recording ... 9
I (10915) RECORDING_TO_SDCARD: [ * ] Recording ... 10
W (10975) RECORDING_TO_SDCARD: [ * ] Stop event received
I (10975) RECORDING_TO_SDCARD: [ 7 ] Stop audio_pipeline
E (10975) AUDIO_ELEMENT: [opus] Element already stopped
E (10975) AUDIO_ELEMENT: [res] Element already stopped
E (10985) AUDIO_ELEMENT: [file] Element already stopped
W (10995) AUDIO_PIPELINE: There are no listener registered
W (10995) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11005) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11015) AUDIO_ELEMENT: [res] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11025) AUDIO_ELEMENT: [opus] Element has not create when AUDIO_ELEMENT_TERMINATE

```

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
