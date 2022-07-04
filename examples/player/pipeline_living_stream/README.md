# Play HTTP Live Streaming

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief


This example demonstrates how to play Internet radio in real-time over HTTP Live Streaming (HLS) in the ESP-ADF.


The pipeline is as follows:

```
[living_server] ---> http_stream ---> aac_decoder ---> i2s_stream ---> [codec_chip]
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

- After the example starts running, it connects to the Wi-Fi hotspot actively. If the connection succeeds, the example fetches the built-in live streaming audio from the Dragonfly FM Internet Radio for playback. The log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7556
load:0x40078000,len:14368
ho 0 tail 12 room 4
load:0x40080400,len:5360
0x40080400: _init at ??:?

entry 0x40080710
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 11:32:05
I (29) boot: chip revision: 3
I (33) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) qio_mode: Enabling default flash chip QIO
I (45) boot.esp32: SPI Speed      : 80MHz
I (50) boot.esp32: SPI Mode       : QIO
I (55) boot.esp32: SPI Flash Size : 4MB
I (59) boot: Enabling RNG early entropy source...
I (65) boot: Partition Table:
I (68) boot: ## Label            Usage          Type ST Offset   Length
I (75) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (83) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (90) boot:  2 factory          factory app      00 00 00010000 00300000
I (98) boot: End of partition table
I (102) boot_comm: chip revision: 3, min. application chip revision: 0
I (109) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2f1ac (192940) map
I (182) esp_image: segment 1: paddr=0x0003f1d4 vaddr=0x3ffb0000 size=0x00e44 (  3652) load
I (184) esp_image: segment 2: paddr=0x00040020 vaddr=0x400d0020 size=0xb53b8 (742328) map
0x400d0020: _stext at ??:?

I (434) esp_image: segment 3: paddr=0x000f53e0 vaddr=0x3ffb0e44 size=0x0259c (  9628) load
I (438) esp_image: segment 4: paddr=0x000f7984 vaddr=0x40080000 size=0x1b5f4 (112116) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (500) boot: Loaded app from partition at offset 0x10000
I (500) boot: Disabling RNG early entropy source...
I (501) psram: This chip is ESP32-D0WD
I (505) spiram: Found 64MBit SPI RAM device
I (510) spiram: SPI RAM mode: flash 80m sram 80m
I (515) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (522) cpu_start: Pro cpu up.
I (526) cpu_start: Application information:
I (531) cpu_start: Project name:     living_stream_m3u_aac
I (537) cpu_start: App version:      v2.2-230-g20fecbdb-dirty
I (543) cpu_start: Compile time:     Nov 16 2021 11:32:00
I (550) cpu_start: ELF file SHA256:  5d629db79b39127d...
I (556) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (561) cpu_start: Starting app cpu, entry point is 0x40081e68
0x40081e68: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1065) spiram: SPI SRAM memory test OK
I (1065) heap_init: Initializing. RAM available for dynamic allocation:
I (1065) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1071) heap_init: At 3FFB7BB8 len 00028448 (161 KiB): DRAM
I (1078) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1084) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1091) heap_init: At 4009B5F4 len 00004A0C (18 KiB): IRAM
I (1097) cpu_start: Pro cpu start user code
I (1102) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1122) spi_flash: detected chip: gd
I (1122) spi_flash: flash io: qio
W (1122) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1134) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1144) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1174) HTTP_LIVINGSTREAM_EXAMPLE: [ 1 ] Start audio codec chip
I (1174) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1184) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1184) ES8388_DRIVER: init,out:02, in:00
I (1194) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (1204) HTTP_LIVINGSTREAM_EXAMPLE: [2.0] Create audio pipeline for playback
I (1204) HTTP_LIVINGSTREAM_EXAMPLE: [2.1] Create http stream to read data
I (1214) HTTP_LIVINGSTREAM_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (1224) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1224) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1254) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1254) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1264) HTTP_LIVINGSTREAM_EXAMPLE: [2.3] Create aac decoder to decode aac file
I (1264) HTTP_LIVINGSTREAM_EXAMPLE: [2.4] Register all elements to audio pipeline
I (1274) HTTP_LIVINGSTREAM_EXAMPLE: [2.5] Link it together http_stream-->aac_decoder-->i2s_stream-->[codec_chip]
I (1284) AUDIO_PIPELINE: link el->rb, el:0x3f80051c, tag:http, rb:0x3f8009a8
I (1294) AUDIO_PIPELINE: link el->rb, el:0x3f800840, tag:aac, rb:0x3f8059e8
I (1304) HTTP_LIVINGSTREAM_EXAMPLE: [2.6] Set up  uri (http as http_stream, aac as aac decoder, and default output is i2s)
I (1314) HTTP_LIVINGSTREAM_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (1324) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1344) wifi:wifi driver task: 3ffca620, prio:23, stack:6656, core=0
I (1344) system_api: Base MAC address is not set
I (1344) system_api: read default base MAC address from EFUSE
I (1354) wifi:wifi firmware version: bb6888c
I (1354) wifi:wifi certification version: v7.0
I (1354) wifi:config NVS flash: enabled
I (1364) wifi:config nano formating: disabled
I (1364) wifi:Init data frame dynamic rx buffer num: 32
I (1364) wifi:Init management frame dynamic rx buffer num: 32
I (1374) wifi:Init management short buffer num: 32
I (1374) wifi:Init static tx buffer num: 16
I (1384) wifi:Init tx cache buffer num: 32
I (1384) wifi:Init static rx buffer size: 1600
I (1394) wifi:Init static rx buffer num: 10
I (1394) wifi:Init dynamic rx buffer num: 32
I (1394) wifi_init: rx ba win: 6
I (1404) wifi_init: tcpip mbox: 32
I (1404) wifi_init: udp mbox: 6
I (1414) wifi_init: tcp mbox: 6
I (1414) wifi_init: tcp tx win: 5744
I (1414) wifi_init: tcp rx win: 5744
I (1424) wifi_init: tcp mss: 1440
I (1424) wifi_init: WiFi IRAM OP enabled
I (1434) wifi_init: WiFi RX IRAM OP enabled
I (1434) phy_init: phy_version 4660,0162888,Dec 23 2020
I (1534) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2744) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (3404) wifi:state: init -> auth (b0)
I (3404) wifi:state: auth -> assoc (0)
I (3414) wifi:state: assoc -> run (10)
I (3424) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (3434) wifi:security: WPA2-PSK, phy: bgn, rssi: -38
I (3434) wifi:pm start, type: 1

W (3434) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (3534) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (5174) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (5174) PERIPH_WIFI: Got ip:192.168.5.187
I (5174) HTTP_LIVINGSTREAM_EXAMPLE: [ 4 ] Set up  event listener
I (5184) HTTP_LIVINGSTREAM_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (5184) HTTP_LIVINGSTREAM_EXAMPLE: [4.2] Listening event from peripherals
I (5194) HTTP_LIVINGSTREAM_EXAMPLE: [ 5 ] Start audio_pipeline
I (5204) AUDIO_THREAD: The http task allocate stack on external memory
I (5214) AUDIO_ELEMENT: [http-0x3f80051c] Element task created
I (5214) AUDIO_THREAD: The aac task allocate stack on external memory
I (5224) AUDIO_ELEMENT: [aac-0x3f800840] Element task created
I (5234) AUDIO_ELEMENT: [i2s-0x3f8006a4] Element task created
I (5234) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4280732 Bytes, Inter:153192 Bytes, Dram:134272 Bytes

I (5244) AUDIO_ELEMENT: [http] AEL_MSG_CMD_RESUME,state:1
I (5254) AUDIO_ELEMENT: [aac] AEL_MSG_CMD_RESUME,state:1
I (5264) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (5264) I2S_STREAM: AUDIO_STREAM_WRITER
I (5274) AUDIO_PIPELINE: Pipeline started
I (5474) HTTP_STREAM: total_bytes=781
I (5474) HTTP_STREAM: Live stream URI. Need to be fetched again!
I (5574) HTTP_STREAM: total_bytes=57630
I (5574) CODEC_ELEMENT_HELPER: The element is 0x3f800840. The reserve data 2 is 0x0.
I (5574) AAC_DECODER: a new song playing
I (5574) AAC_DECODER: this audio is RAW AAC
I (5604) HTTP_LIVINGSTREAM_EXAMPLE: [ * ] Receive music info from aac decoder, sample_rates=22050, bits=16, ch=2
I (5634) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (5654) I2S: APLL: Req RATE: 22050, real rate: 22049.982, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 5644795.500, SCLK: 705599.437500, diva: 1, divb: 0
I (5664) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (5664) I2S_STREAM: AUDIO_STREAM_WRITER
W (9854) HTTP_STREAM: No more data,errno:0, total_bytes:57630, rlen = 0
I (9854) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (9914) HTTP_STREAM: total_bytes=57491
W (16924) HTTP_STREAM: No more data,errno:0, total_bytes:57491, rlen = 0
I (16924) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (16974) HTTP_STREAM: total_bytes=57452
W (23964) HTTP_STREAM: No more data,errno:0, total_bytes:57452, rlen = 0
I (23964) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (24024) HTTP_STREAM: total_bytes=57550
I (24104) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (24104) HEADPHONE: Headphone jack inserted
W (31024) HTTP_STREAM: No more data,errno:0, total_bytes:57550, rlen = 0
I (31024) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (31094) HTTP_STREAM: total_bytes=57561
W (38084) HTTP_STREAM: No more data,errno:0, total_bytes:57561, rlen = 0
I (38084) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (38154) HTTP_STREAM: total_bytes=57834
W (45194) HTTP_STREAM: No more data,errno:0, total_bytes:57834, rlen = 0
I (45194) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (45254) HTTP_STREAM: total_bytes=57433
W (52214) HTTP_STREAM: No more data,errno:0, total_bytes:57433, rlen = 0
I (52214) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (52454) HTTP_STREAM: total_bytes=57466
W (59264) HTTP_STREAM: No more data,errno:0, total_bytes:57466, rlen = 0
I (59264) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (59264) HTTP_STREAM: Fetching again...
I (59514) HTTP_STREAM: total_bytes=781
W (59514) HLS_PLAYLIST: URI exist
I (59514) HTTP_STREAM: Live stream URI. Need to be fetched again!
I (59594) HTTP_STREAM: total_bytes=57561
W (66184) HTTP_STREAM: No more data,errno:0, total_bytes:57561, rlen = 0
I (66184) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (66244) HTTP_STREAM: total_bytes=57535
W (73244) HTTP_STREAM: No more data,errno:0, total_bytes:57535, rlen = 0
I (73244) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (73304) HTTP_STREAM: total_bytes=57477
W (80304) HTTP_STREAM: No more data,errno:0, total_bytes:57477, rlen = 0
I (80304) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (80364) HTTP_STREAM: total_bytes=57658
W (87364) HTTP_STREAM: No more data,errno:0, total_bytes:57658, rlen = 0
I (87364) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (87424) HTTP_STREAM: total_bytes=57451
W (94414) HTTP_STREAM: No more data,errno:0, total_bytes:57451, rlen = 0
I (94414) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (94474) HTTP_STREAM: total_bytes=57536
W (101474) HTTP_STREAM: No more data,errno:0, total_bytes:57536, rlen = 0
I (101474) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (101534) HTTP_STREAM: total_bytes=57514
W (108534) HTTP_STREAM: No more data,errno:0, total_bytes:57514, rlen = 0
I (108544) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (108544) HTTP_STREAM: Fetching again...
I (108964) HTTP_STREAM: total_bytes=781
I (108964) HTTP_STREAM: Live stream URI. Need to be fetched again!
I (109054) HTTP_STREAM: total_bytes=57514
W (115464) HTTP_STREAM: No more data,errno:0, total_bytes:57514, rlen = 0
I (115464) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (115514) HTTP_STREAM: total_bytes=57570
W (122514) HTTP_STREAM: No more data,errno:0, total_bytes:57570, rlen = 0
I (122514) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (122564) HTTP_STREAM: total_bytes=57545
W (129574) HTTP_STREAM: No more data,errno:0, total_bytes:57545, rlen = 0
I (129574) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (129634) HTTP_STREAM: total_bytes=57572
W (136634) HTTP_STREAM: No more data,errno:0, total_bytes:57572, rlen = 0
I (136634) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (136694) HTTP_STREAM: total_bytes=57376
W (143694) HTTP_STREAM: No more data,errno:0, total_bytes:57376, rlen = 0
I (143694) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (143754) HTTP_STREAM: total_bytes=57558
W (150754) HTTP_STREAM: No more data,errno:0, total_bytes:57558, rlen = 0
I (150754) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (150804) HTTP_STREAM: total_bytes=57557
W (157814) HTTP_STREAM: No more data,errno:0, total_bytes:57557, rlen = 0
I (157814) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (157874) HTTP_STREAM: total_bytes=57525
W (164864) HTTP_STREAM: No more data,errno:0, total_bytes:57525, rlen = 0
I (164864) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (164864) HTTP_STREAM: Fetching again...
I (165014) HTTP_STREAM: total_bytes=781
W (165014) HLS_PLAYLIST: URI exist

```


