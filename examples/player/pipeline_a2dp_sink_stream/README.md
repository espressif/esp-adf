# A2DP SINK Stream

- [中文版本](./README_CN.md)
- Baisc Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This A2DP SINK example uses the Classic Bluetooth A2DP profile to distribute and receive audio streams and the AVRCP profile to notify and control media information. Applications such as Bluetooth speakers or Bluetooth headsets can use this example as a reference for basic functions.

The complete pipeline of A2DP SINK is as follows:

```c
aadp_source ---> aadp_sink_stream ---> i2s_stream_writer ---> codec_chip ---> speaker
```

As A2DP SINK role, the device in this example needs to be paired and connected with A2DP SOURCE (usually a mobile phone) in order to play the audio distributed by the SOURCE.


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

The default IDF branch of this example is ADF's built-in branch `$ADF_PATH/esp-idf`.


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

- The example starts running. The development board is waiting for connection to the mobile phone over Bluetooth.

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 17:03:07
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 2MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00124f80
I (90) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2c878 (182392) map
I (179) esp_image: segment 1: paddr=0x0003c8a0 vaddr=0x3ffbdb60 size=0x0344c ( 13388) load
I (185) esp_image: segment 2: paddr=0x0003fcf4 vaddr=0x40080000 size=0x00324 (   804) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (186) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xae738 (714552) map
0x400d0020: _stext at ??:?

I (467) esp_image: segment 4: paddr=0x000ee760 vaddr=0x40080324 size=0x1701c ( 94236) load
0x40080324: _KernelExceptionVector at ??:?

I (521) boot: Loaded app from partition at offset 0x10000
I (521) boot: Disabling RNG early entropy source...
I (522) cpu_start: Pro cpu up.
I (525) cpu_start: Application information:
I (530) cpu_start: Project name:     bt_sink_demo
I (535) cpu_start: App version:      v2.2-201-g6fa02923
I (541) cpu_start: Compile time:     Sep 30 2021 17:03:01
I (547) cpu_start: ELF file SHA256:  70fe532bd3fbc1b2...
I (553) cpu_start: ESP-IDF:          v4.2.2
I (558) cpu_start: Starting app cpu, entry point is 0x40081a18
0x40081a18: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (569) heap_init: Initializing. RAM available for dynamic allocation:
I (576) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (582) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (588) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (594) heap_init: At 3FFCF0E0 len 00010F20 (67 KiB): DRAM
I (600) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (606) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (613) heap_init: At 40097340 len 00008CC0 (35 KiB): IRAM
I (619) cpu_start: Pro cpu start user code
I (637) spi_flash: detected chip: gd
I (638) spi_flash: flash io: dio
W (638) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (648) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (695) BLUETOOTH_EXAMPLE: [ 1 ] Init Bluetooth
I (695) BTDM_INIT: BT controller compile version [ba56601]
I (695) system_api: Base MAC address is not set
I (695) system_api: read default base MAC address from EFUSE
I (705) phy_init: phy_version 4660,0162888,Dec 23 2020
I (1395) BLUETOOTH_EXAMPLE: [ 2 ] Start codec chip
I (1405) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1425) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1425) ES8388_DRIVER: init,out:02, in:00
I (1435) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (1435) BLUETOOTH_EXAMPLE: [ 3 ] Create audio pipeline for playback
I (1435) BLUETOOTH_EXAMPLE: [4] Create i2s stream to write data to codec chip
I (1445) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1455) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1485) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1485) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1495) BLUETOOTH_EXAMPLE: [4.1] Get Bluetooth stream
W (1495) BT_BTC: A2DP Enable with AVRC
I (1515) A2DP_STREAM: Unhandled A2DP event: 4
I (1515) BLUETOOTH_EXAMPLE: [4.2] Register all elements to audio pipeline
I (1515) BLUETOOTH_EXAMPLE: [4.3] Link it together [Bluetooth]-->bt_stream_reader-->i2s_stream_writer-->[codec_chip]
I (1535) AUDIO_PIPELINE: link el->rb, el:0x3ffd7c74, tag:bt, rb:0x3ffd80e0
I (1535) BLUETOOTH_EXAMPLE: [ 5 ] Initialize peripherals
E (1545) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1555) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1565) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1575) BLUETOOTH_EXAMPLE: [ 5.1 ] Create and start input key service
I (1575) BLUETOOTH_EXAMPLE: [5.2] Create Bluetooth peripheral
W (1595) PERIPH_TOUCH: _touch_init
I (1595) BLUETOOTH_EXAMPLE: [5.3] Start all peripherals
I (1595) BLUETOOTH_EXAMPLE: [ 6 ] Set up  event listener
I (1605) BLUETOOTH_EXAMPLE: [6.1] Listening event from all elements of pipeline
I (1615) BLUETOOTH_EXAMPLE: [ 7 ] Start audio_pipeline
I (1615) AUDIO_ELEMENT: [bt-0x3ffd7c74] Element task created
I (1635) AUDIO_ELEMENT: [i2s-0x3ffd5f10] Element task created
I (1635) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:133036 Bytes

