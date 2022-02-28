# 百度语音合成例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

此示例的目的是展示如何使用 ADF 播放由百度在线语音合成 (text-to-speech, TTS) 服务生成的音频。本示例默认是中文文本，但也支持其他一些语言，更多的技术细节可以参考 [百度语音合成文档](http://ai.baidu.com/tech/speech/tts) 页面。


获取百度在线语音合成 MP3 音频管道如下：

```
[baidu_tts_server] ---> http_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
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

此外，还需要在 [百度在线语音合成页面](http://ai.baidu.com/tech/speech/tts) 申请语音合成应用，并把申请到的 `API Key` 和 `Secret Key` 分别填入 `menuconfig` 的配置中，用来和百度 TTS 服务器鉴权。


```
menuconfig > Example Configuration > `Baidu speech access key ID` and `Baidu speech access secret`

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

- 例程开始运行后，按照配置首先尝试连接 Wi-Fi 网络，然后和百度 TTS 服务器鉴权，成功后会向服务器请求中文文本 `欢迎使用乐鑫音频平台，想了解更多方案信息请联系我们` 的合成音频，最后服务器返回 MP3 音频，打印如下：

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
I (27) boot: compile time 15:14:40
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x26c3c (158780) map
I (172) esp_image: segment 1: paddr=0x00036c64 vaddr=0x3ffb0000 size=0x0336c ( 13164) load
I (177) esp_image: segment 2: paddr=0x00039fd8 vaddr=0x40080000 size=0x06040 ( 24640) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (189) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xa77a0 (685984) map
0x400d0020: _stext at ??:?

I (451) esp_image: segment 4: paddr=0x000e77c8 vaddr=0x40086040 size=0x1149c ( 70812) load
0x40086040: bt_track_pll_cap at /home/cff/gittree/chip7.1_phy/chip_7.1/board_code/app_test/pp/phy/phy_chip_v7_ana.c:1595

I (494) boot: Loaded app from partition at offset 0x10000
I (494) boot: Disabling RNG early entropy source...
I (495) cpu_start: Pro cpu up.
I (498) cpu_start: Application information:
I (503) cpu_start: Project name:     play_baidu_tts_mp3
I (509) cpu_start: App version:      v2.2-221-gd27be99e
I (515) cpu_start: Compile time:     Nov 10 2021 15:14:34
I (521) cpu_start: ELF file SHA256:  1df6fe3040a08d9b...
I (527) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (533) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (544) heap_init: Initializing. RAM available for dynamic allocation:
I (551) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (557) heap_init: At 3FFB7EB0 len 00028150 (160 KiB): DRAM
I (563) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (569) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (576) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (582) cpu_start: Pro cpu start user code
I (600) spi_flash: detected chip: gd
I (601) spi_flash: flash io: dio
W (601) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (611) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (656) BAIDU_SPEECH_EXAMPLE: [ 0 ] Start and wait for Wi-Fi network
I (666) wifi:wifi driver task: 3ffc1c80, prio:23, stack:6656, core=0
I (666) system_api: Base MAC address is not set
I (666) system_api: read default base MAC address from EFUSE
I (676) wifi:wifi firmware version: bb6888c
I (676) wifi:wifi certification version: v7.0
I (676) wifi:config NVS flash: enabled
I (676) wifi:config nano formating: disabled
I (686) wifi:Init data frame dynamic rx buffer num: 32
I (686) wifi:Init management frame dynamic rx buffer num: 32
I (696) wifi:Init management short buffer num: 32
I (696) wifi:Init dynamic tx buffer num: 32
I (706) wifi:Init static rx buffer size: 1600
I (706) wifi:Init static rx buffer num: 10
I (706) wifi:Init dynamic rx buffer num: 32
I (716) wifi_init: rx ba win: 6
I (716) wifi_init: tcpip mbox: 32
I (726) wifi_init: udp mbox: 6
I (726) wifi_init: tcp mbox: 6
I (726) wifi_init: tcp tx win: 5744
I (736) wifi_init: tcp rx win: 5744
I (736) wifi_init: tcp mss: 1440
I (746) wifi_init: WiFi IRAM OP enabled
I (746) wifi_init: WiFi RX IRAM OP enabled
I (756) phy_init: phy_version 4660,0162888,Dec 23 2020
I (856) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2076) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2846) wifi:state: init -> auth (b0)
I (2866) wifi:state: auth -> assoc (0)
I (2876) wifi:state: assoc -> run (10)
I (2886) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (2886) wifi:security: WPA2-PSK, phy: bgn, rssi: -38
I (2886) wifi:pm start, type: 1