### Example Log
A complete log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7556
load:0x40078000,len:14368
ho 0 tail 12 room 4
load:0x40080400,len:5360
0x40080400: _init at ??:?

entry 0x40080710
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 11:32:05
I (29) boot: chip revision: 3
I (33) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) qio_mode: Enabling default flash chip QIO
I (45) boot.esp32: SPI Speed      : 80MHz
I (50) boot.esp32: SPI Mode       : QIO
I (55) boot.esp32: SPI Flash Size : 4MB
I (59) boot: Enabling RNG early entropy source...
I (65) boot: Partition Table:
I (68) boot: ## Label            Usage          Type ST Offset   Length
I (75) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (83) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (90) boot:  2 factory          factory app      00 00 00010000 00300000
I (98) boot: End of partition table
I (102) boot_comm: chip revision: 3, min. application chip revision: 0
I (109) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2f1ac (192940) map
I (182) esp_image: segment 1: paddr=0x0003f1d4 vaddr=0x3ffb0000 size=0x00e44 (  3652) load
I (184) esp_image: segment 2: paddr=0x00040020 vaddr=0x400d0020 size=0xb53b8 (742328) map
0x400d0020: _stext at ??:?

I (434) esp_image: segment 3: paddr=0x000f53e0 vaddr=0x3ffb0e44 size=0x0259c (  9628) load
I (438) esp_image: segment 4: paddr=0x000f7984 vaddr=0x40080000 size=0x1b5f4 (112116) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (500) boot: Loaded app from partition at offset 0x10000
I (500) boot: Disabling RNG early entropy source...
I (501) psram: This chip is ESP32-D0WD
I (505) spiram: Found 64MBit SPI RAM device
I (510) spiram: SPI RAM mode: flash 80m sram 80m
I (515) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (522) cpu_start: Pro cpu up.
I (526) cpu_start: Application information:
I (531) cpu_start: Project name:     living_stream_m3u_aac
I (537) cpu_start: App version:      v2.2-230-g20fecbdb-dirty
I (543) cpu_start: Compile time:     Nov 16 2021 11:32:00
I (550) cpu_start: ELF file SHA256:  5d629db79b39127d...
I (556) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (561) cpu_start: Starting app cpu, entry point is 0x40081e68
0x40081e68: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1065) spiram: SPI SRAM memory test OK
I (1065) heap_init: Initializing. RAM available for dynamic allocation:
I (1065) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1071) heap_init: At 3FFB7BB8 len 00028448 (161 KiB): DRAM
I (1078) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1084) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1091) heap_init: At 4009B5F4 len 00004A0C (18 KiB): IRAM
I (1097) cpu_start: Pro cpu start user code
I (1102) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1122) spi_flash: detected chip: gd
I (1122) spi_flash: flash io: qio
W (1122) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1134) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1144) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1174) HTTP_LIVINGSTREAM_EXAMPLE: [ 1 ] Start audio codec chip
I (1174) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1184) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1184) ES8388_DRIVER: init,out:02, in:00
I (1194) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (1204) HTTP_LIVINGSTREAM_EXAMPLE: [2.0] Create audio pipeline for playback
I (1204) HTTP_LIVINGSTREAM_EXAMPLE: [2.1] Create http stream to read data
I (1214) HTTP_LIVINGSTREAM_EXAMPLE: [2.2] Create i2s stream to write data to codec chip
I (1224) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1224) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1254) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1254) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1264) HTTP_LIVINGSTREAM_EXAMPLE: [2.3] Create aac decoder to decode aac file
I (1264) HTTP_LIVINGSTREAM_EXAMPLE: [2.4] Register all elements to audio pipeline
I (1274) HTTP_LIVINGSTREAM_EXAMPLE: [2.5] Link it together http_stream-->aac_decoder-->i2s_stream-->[codec_chip]
I (1284) AUDIO_PIPELINE: link el->rb, el:0x3f80051c, tag:http, rb:0x3f8009a8
I (1294) AUDIO_PIPELINE: link el->rb, el:0x3f800840, tag:aac, rb:0x3f8059e8
I (1304) HTTP_LIVINGSTREAM_EXAMPLE: [2.6] Set up  uri (http as http_stream, aac as aac decoder, and default output is i2s)
I (1314) HTTP_LIVINGSTREAM_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (1324) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1344) wifi:wifi driver task: 3ffca620, prio:23, stack:6656, core=0
I (1344) system_api: Base MAC address is not set
I (1344) system_api: read default base MAC address from EFUSE
I (1354) wifi:wifi firmware version: bb6888c
I (1354) wifi:wifi certification version: v7.0
I (1354) wifi:config NVS flash: enabled
I (1364) wifi:config nano formating: disabled
I (1364) wifi:Init data frame dynamic rx buffer num: 32
I (1364) wifi:Init management frame dynamic rx buffer num: 32
I (1374) wifi:Init management short buffer num: 32
I (1374) wifi:Init static tx buffer num: 16
I (1384) wifi:Init tx cache buffer num: 32
I (1384) wifi:Init static rx buffer size: 1600
I (1394) wifi:Init static rx buffer num: 10
I (1394) wifi:Init dynamic rx buffer num: 32
I (1394) wifi_init: rx ba win: 6
I (1404) wifi_init: tcpip mbox: 32
I (1404) wifi_init: udp mbox: 6
I (1414) wifi_init: tcp mbox: 6
I (1414) wifi_init: tcp tx win: 5744
I (1414) wifi_init: tcp rx win: 5744
I (1424) wifi_init: tcp mss: 1440
I (1424) wifi_init: WiFi IRAM OP enabled
I (1434) wifi_init: WiFi RX IRAM OP enabled
I (1434) phy_init: phy_version 4660,0162888,Dec 23 2020
I (1534) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2744) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (3404) wifi:state: init -> auth (b0)
I (3404) wifi:state: auth -> assoc (0)
I (3414) wifi:state: assoc -> run (10)
I (3424) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (3434) wifi:security: WPA2-PSK, phy: bgn, rssi: -38
I (3434) wifi:pm start, type: 1

