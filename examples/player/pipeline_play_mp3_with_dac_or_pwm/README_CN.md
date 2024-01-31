# 使用 PWM 或 I2S-DAC 播放 flash 内嵌的 MP3 音乐

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例使用 MP3 解码器的回调函数 `mp3_music_read_cb` 读取嵌入在 flash 中的 MP3 文件，数据经过解码器解码处理后，最终通过 PWM 方式输出音乐。

需要说明，对于 ESP32 的 I2S-DAC 输出，只有 GPIO25，GPIO26 两个管脚可使用； 对于 PWM 方式输出，只需要 GPIO 是具有输出功能的管脚即可。

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


**注意：**
- 如果选择 PWM 输出音乐，您需要一个 ESP32 开发板，其有合适的 GPIO 外露输出 PWM 波形。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`。

本例程选择 `ESP32-Lyrat V4.3` 开发板上外漏的 I2C_SCK 和 I2C_SDA 两个管脚输出 PWM 波形。

```c
menuconfig > Example Configuration > PWM Stream Right Output GPIO NUM > 18
```

```c
menuconfig > Example Configuration > PWM Stream Left Output GPIO NUM > 23
```

### 编译和下载
请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。

## 如何使用例程
### 功能和用法

**1. PWM 输出方式**

依据下图把 ESP32 开发板的 GPIO18、GPIO23 和 GND 管脚连接到耳机，如果使用其他 GPIO，请在 `menuconfig` 中更改。


PWM 输出连接方式：

```
GPIO18 +--------------+
                      |  __  
                      | /  \ _
                     +-+    | |
                     | |    | |  Earphone
                     +-+    |_|
                      | \__/
             330R     |
GND +-------/\/\/\----+
                      |  __  
                      | /  \ _
                     +-+    | |
                     | |    | |  Earphone
                     +-+    |_|
                      | \__/
                      |
GPIO23 +--------------+
``` 

### 日志输出
本例选取完整的从启动到初始化完成的 log，示例如下：

```c
I (66) boot: Chip Revision: 3
I (71) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (42) boot: ESP-IDF v3.3.2-107-g722043f73 2nd stage bootloader
I (42) boot: compile time 17:07:48
I (42) boot: Enabling RNG early entropy source...
I (48) qio_mode: Enabling default flash chip QIO
I (53) boot: SPI Speed      : 80MHz
I (57) boot: SPI Mode       : QIO
I (61) boot: SPI Flash Size : 8MB
I (65) boot: Partition Table:
I (69) boot: ## Label            Usage          Type ST Offset   Length
I (76) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (83) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (91) boot:  2 factory          factory app      00 00 00010000 00100000
I (98) boot: End of partition table
I (103) boot_comm: chip revision: 3, min. application chip revision: 0
I (110) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x22174 (139636) map
I (156) esp_image: segment 1: paddr=0x0003219c vaddr=0x3ffb0000 size=0x01ee4 (  7908) load
I (159) esp_image: segment 2: paddr=0x00034088 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /workshop/audio/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (163) esp_image: segment 3: paddr=0x00034490 vaddr=0x40080400 size=0x08f5c ( 36700) load
I (184) esp_image: segment 4: paddr=0x0003d3f4 vaddr=0x00000000 size=0x02c1c ( 11292) 
I (187) esp_image: segment 5: paddr=0x00040018 vaddr=0x400d0018 size=0x1ec20 (125984) map
0x400d0018: _flash_cache_start at ??:?

I (230) boot: Loaded app from partition at offset 0x10000
I (230) boot: Disabling RNG early entropy source...
I (231) cpu_start: Pro cpu up.
I (234) cpu_start: Application information:
I (239) cpu_start: Project name:     play_mp3_pwm_or_dac
I (245) cpu_start: App version:      v2.2-151-g0db7cb28-dirty
I (252) cpu_start: Compile time:     Jun 23 2021 17:07:46
I (258) cpu_start: ELF file SHA256:  ba34710dd9b2148c...
I (264) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (270) cpu_start: Starting app cpu, entry point is 0x40080fac
0x40080fac: call_start_cpu1 at /workshop/audio/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (280) heap_init: Initializing. RAM available for dynamic allocation:
I (287) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (293) heap_init: At 3FFB2F70 len 0002D090 (180 KiB): DRAM
I (299) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (306) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (312) heap_init: At 4008935C len 00016CA4 (91 KiB): IRAM
I (318) cpu_start: Pro cpu start user code
I (335) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (336) MP3_PWM_DAC_EXAMPLE: [ 1 ] Periph init
I (336) MP3_PWM_DAC_EXAMPLE: [ 2 ] Create audio pipeline for playback
I (346) MP3_PWM_DAC_EXAMPLE: [2.1] Create output stream to write data to codec chip
I (356) MP3_PWM_DAC_EXAMPLE: [2.2] Create wav decoder to decode wav file
I (356) MP3_PWM_DAC_EXAMPLE: [2.3] Register all elements to audio pipeline
I (366) MP3_PWM_DAC_EXAMPLE: [2.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->output_stream-->[codec_chip]
I (376) MP3_PWM_DAC_EXAMPLE: [ 3 ] Set up  event listener
I (386) MP3_PWM_DAC_EXAMPLE: [3.1] Listening event from all elements of pipeline
I (396) MP3_PWM_DAC_EXAMPLE: [3.2] Listening event from peripherals
I (396) MP3_PWM_DAC_EXAMPLE: [ 4 ] Start audio_pipeline
I (406) MP3_PWM_DAC_EXAMPLE: [ 5 ] Listen for all pipeline events
I (416) MP3_PWM_DAC_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (7026) MP3_PWM_DAC_EXAMPLE: [ * ] Stop event received
I (7026) MP3_PWM_DAC_EXAMPLE: [ 6 ] Stop audio_pipeline
E (7026) AUDIO_ELEMENT: [mp3] Element already stopped
E (7026) AUDIO_ELEMENT: [output] Element already stopped
W (7036) AUDIO_PIPELINE: There are no listener registered
W (7036) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7046) AUDIO_ELEMENT: [output] Element has not create when AUDIO_ELEMENT_TERMINATE

```

## Troubleshooting

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
