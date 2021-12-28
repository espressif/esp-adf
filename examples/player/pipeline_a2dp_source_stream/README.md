# A2DP SOURCE Stream

- [中文版本](./README_CN.md)
- Baisc Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how a Classic Bluetooth A2DP SOURCE distributes audio steams. It can be used as an reference to develop portable audio players or microphone applications that transmit audio streams to A2DP SINK.

The complete pipeline of A2DP SOURCE is as follows:

```
sdcard ---> fatfs_stream ---> mp3_decoder ---> bt_stream ---> aadp_source
```

As the A2DP SOURCE role, the device in this example needs to be paired and connected with A2DP SINK (usually a Bluetooth speaker) in order to play the distributed audio.


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

In this example, you need to prepare an microSD card with an audio file in MP3 format named `test.mp3`, and insert the card into the development board.

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

You need to configure the name of the A2DP SINK device in menuconfig in order to connect to it.

```
menuconfig > Example Configuration > BT remote device name
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

- After the example starts running, the development board will scan the surrounding Bluetooth devices for the device with the name set in menuconfig, and connect to it.

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 17:52:26
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 2MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (74) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 001e8480
I (89) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x38514 (230676) map
I (206) esp_image: segment 1: paddr=0x0004853c vaddr=0x3ffbdb60 size=0x03730 ( 14128) load
I (213) esp_image: segment 2: paddr=0x0004bc74 vaddr=0x40080000 size=0x043a4 ( 17316) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (221) esp_image: segment 3: paddr=0x00050020 vaddr=0x400d0020 size=0xcb478 (832632) map
0x400d0020: _stext at ??:?

I (569) esp_image: segment 4: paddr=0x0011b4a0 vaddr=0x400843a4 size=0x1a32c (107308) load
0x400843a4: psram_enable at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/spiram_psram.c:950

I (637) boot: Loaded app from partition at offset 0x10000
I (637) boot: Disabling RNG early entropy source...
I (638) psram: This chip is ESP32-D0WD
I (644) spiram: Found 64MBit SPI RAM device
I (647) spiram: SPI RAM mode: flash 40m sram 40m
I (652) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (660) cpu_start: Pro cpu up.
I (663) cpu_start: Application information:
I (668) cpu_start: Project name:     bt_source_demo
I (674) cpu_start: App version:      v2.2-201-g6fa02923
I (680) cpu_start: Compile time:     Sep 30 2021 17:55:05
I (686) cpu_start: ELF file SHA256:  db1edda26abcb241...
I (692) cpu_start: ESP-IDF:          v4.2.2
I (696) cpu_start: Starting app cpu, entry point is 0x4008206c
0x4008206c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (690) cpu_start: App cpu up.
I (1591) spiram: SPI SRAM memory test OK
I (1592) heap_init: Initializing. RAM available for dynamic allocation:
I (1592) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1598) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (1604) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1610) heap_init: At 3FFC33E0 len 0001CC20 (115 KiB): DRAM
I (1617) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1623) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1630) heap_init: At 4009E6D0 len 00001930 (6 KiB): IRAM
I (1636) cpu_start: Pro cpu start user code
I (1641) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1662) spi_flash: detected chip: gd
I (1663) spi_flash: flash io: dio
W (1663) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1673) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1684) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1714) BLUETOOTH_SOURCE_EXAMPLE: [ 1 ] Mount sdcard
I (2214) BLUETOOTH_SOURCE_EXAMPLE: [ 2 ] Start codec chip
E (2214) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (2234) BLUETOOTH_SOURCE_EXAMPLE: [3.0] Create audio pipeline for playback
I (2234) BLUETOOTH_SOURCE_EXAMPLE: [3.1] Create fatfs stream to read data from sdcard
I (2244) BLUETOOTH_SOURCE_EXAMPLE: [3.2] Create mp3 decoder to decode mp3 file
I (2254) BLUETOOTH_SOURCE_EXAMPLE: [3.3] Create Bluetooth stream
I (2734) BLUETOOTH_SOURCE_EXAMPLE: [3.4] Register all elements to audio pipeline
W (2734) BT_BTC: A2DP Enable with AVRC
I (2734) BLUETOOTH_SOURCE_EXAMPLE: [3.5] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->bt_stream-->[bt sink]
I (2754) BLUETOOTH_SOURCE_EXAMPLE: [3.6] Set up  uri (file as fatfs_stream, mp3 as mp3 decoder, and default output is i2s)
I (2754) BLUETOOTH_SOURCE_EXAMPLE: Discovery started.
I (2764) BLUETOOTH_SOURCE_EXAMPLE: [3.7] Create bt peripheral
I (2774) BLUETOOTH_SOURCE_EXAMPLE: [3.8] Start bt peripheral
I (2784) BLUETOOTH_SOURCE_EXAMPLE: [ 4 ] Set up  event listener
I (2784) BLUETOOTH_SOURCE_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (2794) BLUETOOTH_SOURCE_EXAMPLE: [4.2] Listening event from peripherals
I (2804) BLUETOOTH_SOURCE_EXAMPLE: [ 5 ] Start audio_pipeline
I (2814) BLUETOOTH_SOURCE_EXAMPLE: [ 6 ] Listen for all pipeline events
I (2844) BLUETOOTH_SOURCE_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (3974) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 24:0a:c4:e8:11:72
I (3974) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x240414
I (3974) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -72
I (3984) BLUETOOTH_SOURCE_EXAMPLE: --Name: ESP_SPEAKER
I (3994) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (4304) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 24:0a:c4:e8:11:72
I (4304) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x240414
I (4304) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -73
I (4314) BLUETOOTH_SOURCE_EXAMPLE: --Name: ESP_SPEAKER
I (4324) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (4424) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 24:0a:c4:e8:11:72
I (4424) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x240414
I (4434) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -70
I (4434) BLUETOOTH_SOURCE_EXAMPLE: --Name: ESP_SPEAKER
I (4444) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (5124) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 00:1a:7d:da:71:11
I (5124) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0xc0104
I (5134) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -58
I (5134) BLUETOOTH_SOURCE_EXAMPLE: --Name: Ubuntu 18.04.5 LTS
I (5144) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (5224) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 00:1a:7d:da:71:11
I (5224) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0xc0104
I (5234) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -68
I (5234) BLUETOOTH_SOURCE_EXAMPLE: --Name: Ubuntu 18.04.5 LTS
I (5244) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (6334) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 14:60:cb:82:72:55
I (6334) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x7a020c
I (6334) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -63
I (6334) BLUETOOTH_SOURCE_EXAMPLE: --Name: Heap Corrupt
I (6344) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt

```

