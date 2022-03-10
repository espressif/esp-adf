# 同时录制 WAV 和 AMR 文件到 microSD 卡例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程使用管道 API 建立了 1 路 I2S 输入和 2 路文件输出的音频数据流，功能是同时录制 10 秒的 AMR 和 WAV 音频文件，然后保存到 microSD 卡中。使用元素 API 例程请参考 [element_wav_amr_sdcard](../element_wav_amr_sdcard/README_CN.md)。

其中，AMR 支持 AMR-NB、AMR-WB 两种种音频编码器。默认选择 AMR-NB 编码器录制音频保存在 microSD 卡中。

录音例程的管道如下所示：

```
[mic] ---> codec_chip ---> i2s_stream ---> wav_encoder ---> fatfs_stream ---> [sdcard]
                                      |
                                       ---> raw_stream ---> amr_encoder ---> fatfs_stream ---> [sdcard]
                                                                ▲
                                                        ┌───────┴────────┐
                                                        │ AMR-NB_ENCODER │
                                                        │ AMR-WB_EMCODER │
                                                        └────────────────┘
```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中 [例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性) 中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程需要准备一张 microSD 卡，并预先插入开发板中备用。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3` ，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程默认的编码是 `AMR-NB`，如果更改为 `AMR-WB` 编码，需要在 `menuconfig` 中选择。

```
menuconfig > Example configuration > Audio encoder file type  > amrwb
```

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)，并在页面左上角选择芯片和版本，查看对应的文档。

## 如何使用例程

### 功能和用法

例程开始运行后，会提示录音开始，并打印录音读秒时间。日志如下：

```
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
I (27) boot: compile time 14:44:25
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x41b5c (269148) map
I (214) esp_image: segment 1: paddr=0x00051b84 vaddr=0x3ffb0000 size=0x02218 (  8728) load
I (217) esp_image: segment 2: paddr=0x00053da4 vaddr=0x40080000 size=0x0c274 ( 49780) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (242) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0x38c68 (232552) map
0x400d0020: _stext at ??:?

I (331) esp_image: segment 4: paddr=0x00098c90 vaddr=0x4008c274 size=0x018d4 (  6356) load
0x4008c274: spi_flash_ll_get_buffer_data at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/spi_flash_ll.h:146
 (inlined by) spi_flash_hal_read at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/hal/spi_flash_hal_common.inc:122

