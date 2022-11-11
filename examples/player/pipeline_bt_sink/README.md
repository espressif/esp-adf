# Classic Bluetooth Sink Pipeline

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how to use the Classic Bluetooth sink of the ESP-ADF to transfer a song played by a cell phone (source) to the development board via Bluetooth.


The pipeline for the development board to receive voice data and decode it for playback is as follows:

```
[Bluetooth] ---> bt_stream_reader ---> i2s_stream_writer ---> [codec_chip]
```


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch
This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.


### Configuration

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace PORT with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

### Example Functionality

- After the example starts running, it waits for you to connect the phone to the development board actively. The broadcast name of the development board is "ESP_ADF_SPEAKER". The log is as follows:

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
I (27) boot: compile time 16:08:26
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
I (83) boot:  2 factory          factory app      00 00 00010000 00124f80
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2c894 (182420) map
I (181) esp_image: segment 1: paddr=0x0003c8bc vaddr=0x3ffbdb60 size=0x0345c ( 13404) load
I (186) esp_image: segment 2: paddr=0x0003fd20 vaddr=0x40080000 size=0x002f8 (   760) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (188) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xae67c (714364) map
0x400d0020: _stext at ??:?

I (468) esp_image: segment 4: paddr=0x000ee6a4 vaddr=0x400802f8 size=0x170b0 ( 94384) load
0x400802f8: _NMIExceptionVector at ??:?

I (522) boot: Loaded app from partition at offset 0x10000
I (522) boot: Disabling RNG early entropy source...
I (523) cpu_start: Pro cpu up.
I (526) cpu_start: Application information:
I (531) cpu_start: Project name:     play_bt_music
I (537) cpu_start: App version:      v2.2-221-gd27be99e
I (543) cpu_start: Compile time:     Nov 10 2021 16:08:19
I (549) cpu_start: ELF file SHA256:  bbd4b30b5e82b1c4...
I (555) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (561) cpu_start: Starting app cpu, entry point is 0x40081a1c
0x40081a1c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (571) heap_init: Initializing. RAM available for dynamic allocation:
I (578) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (584) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (590) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (596) heap_init: At 3FFCF0B8 len 00010F48 (67 KiB): DRAM
I (602) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (609) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (615) heap_init: At 400973A8 len 00008C58 (35 KiB): IRAM
I (621) cpu_start: Pro cpu start user code
I (640) spi_flash: detected chip: gd
I (640) spi_flash: flash io: dio
W (640) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (650) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (701) BLUETOOTH_EXAMPLE: [ 1 ] Create Bluetooth service
I (701) BTDM_INIT: BT controller compile version [ba56601]
I (701) system_api: Base MAC address is not set
I (711) system_api: read default base MAC address from EFUSE
I (721) phy_init: phy_version 4660,0162888,Dec 23 2020
W (1421) BT_BTC: A2DP Enable with AVRC
I (1441) BLUETOOTH_EXAMPLE: [ 2 ] Start codec chip
I (1451) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1471) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1471) ES8388_DRIVER: init,out:02, in:00
I (1481) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (1481) BLUETOOTH_EXAMPLE: [ 3 ] Create audio pipeline for playback
I (1481) BLUETOOTH_EXAMPLE: [3.1] Create i2s stream to write data to codec chip
I (1491) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1501) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1531) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1531) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1531) BLUETOOTH_EXAMPLE: [3.2] Get Bluetooth stream
I (1541) BLUETOOTH_EXAMPLE: [3.2] Register all elements to audio pipeline
I (1551) BLUETOOTH_EXAMPLE: [3.3] Link it together [Bluetooth]-->bt_stream_reader-->i2s_stream_writer-->[codec_chip]
I (1561) AUDIO_PIPELINE: link el->rb, el:0x3ffd9d14, tag:bt, rb:0x3ffda070
I (1571) BLUETOOTH_EXAMPLE: [ 4 ] Initialize peripherals
E (1571) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1581) BLUETOOTH_EXAMPLE: [4.1] Initialize Touch peripheral
I (1591) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1601) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1611) BLUETOOTH_EXAMPLE: [4.2] Create Bluetooth peripheral
I (1611) BLUETOOTH_EXAMPLE: [4.2] Start all peripherals
W (1631) PERIPH_TOUCH: _touch_init
I (1631) BLUETOOTH_EXAMPLE: [ 5 ] Set up  event listener
I (1631) BLUETOOTH_EXAMPLE: [5.1] Listening event from all elements of pipeline
I (1641) BLUETOOTH_EXAMPLE: [5.2] Listening event from peripherals
I (1651) BLUETOOTH_EXAMPLE: [ 6 ] Start audio_pipeline
I (1651) AUDIO_ELEMENT: [bt-0x3ffd9d14] Element task created
I (1671) AUDIO_ELEMENT: [i2s-0x3ffd7c20] Element task created
I (1671) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:132040 Bytes