W (2886) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (2926) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4656) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (4656) PERIPH_WIFI: Got ip:192.168.5.187
I (4656) BAIDU_SPEECH_EXAMPLE: [ 1 ] Start audio codec chip
I (4666) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (4676) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (4696) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (4696) ES8388_DRIVER: init,out:02, in:00
I (4706) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (4716) BAIDU_SPEECH_EXAMPLE: [2.0] Create audio pipeline for playback
I (4716) BAIDU_SPEECH_EXAMPLE: [2.1] Create http stream to read data
I (4716) BAIDU_SPEECH_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (4726) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (4736) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (4766) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (4776) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (4776) BAIDU_SPEECH_EXAMPLE: [2.3] Create mp3 decoder to decode mp3 file
I (4786) MP3_DECODER: MP3 init
I (4786) BAIDU_SPEECH_EXAMPLE: [2.4] Register all elements to audio pipeline
I (4796) BAIDU_SPEECH_EXAMPLE: [2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (4806) AUDIO_PIPELINE: link el->rb, el:0x3ffc732c, tag:http, rb:0x3ffc9ac8
I (4816) AUDIO_PIPELINE: link el->rb, el:0x3ffc9750, tag:mp3, rb:0x3ffcec04
I (4816) BAIDU_SPEECH_EXAMPLE: [2.6] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)
I (4826) BAIDU_SPEECH_EXAMPLE: [ 4 ] Set up  event listener
I (4836) BAIDU_SPEECH_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (4846) BAIDU_SPEECH_EXAMPLE: [4.2] Listening event from peripherals
I (4856) BAIDU_SPEECH_EXAMPLE: [ 5 ] Start audio_pipeline
I (4856) AUDIO_ELEMENT: [http-0x3ffc732c] Element task created
I (4866) AUDIO_ELEMENT: [mp3-0x3ffc9750] Element task created
I (4876) AUDIO_ELEMENT: [i2s-0x3ffc93d8] Element task created
I (4876) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:177400 Bytes

I (4886) AUDIO_ELEMENT: [http] AEL_MSG_CMD_RESUME,state:1
I (4896) AUDIO_ELEMENT: [mp3] AEL_MSG_CMD_RESUME,state:1
I (4896) MP3_DECODER: MP3 opened
I (4906) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (4906) I2S_STREAM: AUDIO_STREAM_WRITER
I (4916) AUDIO_PIPELINE: Pipeline started
I (4936) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (4976) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (4976) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (4986) I2S_STREAM: AUDIO_STREAM_WRITER
I (5866) BAIDU_AUTH: Access token=24.82ecf03372e3a42008fec94106143867.1639120517.282335-11047063
I (9286) HTTP_STREAM: total_bytes=12600
I (9336) BAIDU_SPEECH_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=16000, bits=16, ch=1
W (9366) HTTP_STREAM: No more data,errno:0, total_bytes:12600, rlen = 0
I (9366) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (9476) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (9496) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (9506) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (9506) I2S_STREAM: AUDIO_STREAM_WRITER
I (14116) AUDIO_ELEMENT: IN-[mp3] AEL_IO_DONE,-2
I (15166) MP3_DECODER: Closed
I (15276) AUDIO_ELEMENT: IN-[i2s] AEL_IO_DONE,-2
W (15496) BAIDU_SPEECH_EXAMPLE: [ * ] Stop event received
I (15496) BAIDU_SPEECH_EXAMPLE: [ 6 ] Stop audio_pipeline
E (15506) AUDIO_ELEMENT: [http] Element already stopped
E (15506) AUDIO_ELEMENT: [mp3] Element already stopped
E (15516) AUDIO_ELEMENT: [i2s] Element already stopped
W (15516) AUDIO_PIPELINE: There are no listener registered
I (15526) AUDIO_PIPELINE: audio_pipeline_unlinked
W (15526) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (15536) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (15546) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
I (15566) wifi:state: run -> init (0)
I (15566) wifi:pm stop, total sleep time: 10204939 us / 12675598 us