I (341) boot: Loaded app from partition at offset 0x10000
I (342) boot: Disabling RNG early entropy source...
I (343) cpu_start: Pro cpu up.
I (346) cpu_start: Application information:
I (351) cpu_start: Project name:     pipeline_wav_amr_sdcard
I (357) cpu_start: App version:      v2.2-242-g571753b2-dirty
I (364) cpu_start: Compile time:     Nov 22 2021 14:44:18
I (370) cpu_start: ELF file SHA256:  22f1f8f86cf0d8bd...
I (376) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (382) cpu_start: Starting app cpu, entry point is 0x4008198c
0x4008198c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (392) heap_init: Initializing. RAM available for dynamic allocation:
I (399) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (405) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (412) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (418) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (424) heap_init: At 4008DB48 len 000124B8 (73 KiB): IRAM
I (431) cpu_start: Pro cpu start user code
I (448) spi_flash: detected chip: gd
I (449) spi_flash: flash io: dio
W (449) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (459) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (475) PIPELINR_REC_WAV_AMR_SDCARD: [1.0] Mount sdcard
I (983) PIPELINR_REC_WAV_AMR_SDCARD: [2.0] Start codec chip
E (983) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1037) PIPELINR_REC_WAV_AMR_SDCARD: [3.0] Create audio pipeline_wav for recording
I (1037) PIPELINR_REC_WAV_AMR_SDCARD: [3.1] Create i2s stream to read audio data from codec chip
I (1045) PIPELINR_REC_WAV_AMR_SDCARD: [3.2] Create wav encoder to encode wav format
I (1052) PIPELINR_REC_WAV_AMR_SDCARD: [3.3] Create fatfs stream to write data to sdcard
I (1060) PIPELINR_REC_WAV_AMR_SDCARD: [3.4] Register all elements to audio pipeline
I (1069) PIPELINR_REC_WAV_AMR_SDCARD: [3.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]
I (1081) PIPELINR_REC_WAV_AMR_SDCARD: [3.6] Set up  uri (file as fatfs_stream, wav as wav encoder)
I (1090) PIPELINR_REC_WAV_AMR_SDCARD: [4.0] Create audio amr_pipeline for recording
I (1099) PIPELINR_REC_WAV_AMR_SDCARD: [4.1] Create raw stream to write data
I (1106) PIPELINR_REC_WAV_AMR_SDCARD: [4.2] Create amr encoder to encode wav format
I (1115) PIPELINR_REC_WAV_AMR_SDCARD: [4.3] Create fatfs stream to write data to sdcard
I (1123) PIPELINR_REC_WAV_AMR_SDCARD: [4.4] Register all elements to audio amr_pipeline
I (1132) PIPELINR_REC_WAV_AMR_SDCARD: [4.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]
I (1145) PIPELINR_REC_WAV_AMR_SDCARD: [4.6] Create ringbuf to link  i2s
I (1151) PIPELINR_REC_WAV_AMR_SDCARD: [4.7] Set up  uri (file as fatfs_stream, wav as wav encoder)
I (1161) PIPELINR_REC_WAV_AMR_SDCARD: [5.0] Set up  event listener
I (1168) PIPELINR_REC_WAV_AMR_SDCARD: [5.1] Listening event from peripherals
I (1176) PIPELINR_REC_WAV_AMR_SDCARD: [6.0] start audio_pipeline
I (1186) PIPELINR_REC_WAV_AMR_SDCARD: [7.0] Listen for all pipeline events, record for 10 seconds
I (2224) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 1
I (3240) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 2
I (4240) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 3
I (5290) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 4
I (6311) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 5
I (7324) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 6
I (8365) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 7
I (9378) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 8
I (10390) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 9
I (11449) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 10
I (11450) PIPELINR_REC_WAV_AMR_SDCARD: Finishing recording
W (11451) AUDIO_ELEMENT: IN-[wav] AEL_IO_ABORT
W (11456) AUDIO_ELEMENT: IN-[amr] AEL_IO_ABORT
E (11462) AUDIO_ELEMENT: [wav] Element already stopped
I (11498) PIPELINR_REC_WAV_AMR_SDCARD: [8.0] Stop audio_pipeline
W (11498) AUDIO_PIPELINE: There are no listener registered
W (11500) AUDIO_PIPELINE: There are no listener registered
W (11506) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11514) AUDIO_ELEMENT: [wav] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11522) AUDIO_ELEMENT: [wav_file] Element has not create when AUDIO_ELEMENT_TERMINATE
```


### 日志输出

以下为本例程的完整日志。

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
I (27) boot: compile time 14:44:25
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x41b5c (269148) map
I (214) esp_image: segment 1: paddr=0x00051b84 vaddr=0x3ffb0000 size=0x02218 (  8728) load
I (217) esp_image: segment 2: paddr=0x00053da4 vaddr=0x40080000 size=0x0c274 ( 49780) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (242) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0x38c68 (232552) map
0x400d0020: _stext at ??:?

I (331) esp_image: segment 4: paddr=0x00098c90 vaddr=0x4008c274 size=0x018d4 (  6356) load
0x4008c274: spi_flash_ll_get_buffer_data at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/spi_flash_ll.h:146
 (inlined by) spi_flash_hal_read at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/hal/spi_flash_hal_common.inc:122

I (341) boot: Loaded app from partition at offset 0x10000
I (342) boot: Disabling RNG early entropy source...
I (343) cpu_start: Pro cpu up.
I (346) cpu_start: Application information:
I (351) cpu_start: Project name:     pipeline_wav_amr_sdcard
I (357) cpu_start: App version:      v2.2-242-g571753b2-dirty
I (364) cpu_start: Compile time:     Nov 22 2021 14:44:18
I (370) cpu_start: ELF file SHA256:  22f1f8f86cf0d8bd...
I (376) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (382) cpu_start: Starting app cpu, entry point is 0x4008198c
0x4008198c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (392) heap_init: Initializing. RAM available for dynamic allocation:
I (399) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (405) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (412) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (418) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (424) heap_init: At 4008DB48 len 000124B8 (73 KiB): IRAM
I (431) cpu_start: Pro cpu start user code
I (448) spi_flash: detected chip: gd
I (449) spi_flash: flash io: dio
W (449) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (459) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (475) PIPELINR_REC_WAV_AMR_SDCARD: [1.0] Mount sdcard
I (983) PIPELINR_REC_WAV_AMR_SDCARD: [2.0] Start codec chip
E (983) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1037) PIPELINR_REC_WAV_AMR_SDCARD: [3.0] Create audio pipeline_wav for recording
I (1037) PIPELINR_REC_WAV_AMR_SDCARD: [3.1] Create i2s stream to read audio data from codec chip
I (1045) PIPELINR_REC_WAV_AMR_SDCARD: [3.2] Create wav encoder to encode wav format
I (1052) PIPELINR_REC_WAV_AMR_SDCARD: [3.3] Create fatfs stream to write data to sdcard
I (1060) PIPELINR_REC_WAV_AMR_SDCARD: [3.4] Register all elements to audio pipeline
I (1069) PIPELINR_REC_WAV_AMR_SDCARD: [3.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]
I (1081) PIPELINR_REC_WAV_AMR_SDCARD: [3.6] Set up  uri (file as fatfs_stream, wav as wav encoder)
I (1090) PIPELINR_REC_WAV_AMR_SDCARD: [4.0] Create audio amr_pipeline for recording
I (1099) PIPELINR_REC_WAV_AMR_SDCARD: [4.1] Create raw stream to write data
I (1106) PIPELINR_REC_WAV_AMR_SDCARD: [4.2] Create amr encoder to encode wav format
I (1115) PIPELINR_REC_WAV_AMR_SDCARD: [4.3] Create fatfs stream to write data to sdcard
I (1123) PIPELINR_REC_WAV_AMR_SDCARD: [4.4] Register all elements to audio amr_pipeline
I (1132) PIPELINR_REC_WAV_AMR_SDCARD: [4.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]
I (1145) PIPELINR_REC_WAV_AMR_SDCARD: [4.6] Create ringbuf to link  i2s
I (1151) PIPELINR_REC_WAV_AMR_SDCARD: [4.7] Set up  uri (file as fatfs_stream, wav as wav encoder)
I (1161) PIPELINR_REC_WAV_AMR_SDCARD: [5.0] Set up  event listener
I (1168) PIPELINR_REC_WAV_AMR_SDCARD: [5.1] Listening event from peripherals
I (1176) PIPELINR_REC_WAV_AMR_SDCARD: [6.0] start audio_pipeline
I (1186) PIPELINR_REC_WAV_AMR_SDCARD: [7.0] Listen for all pipeline events, record for 10 seconds
I (2224) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 1
I (3240) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 2
I (4240) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 3
I (5290) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 4
I (6311) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 5
I (7324) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 6
I (8365) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 7
I (9378) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 8
I (10390) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 9
I (11449) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 10
I (11450) PIPELINR_REC_WAV_AMR_SDCARD: Finishing recording
W (11451) AUDIO_ELEMENT: IN-[wav] AEL_IO_ABORT
W (11456) AUDIO_ELEMENT: IN-[amr] AEL_IO_ABORT
E (11462) AUDIO_ELEMENT: [wav] Element already stopped
I (11498) PIPELINR_REC_WAV_AMR_SDCARD: [8.0] Stop audio_pipeline
W (11498) AUDIO_PIPELINE: There are no listener registered
W (11500) AUDIO_PIPELINE: There are no listener registered
W (11506) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11514) AUDIO_ELEMENT: [wav] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11522) AUDIO_ELEMENT: [wav_file] Element has not create when AUDIO_ELEMENT_TERMINATE
```

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
