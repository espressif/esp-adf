# 自动语音识别 (ASR) 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")

## 例程简介

该示例演示了在官方语音开发板上进行语音识别功能的过程，它包含了唤醒词和命令词两个部分。

- 当前支持的唤醒词有 3 条，你可以在 `menuconfig` 中配置使用它们，默认唤醒词是 “嗨，乐鑫”，如下表：

| # | 唤醒词    | 英文含义      | 发音          |
|:-:|--------|---------------|---------------|
| 1 | 嗨，乐鑫   | Hi, Espressif | Hāi, lè xīn   |
| 2 | 你好，小智 | Hi, Xiaozhi   | Hāi, xiǎo zhì |
| 3 | 嗨，杰森   | Hi, Jeson     | Hāi, jié sēn  |

- 当前支持的命令词有 20 条，如下表：

| #  | 命令词       | 英文含义                            | 发音                    |
|:--:|-----------|-------------------------------------|-------------------------|
| 0  | 打开空调     | turn on air conditioner             | dǎ kāi kōng tiáo        |
| 1  | 关闭空调     | turn off air conditioner            | guān bì kōng tiáo       |
| 2  | 增大风速     | increase fan speed                  | zēng dà fēng sù         |
| 3  | 减小风速     | decrease fan speed                  | jiǎn xiǎo fēng sù       |
| 4  | 升高一度     | increase temperature by one degree  | shēng gāo yī dù         |
| 5  | 降低一度     | decrease temperature by one degreee | jiàng dī yī dù          |
| 6  | 制热模式     | heat mode                           | zhì rè mó shì           |
| 7  | 制冷模式     | cool mode                           | zhì lěng mó shì         |
| 8  | 送风模式     | fan mode                            | sòng fēng mó shì        |
| 9  | 节能模式     | energy saver mode                   | jié néng mó shì         |
| 10 | 关闭节能模式 | turn off energy saver mode          | guān bì jié néng mó shì |
| 11 | 除湿模式     | dry mode                            | chú shī mó shì          |
| 12 | 关闭除湿模式 | turn off dry mode                   | guān bì chú shī mó shì  |
| 13 | 打开蓝牙     | turn on Bluetooth                   | dǎ kāi lán yá           |
| 14 | 关闭蓝牙     | turn off Bluetooth                  | guān bì lán yá          |
| 15 | 播放歌曲     | play                                | bō fàng gē qǔ           |
| 16 | 暂停播放     | stop playing                        | zàn tíng bō fàng        |
| 17 | 定时一小时   | set timer for one hour              | dìng shí yī xiǎo shí    |
| 18 | 打开电灯     | turn on light                       | dǎ kāi diàn dēng        |
| 19 | 关闭电灯     | turn off light                      | guān bì diǎn dēng       |


以下为唤醒词和命令词的使用流程：

1. USER：“嗨，乐鑫”
2. ASR：“叮咚”
3. USER：“打开空调”
4. ASR：“好的”


## 环境配置

### 硬件要求

本例程可在标有绿色复选框的开发板上运行。请记住，如下面的 *配置* 一节所述，可以在 `menuconfig` 中选择开发板。

| 开发板名称 | 开始入门 | 芯片 | 兼容性 |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |

## 编译和下载

### IDF 默认分支
本例程默认 IDF 为 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

如果你想使用“你好小智”作为唤醒词，打开 menuconfig，进入`Wake word name`并选择：

```
menuconfig > ESP Speech Recognition > `Wake word name` > `nihaoxiaozhi (WakeNet5X3)`
```

### 编译和下载
请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。

**注意：**

此例子的 CMakeLists.txt 中有自动烧录提示音的处理函数，用户在本例程目录下使用 `idf.py flash` 命令烧写固件的时候，脚本会查找 `partition` 分区表的 `flash_tone` 的烧写地址，然后在烧写 `speech_recognition_example.bin` 的同时也会自动把提示音文件 `audio_tone.bin` 烧写到 `partition` 的指定分区中。



## 如何使用例程

### 功能和用法

- 例程开始运行后，ASR 例程等待用户的唤醒词唤醒，打印如下：

```c
W (2225) I2S: I2S driver already installed
I (2226) example_asr_keywords: esp_audio instance is:0x3f80dc80
I (2226) example_asr_keywords: [ 5 ] Start audio_pipeline
```
- 此时用户先说唤醒词，比如：“嗨，乐鑫”，这时候会听到反馈的提示音“叮咚”。

```c
W (6152) TONE_STREAM: No more data,ret:0 ,info.byte_pos:8527
W (6586) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (6586) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (6587) example_asr_keywords: wake up
```

- 用户接着再说出命令词，比如说：“打开蓝牙”，如果系统识别了你的语音命令，则会给出语音反馈“好的”

```c
I (8997) example_asr_keywords: turn on bt
W (8998) AUDIO_PIPELINE: There are no listener registered
W (9040) TONE_STREAM: No more data,ret:0 ,info.byte_pos:6384
W (9536) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (9536) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED

```


### 日志输出

以下为本例程的完整日志。

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7500
ho 0 tail 12 room 4
load:0x40078000,len:13904
load:0x40080400,len:5296
0x40080400: _init at ??:?