- 一旦蓝牙扫描配备到设备名，然后会进行连接，打印如下：

```c
I (6334) BLUETOOTH_SOURCE_EXAMPLE: --Name: Heap Corrupt
I (6344) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (6354) BLUETOOTH_SOURCE_EXAMPLE: Found a target device, address 14:60:cb:82:72:55, name Heap Corrupt
I (6364) BLUETOOTH_SOURCE_EXAMPLE: Cancel device discovery ...
I (6374) BLUETOOTH_SOURCE_EXAMPLE: Device discovery stopped.
I (6374) BLUETOOTH_SOURCE_EXAMPLE: a2dp connecting to peer: Heap Corrupt
E (6384) BT_APPL: reset flags
W (7514) BT_BTC: BTA_AV_OPEN_EVT::FAILED status: 2

W (7514) BLUETOOTH_SOURCE_EXAMPLE: [ * ] Bluetooth disconnected or suspended
I (7514) BLUETOOTH_SOURCE_EXAMPLE: [ 7 ] Stop audio_pipeline
I (7514) BLUETOOTH_SOURCE_EXAMPLE: Discovery started.
W (7524) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (7534) MP3_DECODER: output aborted -3
W (7524) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (7544) AUDIO_PIPELINE: There are no listener registered
W (7544) AUDIO_ELEMENT: [bt] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7554) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7564) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7594) BT_L2CAP: L2CAP - PSM: 0x0019 not found for deregistration
W (7594) BT_L2CAP: L2CAP - PSM: 0x0017 not found for deregistration
I (7594) BLUETOOTH_SOURCE_EXAMPLE: Device discovery failed, continue to discover...
I (7604) BLUETOOTH_SOURCE_EXAMPLE: Device discovery failed, continue to discover...
I (7614) BLUETOOTH_SOURCE_EXAMPLE: Discovery started.
I (7734) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 24:0a:c4:e8:11:72
I (7734) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x240414
I (7734) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -78
I (7734) BLUETOOTH_SOURCE_EXAMPLE: --Name: ESP_SPEAKER
I (7744) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (8004) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 00:1a:7d:da:71:11
I (8004) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0xc0104
I (8004) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -59
I (8004) BLUETOOTH_SOURCE_EXAMPLE: --Name: Ubuntu 18.04.5 LTS
I (8014) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (8024) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 14:60:cb:82:72:55
I (8024) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x7a020c
I (8034) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -55
I (8044) BLUETOOTH_SOURCE_EXAMPLE: --Name: Heap Corrupt
I (8044) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (8054) BLUETOOTH_SOURCE_EXAMPLE: Found a target device, address 14:60:cb:82:72:55, name Heap Corrupt
I (8064) BLUETOOTH_SOURCE_EXAMPLE: Cancel device discovery ...
I (8074) BLUETOOTH_SOURCE_EXAMPLE: Device discovery stopped.
I (8074) BLUETOOTH_SOURCE_EXAMPLE: a2dp connecting to peer: Heap Corrupt

```


