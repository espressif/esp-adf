# 播放 HTTP MP3 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介


本例程在 ADF 框架下实现在线 HTTP 的 MP3 音频流的播放演示。


本例程的管道如下图：

```
[http_server] ---> http_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
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

本例程需要先配置 Wi-Fi 连接信息，通过运行 `menuconfig > Example Configuration` 填写 `Wi-Fi SSID` 和 `Wi-Fi Password`。

```
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程

### 功能和用法

- 例程开始运行后，将主动连接 Wi-Fi 热点，如连接成功，则获取 HTTP 服务器的在线 MP3 音频进行播放，打印如下：

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
I (27) boot: compile time 19:12:45
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x26784 (157572) map
I (171) esp_image: segment 1: paddr=0x000367ac vaddr=0x3ffb0000 size=0x0336c ( 13164) load
I (177) esp_image: segment 2: paddr=0x00039b20 vaddr=0x40080000 size=0x064f8 ( 25848) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (189) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xa6fb0 (683952) map
0x400d0020: _stext at ??:?

I (450) esp_image: segment 4: paddr=0x000e6fd8 vaddr=0x400864f8 size=0x10fe4 ( 69604) load
0x400864f8: set_tx_gain_table at /home/cff/gittree/chip7.1_phy/chip_7.1/board_code/app_test/pp/phy/phy_chip_v7.c:1108

I (493) boot: Loaded app from partition at offset 0x10000
I (493) boot: Disabling RNG early entropy source...
I (494) cpu_start: Pro cpu up.
I (497) cpu_start: Application information:
I (502) cpu_start: Project name:     play_http_mp3
I (508) cpu_start: App version:      v2.2-230-g20fecbdb
I (513) cpu_start: Compile time:     Nov 16 2021 11:20:19
I (520) cpu_start: ELF file SHA256:  7792cd659d27ccf4...
I (526) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (532) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (542) heap_init: Initializing. RAM available for dynamic allocation:
I (549) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (555) heap_init: At 3FFB7AA8 len 00028558 (161 KiB): DRAM
I (561) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (567) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (574) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (580) cpu_start: Pro cpu start user code
I (598) spi_flash: detected chip: gd
I (599) spi_flash: flash io: dio
W (599) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (609) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (656) HTTP_MP3_EXAMPLE: [ 1 ] Start audio codec chip
I (676) HTTP_MP3_EXAMPLE: [2.0] Create audio pipeline for playback
I (676) HTTP_MP3_EXAMPLE: [2.1] Create http stream to read data
I (676) HTTP_MP3_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (716) HTTP_MP3_EXAMPLE: [2.3] Create mp3 decoder to decode mp3 file
I (716) HTTP_MP3_EXAMPLE: [2.4] Register all elements to audio pipeline
I (716) HTTP_MP3_EXAMPLE: [2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (726) HTTP_MP3_EXAMPLE: [2.6] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)
I (736) HTTP_MP3_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (746) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (3096) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (5156) HTTP_MP3_EXAMPLE: [ 4 ] Set up  event listener
I (5156) HTTP_MP3_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (5156) HTTP_MP3_EXAMPLE: [4.2] Listening event from peripherals
I (5166) HTTP_MP3_EXAMPLE: [ 5 ] Start audio_pipeline
I (8346) HTTP_MP3_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (226316) HTTP_STREAM: No more data,errno:0, total_bytes:2994349, rlen = 0
W (227736) HTTP_MP3_EXAMPLE: [ * ] Stop event received
I (227736) HTTP_MP3_EXAMPLE: [ 6 ] Stop audio_pipeline
E (227736) AUDIO_ELEMENT: [http] Element already stopped
E (227736) AUDIO_ELEMENT: [mp3] Element already stopped
E (227746) AUDIO_ELEMENT: [i2s] Element already stopped
W (227756) AUDIO_PIPELINE: There are no listener registered
W (227756) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (227766) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (227776) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (227796) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (227796) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3

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
I (27) boot: compile time 19:12:45
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x26784 (157572) map
I (171) esp_image: segment 1: paddr=0x000367ac vaddr=0x3ffb0000 size=0x0336c ( 13164) load
I (177) esp_image: segment 2: paddr=0x00039b20 vaddr=0x40080000 size=0x064f8 ( 25848) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (189) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xa6fb0 (683952) map
0x400d0020: _stext at ??:?

I (450) esp_image: segment 4: paddr=0x000e6fd8 vaddr=0x400864f8 size=0x10fe4 ( 69604) load
0x400864f8: set_tx_gain_table at /home/cff/gittree/chip7.1_phy/chip_7.1/board_code/app_test/pp/phy/phy_chip_v7.c:1108

I (493) boot: Loaded app from partition at offset 0x10000
I (493) boot: Disabling RNG early entropy source...
I (494) cpu_start: Pro cpu up.
I (497) cpu_start: Application information:
I (502) cpu_start: Project name:     play_http_mp3
I (508) cpu_start: App version:      v2.2-230-g20fecbdb
I (513) cpu_start: Compile time:     Nov 16 2021 11:20:19
I (520) cpu_start: ELF file SHA256:  7792cd659d27ccf4...
I (526) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (532) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (542) heap_init: Initializing. RAM available for dynamic allocation:
I (549) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (555) heap_init: At 3FFB7AA8 len 00028558 (161 KiB): DRAM
I (561) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (567) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (574) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (580) cpu_start: Pro cpu start user code
I (598) spi_flash: detected chip: gd
I (599) spi_flash: flash io: dio
W (599) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (609) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (656) HTTP_MP3_EXAMPLE: [ 1 ] Start audio codec chip
I (676) HTTP_MP3_EXAMPLE: [2.0] Create audio pipeline for playback
I (676) HTTP_MP3_EXAMPLE: [2.1] Create http stream to read data
I (676) HTTP_MP3_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (716) HTTP_MP3_EXAMPLE: [2.3] Create mp3 decoder to decode mp3 file
I (716) HTTP_MP3_EXAMPLE: [2.4] Register all elements to audio pipeline
I (716) HTTP_MP3_EXAMPLE: [2.5] Link it together http_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (726) HTTP_MP3_EXAMPLE: [2.6] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)
I (736) HTTP_MP3_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (746) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (3096) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (5156) HTTP_MP3_EXAMPLE: [ 4 ] Set up  event listener
I (5156) HTTP_MP3_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (5156) HTTP_MP3_EXAMPLE: [4.2] Listening event from peripherals
I (5166) HTTP_MP3_EXAMPLE: [ 5 ] Start audio_pipeline
I (8346) HTTP_MP3_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (226316) HTTP_STREAM: No more data,errno:0, total_bytes:2994349, rlen = 0
W (227736) HTTP_MP3_EXAMPLE: [ * ] Stop event received
I (227736) HTTP_MP3_EXAMPLE: [ 6 ] Stop audio_pipeline
E (227736) AUDIO_ELEMENT: [http] Element already stopped
E (227736) AUDIO_ELEMENT: [mp3] Element already stopped
E (227746) AUDIO_ELEMENT: [i2s] Element already stopped
W (227756) AUDIO_PIPELINE: There are no listener registered
W (227756) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (227766) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (227776) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (227796) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (227796) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3

```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
