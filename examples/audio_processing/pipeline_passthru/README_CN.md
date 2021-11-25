# 音频透传例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_regular.png "中级")


## 例程简介

本例程演示了如何把 `aux_in` 端口接收到的音频透传到耳机或扬声器接口输出。


本例程透传的 pipeline 如下：

```c

[codec_chip] ---> i2s_stream_reader ---> i2s_stream_writer ---> [codec_chip]

```


## 环境配置

### 硬件要求

本例程可在标有绿色复选框的开发板上运行。请记住，如下面的 *配置* 一节所述，可以在 `menuconfig` 中选择开发板。

| 开发板名称 | 开始入门 | 芯片 | 兼容性 |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |


## 编译和下载

### IDF 默认分支
本例程默认 IDF 为 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置


本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，需要检查开发板是否支持 `AUX_IN` 功能，配置方法在 menuconfig 中选择开发板的配置，例如选择 `ESP32-LyraTD-MSC V2.2`。

```
menuconfig > Audio HAL > ESP32-LyraTD-MSC V2.2
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
I (27) boot: compile time 19:20:43
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0a290 ( 41616) map
I (127) esp_image: segment 1: paddr=0x0001a2b8 vaddr=0x3ffb0000 size=0x02118 (  8472) load
I (131) esp_image: segment 2: paddr=0x0001c3d8 vaddr=0x40080000 size=0x03c40 ( 15424) load
0x40080000: _WindowOverflow4 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (141) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x1f2ac (127660) map
0x400d0020: _stext at ??:?

I (191) esp_image: segment 4: paddr=0x0003f2d4 vaddr=0x40083c40 size=0x082a4 ( 33444) load
0x40083c40: i2c_master_cmd_begin_static at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/driver/i2c.c:1020

I (213) boot: Loaded app from partition at offset 0x10000
I (213) boot: Disabling RNG early entropy source...
I (213) cpu_start: Pro cpu up.
I (217) cpu_start: Application information:
I (222) cpu_start: Project name:     passthru
I (227) cpu_start: App version:      v2.2-216-gd2bde476-dirty
I (233) cpu_start: Compile time:     Nov  7 2021 19:20:38
I (239) cpu_start: ELF file SHA256:  d7a10444c16b7e60...
I (245) cpu_start: ESP-IDF:          v4.2.2-1-gf84c8c20f
I (251) cpu_start: Starting app cpu, entry point is 0x40081704
0x40081704: call_start_cpu1 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (262) heap_init: Initializing. RAM available for dynamic allocation:
I (269) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (275) heap_init: At 3FFB2990 len 0002D670 (181 KiB): DRAM
I (281) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (287) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (294) heap_init: At 4008BEE4 len 0001411C (80 KiB): IRAM
I (300) cpu_start: Pro cpu start user code
I (318) spi_flash: detected chip: generic
I (319) spi_flash: flash io: dio
W (319) spi_flash: Detected size(4096k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (329) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (340) PASSTHRU: [ 1 ] Start codec chip
I (350) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
I (370) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (370) ES8388_DRIVER: init,out:02, in:00
I (380) AUDIO_HAL: Codec mode is 4, Ctrl:1
I (380) PASSTHRU: [ 2 ] Create audio pipeline for playback
I (380) PASSTHRU: [3.1] Create i2s stream to write data to codec chip
I (390) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (400) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (430) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (430) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (430) PASSTHRU: [3.2] Create i2s stream to read data from codec chip
W (440) I2S: I2S driver already installed
I (450) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (450) PASSTHRU: [3.3] Register all elements to audio pipeline
I (460) PASSTHRU: [3.4] Link it together [codec_chip]-->i2s_stream_reader-->i2s_stream_writer-->[codec_chip]
I (470) AUDIO_PIPELINE: link el->rb, el:0x3ffb8824, tag:i2s_read, rb:0x3ffb8ba8
I (480) PASSTHRU: [ 4 ] Set up  event listener
I (480) PASSTHRU: [4.1] Listening event from all elements of pipeline
I (490) PASSTHRU: [ 5 ] Start audio_pipeline
I (490) AUDIO_ELEMENT: [i2s_read-0x3ffb8824] Element task created
I (500) AUDIO_ELEMENT: [i2s_write-0x3ffb8498] Element task created
I (510) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:268596 Bytes

I (510) AUDIO_ELEMENT: [i2s_read] AEL_MSG_CMD_RESUME,state:1
I (520) I2S_STREAM: AUDIO_STREAM_READER,Rate:44100,ch:2
I (550) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (550) AUDIO_ELEMENT: [i2s_write] AEL_MSG_CMD_RESUME,state:1
I (560) I2S_STREAM: AUDIO_STREAM_WRITER
I (560) AUDIO_PIPELINE: Pipeline started
I (570) PASSTHRU: [ 6 ] Listen for all pipeline events


```

- 当按下音频板上的 [REC] 按钮时，本例将进行录音，重采样数据后编码为 WAV 文件，并写入到 SD 卡中保存,打印如下：


```c

```



### 日志输出
本例选取完整的从启动到初始化完成的 log，示例如下：

```c

```



## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