I (1645) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (1645) I2S_STREAM: AUDIO_STREAM_WRITER
I (1655) AUDIO_PIPELINE: Pipeline started
I (1655) BLUETOOTH_EXAMPLE: [ 8 ] Listen for all pipeline events
```

- Start scanning for the Bluetooth device named `ESP_SINK_STREAM_DEMO` on your mobile phone and connect to it. The log is as follows:

```c
E (265275) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (265285) A2DP_STREAM: A2DP bd address:, [14:60:cb:82:72:55]
I (265305) A2DP_STREAM: AVRC conn_state evt: state 1, [14:60:cb:82:72:55]
I (265455) A2DP_STREAM: A2DP audio stream configuration, codec type 0
I (265455) A2DP_STREAM: Bluetooth configured, sample rate=44100
I (265465) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=44100, bits=16, ch=2
I (265495) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (265545) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (265545) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (265555) I2S_STREAM: AUDIO_STREAM_WRITER
I (265855) A2DP_STREAM: AVRC remote features 25b, CT features 2
W (265995) BT_APPL: new conn_srvc id:19, app_id:0
I (265995) A2DP_STREAM: A2DP bd address:, [14:60:cb:82:72:55]
I (265995) A2DP_STREAM: A2DP connection state =  CONNECTED
I (266535) A2DP_STREAM: AVRC remote features 25b, CT features 2
I (266605) A2DP_STREAM: AVRC register event notification: 13, param: 0x0
I (266615) A2DP_STREAM: AVRC set absolute volume: 40%
I (266615) A2DP_STREAM: Volume is set by remote controller 40%

```

- After a successful connection, open a player on your phone, click the play button, and then the development board will output the audio distributed by the phone’s Bluetooth. The log is as follows:

```c
I (294305) A2DP_STREAM: AVRC set absolute volume: 23%
I (294305) A2DP_STREAM: Volume is set by remote controller 23%

I (294335) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

W (294335) BT_APPL: new conn_srvc id:19, app_id:1
I (315545) A2DP_STREAM: AVRC set absolute volume: 23%
I (315545) A2DP_STREAM: Volume is set by remote controller 23%

I (322135) A2DP_STREAM: AVRC set absolute volume: 23%
I (322135) A2DP_STREAM: Volume is set by remote controller 23%

I (327765) A2DP_STREAM: AVRC set absolute volume: 23%
I (327765) A2DP_STREAM: Volume is set by remote controller 23%

```

- This example also supports AVRCP to control media playback operations, such as volume up, volume down, next song, previous song, etc. The log is as follows:

```c
I (327765) A2DP_STREAM: AVRC set absolute volume: 23%
I (327765) A2DP_STREAM: Volume is set by remote controller 23%

I (330095) A2DP_STREAM: AVRC set absolute volume: 29%
I (330095) A2DP_STREAM: Volume is set by remote controller 29%

I (331115) A2DP_STREAM: AVRC set absolute volume: 36%
I (331115) A2DP_STREAM: Volume is set by remote controller 36%

I (332935) A2DP_STREAM: AVRC set absolute volume: 29%
I (332935) A2DP_STREAM: Volume is set by remote controller 29%

I (333695) A2DP_STREAM: AVRC set absolute volume: 23%
I (333695) A2DP_STREAM: Volume is set by remote controller 23%