I (1681) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (1681) I2S_STREAM: AUDIO_STREAM_WRITER
I (1691) AUDIO_PIPELINE: Pipeline started
I (1691) BLUETOOTH_EXAMPLE: [ 7 ] Listen for all pipeline events

```

- When the phone is connected to the development board and starts playing music, the log is as follows:

```c
E (226361) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (226411) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=44100, bits=16, ch=2
I (226451) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (226491) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (226491) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (226501) I2S_STREAM: AUDIO_STREAM_WRITER
W (226821) BT_APPL: new conn_srvc id:19, app_id:0
I (259861) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

W (259861) BT_APPL: new conn_srvc id:19, app_id:1

```

- Also, you can use the keys on the development board to start playback, pause playback, and perform other operations. The log is as follows:

```c
I (1225481) BLUETOOTH_EXAMPLE: [ * ] [Play] touch tap event
I (1225801) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

I (1234631) BLUETOOTH_EXAMPLE: [ * ] [Set] touch tap event
I (1238081) BLUETOOTH_EXAMPLE: [ * ] [Vol-] touch tap event
I (1241831) BLUETOOTH_EXAMPLE: [ * ] [Vol-] touch tap event
I (1243181) BLUETOOTH_EXAMPLE: [ * ] [Vol+] touch tap event
I (1243781) BLUETOOTH_EXAMPLE: [ * ] [Vol+] touch tap event
I (1254881) BLUETOOTH_EXAMPLE: [ * ] [Vol-] touch tap event

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
I (27) boot: compile time 16:08:26
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
I (83) boot:  2 factory          factory app      00 00 00010000 00124f80
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2c894 (182420) map
I (181) esp_image: segment 1: paddr=0x0003c8bc vaddr=0x3ffbdb60 size=0x0345c ( 13404) load
I (186) esp_image: segment 2: paddr=0x0003fd20 vaddr=0x40080000 size=0x002f8 (   760) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (188) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xae67c (714364) map
0x400d0020: _stext at ??:?

I (468) esp_image: segment 4: paddr=0x000ee6a4 vaddr=0x400802f8 size=0x170b0 ( 94384) load
0x400802f8: _NMIExceptionVector at ??:?