### Example Log

A complete log is as follows:

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 17:52:26
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 2MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (74) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 001e8480
I (89) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x38514 (230676) map
I (206) esp_image: segment 1: paddr=0x0004853c vaddr=0x3ffbdb60 size=0x03730 ( 14128) load
I (213) esp_image: segment 2: paddr=0x0004bc74 vaddr=0x40080000 size=0x043a4 ( 17316) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (221) esp_image: segment 3: paddr=0x00050020 vaddr=0x400d0020 size=0xcb478 (832632) map
0x400d0020: _stext at ??:?

I (569) esp_image: segment 4: paddr=0x0011b4a0 vaddr=0x400843a4 size=0x1a32c (107308) load
0x400843a4: psram_enable at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/spiram_psram.c:950

I (637) boot: Loaded app from partition at offset 0x10000
I (637) boot: Disabling RNG early entropy source...
I (638) psram: This chip is ESP32-D0WD
I (644) spiram: Found 64MBit SPI RAM device
I (647) spiram: SPI RAM mode: flash 40m sram 40m
I (652) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (660) cpu_start: Pro cpu up.
I (663) cpu_start: Application information:
I (668) cpu_start: Project name:     bt_source_demo
I (674) cpu_start: App version:      v2.2-201-g6fa02923
I (680) cpu_start: Compile time:     Sep 30 2021 17:55:05
I (686) cpu_start: ELF file SHA256:  db1edda26abcb241...
I (692) cpu_start: ESP-IDF:          v4.2.2
I (696) cpu_start: Starting app cpu, entry point is 0x4008206c
0x4008206c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (690) cpu_start: App cpu up.
I (1591) spiram: SPI SRAM memory test OK
I (1592) heap_init: Initializing. RAM available for dynamic allocation:
I (1592) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1598) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (1604) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1610) heap_init: At 3FFC33E0 len 0001CC20 (115 KiB): DRAM
I (1617) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1623) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1630) heap_init: At 4009E6D0 len 00001930 (6 KiB): IRAM
I (1636) cpu_start: Pro cpu start user code
I (1641) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1662) spi_flash: detected chip: gd
I (1663) spi_flash: flash io: dio
W (1663) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1673) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1684) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1714) BLUETOOTH_SOURCE_EXAMPLE: [ 1 ] Mount sdcard
I (2214) BLUETOOTH_SOURCE_EXAMPLE: [ 2 ] Start codec chip
E (2214) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (2234) BLUETOOTH_SOURCE_EXAMPLE: [3.0] Create audio pipeline for playback
I (2234) BLUETOOTH_SOURCE_EXAMPLE: [3.1] Create fatfs stream to read data from sdcard
I (2244) BLUETOOTH_SOURCE_EXAMPLE: [3.2] Create mp3 decoder to decode mp3 file
I (2254) BLUETOOTH_SOURCE_EXAMPLE: [3.3] Create Bluetooth stream
I (2734) BLUETOOTH_SOURCE_EXAMPLE: [3.4] Register all elements to audio pipeline
W (2734) BT_BTC: A2DP Enable with AVRC
I (2734) BLUETOOTH_SOURCE_EXAMPLE: [3.5] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->bt_stream-->[bt sink]
I (2754) BLUETOOTH_SOURCE_EXAMPLE: [3.6] Set up  uri (file as fatfs_stream, mp3 as mp3 decoder, and default output is i2s)
I (2754) BLUETOOTH_SOURCE_EXAMPLE: Discovery started.
I (2764) BLUETOOTH_SOURCE_EXAMPLE: [3.7] Create bt peripheral
I (2774) BLUETOOTH_SOURCE_EXAMPLE: [3.8] Start bt peripheral
I (2784) BLUETOOTH_SOURCE_EXAMPLE: [ 4 ] Set up  event listener
I (2784) BLUETOOTH_SOURCE_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (2794) BLUETOOTH_SOURCE_EXAMPLE: [4.2] Listening event from peripherals
I (2804) BLUETOOTH_SOURCE_EXAMPLE: [ 5 ] Start audio_pipeline
I (2814) BLUETOOTH_SOURCE_EXAMPLE: [ 6 ] Listen for all pipeline events
I (2844) BLUETOOTH_SOURCE_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (3974) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 24:0a:c4:e8:11:72
I (3974) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x240414
I (3974) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -72
I (3984) BLUETOOTH_SOURCE_EXAMPLE: --Name: ESP_SPEAKER
I (3994) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (4304) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 24:0a:c4:e8:11:72
I (4304) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x240414
I (4304) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -73
I (4314) BLUETOOTH_SOURCE_EXAMPLE: --Name: ESP_SPEAKER
I (4324) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (4424) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 24:0a:c4:e8:11:72
I (4424) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x240414
I (4434) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -70
I (4434) BLUETOOTH_SOURCE_EXAMPLE: --Name: ESP_SPEAKER
I (4444) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (5124) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 00:1a:7d:da:71:11
I (5124) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0xc0104
I (5134) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -58
I (5134) BLUETOOTH_SOURCE_EXAMPLE: --Name: Ubuntu 18.04.5 LTS
I (5144) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (5224) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 00:1a:7d:da:71:11
I (5224) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0xc0104
I (5234) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -68
I (5234) BLUETOOTH_SOURCE_EXAMPLE: --Name: Ubuntu 18.04.5 LTS
I (5244) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (6334) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 14:60:cb:82:72:55
I (6334) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x7a020c
I (6334) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -63
I (6334) BLUETOOTH_SOURCE_EXAMPLE: --Name: Heap Corrupt
I (6344) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (6354) BLUETOOTH_SOURCE_EXAMPLE: Found a target device, address 14:60:cb:82:72:55, name Heap Corrupt
I (6364) BLUETOOTH_SOURCE_EXAMPLE: Cancel device discovery ...
I (6374) BLUETOOTH_SOURCE_EXAMPLE: Device discovery stopped.
I (6374) BLUETOOTH_SOURCE_EXAMPLE: a2dp connecting to peer: Heap Corrupt
E (6384) BT_APPL: reset flags
W (7514) BT_BTC: BTA_AV_OPEN_EVT::FAILED status: 2

