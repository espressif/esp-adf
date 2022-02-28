# 音频塑造 (audio forge) 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_regular.png "中级")


## 例程简介

此示例展示了如何使用 ESP-ADF 的多输入管道通过音频塑造 (audio forge) 处理播放多个音频文件。

音频塑造包含了重采样 (resample)、向下混叠 (downmix)、自动电平控制 (ALC)、均衡器 (equalizer)、变声 (sonic) 等处理过程，通过 audio_forge 的结构体成员 component_select 的掩码来组合这几个功能的选择。


此例程音频塑造的管道如下：

```

              ---> fatfs_stream ---> wav_decoder ---> raw_stream ---> 
             |                                                       |
[sdcard] --->                (NUMBER_SOURCE_FILE)                     ---> audio_forge ---> i2s_stream ---> [codec_chip]
             |                                                       |          ▲
              ---> fatfs_stream ---> wav_decoder ---> raw_stream --->           |      
                                                                        ┌───────┴────────┐
                                                                        │    resample    │
                                                                        │    downmix     │
                                                                        │    ALC         │
                                                                        │    equalizer   │
                                                                        │    sonic       │
                                                                        └────────────────┘

```


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程需要准备一张 microSD 卡以及两首 WAV 音频文件，将音频文件分别重命名为 `test1.wav` 和 `test2.wav` 并拷贝到 microSD 卡中，然后把 microSD 卡插入开发板备用，

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

- 例程开始运行后，等待用户按下 [Mode] 按键开始执行例程，打印如下：

