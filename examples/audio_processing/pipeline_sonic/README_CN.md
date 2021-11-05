# 音频变声例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了 ADF 的重采样 (resample) 使用方法，录音的时候把 48000 Hz、16 位、2 通道的音频数据重采样为 16000 Hz、16 位、1 通道的数据，然后编码为 WAV 格式并写入 SD 卡中，回放的时候把 SD 卡中的 16000 Hz、16 位、1 通道的 WAV 文件读出，重采样数据为 48000 Hz、16 位、2 通道，然后通过 I2S 外设把录音数据播放出来。


1.对于录音过程：

- 设置 I2S 并以 48000 Hz、16 位、立体声的采样率获取音频。
- 使用重采样过滤器转换数据为 16000 Hz、16 位、1 通道。
- 使用 WAV 编码器进行数据编码。
- 写入 microSD 卡。

录音的重采样并保存到 SDcard 的 pipeline 如下：

```c
mic ---> codec_chip ---> i2s_stream ---> resample_filter ---> wav_encoder ---> fatfs_stream ---> sdcard

```


2.对于录音回放过程：

- 读取 microSD 卡录制的文件，采样率为 16000 Hz，16 位，1 通道。
- 用 WAV 解码器解码文件数据。
- 使用重采样过滤器转换数据为 48000 Hz、16 位、2 通道。
- 将音频写入 I2S 外设。


读取 SDcard 的文件重采样后通过 I2S 播放 pipeline 如下：

```c

sdcard ---> fatfs_stream ---> wav_decoder ---> resample_filter ---> i2s_stream ---> codec_chip ---> speaker

```


## 环境配置

### 硬件要求

本例程可在标有绿色复选框的开发板上运行。请记住，如下面的 *配置* 一节所述，可以在 `menuconfig` 中选择开发板。

| 开发板名称 | 开始入门 | 芯片 | 兼容性 |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |


## 编译和下载

### IDF 默认分支
本例程默认 IDF 为 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置


本例程需要准备一张 microSD 卡，插入开发板备用，用于保存录音的 WAV 音频文件。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

如果你需要修改录音文件名，本例程建议同时打开 FATFS 长文件名支持。

```
menuconfig > Component config > FAT Filesystem support > Long filename support
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

- 例程开始运行后，等待用户按下 [REC] 录音键录音，打印如下：

```c
ets Jun  8 2016 00:22:57

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7232
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-gf84c8c20f 2nd stage bootloader
I (27) boot: compile time 19:47:37
I (28) boot: chip revision: 1
I (31) boot_comm: chip revision: 1, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (48) boot.esp32: SPI Flash Size : 2MB
I (52) boot: Enabling RNG early entropy source...
I (58) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 1, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x10720 ( 67360) map
I (137) esp_image: segment 1: paddr=0x00020748 vaddr=0x3ffb0000 size=0x02224 (  8740) load
I (141) esp_image: segment 2: paddr=0x00022974 vaddr=0x40080000 size=0x0d6a4 ( 54948) load
0x40080000: _WindowOverflow4 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (168) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x2ee44 (192068) map
0x400d0020: _stext at ??:?

I (241) esp_image: segment 4: paddr=0x0005ee6c vaddr=0x4008d6a4 size=0x005f4 (  1524) load
0x4008d6a4: spi_flash_chip_generic_reset at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/spi_flash/spi_flash_chip_generic.c:52

I (250) boot: Loaded app from partition at offset 0x10000
I (250) boot: Disabling RNG early entropy source...
I (251) cpu_start: Pro cpu up.
I (254) cpu_start: Application information:
I (259) cpu_start: Project name:     sonic_app
I (264) cpu_start: App version:      v2.2-216-gd2bde476-dirty
I (271) cpu_start: Compile time:     Nov  7 2021 19:47:30
I (277) cpu_start: ELF file SHA256:  5d6be7336b4f26b3...
I (283) cpu_start: ESP-IDF:          v4.2.2-1-gf84c8c20f
I (289) cpu_start: Starting app cpu, entry point is 0x400819b8
0x400819b8: call_start_cpu1 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (299) heap_init: Initializing. RAM available for dynamic allocation:
I (306) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (312) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (318) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (325) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (331) heap_init: At 4008DC98 len 00012368 (72 KiB): IRAM
I (337) cpu_start: Pro cpu start user code
I (355) spi_flash: detected chip: generic
I (356) spi_flash: flash io: dio
W (356) spi_flash: Detected size(4096k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (367) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (878) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (898) PERIPH_TOUCH: _touch_init
I (918) SONIC_EXAMPLE: [1.1] Initialize recorder pipeline
I (918) SONIC_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (958) SONIC_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (958) SONIC_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (958) I2S: I2S driver already installed
I (968) SONIC_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (988) SONIC_EXAMPLE: [ 3 ] Set up  event listener
W (988) SONIC_EXAMPLE: Press [Rec] to start recording

```

- 当按下音频板上的 [REC] 按钮时，本例将进行录音，语音数据经过变声后输出，打印如下：

```c
W (988) SONIC_EXAMPLE: Press [Rec] to start recording
E (3108) SONIC_EXAMPLE: Now recording, release [Rec] to STOP
W (3108) AUDIO_PIPELINE: Without stop, st:1
W (3108) AUDIO_PIPELINE: Without wait stop, st:1
W (3108) AUDIO_ELEMENT: [file_reader] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3118) AUDIO_ELEMENT: [wav_decoder] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3128) AUDIO_ELEMENT: [sonic] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3138) AUDIO_ELEMENT: [i2s_writer] Element has not create when AUDIO_ELEMENT_TERMINATE
I (3158) SONIC_EXAMPLE: Setup file path to save recorded audio
I (5938) SONIC_EXAMPLE: START Playback
W (5938) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
E (5938) AUDIO_ELEMENT: [wav_encoder] Element already stopped
W (5948) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
I (5978) SONIC_EXAMPLE: Setup file path to read the wav audio to play
W (6788) FATFS_STREAM: No more data, ret:0