I (15566) wifi:new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (15576) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (15586) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3
I (15586) wifi:flush txq
I (15596) wifi:stop sw txq
I (15596) wifi:lmac stop hw txq
I (15596) wifi:Deinit lldesc rx mblock:10

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
I (27) boot: compile time 15:14:40
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x26c3c (158780) map
I (172) esp_image: segment 1: paddr=0x00036c64 vaddr=0x3ffb0000 size=0x0336c ( 13164) load
I (177) esp_image: segment 2: paddr=0x00039fd8 vaddr=0x40080000 size=0x06040 ( 24640) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (189) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xa77a0 (685984) map
0x400d0020: _stext at ??:?

I (451) esp_image: segment 4: paddr=0x000e77c8 vaddr=0x40086040 size=0x1149c ( 70812) load
0x40086040: bt_track_pll_cap at /home/cff/gittree/chip7.1_phy/chip_7.1/board_code/app_test/pp/phy/phy_chip_v7_ana.c:1595

I (494) boot: Loaded app from partition at offset 0x10000
I (494) boot: Disabling RNG early entropy source...
I (495) cpu_start: Pro cpu up.
I (498) cpu_start: Application information:
I (503) cpu_start: Project name:     play_baidu_tts_mp3
I (509) cpu_start: App version:      v2.2-221-gd27be99e
I (515) cpu_start: Compile time:     Nov 10 2021 15:14:34
I (521) cpu_start: ELF file SHA256:  1df6fe3040a08d9b...
I (527) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (533) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (544) heap_init: Initializing. RAM available for dynamic allocation:
I (551) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (557) heap_init: At 3FFB7EB0 len 00028150 (160 KiB): DRAM
I (563) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (569) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (576) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (582) cpu_start: Pro cpu start user code
I (600) spi_flash: detected chip: gd
I (601) spi_flash: flash io: dio
W (601) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (611) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (656) BAIDU_SPEECH_EXAMPLE: [ 0 ] Start and wait for Wi-Fi network
I (666) wifi:wifi driver task: 3ffc1c80, prio:23, stack:6656, core=0
I (666) system_api: Base MAC address is not set
I (666) system_api: read default base MAC address from EFUSE
I (676) wifi:wifi firmware version: bb6888c
I (676) wifi:wifi certification version: v7.0
I (676) wifi:config NVS flash: enabled
I (676) wifi:config nano formating: disabled
I (686) wifi:Init data frame dynamic rx buffer num: 32
I (686) wifi:Init management frame dynamic rx buffer num: 32
I (696) wifi:Init management short buffer num: 32
I (696) wifi:Init dynamic tx buffer num: 32
I (706) wifi:Init static rx buffer size: 1600
I (706) wifi:Init static rx buffer num: 10
I (706) wifi:Init dynamic rx buffer num: 32
I (716) wifi_init: rx ba win: 6
I (716) wifi_init: tcpip mbox: 32
I (726) wifi_init: udp mbox: 6
I (726) wifi_init: tcp mbox: 6
I (726) wifi_init: tcp tx win: 5744
I (736) wifi_init: tcp rx win: 5744
I (736) wifi_init: tcp mss: 1440
I (746) wifi_init: WiFi IRAM OP enabled
I (746) wifi_init: WiFi RX IRAM OP enabled
I (756) phy_init: phy_version 4660,0162888,Dec 23 2020
I (856) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2076) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2846) wifi:state: init -> auth (b0)
I (2866) wifi:state: auth -> assoc (0)
I (2876) wifi:state: assoc -> run (10)
I (2886) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (2886) wifi:security: WPA2-PSK, phy: bgn, rssi: -38
I (2886) wifi:pm start, type: 1

