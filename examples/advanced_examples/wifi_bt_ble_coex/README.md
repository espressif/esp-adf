# Coexistence among Wi-Fi, A2DP, HFP and Bluetooth LE

- [中文版本](./README_CN.md)
- Baisc Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example creates several GATT services and starts ADV with the name of `BLUFI_DEVICE`, and then waits for connection. The device can be configured to connect to Wi-Fi and BluFi services. When Wi-Fi is connected, the example demonstrates how to create an HTTP STREAM module, which can play music with HTTP URIs.

The Classic Bluetooth part of this example implements the A2DP SINK. After the program is launched, Bluetooth devices such as smart phones can find the device named `ESP_ADF_COEX_EXAMPLE` and connect to it. Then, you can use the AVRCP configuration file to control the Bluetooth music status.


## Environment Setup

#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the table below. Please remember to select the board in menuconfig as discussed in Section *Configuration* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Incompatible") |


## Build and Flash

### Default IDF Branch

This example involves the coexistence of Wi-Fi, A2DP, HFP, and Bluetooth LE, so the IDF branch that optimizes coexistence is used - [audio/stack_on_psram_v3.3](https://github.com/espressif/esp-idf/tree/audio/stack_on_psram_v3.3).


### Configuration

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
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

- In this example, the coexisting Bluetooth LE service can also be used. Open your mobile phone `EspBluFi` application to connect to the Bluetooth LE device named `BLUFI_DEVICE`, configure the SSID and password, and then connect to the Wi-Fi network.

- After the example program starts running, it waits for you to connect the mobile phone to the device. The A2DP device advertising name is `ESP_ADF_COEX_EXAMPLE`. The log is as follows:

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 19:42:18
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 40MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 4MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (68) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00300000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x4fe38 (327224) map
I (220) esp_image: segment 1: paddr=0x0005fe60 vaddr=0x3ffbdb60 size=0x001b8 (   440) load
I (221) esp_image: segment 2: paddr=0x00060020 vaddr=0x400d0020 size=0x146e50 (1338960) map
0x400d0020: _stext at ??:?

I (737) esp_image: segment 3: paddr=0x001a6e78 vaddr=0x3ffbdd18 size=0x040f0 ( 16624) load
I (744) esp_image: segment 4: paddr=0x001aaf70 vaddr=0x40080000 size=0x1d354 (119636) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (812) boot: Loaded app from partition at offset 0x10000
I (813) boot: Disabling RNG early entropy source...
I (813) psram: This chip is ESP32-D0WD
I (818) spiram: Found 64MBit SPI RAM device
I (822) spiram: SPI RAM mode: flash 40m sram 40m
I (827) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (835) cpu_start: Pro cpu up.
I (838) cpu_start: Application information:
I (843) cpu_start: Project name:     wifi_bt_ble_coex
I (849) cpu_start: App version:      v2.2-201-g6fa02923
I (855) cpu_start: Compile time:     Oct  8 2021 19:42:12
I (861) cpu_start: ELF file SHA256:  d0e590c88daac1b0...
I (867) cpu_start: ESP-IDF:          v4.2.2
I (872) cpu_start: Starting app cpu, entry point is 0x40081d3c
0x40081d3c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1766) spiram: SPI SRAM memory test OK
I (1775) heap_init: Initializing. RAM available for dynamic allocation:
I (1775) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1777) heap_init: At 3FFB7468 len 00000B98 (2 KiB): DRAM
I (1783) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1790) heap_init: At 3FFC3A48 len 0001C5B8 (113 KiB): DRAM
I (1796) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1802) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1809) heap_init: At 4009D354 len 00002CAC (11 KiB): IRAM
I (1815) cpu_start: Pro cpu start user code
I (1820) spiram: Adding pool of 4020K of external SPI memory to heap allocator
I (1843) spi_flash: detected chip: gd
I (1843) spi_flash: flash io: dio
W (1843) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1854) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1864) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1914) COEX_EXAMPLE: [ 1 ] Create coex handle for a2dp-gatt-wifi
I (1914) COEX_EXAMPLE: [ 2 ] Initialize peripherals
I (1914) COEX_EXAMPLE: [ 3 ] create and start input key service
I (1914) COEX_EXAMPLE: [ 4 ] Start a2dp and blufi network
W (1934) PERIPH_TOUCH: _touch_init
I (1934) COEX_EXAMPLE: [4.1] Init Bluetooth
I (2454) COEX_EXAMPLE: [4.2] init gatts
I (2454) COEX_EXAMPLE: Blufi module init
E (2454) DISPATCHER: exe first list: 0x0
I (2484) COEX_EXAMPLE: [4.3] Create Bluetooth peripheral
I (2484) COEX_EXAMPLE: [4.4] Start peripherals
I (2484) COEX_EXAMPLE: [4.5] init hfp_stream
E (2534) gpio: gpio_install_isr_service(438): GPIO isr service already installed

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-9-g84df87e-037bef3-09be8fe                |
|                     Compile date: Jul 20 2021-13:51:36                     |
------------------------------------------------------------------------------
W (2744) BT_BTC: A2DP Enable with AVRC
I (2754) COEX_EXAMPLE: A2DP sink user cb
I (2754) COEX_EXAMPLE: User cb A2DP event: 4
I (2804) COEX_EXAMPLE: Func:setup_player, Line:298, MEM Total:4150532 Bytes, Inter:49696 Bytes, Dram:38296 Bytes

