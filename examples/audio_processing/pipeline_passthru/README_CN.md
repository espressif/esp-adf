# 音频透传例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_regular.png "中级")


## 例程简介

本例程演示了如何把 `aux_in` 端口接收到的音频透传到耳机或扬声器接口输出。

音频透传的应用场景：

- 在启用新硬件设计时，从头到尾验证音频管道完整性。
- 通过音频路径检查左右声道的一致性。
- 与音频测试装置结合使用测量 THD+N，用于生产线测试或性能评估。

本例程透传的管道如下：

```

[codec_chip] ---> i2s_stream_reader ---> i2s_stream_writer ---> [codec_chip]

```


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程需要准备一条 aux 音频线，连接开发板的 `AUX_IN` 和音频输出端的 `Line-Out`。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，目前仅支持该开发板。


### 编译和下载
请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。

## 如何使用例程

### 功能和用法

- 例程开始运行后，等待 `AUX_IN` 信号输入。本例使用 aux 线，一端连接电脑的 `Line-Out`，一端插入开发板的 `AUX_IN` 插孔，打印如下：

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 11:27:17
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 2MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0a194 ( 41364) map
I (127) esp_image: segment 1: paddr=0x0001a1bc vaddr=0x3ffb0000 size=0x02110 (  8464) load
I (131) esp_image: segment 2: paddr=0x0001c2d4 vaddr=0x40080000 size=0x03d44 ( 15684) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (140) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x1f2ac (127660) map
0x400d0020: _stext at ??:?

I (191) esp_image: segment 4: paddr=0x0003f2d4 vaddr=0x40083d44 size=0x081a0 ( 33184) load
0x40083d44: i2c_master_cmd_begin_static at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/driver/i2c.c:1014

I (212) boot: Loaded app from partition at offset 0x10000
I (213) boot: Disabling RNG early entropy source...
I (213) cpu_start: Pro cpu up.
I (217) cpu_start: Application information:
I (221) cpu_start: Project name:     passthru
I (226) cpu_start: App version:      v2.2-216-gbb375dee-dirty
I (233) cpu_start: Compile time:     Nov  8 2021 11:27:11
I (239) cpu_start: ELF file SHA256:  26639813b619eb6a...
I (245) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (251) cpu_start: Starting app cpu, entry point is 0x40081704
0x40081704: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (261) heap_init: Initializing. RAM available for dynamic allocation:
I (268) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (274) heap_init: At 3FFB2988 len 0002D678 (181 KiB): DRAM
I (281) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (287) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (293) heap_init: At 4008BEE4 len 0001411C (80 KiB): IRAM
I (299) cpu_start: Pro cpu start user code
I (318) spi_flash: detected chip: gd
I (318) spi_flash: flash io: dio
W (318) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (328) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (339) PASSTHRU: [ 1 ] Start codec chip
I (349) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (369) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (369) ES8388_DRIVER: init,out:02, in:00
I (379) AUDIO_HAL: Codec mode is 4, Ctrl:1
I (379) PASSTHRU: [ 2 ] Create audio pipeline for playback
I (379) PASSTHRU: [3.1] Create i2s stream to write data to codec chip
I (389) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (399) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (429) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (429) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (429) PASSTHRU: [3.2] Create i2s stream to read data from codec chip
W (439) I2S: I2S driver already installed
I (449) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (449) PASSTHRU: [3.3] Register all elements to audio pipeline
I (459) PASSTHRU: [3.4] Link it together [codec_chip]-->i2s_stream_reader-->i2s_stream_writer-->[codec_chip]
I (469) AUDIO_PIPELINE: link el->rb, el:0x3ffb881c, tag:i2s_read, rb:0x3ffb8ba0
I (479) PASSTHRU: [ 4 ] Set up  event listener
I (479) PASSTHRU: [4.1] Listening event from all elements of pipeline
I (489) PASSTHRU: [ 5 ] Start audio_pipeline
I (489) AUDIO_ELEMENT: [i2s_read-0x3ffb881c] Element task created
I (499) AUDIO_ELEMENT: [i2s_write-0x3ffb8490] Element task created
I (509) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:268604 Bytes