```c
ets Jun  8 2016 00:22:57

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7584
load:0x40078000,len:14368
load:0x40080400,len:5360
0x40080400: _init at ??:?

entry 0x40080710
I (27) boot: ESP-IDF v4.2.2-1-gf84c8c20f 2nd stage bootloader
I (27) boot: compile time 14:52:15
I (27) boot: chip revision: 1
I (31) boot_comm: chip revision: 1, min. bootloader chip revision: 0
I (38) qio_mode: Enabling default flash chip QIO
I (44) boot.esp32: SPI Speed      : 80MHz
I (48) boot.esp32: SPI Mode       : QIO
I (53) boot.esp32: SPI Flash Size : 4MB
I (57) boot: Enabling RNG early entropy source...
I (63) boot: Partition Table:
I (66) boot: ## Label            Usage          Type ST Offset   Length
I (74) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (81) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (89) boot:  2 factory          factory app      00 00 00010000 00300000
I (96) boot: End of partition table
I (100) boot_comm: chip revision: 1, min. application chip revision: 0
I (107) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x3cdb8 (249272) map
I (199) esp_image: segment 1: paddr=0x0004cde0 vaddr=0x3ffb0000 size=0x0231c (  8988) load
I (203) esp_image: segment 2: paddr=0x0004f104 vaddr=0x40080000 size=0x00f14 (  3860) load
0x40080000: _WindowOverflow4 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (207) esp_image: segment 3: paddr=0x00050020 vaddr=0x400d0020 size=0x393e4 (234468) map
0x400d0020: _stext at ??:?

I (292) esp_image: segment 4: paddr=0x0008940c vaddr=0x40080f14 size=0x10108 ( 65800) load
I (328) boot: Loaded app from partition at offset 0x10000
I (328) boot: Disabling RNG early entropy source...
I (328) psram: This chip is ESP32-D0WD
I (333) spiram: Found 64MBit SPI RAM device
I (337) spiram: SPI RAM mode: flash 80m sram 80m
I (343) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (350) cpu_start: Pro cpu up.
I (354) cpu_start: Application information:
I (358) cpu_start: Project name:     audio_forge_pipeline
I (365) cpu_start: App version:      v2.2-216-gd2bde476
I (370) cpu_start: Compile time:     Nov  6 2021 14:52:09
I (377) cpu_start: ELF file SHA256:  b99aae6359e77712...
I (383) cpu_start: ESP-IDF:          v4.2.2-1-gf84c8c20f
I (388) cpu_start: Starting app cpu, entry point is 0x40081dd4
0x40081dd4: call_start_cpu1 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (895) spiram: SPI SRAM memory test OK
I (895) heap_init: Initializing. RAM available for dynamic allocation:
I (895) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (901) heap_init: At 3FFB2DB0 len 0002D250 (180 KiB): DRAM
I (908) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (914) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (920) heap_init: At 4009101C len 0000EFE4 (59 KiB): IRAM
I (927) cpu_start: Pro cpu start user code
I (931) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (952) spi_flash: detected chip: generic
I (952) spi_flash: flash io: qio
I (952) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (965) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (974) AUDIO_FORGE_PIPELINE: [1.0] Start audio codec chip
I (980) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
I (1003) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (1003) ES8388_DRIVER: init,out:02, in:00
I (1012) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (1016) AUDIO_FORGE_PIPELINE: [2.0] Start and wait for SDCARD to mount
E (1018) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1026) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1089) SDCARD: CID name BB1QT!

I (1534) AUDIO_FORGE_PIPELINE: [3.0] Create pipeline_mix to mix
I (1534) AUDIO_FORGE_PIPELINE: [3.1] Create audio_forge
I (1535) AUDIO_FORGE_PIPELINE: [3.2] Create i2s stream to read audio data from codec chip
I (1540) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
I (1553) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
W (1580) PERIPH_TOUCH: _touch_init
I (1581) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1581) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1611) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1615) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1645) I2S: APLL: Req RATE: 11025, real rate: 11024.991, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 2822397.750, SCLK: 352799.718750, diva: 1, divb: 0
I (1648) AUDIO_FORGE_PIPELINE: [3.3] Link elements together audio_forge-->i2s_writer
I (1657) I2S_STREAM: AUDIO_STREAM_WRITER
I (1661) AUDIO_FORGE_PIPELINE: [3.4] Link elements together audio_forge-->i2s_stream-->[codec_chip]
I (1671) AUDIO_FORGE_PIPELINE: [4.0] Create Fatfs stream to read input data
I (1678) AUDIO_FORGE_PIPELINE: [4.1] Create wav decoder to decode wav file
I (1686) AUDIO_FORGE_PIPELINE: [4.2] Create raw stream of base wav to write data
I (1694) AUDIO_FORGE_PIPELINE: [5.0] Set up  event listener
I (1701) AUDIO_PIPELINE: link el->rb, el:0x3f8071c4, tag:file, rb:0x3f8075ac
I (1709) AUDIO_PIPELINE: link el->rb, el:0x3f807324, tag:wav, rb:0x3f8095ec
I (1718) AUDIO_PIPELINE: link el->rb, el:0x3f80b694, tag:file, rb:0x3f80ba7c
I (1724) AUDIO_PIPELINE: link el->rb, el:0x3f80b7f4, tag:wav, rb:0x3f80dabc
I (1731) AUDIO_FORGE_PIPELINE: [5.1] Listening event from peripherals
```

- 当按下音频板上的 [Mode] 按钮时，本例读取 microSD 卡中的 WAV 文件，然后根据音频塑造的配置处理音频并播放，打印如下：

