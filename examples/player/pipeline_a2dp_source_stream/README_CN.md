# A2DP SOURCE 流例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程是 A2DP SOURCE 例程，使用经典蓝牙的 A2DP SOURCE 角色进行音频流分发，应用程序可以利用这个例子来实现便携式音频播放器或麦克风应用，将音频流传输到 A2DP SINK 接收设备。

A2DP SOURCE 的完整管道如下：

```c
sdcard ---> fatfs_stream ---> mp3_decoder ---> bt_stream ---> aadp_source
```

本例程作为 A2DP 的 SOURCE 角色，需要和 A2DP SINK（一般是蓝牙音箱）配对连接，用来播放下发的音频。


## 环境配置


### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程需要准备一张 microSD 卡，并自备一首 MP3 格式的音频文件，命名为 `test.mp3`，然后把 microSD 卡插入开发板备用。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程需要配置 A2DP SINK 角色的设备名，用来连接蓝牙 SINK 设备，在下面的 menuconfig 菜单中配置。

```
menuconfig > Example Configuration > BT remote device name
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

- 例程开始运行后，开发板会扫描周围的蓝牙设备，试图匹配 `menuconfig` 中设置的蓝牙设备名进行连接，打印如下：

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


### 日志输出

以下为本例程的完整日志。

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


## 故障排除

- 目前阶段，ESP32 A2DP 支持的音频编解码器是 SBC。SBC 音频流由 PCM 数据编码，通常格式为 44.1 kHz 采样率、双通道、16 位采样数据。也可以支持其他 SBC 配置，但需要对协议堆栈进行额外修改。
- 当前使用限制，ESP32 A2DP SOURCE 最多只能支持与一个远程 A2DP SINK 设备的连接。此外，A2DP SOURCE 不能与 A2DP SINK 同时使用，但可以与其他配置文件一起使用，例如 SPP 和 HFP。


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
