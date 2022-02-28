# 亚马逊云 Amazon Polly 语音合成例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

此示例的目的是展示如何使用 ADF 播放由亚马逊 (Amazon) 在线语音合成服务 Amazon Polly 生成的 MP3 音频。本示例默认是英文文本，但也支持其他一些其他国家的语言，更多的技术细节可以参考 [Amazon Polly](https://aws.amazon.com/cn/polly/) 页面。


获取亚马逊在线语音合成 MP3 音频管道如下：

```
[amazon_polly_server] ---> http_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置


本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例需要连接 Wi-Fi 网络，通过运行 `menuconfig` 来配置 Wi-Fi 信息。

```
 menuconfig > Example Configuration > `WiFi SSID` and `WiFi Password`
```

此外，还需要在 [Amazon Polly](https://aws.amazon.com/cn/polly/) 申请语音合成应用，并把申请到的 `Amazon service access key ID` 和 `Amazon service access secret` 分别填入 `menuconfig` 的配置中，用来和亚马逊 Amazon Polly 服务器鉴权。

```
menuconfig > Example Configuration > `Amazon service access key ID` and `Amazon service access secret`
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

例程开始运行后，按照配置首先尝试连接 Wi-Fi 网络，然后和亚马逊 Amazon Polly 服务器鉴权，成功后会向服务器请求以下英文文本的合成音频，最后 Amazon Polly 服务器返回 MP3 音频。

*"Espressif Systems is a multinational, fabless semiconductor company, with headquarters in Shanghai, China. We specialize in producing highly-integrated, low-power, WiFi-and-Bluetooth IoT solutions. Among our most popular chips are ESP8266 and ESP32. We are committed to creating IoT solutions that are power-efficient, robust and secure."* 

相关日志打印如下：

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
I (27) boot: compile time 15:29:48
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00300000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x26d0c (158988) map
I (172) esp_image: segment 1: paddr=0x00036d34 vaddr=0x3ffb0000 size=0x033a8 ( 13224) load
I (177) esp_image: segment 2: paddr=0x0003a0e4 vaddr=0x40080000 size=0x05f34 ( 24372) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (189) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xa7b9c (687004) map
0x400d0020: _stext at ??:?

I (451) esp_image: segment 4: paddr=0x000e7bc4 vaddr=0x40085f34 size=0x115a8 ( 71080) load
0x40085f34: btpwr_tsens_track at /home/cff/gittree/chip7.1_phy/chip_7.1/board_code/app_test/pp/phy/phy_chip_v7_ana.c:1487

I (495) boot: Loaded app from partition at offset 0x10000
I (495) boot: Disabling RNG early entropy source...
I (495) cpu_start: Pro cpu up.
I (499) cpu_start: Application information:
I (504) cpu_start: Project name:     play_aws_polly_mp3
I (510) cpu_start: App version:      v2.2-234-g771e2c30
I (516) cpu_start: Compile time:     Nov 15 2021 15:29:38
I (522) cpu_start: ELF file SHA256:  f49b6d83ddd2c894...
I (528) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (534) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (544) heap_init: Initializing. RAM available for dynamic allocation:
I (551) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (557) heap_init: At 3FFB83A0 len 00027C60 (159 KiB): DRAM
I (563) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (570) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (576) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (582) cpu_start: Pro cpu start user code
I (601) spi_flash: detected chip: gd
I (601) spi_flash: flash io: dio
W (601) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (611) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (658) AWS_POLLY_EXAMPLE: [ 0 ] Start and wait for Wi-Fi network
I (668) wifi:wifi driver task: 3ffc2390, prio:23, stack:6656, core=0
I (668) system_api: Base MAC address is not set
I (668) system_api: read default base MAC address from EFUSE
I (678) wifi:wifi firmware version: bb6888c
I (678) wifi:wifi certification version: v7.0
I (678) wifi:config NVS flash: enabled
I (688) wifi:config nano formating: disabled
I (688) wifi:Init data frame dynamic rx buffer num: 32
I (688) wifi:Init management frame dynamic rx buffer num: 32
I (698) wifi:Init management short buffer num: 32
I (698) wifi:Init dynamic tx buffer num: 32
I (708) wifi:Init static rx buffer size: 1600
I (708) wifi:Init static rx buffer num: 10
I (718) wifi:Init dynamic rx buffer num: 32
I (718) wifi_init: rx ba win: 6
I (718) wifi_init: tcpip mbox: 32
I (728) wifi_init: udp mbox: 6
I (728) wifi_init: tcp mbox: 6
I (728) wifi_init: tcp tx win: 5744
I (738) wifi_init: tcp rx win: 5744
I (738) wifi_init: tcp mss: 1440
I (748) wifi_init: WiFi IRAM OP enabled
I (748) wifi_init: WiFi RX IRAM OP enabled
I (758) phy_init: phy_version 4660,0162888,Dec 23 2020
I (878) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2108) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (3268) wifi:state: init -> auth (b0)
I (3278) wifi:state: auth -> assoc (0)
I (3288) wifi:state: assoc -> run (10)
I (3308) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (3308) wifi:security: WPA2-PSK, phy: bgn, rssi: -35
I (3308) wifi:pm start, type: 1

