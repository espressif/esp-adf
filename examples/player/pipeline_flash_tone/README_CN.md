# 从 Flash 中播放 MP3 文件例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_regular.png "中级")


## 例程简介

本例程演示了 ADF 使用音频管道 API 播放存储在 flash 中的 MP3 文件，一般可在项目中作为系统提示音使用。

开发板获取 flash 中存储的 MP3 文件并解码播放的管道如下：

```
[flash] ---> tone_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
```

### 预备知识

本例程在 `/tools/audio-esp.bin` 和 `/components/audio_flash_tone/` 目录下已经帮助用户生成了例程所需的 bin 文件和音频文件在 flash 中地址的源代码文件。

如果用户需要生成自己的 `audio-esp.bin`，则需要执行 `mk_audio_bin.py` 脚本（位于 $ADF_PATH/tools/audio_tone/mk_audio_tone.py），并且指定相关文件的路径。

 源 MP3 文件在 `tone_mp3_folder` 文件夹中，生成的 C 文件、H 文件以及二进制 bin 文件都存放在此目录下。

```
python3 $ADF_PATH/tools/audio_tone/mk_audio_tone.py -f ./ -r tone_mp3_folder
```

请使用 *python3 $ADF_PATH/tools/audio_tone/mk_audio_tone.py --help* 查看更多脚本信息。

本例程默认的 `audio-esp.bin` 包含如下音频文件：

```c
   "flash://tone/0_Bt_Reconnect.mp3",
   "flash://tone/1_Wechat.mp3",
   "flash://tone/2_Welcome_To_Wifi.mp3",
   "flash://tone/3_New_Version_Available.mp3",
   "flash://tone/4_Bt_Success.mp3",
   "flash://tone/5_Freetalk.mp3",
   "flash://tone/6_Upgrade_Done.mp3",
   "flash://tone/7_shutdown.mp3",
   "flash://tone/8_Alarm.mp3",
   "flash://tone/9_Wifi_Success.mp3",
   "flash://tone/10_Under_Smartconfig.mp3",
   "flash://tone/11_Out_Of_Power.mp3",
   "flash://tone/12_server_connect.mp3",
   "flash://tone/13_hello.mp3",
   "flash://tone/14_new_message.mp3",
   "flash://tone/15_Please_Retry_Wifi.mp3",
   "flash://tone/16_please_setting_wifi.mp3",
   "flash://tone/17_Welcome_To_Bt.mp3",
   "flash://tone/18_Wifi_Time_Out.mp3",
   "flash://tone/19_Wifi_Reconnect.mp3",
   "flash://tone/20_server_disconnect.mp3",
```


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。如果选择 `CONFIG_ESP32_C3_LYRA_V2_BOARD`，则需要在 `$ADF_PATH/esp-idf` 目录下应用`idf_v4.4_i2s_c3_pdm_tx.patch`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

此例程的 `flashTone` 在 `partition_flash_tone.csv` 中地址配置如下，用户可以根据自己的项目 flash 分区灵活配置地址。

```
flashTone,data,  0x04,  0x110000 , 500K,
```


### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```


此外，本例程还需烧录 `/tools/audio-esp.bin` 到 `partition_flash_tone.csv` 的 `flashTone` 分区，请使用如下命令。
若使用 `CONFIG_ESP32_C3_LYRA_V2_BOARD` ,请将下面命令中的 `esp32` 替换为 `esp32c3` 。

```
python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0x110000 ./tools/audio-esp.bin

esptool.py v2.8
Serial port /dev/ttyUSB0
Connecting.....
Chip is ESP32D0WDQ5 (revision 3)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None
Crystal is 40MHz
MAC: 94:b9:7e:65:c2:44
Uploading stub...
Running stub...
Stub running...
Changing baud rate to 921600
Changed.
Configuring flash size...
Auto-detected Flash size: 8MB
Compressed 434756 bytes to 399938...
Wrote 434756 bytes (399938 compressed) at 0x00110000 in 5.0 seconds (effective 689.2 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
```


使用 ``Ctrl-]`` 退出调试界面。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程

### 功能和用法

