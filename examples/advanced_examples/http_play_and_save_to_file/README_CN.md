# HTTP 流的播放和下载例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介


此示例演示了在HTTP 流的管道中使用 ADF 的多输出管道接口，实现一边播放网络歌曲，一边下载歌曲到 microSD 卡中的过程。

多输出管道的结构如下图所示：

```
http_stream_reader ---> mp3_decoder ---> i2s_stream ---> codec chip
                    |
                    v
                    raw_stream ---> fatfs_stream ---> SD card
```

在第一个管道中，`http_stream_reader` 从网络获取 MP3 歌曲。然后数据经过 MP3 解码器解码，解码后数据通过 I2S 流传输到 `codec_chip`。最后，PA 驱动扬声器播放音乐。

另一个管道的前端是 `raw_stream`，它通过多输出管道接口连接到 `http_stream_reader`，读取到的数据最终由 `fatfs_stream` 写入到 microSD 卡中存储。

通过使用 ADF 的多输出管道接口，我们连接了两条管道，不仅完成了网络音频的播放，同时也完成了网络音频下载到 microSD 卡中的操作。

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程需要准备一张 microSD 卡，用于保存网络下载的音频文件。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程同时需要打开 FatFs 长文件名支持。

```
menuconfig > Component config > FAT Filesystem support > Long filename support
```

本例需要连接 Wi-Fi 网络，通过运行 `menuconfig` 来配置 Wi-Fi 信息。

```
 menuconfig > Example Configuration > `WiFi SSID` and `WiFi Password`
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


- 代码烧录成功且开机后，程序会首先去连接 Wi-Fi 网络。

- 连接 HTTP 服务器检索文件成功后，音乐自动开始播放。

- 一旦程序完成且音乐播放结束后，您可以在 microSD 卡里找到网络下载的名为 `test_output.mp3` 的音频文件。


### 日志输出

以下为本例程的完整日志。

```c
ets Jul 29 2019 12:21:46

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
I (29) boot: compile time 19:59:00
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
I (82) boot:  2 factory          factory app      00 00 00010000 00300000
I (90) boot: End of partition table
I (94) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x52934 (338228) map
I (202) esp_image: segment 1: paddr=0x0006295c vaddr=0x3ffb0000 size=0x033f4 ( 13300) load
I (207) esp_image: segment 2: paddr=0x00065d58 vaddr=0x40080000 size=0x0a2c0 ( 41664) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (224) esp_image: segment 3: paddr=0x00070020 vaddr=0x400d0020 size=0xa3808 (669704) map
0x400d0020: _stext at ??:?

I (421) esp_image: segment 4: paddr=0x00113830 vaddr=0x4008a2c0 size=0x0e12c ( 57644) load
0x4008a2c0: print_registers at /workshop/esp-idfs/esp-idf-v4.2.2/components/esp_system/port/panic_handler.c:200
 (inlined by) print_state_for_core at /workshop/esp-idfs/esp-idf-v4.2.2/components/esp_system/port/panic_handler.c:248

