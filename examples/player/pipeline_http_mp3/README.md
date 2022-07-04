# Play MP3 File from HTTP

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief


This example demonstrates how to play MP3 audio streams from an online HTTP in the ESP-ADF.


The pipeline is as follows:

```
[http_server] ---> http_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
```


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.
If you select `CONFIG_ESP32_C3_LYRA_V2_BOARD`, you need to apply `idf_v4.4_i2s_c3_pdm_tx.patch` in the `$ADF_PATH/esp-idf` directory.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

Run `menuconfig > Example Configuration` to first configure the Wi-Fi connection information and fill in the `Wi-Fi SSID` and `Wi-Fi Password`.

```
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.


## How to Use the Example

### Example Functionality

- After the example starts running, it connects to the Wi-Fi hotspot actively. If the connection succeeds, the example aquires the online MP3 audio file from the HTTP server for playback. The log is as follows:

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


### Example Log
A complete log is as follows:

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


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