```


### 日志输出
本例选取完整的从启动到初始化完成的 log，示例如下：

```c
ets Jun  8 2016 00:22:57

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7232
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-gf84c8c20f 2nd stage bootloader
I (27) boot: compile time 19:47:37
I (28) boot: chip revision: 1
I (31) boot_comm: chip revision: 1, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (48) boot.esp32: SPI Flash Size : 2MB
I (52) boot: Enabling RNG early entropy source...
I (58) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 1, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x10720 ( 67360) map
I (137) esp_image: segment 1: paddr=0x00020748 vaddr=0x3ffb0000 size=0x02224 (  8740) load
I (141) esp_image: segment 2: paddr=0x00022974 vaddr=0x40080000 size=0x0d6a4 ( 54948) load
0x40080000: _WindowOverflow4 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (168) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x2ee44 (192068) map
0x400d0020: _stext at ??:?

I (241) esp_image: segment 4: paddr=0x0005ee6c vaddr=0x4008d6a4 size=0x005f4 (  1524) load
0x4008d6a4: spi_flash_chip_generic_reset at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/spi_flash/spi_flash_chip_generic.c:52

I (250) boot: Loaded app from partition at offset 0x10000
I (250) boot: Disabling RNG early entropy source...
I (251) cpu_start: Pro cpu up.
I (254) cpu_start: Application information:
I (259) cpu_start: Project name:     sonic_app
I (264) cpu_start: App version:      v2.2-216-gd2bde476-dirty
I (271) cpu_start: Compile time:     Nov  7 2021 19:47:30
I (277) cpu_start: ELF file SHA256:  5d6be7336b4f26b3...
I (283) cpu_start: ESP-IDF:          v4.2.2-1-gf84c8c20f
I (289) cpu_start: Starting app cpu, entry point is 0x400819b8
0x400819b8: call_start_cpu1 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (299) heap_init: Initializing. RAM available for dynamic allocation:
I (306) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (312) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (318) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (325) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (331) heap_init: At 4008DC98 len 00012368 (72 KiB): IRAM
I (337) cpu_start: Pro cpu start user code
I (355) spi_flash: detected chip: generic
I (356) spi_flash: flash io: dio
W (356) spi_flash: Detected size(4096k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (367) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (878) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (898) PERIPH_TOUCH: _touch_init
I (918) SONIC_EXAMPLE: [1.1] Initialize recorder pipeline
I (918) SONIC_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (958) SONIC_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (958) SONIC_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (958) I2S: I2S driver already installed
I (968) SONIC_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (988) SONIC_EXAMPLE: [ 3 ] Set up  event listener
W (988) SONIC_EXAMPLE: Press [Rec] to start recording
E (3108) SONIC_EXAMPLE: Now recording, release [Rec] to STOP
W (3108) AUDIO_PIPELINE: Without stop, st:1
W (3108) AUDIO_PIPELINE: Without wait stop, st:1
W (3108) AUDIO_ELEMENT: [file_reader] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3118) AUDIO_ELEMENT: [wav_decoder] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3128) AUDIO_ELEMENT: [sonic] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3138) AUDIO_ELEMENT: [i2s_writer] Element has not create when AUDIO_ELEMENT_TERMINATE
I (3158) SONIC_EXAMPLE: Setup file path to save recorded audio
I (5938) SONIC_EXAMPLE: START Playback
W (5938) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
E (5938) AUDIO_ELEMENT: [wav_encoder] Element already stopped
W (5948) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
I (5978) SONIC_EXAMPLE: Setup file path to read the wav audio to play
W (6788) FATFS_STREAM: No more data, ret:0

```



## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