I (455) boot: Loaded app from partition at offset 0x10000
I (455) boot: Disabling RNG early entropy source...
I (455) psram: This chip is ESP32-D0WD
I (460) spiram: Found 64MBit SPI RAM device
I (464) spiram: SPI RAM mode: flash 80m sram 80m
I (470) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (477) cpu_start: Pro cpu up.
I (481) cpu_start: Application information:
I (486) cpu_start: Project name:     play_and_save
I (491) cpu_start: App version:      1
I (495) cpu_start: Compile time:     Aug 12 2021 20:00:58
I (501) cpu_start: ELF file SHA256:  269a77344f23032d...
I (507) cpu_start: ESP-IDF:          v4.2.2
I (512) cpu_start: Starting app cpu, entry point is 0x40081df0
0x40081df0: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1010) spiram: SPI SRAM memory test OK
I (1011) heap_init: Initializing. RAM available for dynamic allocation:
I (1011) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1017) heap_init: At 3FFB4668 len 0002B998 (174 KiB): DRAM
I (1023) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1029) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1036) heap_init: At 400983EC len 00007C14 (31 KiB): IRAM
I (1042) cpu_start: Pro cpu start user code
I (1047) spiram: Adding pool of 4082K of external SPI memory to heap allocator
I (1067) spi_flash: detected chip: gd
I (1068) spi_flash: flash io: qio
W (1068) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1079) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1094) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1127) PLAY_AND_SAVE_FILE: [ 1 ] Start audio codec chip
I (1127) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1160) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1160) ES8388_DRIVER: init,out:02, in:00
I (1179) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (1189) PLAY_AND_SAVE_FILE: [2.0] Create audio pipeline for playback
I (1189) PLAY_AND_SAVE_FILE: [2.1] Create http stream to read data
I (1192) PLAY_AND_SAVE_FILE: [2.2] Create i2s stream to write data to codec chip
I (1200) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1207) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1228) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1231) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1236) PLAY_AND_SAVE_FILE: [2.3] Create mp3 decoder to decode mp3 file
I (1244) MP3_DECODER: MP3 init
I (1247) PLAY_AND_SAVE_FILE: [2.4] Register all elements to audio pipeline
I (1255) PLAY_AND_SAVE_FILE: [2.5] Link elements together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (1267) AUDIO_PIPELINE: link el->rb, el:0x3f80379c, tag:http, rb:0x3f803c04
I (1274) AUDIO_PIPELINE: link el->rb, el:0x3f803a9c, tag:mp3, rb:0x3f808c44
I (1281) PLAY_AND_SAVE_FILE: [2.6] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)
I (1292) PLAY_AND_SAVE_FILE: [3.0] Create fatfs stream to save file
I (1299) PLAY_AND_SAVE_FILE: [3.1] Create raw stream to read data
I (1306) PLAY_AND_SAVE_FILE: [3.2] Create pipeline to save audio file
I (1313) PLAY_AND_SAVE_FILE: [3.3] Set up  uri for save file name
I (1320) PLAY_AND_SAVE_FILE: [3.4] Register all elements to pipeline_save
I (1327) PLAY_AND_SAVE_FILE: [3.5] Link elements together raw_stream-->fatfs_stream
I (1336) AUDIO_PIPELINE: link el->rb, el:0x3f8095ec, tag:raw, rb:0x3f809780
I (1343) PLAY_AND_SAVE_FILE: [3.6] Connect input ringbuffer of pipeline_save to http stream multi output
I (1353) PLAY_AND_SAVE_FILE: [4.0] Start and wait for Wi-Fi network
E (1360) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1371) wifi:wifi driver task: 3ffc7458, prio:23, stack:6656, core=0
I (1375) system_api: Base MAC address is not set
I (1380) system_api: read default base MAC address from EFUSE
I (1399) wifi:wifi firmware version: bb6888c
I (1399) wifi:wifi certification version: v7.0
I (1399) wifi:config NVS flash: enabled
I (1400) wifi:config nano formating: disabled
I (1405) wifi:Init data frame dynamic rx buffer num: 32
I (1409) wifi:Init management frame dynamic rx buffer num: 32
I (1415) wifi:Init management short buffer num: 32
I (1420) wifi:Init static tx buffer num: 16
I (1424) wifi:Init tx cache buffer num: 32
I (1427) wifi:Init static rx buffer size: 1600
I (1431) wifi:Init static rx buffer num: 16
I (1435) wifi:Init dynamic rx buffer num: 32
I (1439) wifi_init: rx ba win: 16
I (1443) wifi_init: tcpip mbox: 32
I (1447) wifi_init: udp mbox: 6
I (1451) wifi_init: tcp mbox: 6
I (1455) wifi_init: tcp tx win: 5744
I (1459) wifi_init: tcp rx win: 5744
I (1463) wifi_init: tcp mss: 1440
I (1467) wifi_init: WiFi/LWIP prefer SPIRAM
I (1472) wifi_init: WiFi IRAM OP enabled
I (1477) wifi_init: WiFi RX IRAM OP enabled
I (1482) phy_init: phy_version 4660,0162888,Dec 23 2020
I (1575) wifi:mode : sta (94:b9:7e:65:c2:44)
I (1581) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1
I (2059) wifi:state: init -> auth (b0)
I (2069) wifi:state: auth -> assoc (0)
I (2081) wifi:state: assoc -> run (10)
I (2089) wifi:connected with esp32, aid = 2, channel 1, BW20, bssid = fc:ec:da:b7:11:c7
I (2089) wifi:security: WPA2-PSK, phy: bgn, rssi: -41
I (2090) wifi:pm start, type: 1