W (3318) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (3398) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (5158) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (5158) PERIPH_WIFI: Got ip:192.168.5.187
I (5158) AWS_POLLY_EXAMPLE: Initializing SNTP
I (5168) AWS_POLLY_EXAMPLE: Waiting for system time to be set... (1/20)
I (7168) AWS_POLLY_EXAMPLE: Waiting for system time to be set... (2/20)
I (9168) AWS_POLLY_EXAMPLE: [ 1 ] Start audio codec chip
I (9168) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (9168) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (9188) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (9188) ES8388_DRIVER: init,out:02, in:00
I (9198) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (9198) AWS_POLLY_EXAMPLE: [2.0] Create audio pipeline for playback
I (9208) AWS_POLLY_EXAMPLE: [2.1] Create http stream to read data
I (9208) AWS_POLLY_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (9218) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (9228) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (9258) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (9258) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (9268) AWS_POLLY_EXAMPLE: [2.3] Create mp3 decoder to decode mp3 file
I (9268) MP3_DECODER: MP3 init
I (9278) AWS_POLLY_EXAMPLE: [2.4] Register all elements to audio pipeline
I (9288) AWS_POLLY_EXAMPLE: [2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (9298) AUDIO_PIPELINE: link el->rb, el:0x3ffc78fc, tag:http, rb:0x3ffca338
I (9298) AUDIO_PIPELINE: link el->rb, el:0x3ffc9fc0, tag:mp3, rb:0x3ffcf474
I (9308) AWS_POLLY_EXAMPLE: [2.6] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)
I (9318) AWS_POLLY_EXAMPLE: [ 4 ] Set up  event listener
I (9328) AWS_POLLY_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (9338) AWS_POLLY_EXAMPLE: [4.2] Listening event from peripherals
I (9338) AWS_POLLY_EXAMPLE: [ 5 ] Start audio_pipeline
I (9348) AUDIO_ELEMENT: [http-0x3ffc78fc] Element task created
I (9358) AUDIO_ELEMENT: [mp3-0x3ffc9fc0] Element task created
I (9358) AUDIO_ELEMENT: [i2s-0x3ffc9c48] Element task created
I (9368) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:175224 Bytes

I (9378) AUDIO_ELEMENT: [http] AEL_MSG_CMD_RESUME,state:1
I (9388) AWS_POLLY_EXAMPLE: AWS4-HMAC-SHA256 Credential=AKIA6PUYW2ZJFLTDH3O6/20211115/us-east-1/polly/aws4_request, SignedHeaders=content-type;host;x-amz-date, Signature=5f2f0f880a1**********546a5, amz_date=20211115T073123Z, date=20211115
I (9408) AUDIO_ELEMENT: [mp3] AEL_MSG_CMD_RESUME,state:1
I (9408) MP3_DECODER: MP3 opened
I (9418) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (9418) I2S_STREAM: AUDIO_STREAM_WRITER
I (9428) AUDIO_PIPELINE: Pipeline started
I (9448) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (9468) I2S: APLL: Req RATE: 22050, real rate: 22049.982, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 5644795.500, SCLK: 705599.437500, diva: 1, divb: 0
I (9478) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (9478) I2S_STREAM: AUDIO_STREAM_WRITER
Total bytes read: 5120 bytes
I (12238) AWS_POLLY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=1
I (12328) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (12358) I2S: APLL: Req RATE: 22050, real rate: 22049.982, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 5644795.500, SCLK: 705599.437500, diva: 1, divb: 0
I (12358) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
Total bytes read: 145408 bytes
W (32688) HTTP_STREAM: No more data,errno:0, total_bytes:143927, rlen = 0
I (32688) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (36058) AUDIO_ELEMENT: IN-[mp3] AEL_IO_DONE,-2
I (36438) MP3_DECODER: Closed
I (36518) AUDIO_ELEMENT: IN-[i2s] AEL_IO_DONE,-2
W (36688) AWS_POLLY_EXAMPLE: [ * ] Stop event received
I (36688) AWS_POLLY_EXAMPLE: [ 6 ] Stop audio_pipeline
E (36688) AUDIO_ELEMENT: [http] Element already stopped
E (36688) AUDIO_ELEMENT: [mp3] Element already stopped
E (36698) AUDIO_ELEMENT: [i2s] Element already stopped
W (36698) AUDIO_PIPELINE: There are no listener registered
I (36708) AUDIO_PIPELINE: audio_pipeline_unlinked
W (36718) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (36718) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (36728) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
I (36748) wifi:state: run -> init (0)
I (36748) wifi:pm stop, total sleep time: 27148442 us / 33434519 us