I (2804) COEX_EXAMPLE: esp_audio instance is:0x3f813480

I (2814) COEX_EXAMPLE: [ 5 ] Create audio pipeline for playback
I (2814) COEX_EXAMPLE: [5.1] Create i2s stream to read data from codec chip
W (2824) I2S: I2S driver already installed
I (2834) COEX_EXAMPLE: [5.2] Create hfp stream
I (2834) COEX_EXAMPLE: [5.3] Register i2s reader and hfp outgoing to audio pipeline
I (2854) COEX_EXAMPLE: [5.4] Link it together [codec_chip]-->i2s_stream_reader-->filter-->hfp_out_stream-->[Bluetooth]
I (2854) COEX_EXAMPLE: [5.5] Start audio_pipeline out
W (6104) WIFI_SERV: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
W (7904) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (7984) COEX_EXAMPLE: WIFI_CONNECTED
```

- When you connect the mobile phone to the A2DP device, the log will display as follows:

```c
W (76644) BT_APPL: new conn_srvc id:27, app_id:1
W (77114) BT_APPL: bta_hf_client_send_at: No service, skipping 11 command
E (77344) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (77344) COEX_EXAMPLE: A2DP sink user cb
I (77404) COEX_EXAMPLE: A2DP sink user cb
I (77404) COEX_EXAMPLE: User cb A2DP event: 2
W (77574) BT_APPL: new conn_srvc id:19, app_id:0
I (77574) COEX_EXAMPLE: A2DP sink user cb
I (77574) COEX_EXAMPLE: A2DP connected
W (101574) BT_APPL: new conn_srvc id:19, app_id:1
I (101574) COEX_EXAMPLE: A2DP sink user cb
I (101574) COEX_EXAMPLE: User cb A2DP event: 1
I (112794) COEX_EXAMPLE: A2DP sink user cb
I (112794) COEX_EXAMPLE: User cb A2DP event: 1

```

- In this example, you can press [MODE] to enter Classic Bluetooth mode or Wi-Fi mode, and the audio development board will play HTTP STREAM network music or A2DP Bluetooth music.

```c
I (16106) COEX_EXAMPLE: [ * ] input key id is 4, key type is 1
I (16236) COEX_EXAMPLE: [ * ] input key id is 4, key type is 2
I (16236) COEX_EXAMPLE: [ * ] [mode]
I (16236) COEX_EXAMPLE: [ * ] Enter BT mode
W (16496) BT_APPL: new conn_srvc id:19, app_id:1
I (16496) COEX_EXAMPLE: A2DP sink user cb
I (16506) COEX_EXAMPLE: User cb A2DP event: 1
I (25346) COEX_EXAMPLE: [ * ] input key id is 4, key type is 1
I (25496) COEX_EXAMPLE: [ * ] input key id is 4, key type is 2
I (25496) COEX_EXAMPLE: [ * ] [mode]
I (25496) COEX_EXAMPLE: [ * ] Enter WIFI mode
W (25806) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_ABORT
W (25806) AUDIO_ELEMENT: IN-[DEC_pcm] AEL_IO_ABORT
W (25826) ESP_AUDIO_TASK: Destroy the old pipeline, STOPPED
W (25836) ESP_AUDIO_TASK: The old pipeline destroyed, STOPPED
W (25836) AUDIO_PIPELINE: Without stop, st:1
W (25836) AUDIO_PIPELINE: Without wait stop, st:1
W (25846) AUDIO_PIPELINE: There are no listener registered
I (26066) COEX_EXAMPLE: A2DP sink user cb
I (26066) COEX_EXAMPLE: User cb A2DP event: 1