```c
E (71359) AUDIO_FORGE_PIPELINE: audio_forge start
I (71360) AUDIO_ELEMENT: [file-0x3f8071c4] Element task created
I (71361) AUDIO_ELEMENT: [wav-0x3f807324] Element task created
I (71366) AUDIO_ELEMENT: [raw-0x3f807444] Element task created
I (71373) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4372696 Bytes, Inter:331572 Bytes, Dram:270196 Bytes

I (71385) AUDIO_ELEMENT: [file] AEL_MSG_CMD_RESUME,state:1
I (71391) AUDIO_ELEMENT: [wav] AEL_MSG_CMD_RESUME,state:1
I (71398) AUDIO_PIPELINE: Pipeline started
I (71401) FATFS_STREAM: File size: 31670098 byte, file position: 0
I (71410) AUDIO_ELEMENT: [file-0x3f80b694] Element task created
I (71412) CODEC_ELEMENT_HELPER: The element is 0x3f807324. The reserve data 2 is 0x0.
I (71424) WAV_DECODER: a new song playing
I (71430) AUDIO_ELEMENT: [wav-0x3f80b7f4] Element task created
I (71436) AUDIO_ELEMENT: [raw-0x3f80b914] Element task created
I (71441) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4362496 Bytes, Inter:323936 Bytes, Dram:262560 Bytes

I (71453) AUDIO_ELEMENT: [file] AEL_MSG_CMD_RESUME,state:1
I (71460) AUDIO_ELEMENT: [wav] AEL_MSG_CMD_RESUME,state:1
I (71466) AUDIO_PIPELINE: Pipeline started
I (71471) AUDIO_ELEMENT: [audio_forge-0x3f806b80] Element task created
I (71473) FATFS_STREAM: File size: 42346812 byte, file position: 0
I (71485) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4355984 Bytes, Inter:319480 Bytes, Dram:258104 Bytes

I (71487) CODEC_ELEMENT_HELPER: The element is 0x3f80b7f4. The reserve data 2 is 0x0.
I (71505) WAV_DECODER: a new song playing
I (71510) AUDIO_ELEMENT: [audio_forge] AEL_MSG_CMD_RESUME,state:1
I (71517) AUDIO_FORGE: audio_forge opened
I (71517) AUDIO_PIPELINE: Pipeline started
W (71526) AUDIO_FORGE_PIPELINE: [ * ] Receive music info from wav decoder, sample_rates=44100, bits=16, ch=2
W (71537) AUDIO_FORGE_PIPELINE: [ * ] Receive music info from wav decoder, sample_rates=44100, bits=16, ch=2
I (71552) AUDIO_FORGE: audio_forge reopen
W (250912) FATFS_STREAM: No more data, ret:0
I (250912) AUDIO_ELEMENT: IN-[file] AEL_IO_DONE,0
I (250957) AUDIO_ELEMENT: IN-[wav] AEL_IO_DONE,-2
I (250957) DEC_WAV: Closed
W (311470) FATFS_STREAM: No more data, ret:0
I (311470) AUDIO_ELEMENT: IN-[file] AEL_IO_DONE,0
I (311472) AUDIO_ELEMENT: IN-[wav] AEL_IO_DONE,-2
I (311475) DEC_WAV: Closed
I (311526) AUDIO_FORGE: audio forge closed
I (311527) AUDIO_FORGE_PIPELINE: [6.0] Stop pipelines
E (311527) AUDIO_ELEMENT: [file] Element already stopped
E (311531) AUDIO_ELEMENT: [wav] Element already stopped
E (311538) AUDIO_ELEMENT: [file] Element already stopped
E (311543) AUDIO_ELEMENT: [wav] Element already stopped
E (311549) AUDIO_ELEMENT: [audio_forge] Element already stopped
W (311556) AUDIO_PIPELINE: There are no listener registered
I (311563) AUDIO_PIPELINE: audio_pipeline_unlinked
W (311569) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (311577) AUDIO_ELEMENT: [wav] Element has not create when AUDIO_ELEMENT_TERMINATE
I (311584) CODEC_ELEMENT_HELPER: The element is 0x3f807324. The reserve data 2 is 0x0.
W (311593) AUDIO_ELEMENT: [raw] Element has not create when AUDIO_ELEMENT_TERMINATE
W (311601) AUDIO_PIPELINE: There are no listener registered
I (311609) AUDIO_PIPELINE: audio_pipeline_unlinked
W (311613) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (311621) AUDIO_ELEMENT: [wav] Element has not create when AUDIO_ELEMENT_TERMINATE
I (311630) CODEC_ELEMENT_HELPER: The element is 0x3f80b7f4. The reserve data 2 is 0x0.
W (311638) AUDIO_ELEMENT: [raw] Element has not create when AUDIO_ELEMENT_TERMINATE
W (311647) AUDIO_PIPELINE: There are no listener registered
I (311653) AUDIO_PIPELINE: audio_pipeline_unlinked
W (311659) AUDIO_ELEMENT: [audio_forge] Element has not create when AUDIO_ELEMENT_TERMINATE
I (311667) AUDIO_FORGE: audio_forge_destroy
I (311672) AUDIO_FORGE: Func:audio_forge_destroy, Line:377, MEM Total:4412628 Bytes, Inter:333576 Bytes, Dram:272200 Bytes

W (311684) AUDIO_ELEMENT: [iis] Element has not create when AUDIO_ELEMENT_TERMINATE

```