I (519) AUDIO_ELEMENT: [i2s_read] AEL_MSG_CMD_RESUME,state:1
I (519) I2S_STREAM: AUDIO_STREAM_READER,Rate:44100,ch:2
I (549) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (549) AUDIO_ELEMENT: [i2s_write] AEL_MSG_CMD_RESUME,state:1
I (559) I2S_STREAM: AUDIO_STREAM_WRITER
I (559) AUDIO_PIPELINE: Pipeline started
I (569) PASSTHRU: [ 6 ] Listen for all pipeline events
I (9959) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (9959) HEADPHONE: Headphone jack removed

```


### 日志输出
以下是本例程的完整日志。

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 11:27:17
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 2MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0a194 ( 41364) map
I (127) esp_image: segment 1: paddr=0x0001a1bc vaddr=0x3ffb0000 size=0x02110 (  8464) load
I (131) esp_image: segment 2: paddr=0x0001c2d4 vaddr=0x40080000 size=0x03d44 ( 15684) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (140) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x1f2ac (127660) map
0x400d0020: _stext at ??:?

I (191) esp_image: segment 4: paddr=0x0003f2d4 vaddr=0x40083d44 size=0x081a0 ( 33184) load
0x40083d44: i2c_master_cmd_begin_static at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/driver/i2c.c:1014

I (212) boot: Loaded app from partition at offset 0x10000
I (213) boot: Disabling RNG early entropy source...
I (213) cpu_start: Pro cpu up.
I (217) cpu_start: Application information:
I (221) cpu_start: Project name:     passthru
I (226) cpu_start: App version:      v2.2-216-gbb375dee-dirty
I (233) cpu_start: Compile time:     Nov  8 2021 11:27:11
I (239) cpu_start: ELF file SHA256:  26639813b619eb6a...
I (245) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (251) cpu_start: Starting app cpu, entry point is 0x40081704
0x40081704: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (261) heap_init: Initializing. RAM available for dynamic allocation:
I (268) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (274) heap_init: At 3FFB2988 len 0002D678 (181 KiB): DRAM
I (281) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (287) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (293) heap_init: At 4008BEE4 len 0001411C (80 KiB): IRAM
I (299) cpu_start: Pro cpu start user code
I (318) spi_flash: detected chip: gd
I (318) spi_flash: flash io: dio
W (318) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (328) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (339) PASSTHRU: [ 1 ] Start codec chip
I (349) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (369) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (369) ES8388_DRIVER: init,out:02, in:00
I (379) AUDIO_HAL: Codec mode is 4, Ctrl:1
I (379) PASSTHRU: [ 2 ] Create audio pipeline for playback
I (379) PASSTHRU: [3.1] Create i2s stream to write data to codec chip
I (389) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (399) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (429) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (429) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (429) PASSTHRU: [3.2] Create i2s stream to read data from codec chip
W (439) I2S: I2S driver already installed
I (449) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (449) PASSTHRU: [3.3] Register all elements to audio pipeline
I (459) PASSTHRU: [3.4] Link it together [codec_chip]-->i2s_stream_reader-->i2s_stream_writer-->[codec_chip]
I (469) AUDIO_PIPELINE: link el->rb, el:0x3ffb881c, tag:i2s_read, rb:0x3ffb8ba0
I (479) PASSTHRU: [ 4 ] Set up  event listener
I (479) PASSTHRU: [4.1] Listening event from all elements of pipeline
I (489) PASSTHRU: [ 5 ] Start audio_pipeline
I (489) AUDIO_ELEMENT: [i2s_read-0x3ffb881c] Element task created
I (499) AUDIO_ELEMENT: [i2s_write-0x3ffb8490] Element task created
I (509) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:268604 Bytes

I (519) AUDIO_ELEMENT: [i2s_read] AEL_MSG_CMD_RESUME,state:1
I (519) I2S_STREAM: AUDIO_STREAM_READER,Rate:44100,ch:2
I (549) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (549) AUDIO_ELEMENT: [i2s_write] AEL_MSG_CMD_RESUME,state:1
I (559) I2S_STREAM: AUDIO_STREAM_WRITER
I (559) AUDIO_PIPELINE: Pipeline started
I (569) PASSTHRU: [ 6 ] Listen for all pipeline events
I (9959) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (9959) HEADPHONE: Headphone jack removed

```

## 故障排除

如果开发板没有声音，或者声音很微弱，这是因为默认输入 `AUX_IN` 的音量较微弱，所以请将 `Line-Out` 端音量调高，即可听到开发板输出的音频。


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
