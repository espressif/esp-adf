
# 播放 flash 内嵌的 MP3 音乐

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介


本例程介绍了 mp3 和 I2S 两个 element 实现 MP3 音乐的播放。mp3 element 调用函数回调 read_cb 读取 flash 中的音乐文件，解码后用 I2S element 输出音乐。

同时支持了按键控制功能，如：Play 按键支持开始、暂停、恢复播放； Vol+ 音量加和 Vol- 音量减；以及 MODE/FUNC 按键实现切换不同码率（8000 Hz, 22050 Hz, 44100 Hz）的音频播放，set 键结束示例。


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v5.0 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

### 编译和下载
请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.3/esp32/index.html)。

## 如何使用例程

### 功能和用法

- 例程开始运行后，自动播放低码率 8000 Hz 的 mp3 文件，打印如下：

```c
I (260) PLAY_FLASH_MP3_CONTROL: [3.1] Initialize keys on board
I (260) PLAY_FLASH_MP3_CONTROL: [ 4 ] Set up  event listener
W (280) PERIPH_TOUCH: _touch_init
I (280) PLAY_FLASH_MP3_CONTROL: [4.1] Listening event from all elements of pipeline
I (290) PLAY_FLASH_MP3_CONTROL: [4.2] Listening event from peripherals
W (290) PLAY_FLASH_MP3_CONTROL: [ 5 ] Tap touch buttons to control music player:
W (300) PLAY_FLASH_MP3_CONTROL:       [Play] to start, pause and resume, [Set] to stop.
W (310) PLAY_FLASH_MP3_CONTROL:       [Vol-] or [Vol+] to adjust volume.
I (330) PLAY_FLASH_MP3_CONTROL: [ 5.1 ] Start audio_pipeline
I (340) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
```

- 在播放中，可以使用 [Play] 按键进行暂停播放、恢复播放操作。

```c
I (330) PLAY_FLASH_MP3_CONTROL: [ 5.1 ] Start audio_pipeline
I (340) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (7330) PLAY_FLASH_MP3_CONTROL: [ * ] [Play] touch tap event
I (7330) PLAY_FLASH_MP3_CONTROL: [ * ] Pausing audio pipeline
I (8830) PLAY_FLASH_MP3_CONTROL: [ * ] [Play] touch tap event
I (8830) PLAY_FLASH_MP3_CONTROL: [ * ] Resuming audio pipeline
I (8850) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
```

- 使用音量减 [Vol-] 按键减少音量，使用音量加 [Vol+] 按键增加音量。

```c
I (81590) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
I (106330) PLAY_FLASH_MP3_CONTROL: [ * ] [Vol+] touch tap event
I (106340) PLAY_FLASH_MP3_CONTROL: [ * ] Volume set to 79 %
```

- 使用 [Mode] 按键切换不同码率（8000 Hz、22050 Hz、44100 Hz）的 MP3 播放。

```c
I (330) PLAY_FLASH_MP3_CONTROL: [ 5.1 ] Start audio_pipeline
I (340) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (16230) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (16230) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (16230) MP3_DECODER: output aborted -3
I (16420) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
I (19130) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (19130) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (19130) MP3_DECODER: output aborted -3
I (19190) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (21440) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (21440) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (21440) MP3_DECODER: output aborted -3
I (21480) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
```

- 使用 [Set] 按键可以退出例程。

```c
I (21480) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (63580) PLAY_FLASH_MP3_CONTROL: [ * ] [Set] touch tap event
I (63580) PLAY_FLASH_MP3_CONTROL: [ * ] Stopping audio pipeline
I (63580) PLAY_FLASH_MP3_CONTROL: [ 6 ] Stop audio_pipeline
E (63580) AUDIO_ELEMENT: [mp3] Element already stopped
E (63600) AUDIO_ELEMENT: [i2s] Element already stopped
W (63600) AUDIO_PIPELINE: There are no listener registered
W (63600) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (63610) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
```


### 日志输出
本例选取完整的从启动到初始化完成的 log，示例如下：

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
I (40) boot: compile time 15:30:10
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
I (103) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x8a0d0 (565456) map
I (310) esp_image: segment 1: paddr=0x0009a0f8 vaddr=0x3ffb0000 size=0x01f40 (  8000) load
I (314) esp_image: segment 2: paddr=0x0009c040 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (317) esp_image: segment 3: paddr=0x0009c448 vaddr=0x40080400 size=0x03bc8 ( 15304) load
I (332) esp_image: segment 4: paddr=0x000a0018 vaddr=0x400d0018 size=0x26a64 (158308) map
0x400d0018: _flash_cache_start at ??:?