I (337975) A2DP_STREAM: AVRC set absolute volume: 29%
I (337975) A2DP_STREAM: Volume is set by remote controller 29%

I (345145) A2DP_STREAM: AVRC set absolute volume: 62%
I (345145) A2DP_STREAM: Volume is set by remote controller 62%

I (345155) A2DP_STREAM: AVRC set absolute volume: 29%
I (345155) A2DP_STREAM: Volume is set by remote controller 29%

```


### Example Log

A complete log is as follows:

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 17:03:07
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 2MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00124f80
I (90) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2c878 (182392) map
I (179) esp_image: segment 1: paddr=0x0003c8a0 vaddr=0x3ffbdb60 size=0x0344c ( 13388) load
I (185) esp_image: segment 2: paddr=0x0003fcf4 vaddr=0x40080000 size=0x00324 (   804) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (186) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xae738 (714552) map
0x400d0020: _stext at ??:?

I (467) esp_image: segment 4: paddr=0x000ee760 vaddr=0x40080324 size=0x1701c ( 94236) load
0x40080324: _KernelExceptionVector at ??:?

I (521) boot: Loaded app from partition at offset 0x10000
I (521) boot: Disabling RNG early entropy source...
I (522) cpu_start: Pro cpu up.
I (525) cpu_start: Application information:
I (530) cpu_start: Project name:     bt_sink_demo
I (535) cpu_start: App version:      v2.2-201-g6fa02923
I (541) cpu_start: Compile time:     Sep 30 2021 17:03:01
I (547) cpu_start: ELF file SHA256:  70fe532bd3fbc1b2...
I (553) cpu_start: ESP-IDF:          v4.2.2
I (558) cpu_start: Starting app cpu, entry point is 0x40081a18
0x40081a18: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (569) heap_init: Initializing. RAM available for dynamic allocation:
I (576) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (582) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (588) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (594) heap_init: At 3FFCF0E0 len 00010F20 (67 KiB): DRAM
I (600) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (606) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (613) heap_init: At 40097340 len 00008CC0 (35 KiB): IRAM
I (619) cpu_start: Pro cpu start user code
I (637) spi_flash: detected chip: gd
I (638) spi_flash: flash io: dio
W (638) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (648) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (695) BLUETOOTH_EXAMPLE: [ 1 ] Init Bluetooth
I (695) BTDM_INIT: BT controller compile version [ba56601]
I (695) system_api: Base MAC address is not set
I (695) system_api: read default base MAC address from EFUSE
I (705) phy_init: phy_version 4660,0162888,Dec 23 2020
I (1395) BLUETOOTH_EXAMPLE: [ 2 ] Start codec chip
I (1405) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1425) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1425) ES8388_DRIVER: init,out:02, in:00
I (1435) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (1435) BLUETOOTH_EXAMPLE: [ 3 ] Create audio pipeline for playback
I (1435) BLUETOOTH_EXAMPLE: [4] Create i2s stream to write data to codec chip
I (1445) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1455) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1485) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1485) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1495) BLUETOOTH_EXAMPLE: [4.1] Get Bluetooth stream
W (1495) BT_BTC: A2DP Enable with AVRC
I (1515) A2DP_STREAM: Unhandled A2DP event: 4
I (1515) BLUETOOTH_EXAMPLE: [4.2] Register all elements to audio pipeline
I (1515) BLUETOOTH_EXAMPLE: [4.3] Link it together [Bluetooth]-->bt_stream_reader-->i2s_stream_writer-->[codec_chip]
I (1535) AUDIO_PIPELINE: link el->rb, el:0x3ffd7c74, tag:bt, rb:0x3ffd80e0
I (1535) BLUETOOTH_EXAMPLE: [ 5 ] Initialize peripherals
E (1545) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1555) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1565) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1575) BLUETOOTH_EXAMPLE: [ 5.1 ] Create and start input key service
I (1575) BLUETOOTH_EXAMPLE: [5.2] Create Bluetooth peripheral
W (1595) PERIPH_TOUCH: _touch_init
I (1595) BLUETOOTH_EXAMPLE: [5.3] Start all peripherals
I (1595) BLUETOOTH_EXAMPLE: [ 6 ] Set up  event listener
I (1605) BLUETOOTH_EXAMPLE: [6.1] Listening event from all elements of pipeline
I (1615) BLUETOOTH_EXAMPLE: [ 7 ] Start audio_pipeline
I (1615) AUDIO_ELEMENT: [bt-0x3ffd7c74] Element task created
I (1635) AUDIO_ELEMENT: [i2s-0x3ffd5f10] Element task created
I (1635) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:133036 Bytes

I (1645) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (1645) I2S_STREAM: AUDIO_STREAM_WRITER
I (1655) AUDIO_PIPELINE: Pipeline started
I (1655) BLUETOOTH_EXAMPLE: [ 8 ] Listen for all pipeline events
E (265275) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (265285) A2DP_STREAM: A2DP bd address:, [14:60:cb:82:72:55]
I (265305) A2DP_STREAM: AVRC conn_state evt: state 1, [14:60:cb:82:72:55]
I (265455) A2DP_STREAM: A2DP audio stream configuration, codec type 0
I (265455) A2DP_STREAM: Bluetooth configured, sample rate=44100
I (265465) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=44100, bits=16, ch=2
I (265495) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (265545) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (265545) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:4
I (265555) I2S_STREAM: AUDIO_STREAM_WRITER
I (265855) A2DP_STREAM: AVRC remote features 25b, CT features 2
W (265995) BT_APPL: new conn_srvc id:19, app_id:0
I (265995) A2DP_STREAM: A2DP bd address:, [14:60:cb:82:72:55]
I (265995) A2DP_STREAM: A2DP connection state =  CONNECTED
I (266535) A2DP_STREAM: AVRC remote features 25b, CT features 2
I (266605) A2DP_STREAM: AVRC register event notification: 13, param: 0x0
I (266615) A2DP_STREAM: AVRC set absolute volume: 40%
I (266615) A2DP_STREAM: Volume is set by remote controller 40%

I (294305) A2DP_STREAM: AVRC set absolute volume: 23%
I (294305) A2DP_STREAM: Volume is set by remote controller 23%

I (294335) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

W (294335) BT_APPL: new conn_srvc id:19, app_id:1
I (315545) A2DP_STREAM: AVRC set absolute volume: 23%
I (315545) A2DP_STREAM: Volume is set by remote controller 23%

I (322135) A2DP_STREAM: AVRC set absolute volume: 23%
I (322135) A2DP_STREAM: Volume is set by remote controller 23%

I (327765) A2DP_STREAM: AVRC set absolute volume: 23%
I (327765) A2DP_STREAM: Volume is set by remote controller 23%

I (330095) A2DP_STREAM: AVRC set absolute volume: 29%
I (330095) A2DP_STREAM: Volume is set by remote controller 29%

I (331115) A2DP_STREAM: AVRC set absolute volume: 36%
I (331115) A2DP_STREAM: Volume is set by remote controller 36%

I (332935) A2DP_STREAM: AVRC set absolute volume: 29%
I (332935) A2DP_STREAM: Volume is set by remote controller 29%

I (333695) A2DP_STREAM: AVRC set absolute volume: 23%
I (333695) A2DP_STREAM: Volume is set by remote controller 23%

I (337975) A2DP_STREAM: AVRC set absolute volume: 29%
I (337975) A2DP_STREAM: Volume is set by remote controller 29%

I (345145) A2DP_STREAM: AVRC set absolute volume: 62%
I (345145) A2DP_STREAM: Volume is set by remote controller 62%

I (345155) A2DP_STREAM: AVRC set absolute volume: 29%
I (345155) A2DP_STREAM: Volume is set by remote controller 29%

```

## Troubleshooting

- ESP32 A2DP supports the mandatory audio codec SBC. The SBC data stream is transmitted to A2DP SINK, and then decoded into PCM data for output. The PCM data is usually 44.1 kHz sampling rate, dual-channel, and 16-bit sampling data. ESP32 A2DP SINK also supports other decoder configuration, but you need to modify the protocol stack settings.
- ESP32 A2DP SINK can only connect to one remote A2DP SOURCE device at most. In addition, A2DP SINK cannot be used with A2DP SOURCE at the same time, but it can be used with other configuration files, such as SPP and HFP.


## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