I (36748) wifi:new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (36758) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (36768) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3
I (36778) wifi:flush txq
I (36778) wifi:stop sw txq
I (36778) wifi:lmac stop hw txq
I (36778) wifi:Deinit lldesc rx mblock:10

Done

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
I (27) boot: compile time 15:29:48
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00300000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x26d0c (158988) map
I (172) esp_image: segment 1: paddr=0x00036d34 vaddr=0x3ffb0000 size=0x033a8 ( 13224) load
I (177) esp_image: segment 2: paddr=0x0003a0e4 vaddr=0x40080000 size=0x05f34 ( 24372) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (189) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xa7b9c (687004) map
0x400d0020: _stext at ??:?

I (451) esp_image: segment 4: paddr=0x000e7bc4 vaddr=0x40085f34 size=0x115a8 ( 71080) load
0x40085f34: btpwr_tsens_track at /home/cff/gittree/chip7.1_phy/chip_7.1/board_code/app_test/pp/phy/phy_chip_v7_ana.c:1487

I (495) boot: Loaded app from partition at offset 0x10000
I (495) boot: Disabling RNG early entropy source...
I (495) cpu_start: Pro cpu up.
I (499) cpu_start: Application information:
I (504) cpu_start: Project name:     play_aws_polly_mp3
I (510) cpu_start: App version:      v2.2-234-g771e2c30
I (516) cpu_start: Compile time:     Nov 15 2021 15:29:38
I (522) cpu_start: ELF file SHA256:  f49b6d83ddd2c894...
I (528) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (534) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (544) heap_init: Initializing. RAM available for dynamic allocation:
I (551) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (557) heap_init: At 3FFB83A0 len 00027C60 (159 KiB): DRAM
I (563) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (570) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (576) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (582) cpu_start: Pro cpu start user code
I (601) spi_flash: detected chip: gd
I (601) spi_flash: flash io: dio
W (601) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (611) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (658) AWS_POLLY_EXAMPLE: [ 0 ] Start and wait for Wi-Fi network
I (668) wifi:wifi driver task: 3ffc2390, prio:23, stack:6656, core=0
I (668) system_api: Base MAC address is not set
I (668) system_api: read default base MAC address from EFUSE
I (678) wifi:wifi firmware version: bb6888c
I (678) wifi:wifi certification version: v7.0
I (678) wifi:config NVS flash: enabled
I (688) wifi:config nano formating: disabled
I (688) wifi:Init data frame dynamic rx buffer num: 32
I (688) wifi:Init management frame dynamic rx buffer num: 32
I (698) wifi:Init management short buffer num: 32
I (698) wifi:Init dynamic tx buffer num: 32
I (708) wifi:Init static rx buffer size: 1600
I (708) wifi:Init static rx buffer num: 10
I (718) wifi:Init dynamic rx buffer num: 32
I (718) wifi_init: rx ba win: 6
I (718) wifi_init: tcpip mbox: 32
I (728) wifi_init: udp mbox: 6
I (728) wifi_init: tcp mbox: 6
I (728) wifi_init: tcp tx win: 5744
I (738) wifi_init: tcp rx win: 5744
I (738) wifi_init: tcp mss: 1440
I (748) wifi_init: WiFi IRAM OP enabled
I (748) wifi_init: WiFi RX IRAM OP enabled
I (758) phy_init: phy_version 4660,0162888,Dec 23 2020
I (878) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2108) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (3268) wifi:state: init -> auth (b0)
I (3278) wifi:state: auth -> assoc (0)
I (3288) wifi:state: assoc -> run (10)
I (3308) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (3308) wifi:security: WPA2-PSK, phy: bgn, rssi: -35
I (3308) wifi:pm start, type: 1