entry 0x40080710
I (29) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (29) boot: compile time 20:24:41
I (29) boot: chip revision: 3
I (32) qio_mode: Enabling default flash chip QIO
I (37) boot.esp32: SPI Speed      : 80MHz
I (42) boot.esp32: SPI Mode       : QIO
I (46) boot.esp32: SPI Flash Size : 4MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00260000
I (90) boot:  3 flash_tone       Unknown data     01 ff 00270000 00004000
I (97) boot: End of partition table
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x200480 (2098304) map
I (728) esp_image: segment 1: paddr=0x002104a8 vaddr=0x3ffb0000 size=0x0237c (  9084) load
I (731) esp_image: segment 2: paddr=0x0021282c vaddr=0x40080000 size=0x0d7ec ( 55276) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (754) esp_image: segment 3: paddr=0x00220020 vaddr=0x400d0020 size=0x493d0 (299984) map
0x400d0020: _stext at ??:?

I (843) esp_image: segment 4: paddr=0x002693f8 vaddr=0x4008d7ec size=0x01a70 (  6768) load
0x4008d7ec: spi_flash_ll_set_buffer_data at /workshop/esp-idfs/esp-idf-v4.2.2/components/soc/src/esp32/include/hal/spi_flash_ll.h:185
 (inlined by) spi_flash_hal_common_command at /workshop/esp-idfs/esp-idf-v4.2.2/components/soc/src/hal/spi_flash_hal_common.inc:105

I (854) boot: Loaded app from partition at offset 0x10000
I (854) boot: Disabling RNG early entropy source...
I (855) psram: This chip is ESP32-D0WD
I (859) spiram: Found 64MBit SPI RAM device
I (864) spiram: SPI RAM mode: flash 80m sram 80m
I (869) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (876) cpu_start: Pro cpu up.
I (880) cpu_start: Application information:
I (885) cpu_start: Project name:     speech_recognition_example
I (892) cpu_start: App version:      v2.2-185-g0bd7ca0a-dirty
I (898) cpu_start: Compile time:     Aug 23 2021 20:24:35
I (904) cpu_start: ELF file SHA256:  a39af0e1402ef004...
I (910) cpu_start: ESP-IDF:          v4.2.2
I (915) cpu_start: Starting app cpu, entry point is 0x40081c7c
0x40081c7c: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1418) spiram: SPI SRAM memory test OK
I (1418) heap_init: Initializing. RAM available for dynamic allocation:
I (1419) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1425) heap_init: At 3FFB2D98 len 0002D268 (180 KiB): DRAM
I (1431) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1438) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1444) heap_init: At 4008F25C len 00010DA4 (67 KiB): IRAM
I (1450) cpu_start: Pro cpu start user code
I (1455) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1475) spi_flash: detected chip: generic
I (1476) spi_flash: flash io: qio
W (1476) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1487) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1503) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1511) gpio: GPIO[22]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (1521) example_asr_keywords: Initialize SR wn handle
Quantized wakeNet5: wakeNet5X3_v5_hilexin_5_0.97_0.90, mode:0 (Oct 14 2020 16:26:17)
I (1534) example_asr_keywords: keywords: hilexin (index = 1)
Fail: index is out of range, the min index is 1, the max index is 1I (1540) example_asr_keywords: keywords_num = 1, threshold = 0.000000, sample_rate = 16000, chunksize = 480, sizeof_uint16 = 2
WARNING: Sample length must less than 5000ms in the SINGLE_RECOGNITION mode
SINGLE_RECOGNITION: 2_0 MN1_4; core: 0; (May 15 2020 14:50:27)
SHIFT: 8, 12, 17, 17, 19, 17, 6, 16, 15, 14, 
I (1588) example_asr_keywords: keywords_num = 166 , sample_rate = 16000, chunksize = 480, sizeof_uint16 = 2
I (1589) example_asr_keywords: [ 1 ] Start codec chip
I (2095) example_asr_keywords: [ 2.0 ] Create audio pipeline for recording
I (2095) example_asr_keywords: [ 2.1 ] Create i2s stream to read audio data from codec chip
I (2117) example_asr_keywords: [ 2.2 ] Create filter to resample audio data
I (2117) example_asr_keywords: [ 2.3 ] Create raw to receive data
I (2120) example_asr_keywords: [ 3 ] Register all elements to audio pipeline
I (2128) example_asr_keywords: [ 4 ] Link elements together [codec_chip]-->i2s_stream-->filter-->raw-->[SR]
E (2139) gpio: gpio_install_isr_service(438): GPIO isr service already installed

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-9-g84df87e-037bef3-09be8fe                |
|                     Compile date: Jul 20 2021-13:51:36                     |
------------------------------------------------------------------------------
W (2225) I2S: I2S driver already installed
I (2226) example_asr_keywords: esp_audio instance is:0x3f80dc80

I (2226) example_asr_keywords: [ 5 ] Start audio_pipeline
W (6152) TONE_STREAM: No more data,ret:0 ,info.byte_pos:8527
W (6586) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (6586) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (6587) example_asr_keywords: wake up
I (8997) example_asr_keywords: turn on bt
W (8998) AUDIO_PIPELINE: There are no listener registered
W (9040) TONE_STREAM: No more data,ret:0 ,info.byte_pos:6384
W (9536) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (9536) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED

```

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)


我们会尽快回复。