W (2886) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (2926) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4656) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (4656) PERIPH_WIFI: Got ip:192.168.5.187
I (4656) BAIDU_SPEECH_EXAMPLE: [ 1 ] Start audio codec chip
I (4666) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (4676) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (4696) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (4696) ES8388_DRIVER: init,out:02, in:00
I (4706) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (4716) BAIDU_SPEECH_EXAMPLE: [2.0] Create audio pipeline for playback
I (4716) BAIDU_SPEECH_EXAMPLE: [2.1] Create http stream to read data
I (4716) BAIDU_SPEECH_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (4726) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (4736) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (4766) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (4776) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (4776) BAIDU_SPEECH_EXAMPLE: [2.3] Create mp3 decoder to decode mp3 file
I (4786) MP3_DECODER: MP3 init
I (4786) BAIDU_SPEECH_EXAMPLE: [2.4] Register all elements to audio pipeline
I (4796) BAIDU_SPEECH_EXAMPLE: [2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (4806) AUDIO_PIPELINE: link el->rb, el:0x3ffc732c, tag:http, rb:0x3ffc9ac8
I (4816) AUDIO_PIPELINE: link el->rb, el:0x3ffc9750, tag:mp3, rb:0x3ffcec04
I (4816) BAIDU_SPEECH_EXAMPLE: [2.6] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)
I (4826) BAIDU_SPEECH_EXAMPLE: [ 4 ] Set up  event listener
I (4836) BAIDU_SPEECH_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (4846) BAIDU_SPEECH_EXAMPLE: [4.2] Listening event from peripherals
I (4856) BAIDU_SPEECH_EXAMPLE: [ 5 ] Start audio_pipeline
I (4856) AUDIO_ELEMENT: [http-0x3ffc732c] Element task created
I (4866) AUDIO_ELEMENT: [mp3-0x3ffc9750] Element task created
I (4876) AUDIO_ELEMENT: [i2s-0x3ffc93d8] Element task created
I (4876) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:177400 Bytes

I (4886) AUDIO_ELEMENT: [http] AEL_MSG_CMD_RESUME,state:1
I (4896) AUDIO_ELEMENT: [mp3] AEL_MSG_CMD_RESUME,state:1
I (4896) MP3_DECODER: MP3 opened
I (4906) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (4906) I2S_STREAM: AUDIO_STREAM_WRITER
I (4916) AUDIO_PIPELINE: Pipeline started
I (4936) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (4976) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (4976) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (4986) I2S_STREAM: AUDIO_STREAM_WRITER
I (5866) BAIDU_AUTH: Access token=24.82ecf03372e3a42008fec94106143867.1639120517.282335-11047063
I (9286) HTTP_STREAM: total_bytes=12600
I (9336) BAIDU_SPEECH_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=16000, bits=16, ch=1
W (9366) HTTP_STREAM: No more data,errno:0, total_bytes:12600, rlen = 0
I (9366) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (9476) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (9496) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (9506) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (9506) I2S_STREAM: AUDIO_STREAM_WRITER
I (14116) AUDIO_ELEMENT: IN-[mp3] AEL_IO_DONE,-2
I (15166) MP3_DECODER: Closed
I (15276) AUDIO_ELEMENT: IN-[i2s] AEL_IO_DONE,-2
W (15496) BAIDU_SPEECH_EXAMPLE: [ * ] Stop event received
I (15496) BAIDU_SPEECH_EXAMPLE: [ 6 ] Stop audio_pipeline
E (15506) AUDIO_ELEMENT: [http] Element already stopped
E (15506) AUDIO_ELEMENT: [mp3] Element already stopped
E (15516) AUDIO_ELEMENT: [i2s] Element already stopped
W (15516) AUDIO_PIPELINE: There are no listener registered
I (15526) AUDIO_PIPELINE: audio_pipeline_unlinked
W (15526) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (15536) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (15546) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
I (15566) wifi:state: run -> init (0)
I (15566) wifi:pm stop, total sleep time: 10204939 us / 12675598 us

I (15566) wifi:new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (15576) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (15586) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3
I (15586) wifi:flush txq
I (15596) wifi:stop sw txq
I (15596) wifi:lmac stop hw txq
I (15596) wifi:Deinit lldesc rx mblock:10

```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