- 例程开始运行后，如果事前没有烧录 `/tools/audio-esp.bin` 到 `partition_flash_tone.csv ` 的 `flashTone` 分区，例程将会报错，请参考 [编译和下载](#编译和下载) 的说明进行烧录，正常的例程打印如下：

```c
ets Jul 29 2019 12:21:46

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
I (27) boot: compile time 17:06:12
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot:  3 flash_tone       Unknown data     01 ff 00110000 0007d000
I (98) boot: End of partition table
I (102) boot_comm: chip revision: 3, min. application chip revision: 0
I (110) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0e7dc ( 59356) map
I (141) esp_image: segment 1: paddr=0x0001e804 vaddr=0x3ffb0000 size=0x01814 (  6164) load
I (144) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x26c3c (158780) map
0x400d0020: _stext at ??:?

I (208) esp_image: segment 3: paddr=0x00046c64 vaddr=0x3ffb1814 size=0x00950 (  2384) load
I (210) esp_image: segment 4: paddr=0x000475bc vaddr=0x40080000 size=0x0c5a8 ( 50600) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (244) boot: Loaded app from partition at offset 0x10000
I (244) boot: Disabling RNG early entropy source...
I (245) cpu_start: Pro cpu up.
I (248) cpu_start: Application information:
I (253) cpu_start: Project name:     play_tone_mp3
I (259) cpu_start: App version:      v2.2-230-ga154afdc
I (264) cpu_start: Compile time:     Nov 11 2021 17:06:06
I (271) cpu_start: ELF file SHA256:  a51574fed039918d...
I (276) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (282) cpu_start: Starting app cpu, entry point is 0x40081708
0x40081708: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (293) heap_init: Initializing. RAM available for dynamic allocation:
I (300) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (306) heap_init: At 3FFB29E0 len 0002D620 (181 KiB): DRAM
I (312) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (318) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (325) heap_init: At 4008C5A8 len 00013A58 (78 KiB): IRAM
I (331) cpu_start: Pro cpu start user code
I (349) spi_flash: detected chip: gd
I (350) spi_flash: flash io: dio
W (350) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (360) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (371) TONE_MP3_EXAMPLE: [ 1 ] Start codec chip
I (401) TONE_MP3_EXAMPLE: [2.0] Create audio pipeline for playback
I (401) TONE_MP3_EXAMPLE: [2.1] Create tone stream to read data from flash
I (401) TONE_MP3_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (431) TONE_MP3_EXAMPLE: [2.3] Create mp3 decoder to decode mp3 file
I (431) TONE_MP3_EXAMPLE: [2.4] Register all elements to audio pipeline
I (441) TONE_MP3_EXAMPLE: [2.5] Link it together [flash]-->tone_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (451) TONE_MP3_EXAMPLE: [2.6] Set up  uri (file as tone_stream, mp3 as mp3 decoder, and default output is i2s)
I (461) TONE_MP3_EXAMPLE: [ 3 ] Set up event listener
I (461) TONE_MP3_EXAMPLE: [3.1] Listening event from all elements of pipeline
I (471) TONE_MP3_EXAMPLE: [ 4 ] Start audio_pipeline
I (491) TONE_MP3_EXAMPLE: [ 4 ] Listen for all pipeline events
I (491) TONE_MP3_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=48000, bits=16, ch=1
W (721) TONE_STREAM: No more data,ret:0 ,info.byte_pos:7056
W (1231) TONE_MP3_EXAMPLE: [ * ] Stop event received
I (1231) TONE_MP3_EXAMPLE: [ 5 ] Stop audio_pipeline
E (1231) AUDIO_ELEMENT: [tone] Element already stopped
E (1241) AUDIO_ELEMENT: [mp3] Element already stopped
E (1241) AUDIO_ELEMENT: [i2s] Element already stopped
W (1251) AUDIO_PIPELINE: There are no listener registered
W (1251) AUDIO_ELEMENT: [tone] Element has not create when AUDIO_ELEMENT_TERMINATE
W (1261) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (1271) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE

```


### 日志输出

以下为本例程的完整日志。

```c
ets Jul 29 2019 12:21:46

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
I (27) boot: compile time 17:06:12
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot:  3 flash_tone       Unknown data     01 ff 00110000 0007d000
I (98) boot: End of partition table
I (102) boot_comm: chip revision: 3, min. application chip revision: 0
I (110) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0e7dc ( 59356) map
I (141) esp_image: segment 1: paddr=0x0001e804 vaddr=0x3ffb0000 size=0x01814 (  6164) load
I (144) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x26c3c (158780) map
0x400d0020: _stext at ??:?

I (208) esp_image: segment 3: paddr=0x00046c64 vaddr=0x3ffb1814 size=0x00950 (  2384) load
I (210) esp_image: segment 4: paddr=0x000475bc vaddr=0x40080000 size=0x0c5a8 ( 50600) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (244) boot: Loaded app from partition at offset 0x10000
I (244) boot: Disabling RNG early entropy source...
I (245) cpu_start: Pro cpu up.
I (248) cpu_start: Application information:
I (253) cpu_start: Project name:     play_tone_mp3
I (259) cpu_start: App version:      v2.2-230-ga154afdc
I (264) cpu_start: Compile time:     Nov 11 2021 17:06:06
I (271) cpu_start: ELF file SHA256:  a51574fed039918d...
I (276) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (282) cpu_start: Starting app cpu, entry point is 0x40081708
0x40081708: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (293) heap_init: Initializing. RAM available for dynamic allocation:
I (300) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (306) heap_init: At 3FFB29E0 len 0002D620 (181 KiB): DRAM
I (312) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (318) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (325) heap_init: At 4008C5A8 len 00013A58 (78 KiB): IRAM
I (331) cpu_start: Pro cpu start user code
I (349) spi_flash: detected chip: gd
I (350) spi_flash: flash io: dio
W (350) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (360) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (371) TONE_MP3_EXAMPLE: [ 1 ] Start codec chip
I (401) TONE_MP3_EXAMPLE: [2.0] Create audio pipeline for playback
I (401) TONE_MP3_EXAMPLE: [2.1] Create tone stream to read data from flash
I (401) TONE_MP3_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (431) TONE_MP3_EXAMPLE: [2.3] Create mp3 decoder to decode mp3 file
I (431) TONE_MP3_EXAMPLE: [2.4] Register all elements to audio pipeline
I (441) TONE_MP3_EXAMPLE: [2.5] Link it together [flash]-->tone_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (451) TONE_MP3_EXAMPLE: [2.6] Set up  uri (file as tone_stream, mp3 as mp3 decoder, and default output is i2s)
I (461) TONE_MP3_EXAMPLE: [ 3 ] Set up event listener
I (461) TONE_MP3_EXAMPLE: [3.1] Listening event from all elements of pipeline
I (471) TONE_MP3_EXAMPLE: [ 4 ] Start audio_pipeline
I (491) TONE_MP3_EXAMPLE: [ 4 ] Listen for all pipeline events
I (491) TONE_MP3_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=48000, bits=16, ch=1
W (721) TONE_STREAM: No more data,ret:0 ,info.byte_pos:7056
W (1231) TONE_MP3_EXAMPLE: [ * ] Stop event received
I (1231) TONE_MP3_EXAMPLE: [ 5 ] Stop audio_pipeline
E (1231) AUDIO_ELEMENT: [tone] Element already stopped
E (1241) AUDIO_ELEMENT: [mp3] Element already stopped
E (1241) AUDIO_ELEMENT: [i2s] Element already stopped
W (1251) AUDIO_PIPELINE: There are no listener registered
W (1251) AUDIO_ELEMENT: [tone] Element has not create when AUDIO_ELEMENT_TERMINATE
W (1261) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (1271) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE

```


## 故障排除

```c
E (481) TONE_PARTITION: Not flash tone partition
E (481) AUDIO_ELEMENT: [tone] AEL_STATUS_ERROR_OPEN,-1
W (491) AUDIO_ELEMENT: [tone] audio_element_on_cmd_error,7
E (501) TONE_PARTITION: /repo/adfs/bugfix/esp-adf-internal/components/tone_partition/tone_partition.c:204 (tone_partition_deinit): Got NULL Pointer
W (511) AUDIO_ELEMENT: IN-[mp3] AEL_IO_ABORT
E (511) MP3_DECODER: failed to read audio data (line 119)
W (521) AUDIO_ELEMENT: [mp3] AEL_IO_ABORT, -3
W (531) AUDIO_ELEMENT: IN-[i2s] AEL_IO_ABORT
```

如果遇到上述的错误，请把按照 [编译和下载](#编译和下载) 的说明烧录 `/tools/audio-esp.bin` 到 `partition_flash_tone.csv ` 的 `flashTone` 分区。


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