```

### Example Log

A complete log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7152
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 19:42:18
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 40MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 4MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (68) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00300000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x4fe38 (327224) map
I (220) esp_image: segment 1: paddr=0x0005fe60 vaddr=0x3ffbdb60 size=0x001b8 (   440) load
I (221) esp_image: segment 2: paddr=0x00060020 vaddr=0x400d0020 size=0x146e50 (1338960) map
0x400d0020: _stext at ??:?

I (737) esp_image: segment 3: paddr=0x001a6e78 vaddr=0x3ffbdd18 size=0x040f0 ( 16624) load
I (744) esp_image: segment 4: paddr=0x001aaf70 vaddr=0x40080000 size=0x1d354 (119636) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (812) boot: Loaded app from partition at offset 0x10000
I (813) boot: Disabling RNG early entropy source...
I (813) psram: This chip is ESP32-D0WD
I (818) spiram: Found 64MBit SPI RAM device
I (822) spiram: SPI RAM mode: flash 40m sram 40m
I (827) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (835) cpu_start: Pro cpu up.
I (838) cpu_start: Application information:
I (843) cpu_start: Project name:     wifi_bt_ble_coex
I (849) cpu_start: App version:      v2.2-201-g6fa02923
I (855) cpu_start: Compile time:     Oct  8 2021 19:42:12
I (861) cpu_start: ELF file SHA256:  d0e590c88daac1b0...
I (867) cpu_start: ESP-IDF:          v4.2.2
I (872) cpu_start: Starting app cpu, entry point is 0x40081d3c
0x40081d3c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1766) spiram: SPI SRAM memory test OK
I (1775) heap_init: Initializing. RAM available for dynamic allocation:
I (1775) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1777) heap_init: At 3FFB7468 len 00000B98 (2 KiB): DRAM
I (1783) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1790) heap_init: At 3FFC3A48 len 0001C5B8 (113 KiB): DRAM
I (1796) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1802) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1809) heap_init: At 4009D354 len 00002CAC (11 KiB): IRAM
I (1815) cpu_start: Pro cpu start user code
I (1820) spiram: Adding pool of 4020K of external SPI memory to heap allocator
I (1843) spi_flash: detected chip: gd
I (1843) spi_flash: flash io: dio
W (1843) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1854) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1864) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1914) COEX_EXAMPLE: [ 1 ] Create coex handle for a2dp-gatt-wifi
I (1914) COEX_EXAMPLE: [ 2 ] Initialize peripherals
I (1914) COEX_EXAMPLE: [ 3 ] create and start input key service
I (1914) COEX_EXAMPLE: [ 4 ] Start a2dp and blufi network
W (1934) PERIPH_TOUCH: _touch_init
I (1934) COEX_EXAMPLE: [4.1] Init Bluetooth
I (2454) COEX_EXAMPLE: [4.2] init gatts
I (2454) COEX_EXAMPLE: Blufi module init
E (2454) DISPATCHER: exe first list: 0x0
I (2484) COEX_EXAMPLE: [4.3] Create Bluetooth peripheral
I (2484) COEX_EXAMPLE: [4.4] Start peripherals
I (2484) COEX_EXAMPLE: [4.5] init hfp_stream
E (2534) gpio: gpio_install_isr_service(438): GPIO isr service already installed

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-9-g84df87e-037bef3-09be8fe                |
|                     Compile date: Jul 20 2021-13:51:36                     |
------------------------------------------------------------------------------
W (2744) BT_BTC: A2DP Enable with AVRC
I (2754) COEX_EXAMPLE: A2DP sink user cb
I (2754) COEX_EXAMPLE: User cb A2DP event: 4
I (2804) COEX_EXAMPLE: Func:setup_player, Line:298, MEM Total:4150532 Bytes, Inter:49696 Bytes, Dram:38296 Bytes

I (2804) COEX_EXAMPLE: esp_audio instance is:0x3f813480

I (2814) COEX_EXAMPLE: [ 5 ] Create audio pipeline for playback
I (2814) COEX_EXAMPLE: [5.1] Create i2s stream to read data from codec chip
W (2824) I2S: I2S driver already installed
I (2834) COEX_EXAMPLE: [5.2] Create hfp stream
I (2834) COEX_EXAMPLE: [5.3] Register i2s reader and hfp outgoing to audio pipeline
I (2854) COEX_EXAMPLE: [5.4] Link it together [codec_chip]-->i2s_stream_reader-->filter-->hfp_out_stream-->[Bluetooth]
I (2854) COEX_EXAMPLE: [5.5] Start audio_pipeline out
W (6104) WIFI_SERV: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
W (7904) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (7984) COEX_EXAMPLE: WIFI_CONNECTED
W (76644) BT_APPL: new conn_srvc id:27, app_id:1
W (77114) BT_APPL: bta_hf_client_send_at: No service, skipping 11 command
E (77344) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (77344) COEX_EXAMPLE: A2DP sink user cb
I (77404) COEX_EXAMPLE: A2DP sink user cb
I (77404) COEX_EXAMPLE: User cb A2DP event: 2
W (77574) BT_APPL: new conn_srvc id:19, app_id:0
I (77574) COEX_EXAMPLE: A2DP sink user cb
I (77574) COEX_EXAMPLE: A2DP connected
W (101574) BT_APPL: new conn_srvc id:19, app_id:1
I (101574) COEX_EXAMPLE: A2DP sink user cb
I (101574) COEX_EXAMPLE: User cb A2DP event: 1
I (112794) COEX_EXAMPLE: A2DP sink user cb
I (112794) COEX_EXAMPLE: User cb A2DP event: 1

```

## Troubleshooting

When your application has coexistence issues, please troubleshoot from the following aspects:

- Please check if you use the default `sdkconfig.defaults` configuration. It is strongly recommended to use the default sdkconfig configuration.
- Please check if you enable the dual mode of Bluetooth.
- Please check if you enable the PSRAM configuration, or if your module has the PSRAM chip on the board.



## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