### 日志输出
以下是本例程的完整日志。

```c
ets Jun  8 2016 00:22:57

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7584
load:0x40078000,len:14368
load:0x40080400,len:5360
0x40080400: _init at ??:?

entry 0x40080710
I (27) boot: ESP-IDF v4.2.2-1-gf84c8c20f 2nd stage bootloader
I (27) boot: compile time 14:52:15
I (27) boot: chip revision: 1
I (31) boot_comm: chip revision: 1, min. bootloader chip revision: 0
I (38) qio_mode: Enabling default flash chip QIO
I (44) boot.esp32: SPI Speed      : 80MHz
I (48) boot.esp32: SPI Mode       : QIO
I (53) boot.esp32: SPI Flash Size : 4MB
I (57) boot: Enabling RNG early entropy source...
I (63) boot: Partition Table:
I (66) boot: ## Label            Usage          Type ST Offset   Length
I (74) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (81) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (89) boot:  2 factory          factory app      00 00 00010000 00300000
I (96) boot: End of partition table
I (100) boot_comm: chip revision: 1, min. application chip revision: 0
I (107) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x3cdb8 (249272) map
I (199) esp_image: segment 1: paddr=0x0004cde0 vaddr=0x3ffb0000 size=0x0231c (  8988) load
I (203) esp_image: segment 2: paddr=0x0004f104 vaddr=0x40080000 size=0x00f14 (  3860) load
0x40080000: _WindowOverflow4 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (207) esp_image: segment 3: paddr=0x00050020 vaddr=0x400d0020 size=0x393e4 (234468) map
0x400d0020: _stext at ??:?

I (292) esp_image: segment 4: paddr=0x0008940c vaddr=0x40080f14 size=0x10108 ( 65800) load
I (328) boot: Loaded app from partition at offset 0x10000
I (328) boot: Disabling RNG early entropy source...
I (328) psram: This chip is ESP32-D0WD
I (333) spiram: Found 64MBit SPI RAM device
I (337) spiram: SPI RAM mode: flash 80m sram 80m
I (343) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (350) cpu_start: Pro cpu up.
I (354) cpu_start: Application information:
I (358) cpu_start: Project name:     audio_forge_pipeline
I (365) cpu_start: App version:      v2.2-216-gd2bde476
I (370) cpu_start: Compile time:     Nov  6 2021 14:52:09
I (377) cpu_start: ELF file SHA256:  b99aae6359e77712...
I (383) cpu_start: ESP-IDF:          v4.2.2-1-gf84c8c20f
I (388) cpu_start: Starting app cpu, entry point is 0x40081dd4
0x40081dd4: call_start_cpu1 at /home/hengyongchao/espressif/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (895) spiram: SPI SRAM memory test OK
I (895) heap_init: Initializing. RAM available for dynamic allocation:
I (895) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (901) heap_init: At 3FFB2DB0 len 0002D250 (180 KiB): DRAM
I (908) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (914) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (920) heap_init: At 4009101C len 0000EFE4 (59 KiB): IRAM
I (927) cpu_start: Pro cpu start user code
I (931) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (952) spi_flash: detected chip: generic
I (952) spi_flash: flash io: qio
I (952) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (965) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (974) AUDIO_FORGE_PIPELINE: [1.0] Start audio codec chip
I (980) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
I (1003) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (1003) ES8388_DRIVER: init,out:02, in:00
I (1012) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (1016) AUDIO_FORGE_PIPELINE: [2.0] Start and wait for SDCARD to mount
E (1018) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1026) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1089) SDCARD: CID name BB1QT!

I (1534) AUDIO_FORGE_PIPELINE: [3.0] Create pipeline_mix to mix
I (1534) AUDIO_FORGE_PIPELINE: [3.1] Create audio_forge
I (1535) AUDIO_FORGE_PIPELINE: [3.2] Create i2s stream to read audio data from codec chip
I (1540) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
I (1553) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
W (1580) PERIPH_TOUCH: _touch_init
I (1581) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1581) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1611) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1615) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1645) I2S: APLL: Req RATE: 11025, real rate: 11024.991, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 2822397.750, SCLK: 352799.718750, diva: 1, divb: 0
I (1648) AUDIO_FORGE_PIPELINE: [3.3] Link elements together audio_forge-->i2s_writer
I (1657) I2S_STREAM: AUDIO_STREAM_WRITER
I (1661) AUDIO_FORGE_PIPELINE: [3.4] Link elements together audio_forge-->i2s_stream-->[codec_chip]
I (1671) AUDIO_FORGE_PIPELINE: [4.0] Create Fatfs stream to read input data
I (1678) AUDIO_FORGE_PIPELINE: [4.1] Create wav decoder to decode wav file
I (1686) AUDIO_FORGE_PIPELINE: [4.2] Create raw stream of base wav to write data
I (1694) AUDIO_FORGE_PIPELINE: [5.0] Set up  event listener
I (1701) AUDIO_PIPELINE: link el->rb, el:0x3f8071c4, tag:file, rb:0x3f8075ac
I (1709) AUDIO_PIPELINE: link el->rb, el:0x3f807324, tag:wav, rb:0x3f8095ec
I (1718) AUDIO_PIPELINE: link el->rb, el:0x3f80b694, tag:file, rb:0x3f80ba7c
I (1724) AUDIO_PIPELINE: link el->rb, el:0x3f80b7f4, tag:wav, rb:0x3f80dabc
I (1731) AUDIO_FORGE_PIPELINE: [5.1] Listening event from peripherals
E (71359) AUDIO_FORGE_PIPELINE: audio_forge start
I (71360) AUDIO_ELEMENT: [file-0x3f8071c4] Element task created
I (71361) AUDIO_ELEMENT: [wav-0x3f807324] Element task created
I (71366) AUDIO_ELEMENT: [raw-0x3f807444] Element task created
I (71373) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4372696 Bytes, Inter:331572 Bytes, Dram:270196 Bytes

I (71385) AUDIO_ELEMENT: [file] AEL_MSG_CMD_RESUME,state:1
I (71391) AUDIO_ELEMENT: [wav] AEL_MSG_CMD_RESUME,state:1
I (71398) AUDIO_PIPELINE: Pipeline started
I (71401) FATFS_STREAM: File size: 31670098 byte, file position: 0
I (71410) AUDIO_ELEMENT: [file-0x3f80b694] Element task created
I (71412) CODEC_ELEMENT_HELPER: The element is 0x3f807324. The reserve data 2 is 0x0.
I (71424) WAV_DECODER: a new song playing
I (71430) AUDIO_ELEMENT: [wav-0x3f80b7f4] Element task created
I (71436) AUDIO_ELEMENT: [raw-0x3f80b914] Element task created
I (71441) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4362496 Bytes, Inter:323936 Bytes, Dram:262560 Bytes

I (71453) AUDIO_ELEMENT: [file] AEL_MSG_CMD_RESUME,state:1
I (71460) AUDIO_ELEMENT: [wav] AEL_MSG_CMD_RESUME,state:1
I (71466) AUDIO_PIPELINE: Pipeline started
I (71471) AUDIO_ELEMENT: [audio_forge-0x3f806b80] Element task created
I (71473) FATFS_STREAM: File size: 42346812 byte, file position: 0
I (71485) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4355984 Bytes, Inter:319480 Bytes, Dram:258104 Bytes

I (71487) CODEC_ELEMENT_HELPER: The element is 0x3f80b7f4. The reserve data 2 is 0x0.
I (71505) WAV_DECODER: a new song playing
I (71510) AUDIO_ELEMENT: [audio_forge] AEL_MSG_CMD_RESUME,state:1
I (71517) AUDIO_FORGE: audio_forge opened
I (71517) AUDIO_PIPELINE: Pipeline started
W (71526) AUDIO_FORGE_PIPELINE: [ * ] Receive music info from wav decoder, sample_rates=44100, bits=16, ch=2
W (71537) AUDIO_FORGE_PIPELINE: [ * ] Receive music info from wav decoder, sample_rates=44100, bits=16, ch=2
I (71552) AUDIO_FORGE: audio_forge reopen
W (250912) FATFS_STREAM: No more data, ret:0
I (250912) AUDIO_ELEMENT: IN-[file] AEL_IO_DONE,0
I (250957) AUDIO_ELEMENT: IN-[wav] AEL_IO_DONE,-2
I (250957) DEC_WAV: Closed
W (311470) FATFS_STREAM: No more data, ret:0
I (311470) AUDIO_ELEMENT: IN-[file] AEL_IO_DONE,0
I (311472) AUDIO_ELEMENT: IN-[wav] AEL_IO_DONE,-2
I (311475) DEC_WAV: Closed
I (311526) AUDIO_FORGE: audio forge closed
I (311527) AUDIO_FORGE_PIPELINE: [6.0] Stop pipelines
E (311527) AUDIO_ELEMENT: [file] Element already stopped
E (311531) AUDIO_ELEMENT: [wav] Element already stopped
E (311538) AUDIO_ELEMENT: [file] Element already stopped
E (311543) AUDIO_ELEMENT: [wav] Element already stopped
E (311549) AUDIO_ELEMENT: [audio_forge] Element already stopped
W (311556) AUDIO_PIPELINE: There are no listener registered
I (311563) AUDIO_PIPELINE: audio_pipeline_unlinked
W (311569) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (311577) AUDIO_ELEMENT: [wav] Element has not create when AUDIO_ELEMENT_TERMINATE
I (311584) CODEC_ELEMENT_HELPER: The element is 0x3f807324. The reserve data 2 is 0x0.
W (311593) AUDIO_ELEMENT: [raw] Element has not create when AUDIO_ELEMENT_TERMINATE
W (311601) AUDIO_PIPELINE: There are no listener registered
I (311609) AUDIO_PIPELINE: audio_pipeline_unlinked
W (311613) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (311621) AUDIO_ELEMENT: [wav] Element has not create when AUDIO_ELEMENT_TERMINATE
I (311630) CODEC_ELEMENT_HELPER: The element is 0x3f80b7f4. The reserve data 2 is 0x0.
W (311638) AUDIO_ELEMENT: [raw] Element has not create when AUDIO_ELEMENT_TERMINATE
W (311647) AUDIO_PIPELINE: There are no listener registered
I (311653) AUDIO_PIPELINE: audio_pipeline_unlinked
W (311659) AUDIO_ELEMENT: [audio_forge] Element has not create when AUDIO_ELEMENT_TERMINATE
I (311667) AUDIO_FORGE: audio_forge_destroy
I (311672) AUDIO_FORGE: Func:audio_forge_destroy, Line:377, MEM Total:4412628 Bytes, Inter:333576 Bytes, Dram:272200 Bytes

W (311684) AUDIO_ELEMENT: [iis] Element has not create when AUDIO_ELEMENT_TERMINATE

```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