W (3318) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (3398) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (5158) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (5158) PERIPH_WIFI: Got ip:192.168.5.187
I (5158) AWS_POLLY_EXAMPLE: Initializing SNTP
I (5168) AWS_POLLY_EXAMPLE: Waiting for system time to be set... (1/20)
I (7168) AWS_POLLY_EXAMPLE: Waiting for system time to be set... (2/20)
I (9168) AWS_POLLY_EXAMPLE: [ 1 ] Start audio codec chip
I (9168) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (9168) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (9188) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (9188) ES8388_DRIVER: init,out:02, in:00
I (9198) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (9198) AWS_POLLY_EXAMPLE: [2.0] Create audio pipeline for playback
I (9208) AWS_POLLY_EXAMPLE: [2.1] Create http stream to read data
I (9208) AWS_POLLY_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (9218) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (9228) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (9258) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (9258) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (9268) AWS_POLLY_EXAMPLE: [2.3] Create mp3 decoder to decode mp3 file
I (9268) MP3_DECODER: MP3 init
I (9278) AWS_POLLY_EXAMPLE: [2.4] Register all elements to audio pipeline
I (9288) AWS_POLLY_EXAMPLE: [2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (9298) AUDIO_PIPELINE: link el->rb, el:0x3ffc78fc, tag:http, rb:0x3ffca338
I (9298) AUDIO_PIPELINE: link el->rb, el:0x3ffc9fc0, tag:mp3, rb:0x3ffcf474
I (9308) AWS_POLLY_EXAMPLE: [2.6] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)
I (9318) AWS_POLLY_EXAMPLE: [ 4 ] Set up  event listener
I (9328) AWS_POLLY_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (9338) AWS_POLLY_EXAMPLE: [4.2] Listening event from peripherals
I (9338) AWS_POLLY_EXAMPLE: [ 5 ] Start audio_pipeline
I (9348) AUDIO_ELEMENT: [http-0x3ffc78fc] Element task created
I (9358) AUDIO_ELEMENT: [mp3-0x3ffc9fc0] Element task created
I (9358) AUDIO_ELEMENT: [i2s-0x3ffc9c48] Element task created
I (9368) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:175224 Bytes

I (9378) AUDIO_ELEMENT: [http] AEL_MSG_CMD_RESUME,state:1
I (9388) AWS_POLLY_EXAMPLE: AWS4-HMAC-SHA256 Credential=AKIA6PUYW2ZJFLTDH3O6/20211115/us-east-1/polly/aws4_request, SignedHeaders=content-type;host;x-amz-date, Signature=5f2f0f880a1**********546a5, amz_date=20211115T073123Z, date=20211115
I (9408) AUDIO_ELEMENT: [mp3] AEL_MSG_CMD_RESUME,state:1
I (9408) MP3_DECODER: MP3 opened
I (9418) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (9418) I2S_STREAM: AUDIO_STREAM_WRITER
I (9428) AUDIO_PIPELINE: Pipeline started
I (9448) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (9468) I2S: APLL: Req RATE: 22050, real rate: 22049.982, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 5644795.500, SCLK: 705599.437500, diva: 1, divb: 0
I (9478) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (9478) I2S_STREAM: AUDIO_STREAM_WRITER
Total bytes read: 5120 bytes
I (12238) AWS_POLLY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=1
I (12328) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (12358) I2S: APLL: Req RATE: 22050, real rate: 22049.982, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 5644795.500, SCLK: 705599.437500, diva: 1, divb: 0
I (12358) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
Total bytes read: 145408 bytes
W (32688) HTTP_STREAM: No more data,errno:0, total_bytes:143927, rlen = 0
I (32688) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (36058) AUDIO_ELEMENT: IN-[mp3] AEL_IO_DONE,-2
I (36438) MP3_DECODER: Closed
I (36518) AUDIO_ELEMENT: IN-[i2s] AEL_IO_DONE,-2
W (36688) AWS_POLLY_EXAMPLE: [ * ] Stop event received
I (36688) AWS_POLLY_EXAMPLE: [ 6 ] Stop audio_pipeline
E (36688) AUDIO_ELEMENT: [http] Element already stopped
E (36688) AUDIO_ELEMENT: [mp3] Element already stopped
E (36698) AUDIO_ELEMENT: [i2s] Element already stopped
W (36698) AUDIO_PIPELINE: There are no listener registered
I (36708) AUDIO_PIPELINE: audio_pipeline_unlinked
W (36718) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (36718) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (36728) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
I (36748) wifi:state: run -> init (0)
I (36748) wifi:pm stop, total sleep time: 27148442 us / 33434519 us

I (36748) wifi:new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (36758) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (36768) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3
I (36778) wifi:flush txq
I (36778) wifi:stop sw txq
I (36778) wifi:lmac stop hw txq
I (36778) wifi:Deinit lldesc rx mblock:10

Done

```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