W (7514) BLUETOOTH_SOURCE_EXAMPLE: [ * ] Bluetooth disconnected or suspended
I (7514) BLUETOOTH_SOURCE_EXAMPLE: [ 7 ] Stop audio_pipeline
I (7514) BLUETOOTH_SOURCE_EXAMPLE: Discovery started.
W (7524) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (7534) MP3_DECODER: output aborted -3
W (7524) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (7544) AUDIO_PIPELINE: There are no listener registered
W (7544) AUDIO_ELEMENT: [bt] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7554) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7564) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7594) BT_L2CAP: L2CAP - PSM: 0x0019 not found for deregistration
W (7594) BT_L2CAP: L2CAP - PSM: 0x0017 not found for deregistration
I (7594) BLUETOOTH_SOURCE_EXAMPLE: Device discovery failed, continue to discover...
I (7604) BLUETOOTH_SOURCE_EXAMPLE: Device discovery failed, continue to discover...
I (7614) BLUETOOTH_SOURCE_EXAMPLE: Discovery started.
I (7734) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 24:0a:c4:e8:11:72
I (7734) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x240414
I (7734) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -78
I (7734) BLUETOOTH_SOURCE_EXAMPLE: --Name: ESP_SPEAKER
I (7744) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (8004) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 00:1a:7d:da:71:11
I (8004) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0xc0104
I (8004) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -59
I (8004) BLUETOOTH_SOURCE_EXAMPLE: --Name: Ubuntu 18.04.5 LTS
I (8014) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (8024) BLUETOOTH_SOURCE_EXAMPLE: Scanned device: 14:60:cb:82:72:55
I (8024) BLUETOOTH_SOURCE_EXAMPLE: --Class of Device: 0x7a020c
I (8034) BLUETOOTH_SOURCE_EXAMPLE: --RSSI: -55
I (8044) BLUETOOTH_SOURCE_EXAMPLE: --Name: Heap Corrupt
I (8044) BLUETOOTH_SOURCE_EXAMPLE: need device name Heap Corrupt
I (8054) BLUETOOTH_SOURCE_EXAMPLE: Found a target device, address 14:60:cb:82:72:55, name Heap Corrupt
I (8064) BLUETOOTH_SOURCE_EXAMPLE: Cancel device discovery ...
I (8074) BLUETOOTH_SOURCE_EXAMPLE: Device discovery stopped.
I (8074) BLUETOOTH_SOURCE_EXAMPLE: a2dp connecting to peer: Heap Corrupt

```


## Troubleshooting

- Currently, ESP32 A2DP supports the audio codec SBC. The SBC audio stream is encoded by PCM data, which is usually 44.1 kHz sampling rate, dual-channel, and 16-bit sampling data. Other SBC configurations can also be supported, but additional modifications to the protocol stack are required.
- Currently, ESP32 A2DP SOURCE can only connect to one remote A2DP SINK device at most. In addition, A2DP SOURCE cannot be used with A2DP SINK at the same time, but it can be used with other configuration files, such as SPP and HFP.


## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
