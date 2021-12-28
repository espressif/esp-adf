# Automatic Speech Recognition (ASR)

- [中文版本](./README_CN.md)
- Baisc Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example demonstrates the process of automatically recognizing wake words and command words on the official voice development board.

- Currently, three wake words are supported as shown in the table below. You can configure which one to use in `menuconfig`. The default wake word is “嗨，乐鑫”, which means "Hi, Espressif” in English.

| # | Wake Word | English Meaning | Pronunciation |
|:-:|-----------|-----------------|---------------|
| 1 | 嗨，乐鑫   | Hi, Espressif   | Hāi, lè xīn   |
| 2 | 你好，小智 | Hi, Xiaozhi     | Hāi, xiǎo zhì |
| 3 | 嗨，杰森   | Hi, Jeson       | Hāi, jié sēn  |

- Currently, 20 command words are supported as shown in the table below:

| #  | Command Word | English Meaning                     | Pronunciation           |
|:--:|--------------|-------------------------------------|-------------------------|
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


The following is the process of using wake words and command words:

1. USER: “嗨，乐鑫” (Hi, Espressif)
2. ASR: “叮咚” (the “ding dong” tone)
3. USER: “打开空调” (turn on air conditioner)
4. ASR: “好的” (OK)


## Environment Setup

#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the table below. Please remember to select the board in menuconfig as discussed in Section *Configuration* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "Incompatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Incompatible") |

## Build and Flash

### Default IDF Branch
The default IDF branch of this example is ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

To use “你好小智” (Hi, Xiaozhi) as the wake word, please go to menuconfig > `Wake word name` and select it.

```
menuconfig > ESP Speech Recognition > `Wake word name` > `nihaoxiaozhi (WakeNet5X3)`
```

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.

**NOTE:**

The CMakeLists.txt of this example has a function for automatically flashing the prompt tone. When you run the `idf.py flash` command in the directory of this example to flash firmware, the script will search for the address of the `flash_tone` partition in the partition table, and flash `audio_tone.bin`and `speech_recognition_example.bin` to the corresponding partitions.



## How to Use the Example

### Example Functionality

- The example starts running and is waiting for your wake words. The log is as follows:

```c
W (2225) I2S: I2S driver already installed
I (2226) example_asr_keywords: esp_audio instance is:0x3f80dc80
I (2226) example_asr_keywords: [ 5 ] Start audio_pipeline
```
- When you say the wake word, such as “嗨，乐鑫” (Hi Espressif), the system will respond by making the “ding dong” tone.

```c
W (6152) TONE_STREAM: No more data,ret:0 ,info.byte_pos:8527
W (6586) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (6586) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (6587) example_asr_keywords: wake up
```

- Then, you say a command word, such as “打开蓝牙” (turn on Bluetooth), and the system will respond with the speech “好的” (OK) if it recognizes your speech command.

```c
I (8997) example_asr_keywords: turn on bt
W (8998) AUDIO_PIPELINE: There are no listener registered
W (9040) TONE_STREAM: No more data,ret:0 ,info.byte_pos:6384
W (9536) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (9536) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED

```


### Example Log

The complete log is as follows:

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

## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