W (2094) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (2130) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4126) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (4127) PERIPH_WIFI: Got ip:192.168.5.187
I (4129) PLAY_AND_SAVE_FILE: [4.1] Start and wait for SDCARD to mount
I (4144) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (4189) SDCARD: CID name SA16G!

I (4636) PLAY_AND_SAVE_FILE: [ 5 ] Set up  event listener
I (4636) PLAY_AND_SAVE_FILE: [5.1] Listening event from all elements of audio pipeline
I (4639) PLAY_AND_SAVE_FILE: [5.2] Listening event from peripherals
I (4646) PLAY_AND_SAVE_FILE: [ 6 ] Start pipelines
I (4652) AUDIO_ELEMENT: [http-0x3f80379c] Element task created
I (4658) AUDIO_ELEMENT: [mp3-0x3f803a9c] Element task created
I (4665) AUDIO_ELEMENT: [i2s-0x3f80392c] Element task created
I (4671) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4215964 Bytes, Inter:214864 Bytes, Dram:183136 Bytes

I (4683) AUDIO_ELEMENT: [http] AEL_MSG_CMD_RESUME,state:1
I (4691) AUDIO_ELEMENT: [mp3] AEL_MSG_CMD_RESUME,state:1
I (4695) MP3_DECODER: MP3 opened
I (4699) CODEC_ELEMENT_HELPER: The element is 0x3f803a9c. The reserve data 2 is 0x0.
I (4709) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (4713) I2S_STREAM: AUDIO_STREAM_WRITER
I (4718) AUDIO_PIPELINE: Pipeline started
I (4723) AUDIO_ELEMENT: [raw-0x3f8095ec] Element task created
I (4729) AUDIO_ELEMENT: [file-0x3f8094c8] Element task created
I (4736) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4201960 Bytes, Inter:205792 Bytes, Dram:174064 Bytes

I (4748) AUDIO_ELEMENT: [file] AEL_MSG_CMD_RESUME,state:1
I (4754) AUDIO_PIPELINE: Pipeline started
I (5774) HTTP_STREAM: total_bytes=2994349
I (5821) PLAY_AND_SAVE_FILE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (5877) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (7260) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (7264) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (7269) I2S_STREAM: AUDIO_STREAM_WRITER
W (192840) HTTP_STREAM: No more data,errno:0, total_bytes:2994349, rlen = 0
I (192841) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (192857) AUDIO_ELEMENT: IN-[file] AEL_IO_DONE,-2
I (194125) AUDIO_ELEMENT: IN-[mp3] AEL_IO_DONE,-2
I (194270) MP3_DECODER: Closed
I (194290) AUDIO_ELEMENT: IN-[i2s] AEL_IO_DONE,-2
W (194331) PLAY_AND_SAVE_FILE: [ * ] Stop event received
I (194331) PLAY_AND_SAVE_FILE: [ 7 ] Stop pipelines
E (194331) AUDIO_ELEMENT: [http] Element already stopped
E (194337) AUDIO_ELEMENT: [mp3] Element already stopped
E (194343) AUDIO_ELEMENT: [i2s] Element already stopped
W (194350) AUDIO_PIPELINE: There are no listener registered
I (194356) AUDIO_PIPELINE: audio_pipeline_unlinked
W (194361) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (194369) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (194378) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (194386) AUDIO_PIPELINE: There are no listener registered
I (194392) AUDIO_PIPELINE: audio_pipeline_unlinked
E (194398) AUDIO_ELEMENT: [file] Element already stopped
I (194409) wifi:state: run -> init (0)
I (194410) wifi:pm stop, total sleep time: 122567791 us / 192316346 us

I (194414) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1
W (194421) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (194431) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3
I (194439) wifi:flush txq
I (194441) wifi:stop sw txq
I (194444) wifi:lmac stop hw txq
I (194447) wifi:Deinit lldesc rx mblock:16
```

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