I (390) esp_image: segment 5: paddr=0x000c6a84 vaddr=0x40083fc8 size=0x06994 ( 27028) load
0x40083fc8: i2c_master_cmd_begin_static at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/driver/i2c.c:1134

I (408) boot: Loaded app from partition at offset 0x10000
I (408) boot: Disabling RNG early entropy source...
I (409) cpu_start: Pro cpu up.
I (412) cpu_start: Application information:
I (417) cpu_start: Project name:     play_mp3_control
I (423) cpu_start: App version:      v2.2-85-gf9fa5c94-dirty
I (429) cpu_start: Compile time:     Apr 27 2021 17:11:15
I (435) cpu_start: ELF file SHA256:  87bfd7bdac8a9d70...
I (441) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (447) cpu_start: Starting app cpu, entry point is 0x40081078
0x40081078: call_start_cpu1 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (458) heap_init: Initializing. RAM available for dynamic allocation:
I (465) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (471) heap_init: At 3FFB2FE0 len 0002D020 (180 KiB): DRAM
I (477) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (483) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (490) heap_init: At 4008A95C len 000156A4 (85 KiB): IRAM
I (496) cpu_start: Pro cpu start user code
I (179) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (180) PLAY_FLASH_MP3_CONTROL: [ 1 ] Start audio codec chip
I (200) PLAY_FLASH_MP3_CONTROL: [ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event
I (200) PLAY_FLASH_MP3_CONTROL: [2.1] Create mp3 decoder to decode mp3 file and set custom read callback
I (220) PLAY_FLASH_MP3_CONTROL: [2.2] Create i2s stream to write data to codec chip
I (230) PLAY_FLASH_MP3_CONTROL: [2.3] Register all elements to audio pipeline
I (230) PLAY_FLASH_MP3_CONTROL: [2.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]
I (240) PLAY_FLASH_MP3_CONTROL: [ 3 ] Initialize peripherals
E (250) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (260) PLAY_FLASH_MP3_CONTROL: [3.1] Initialize keys on board
I (260) PLAY_FLASH_MP3_CONTROL: [ 4 ] Set up  event listener
W (280) PERIPH_TOUCH: _touch_init
I (280) PLAY_FLASH_MP3_CONTROL: [4.1] Listening event from all elements of pipeline
I (290) PLAY_FLASH_MP3_CONTROL: [4.2] Listening event from peripherals
W (290) PLAY_FLASH_MP3_CONTROL: [ 5 ] Tap touch buttons to control music player:
W (300) PLAY_FLASH_MP3_CONTROL:       [Play] to start, pause and resume, [Set] to stop.
W (310) PLAY_FLASH_MP3_CONTROL:       [Vol-] or [Vol+] to adjust volume.
I (330) PLAY_FLASH_MP3_CONTROL: [ 5.1 ] Start audio_pipeline
I (340) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (7330) PLAY_FLASH_MP3_CONTROL: [ * ] [Play] touch tap event
I (7330) PLAY_FLASH_MP3_CONTROL: [ * ] Pausing audio pipeline
I (8830) PLAY_FLASH_MP3_CONTROL: [ * ] [Play] touch tap event
I (8830) PLAY_FLASH_MP3_CONTROL: [ * ] Resuming audio pipeline
I (8850) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (86830) PLAY_FLASH_MP3_CONTROL: [ * ] [Vol+] touch tap event
I (86830) PLAY_FLASH_MP3_CONTROL: [ * ] Volume set to 79 %
I (90590) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
E (90590) AUDIO_ELEMENT: [mp3] Element already stopped
E (90590) AUDIO_ELEMENT: [i2s] Element already stopped
I (90610) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
I (92470) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (92470) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (92470) MP3_DECODER: output aborted -3
I (92540) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (94940) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (94940) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (94940) MP3_DECODER: output aborted -3
W (94940) AUDIO_ELEMENT: IN-[i2s] AEL_IO_ABORT
I (95000) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (96670) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (96670) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (96670) MP3_DECODER: output aborted -3
I (96810) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
I (101680) PLAY_FLASH_MP3_CONTROL: [ * ] [Set] touch tap event
I (101680) PLAY_FLASH_MP3_CONTROL: [ * ] Stopping audio pipeline
I (101680) PLAY_FLASH_MP3_CONTROL: [ 6 ] Stop audio_pipeline
W (101690) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (101690) MP3_DECODER: output aborted -3
W (101760) AUDIO_PIPELINE: There are no listener registered
W (101760) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (101760) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
```

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