W (3434) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (3534) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (5174) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (5174) PERIPH_WIFI: Got ip:192.168.5.187
I (5174) HTTP_LIVINGSTREAM_EXAMPLE: [ 4 ] Set up  event listener
I (5184) HTTP_LIVINGSTREAM_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (5184) HTTP_LIVINGSTREAM_EXAMPLE: [4.2] Listening event from peripherals
I (5194) HTTP_LIVINGSTREAM_EXAMPLE: [ 5 ] Start audio_pipeline
I (5204) AUDIO_THREAD: The http task allocate stack on external memory
I (5214) AUDIO_ELEMENT: [http-0x3f80051c] Element task created
I (5214) AUDIO_THREAD: The aac task allocate stack on external memory
I (5224) AUDIO_ELEMENT: [aac-0x3f800840] Element task created
I (5234) AUDIO_ELEMENT: [i2s-0x3f8006a4] Element task created
I (5234) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4280732 Bytes, Inter:153192 Bytes, Dram:134272 Bytes

I (5244) AUDIO_ELEMENT: [http] AEL_MSG_CMD_RESUME,state:1
I (5254) AUDIO_ELEMENT: [aac] AEL_MSG_CMD_RESUME,state:1
I (5264) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (5264) I2S_STREAM: AUDIO_STREAM_WRITER
I (5274) AUDIO_PIPELINE: Pipeline started
I (5474) HTTP_STREAM: total_bytes=781
I (5474) HTTP_STREAM: Live stream URI. Need to be fetched again!
I (5574) HTTP_STREAM: total_bytes=57630
I (5574) CODEC_ELEMENT_HELPER: The element is 0x3f800840. The reserve data 2 is 0x0.
I (5574) AAC_DECODER: a new song playing
I (5574) AAC_DECODER: this audio is RAW AAC
I (5604) HTTP_LIVINGSTREAM_EXAMPLE: [ * ] Receive music info from aac decoder, sample_rates=22050, bits=16, ch=2
I (5634) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (5654) I2S: APLL: Req RATE: 22050, real rate: 22049.982, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 5644795.500, SCLK: 705599.437500, diva: 1, divb: 0
I (5664) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (5664) I2S_STREAM: AUDIO_STREAM_WRITER
W (9854) HTTP_STREAM: No more data,errno:0, total_bytes:57630, rlen = 0
I (9854) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (9914) HTTP_STREAM: total_bytes=57491
W (16924) HTTP_STREAM: No more data,errno:0, total_bytes:57491, rlen = 0
I (16924) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (16974) HTTP_STREAM: total_bytes=57452
W (23964) HTTP_STREAM: No more data,errno:0, total_bytes:57452, rlen = 0
I (23964) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (24024) HTTP_STREAM: total_bytes=57550
I (24104) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (24104) HEADPHONE: Headphone jack inserted
W (31024) HTTP_STREAM: No more data,errno:0, total_bytes:57550, rlen = 0
I (31024) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (31094) HTTP_STREAM: total_bytes=57561
W (38084) HTTP_STREAM: No more data,errno:0, total_bytes:57561, rlen = 0
I (38084) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (38154) HTTP_STREAM: total_bytes=57834
W (45194) HTTP_STREAM: No more data,errno:0, total_bytes:57834, rlen = 0
I (45194) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (45254) HTTP_STREAM: total_bytes=57433
W (52214) HTTP_STREAM: No more data,errno:0, total_bytes:57433, rlen = 0
I (52214) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (52454) HTTP_STREAM: total_bytes=57466
W (59264) HTTP_STREAM: No more data,errno:0, total_bytes:57466, rlen = 0
I (59264) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (59264) HTTP_STREAM: Fetching again...
I (59514) HTTP_STREAM: total_bytes=781
W (59514) HLS_PLAYLIST: URI exist
I (59514) HTTP_STREAM: Live stream URI. Need to be fetched again!
I (59594) HTTP_STREAM: total_bytes=57561
W (66184) HTTP_STREAM: No more data,errno:0, total_bytes:57561, rlen = 0
I (66184) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (66244) HTTP_STREAM: total_bytes=57535
W (73244) HTTP_STREAM: No more data,errno:0, total_bytes:57535, rlen = 0
I (73244) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (73304) HTTP_STREAM: total_bytes=57477
W (80304) HTTP_STREAM: No more data,errno:0, total_bytes:57477, rlen = 0
I (80304) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (80364) HTTP_STREAM: total_bytes=57658
W (87364) HTTP_STREAM: No more data,errno:0, total_bytes:57658, rlen = 0
I (87364) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (87424) HTTP_STREAM: total_bytes=57451
W (94414) HTTP_STREAM: No more data,errno:0, total_bytes:57451, rlen = 0
I (94414) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (94474) HTTP_STREAM: total_bytes=57536
W (101474) HTTP_STREAM: No more data,errno:0, total_bytes:57536, rlen = 0
I (101474) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (101534) HTTP_STREAM: total_bytes=57514
W (108534) HTTP_STREAM: No more data,errno:0, total_bytes:57514, rlen = 0
I (108544) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (108544) HTTP_STREAM: Fetching again...
I (108964) HTTP_STREAM: total_bytes=781
I (108964) HTTP_STREAM: Live stream URI. Need to be fetched again!
I (109054) HTTP_STREAM: total_bytes=57514
W (115464) HTTP_STREAM: No more data,errno:0, total_bytes:57514, rlen = 0
I (115464) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (115514) HTTP_STREAM: total_bytes=57570
W (122514) HTTP_STREAM: No more data,errno:0, total_bytes:57570, rlen = 0
I (122514) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (122564) HTTP_STREAM: total_bytes=57545
W (129574) HTTP_STREAM: No more data,errno:0, total_bytes:57545, rlen = 0
I (129574) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (129634) HTTP_STREAM: total_bytes=57572
W (136634) HTTP_STREAM: No more data,errno:0, total_bytes:57572, rlen = 0
I (136634) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (136694) HTTP_STREAM: total_bytes=57376
W (143694) HTTP_STREAM: No more data,errno:0, total_bytes:57376, rlen = 0
I (143694) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (143754) HTTP_STREAM: total_bytes=57558
W (150754) HTTP_STREAM: No more data,errno:0, total_bytes:57558, rlen = 0
I (150754) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (150804) HTTP_STREAM: total_bytes=57557
W (157814) HTTP_STREAM: No more data,errno:0, total_bytes:57557, rlen = 0
I (157814) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (157874) HTTP_STREAM: total_bytes=57525
W (164864) HTTP_STREAM: No more data,errno:0, total_bytes:57525, rlen = 0
I (164864) AUDIO_ELEMENT: IN-[http] AEL_IO_DONE,0
I (164864) HTTP_STREAM: Fetching again...
I (165014) HTTP_STREAM: total_bytes=781
W (165014) HLS_PLAYLIST: URI exist

```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