I (522) boot: Loaded app from partition at offset 0x10000
I (522) boot: Disabling RNG early entropy source...
I (523) cpu_start: Pro cpu up.
I (526) cpu_start: Application information:
I (531) cpu_start: Project name:     play_bt_music
I (537) cpu_start: App version:      v2.2-221-gd27be99e
I (543) cpu_start: Compile time:     Nov 10 2021 16:08:19
I (549) cpu_start: ELF file SHA256:  bbd4b30b5e82b1c4...
I (555) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (561) cpu_start: Starting app cpu, entry point is 0x40081a1c
0x40081a1c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (571) heap_init: Initializing. RAM available for dynamic allocation:
I (578) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (584) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (590) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (596) heap_init: At 3FFCF0B8 len 00010F48 (67 KiB): DRAM
I (602) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (609) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (615) heap_init: At 400973A8 len 00008C58 (35 KiB): IRAM
I (621) cpu_start: Pro cpu start user code
I (640) spi_flash: detected chip: gd
I (640) spi_flash: flash io: dio
W (640) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (650) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (701) BLUETOOTH_EXAMPLE: [ 1 ] Create Bluetooth service
I (701) BTDM_INIT: BT controller compile version [ba56601]
I (701) system_api: Base MAC address is not set
I (711) system_api: read default base MAC address from EFUSE
I (721) phy_init: phy_version 4660,0162888,Dec 23 2020
W (1421) BT_BTC: A2DP Enable with AVRC
I (1441) BLUETOOTH_EXAMPLE: [ 2 ] Start codec chip
I (1451) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1471) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1471) ES8388_DRIVER: init,out:02, in:00
I (1481) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (1481) BLUETOOTH_EXAMPLE: [ 3 ] Create audio pipeline for playback
I (1481) BLUETOOTH_EXAMPLE: [3.1] Create i2s stream to write data to codec chip
I (1491) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1501) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1531) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1531) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1531) BLUETOOTH_EXAMPLE: [3.2] Get Bluetooth stream
I (1541) BLUETOOTH_EXAMPLE: [3.2] Register all elements to audio pipeline
I (1551) BLUETOOTH_EXAMPLE: [3.3] Link it together [Bluetooth]-->bt_stream_reader-->i2s_stream_writer-->[codec_chip]
I (1561) AUDIO_PIPELINE: link el->rb, el:0x3ffd9d14, tag:bt, rb:0x3ffda070
I (1571) BLUETOOTH_EXAMPLE: [ 4 ] Initialize peripherals
E (1571) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1581) BLUETOOTH_EXAMPLE: [4.1] Initialize Touch peripheral
I (1591) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1601) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1611) BLUETOOTH_EXAMPLE: [4.2] Create Bluetooth peripheral
I (1611) BLUETOOTH_EXAMPLE: [4.2] Start all peripherals
W (1631) PERIPH_TOUCH: _touch_init
I (1631) BLUETOOTH_EXAMPLE: [ 5 ] Set up  event listener
I (1631) BLUETOOTH_EXAMPLE: [5.1] Listening event from all elements of pipeline
I (1641) BLUETOOTH_EXAMPLE: [5.2] Listening event from peripherals
I (1651) BLUETOOTH_EXAMPLE: [ 6 ] Start audio_pipeline
I (1651) AUDIO_ELEMENT: [bt-0x3ffd9d14] Element task created
I (1671) AUDIO_ELEMENT: [i2s-0x3ffd7c20] Element task created
I (1671) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:132040 Bytes

I (1681) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (1681) I2S_STREAM: AUDIO_STREAM_WRITER
I (1691) AUDIO_PIPELINE: Pipeline started
I (1691) BLUETOOTH_EXAMPLE: [ 7 ] Listen for all pipeline events
E (226361) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (226411) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=44100, bits=16, ch=2
I (226451) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (226491) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (226491) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (226501) I2S_STREAM: AUDIO_STREAM_WRITER
W (226821) BT_APPL: new conn_srvc id:19, app_id:0
I (259861) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

W (259861) BT_APPL: new conn_srvc id:19, app_id:1
I (1225481) BLUETOOTH_EXAMPLE: [ * ] [Play] touch tap event
I (1225801) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

I (1234631) BLUETOOTH_EXAMPLE: [ * ] [Set] touch tap event
I (1238081) BLUETOOTH_EXAMPLE: [ * ] [Vol-] touch tap event
I (1241831) BLUETOOTH_EXAMPLE: [ * ] [Vol-] touch tap event
I (1243181) BLUETOOTH_EXAMPLE: [ * ] [Vol+] touch tap event
I (1243781) BLUETOOTH_EXAMPLE: [ * ] [Vol+] touch tap event
I (1254881) BLUETOOTH_EXAMPLE: [ * ] [Vol-] touch tap event

```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
