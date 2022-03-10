# 经典蓝牙发送端 (Source) 管道例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了 ADF 的经典蓝牙发送端 (source) 的使用方法，此例开发板作为经典蓝牙发送端 (source) 角色，通过连接 Wi-Fi 获取 HTTP 服务器上的 MP3 音频文件，音频解码后通过经典蓝牙的 A2DP 协议发送给另一块开发板蓝牙接收端 (sink)。

开发板获取 HTTP 服务器上的 MP3 文件并解码发送的管道如下：

```
[http_server] ---> http_stream ---> mp3_decoder ---> bt_stream_writer ---> [Bluetooth]
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

本例需要连接经典蓝牙的 sink 角色，通过运行 `menuconfig` 来配置远程 sink 的设备名称，默认名为 `ESP-ADF-SPEAKER`。

```
 menuconfig > Example Configuration > (ESP-ADF-SPEAKER) BT remote device name
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

- 例程开始运行后，首先连接 Wi-Fi，连接成功后，开发板开始搜索名称为 "ESP_ADF_SPEAKER" 的 sink 设备，打印如下：

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13664
ho 0 tail 12 room 4
load:0x40080400,len:4632
0x40080400: _init at ??:?

entry 0x400806f4
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 15:08:11
I (29) boot: chip revision: 3
I (33) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) boot.esp32: SPI Speed      : 40MHz
I (45) boot.esp32: SPI Mode       : DIO
I (49) boot.esp32: SPI Flash Size : 2MB
I (54) boot: Enabling RNG early entropy source...
I (59) boot: Partition Table:
I (63) boot: ## Label            Usage          Type ST Offset   Length
I (70) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (78) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (85) boot:  2 factory          factory app      00 00 00010000 001e8480
I (93) boot: End of partition table
I (97) boot_comm: chip revision: 3, min. application chip revision: 0
I (104) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x48854 (297044) map
I (237) esp_image: segment 1: paddr=0x0005887c vaddr=0x3ffbdb60 size=0x04124 ( 16676) load
I (245) esp_image: segment 2: paddr=0x0005c9a8 vaddr=0x40080000 size=0x03670 ( 13936) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (252) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0x13bd30 (1293616) map
0x400d0020: _stext at ??:?

I (792) esp_image: segment 4: paddr=0x0019bd58 vaddr=0x40083670 size=0x17858 ( 96344) load
0x40083670: psram_cache_init at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/spiram_psram.c:1051

I (854) boot: Loaded app from partition at offset 0x10000
I (854) boot: Disabling RNG early entropy source...
I (854) psram: This chip is ESP32-D0WD
I (860) spiram: Found 64MBit SPI RAM device
I (863) spiram: SPI RAM mode: flash 40m sram 40m
I (869) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (876) cpu_start: Pro cpu up.
I (879) cpu_start: Application information:
I (884) cpu_start: Project name:     bt_source_demo
I (890) cpu_start: App version:      v2.2-222-gd3c18cb0-dirty
I (896) cpu_start: Compile time:     Nov 11 2021 15:08:00
I (902) cpu_start: ELF file SHA256:  9cb2e8ab88bcb283...
I (908) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (914) cpu_start: Starting app cpu, entry point is 0x40081ddc
0x40081ddc: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1809) spiram: SPI SRAM memory test OK
I (1809) heap_init: Initializing. RAM available for dynamic allocation:
I (1810) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1816) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (1822) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1828) heap_init: At 3FFC7710 len 000188F0 (98 KiB): DRAM
I (1834) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1841) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1847) heap_init: At 4009AEC8 len 00005138 (20 KiB): IRAM
I (1854) cpu_start: Pro cpu start user code
I (1858) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1880) spi_flash: detected chip: gd
I (1881) spi_flash: flash io: dio
W (1881) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1891) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1902) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1932) BLUETOOTH_SOURCE_EXAMPLE: [ 1 ] Create Bluetooth service
I (1942) BTDM_INIT: BT controller compile version [ba56601]
I (1942) system_api: Base MAC address is not set
I (1942) system_api: read default base MAC address from EFUSE
I (1952) phy_init: phy_version 4660,0162888,Dec 23 2020
I (2432) BLUETOOTH_SERVICE: event: 10
I (2442) BLUETOOTH_SERVICE: Starting device discovery...
I (2442) BLUETOOTH_SOURCE_EXAMPLE: [1.1] Get Bluetooth stream
W (2442) BT_BTC: A2DP Enable with AVRC
I (2442) BLUETOOTH_SOURCE_EXAMPLE: [ 2 ] Create http stream to read data
I (2452) BLUETOOTH_SERVICE: bt_a2d_source_cb state 1, evt 0x4
I (2452) BLUETOOTH_SOURCE_EXAMPLE: [ 3 ] Create mp3 decoder to decode mp3 file
I (2462) BLUETOOTH_SERVICE: event: 10
I (2472) MP3_DECODER: MP3 init
I (2472) BLUETOOTH_SERVICE: event: 10
I (2482) BLUETOOTH_SOURCE_EXAMPLE: [ 4 ] Create audio pipeline for BT Source
I (2482) BLUETOOTH_SERVICE: event: 10
I (2492) BLUETOOTH_SOURCE_EXAMPLE: [4.1] Register all elements to audio pipeline
I (2492) BLUETOOTH_SERVICE: Discovery started.
I (2502) BLUETOOTH_SOURCE_EXAMPLE: [4.2] Link it together http_stream-->mp3_decoder-->bt_stream_writer
I (2522) AUDIO_PIPELINE: link el->rb, el:0x3f809e54, tag:http, rb:0x3f809c00
I (2522) AUDIO_PIPELINE: link el->rb, el:0x3f80a72c, tag:mp3, rb:0x3f80a81c
I (2532) BLUETOOTH_SOURCE_EXAMPLE: [4.3] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)
I (2542) BLUETOOTH_SOURCE_EXAMPLE: [ 5 ] Start and wait for Wi-Fi network
I (2562) wifi:wifi driver task: 3ffde5fc, prio:23, stack:6656, core=0
I (2562) wifi:wifi firmware version: bb6888c
I (2562) wifi:wifi certification version: v7.0
I (2562) wifi:config NVS flash: enabled
I (2572) wifi:config nano formating: disabled
I (2572) wifi:Init data frame dynamic rx buffer num: 32
I (2582) wifi:Init management frame dynamic rx buffer num: 32
I (2582) wifi:Init management short buffer num: 32
I (2592) wifi:Init static tx buffer num: 16
I (2592) wifi:Init tx cache buffer num: 32
I (2592) wifi:Init static rx buffer size: 1600
I (2602) wifi:Init static rx buffer num: 10
I (2602) wifi:Init dynamic rx buffer num: 32
I (2612) wifi_init: rx ba win: 6
I (2612) wifi_init: tcpip mbox: 32
I (2612) wifi_init: udp mbox: 6
I (2622) wifi_init: tcp mbox: 6
I (2622) wifi_init: tcp tx win: 5744
I (2632) wifi_init: tcp rx win: 5744
I (2632) wifi_init: tcp mss: 1440
I (2642) phy_init: phy_version 4660,0162888,Dec 23 2020
I (2652) wifi:mode : sta (94:b9:7e:65:c2:44)
I (3882) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (4452) wifi:state: init -> auth (b0)
I (4472) wifi:state: auth -> assoc (0)
I (4472) wifi:state: assoc -> run (10)
I (4492) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (4492) wifi:security: WPA2-PSK, phy: bgn, rssi: -42
I (4492) wifi:pm start, type: 1

W (4492) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4502) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4692) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (4692) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (4692) BLUETOOTH_SERVICE: --RSSI: -55
I (4702) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (4892) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (4892) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (4892) BLUETOOTH_SERVICE: --RSSI: -55
I (4902) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (6432) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (6432) PERIPH_WIFI: Got ip:192.168.5.187
I (6432) BLUETOOTH_SOURCE_EXAMPLE: [5.1] Create Bluetooth peripheral
I (6442) BLUETOOTH_SOURCE_EXAMPLE: [5.2] Start Bluetooth peripheral
I (6452) BLUETOOTH_SOURCE_EXAMPLE: [ 6 ] Set up  event listener
I (6452) BLUETOOTH_SOURCE_EXAMPLE: [6.1] Listening event from all elements of pipeline
I (6462) BLUETOOTH_SOURCE_EXAMPLE: [6.2] Listening event from peripherals
I (6472) BLUETOOTH_SOURCE_EXAMPLE: [ 7 ] Start audio_pipeline
I (6482) AUDIO_ELEMENT: [http-0x3f809e54] Element task created
I (6482) AUDIO_ELEMENT: [mp3-0x3f80a72c] Element task created
I (6492) AUDIO_ELEMENT: [bt-0x3f809adc] Element task created
I (6492) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4176264 Bytes, Inter:72292 Bytes, Dram:51536 Bytes

I (6512) AUDIO_ELEMENT: [http] AEL_MSG_CMD_RESUME,state:1
I (6522) AUDIO_ELEMENT: [mp3] AEL_MSG_CMD_RESUME,state:1
I (6522) MP3_DECODER: MP3 opened
I (6522) AUDIO_PIPELINE: Pipeline started
I (6532) BLUETOOTH_SOURCE_EXAMPLE: [ 8 ] Listen for all pipeline events
I (9732) HTTP_STREAM: total_bytes=2994349
I (10392) BLUETOOTH_SOURCE_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (14972) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (14972) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (14972) BLUETOOTH_SERVICE: --RSSI: -55
I (14982) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (15072) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (15072) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (15072) BLUETOOTH_SERVICE: --RSSI: -52
I (15072) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (15282) BLUETOOTH_SERVICE: Device discovery failed, continue to discover...
I (15282) BLUETOOTH_SERVICE: Discovery started.
I (15382) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (15382) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (15382) BLUETOOTH_SERVICE: --RSSI: -56
I (15392) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (17552) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (17552) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (17552) BLUETOOTH_SERVICE: --RSSI: -63
I (17562) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (17692) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (17692) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (17692) BLUETOOTH_SERVICE: --RSSI: -57
I (17702) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (22712) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (22712) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (22712) BLUETOOTH_SERVICE: --RSSI: -70
I (22712) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (27882) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (27882) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (27882) BLUETOOTH_SERVICE: --RSSI: -52
I (27892) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (28102) BLUETOOTH_SERVICE: Device discovery failed, continue to discover...
I (28112) BLUETOOTH_SERVICE: Discovery started.
I (30452) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (30452) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (30452) BLUETOOTH_SERVICE: --RSSI: -60
I (30462) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (35582) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (35582) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (35592) BLUETOOTH_SERVICE: --RSSI: -55
I (35592) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (40752) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (40752) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (40752) BLUETOOTH_SERVICE: --RSSI: -57
I (40752) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (40922) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (40922) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (40922) BLUETOOTH_SERVICE: --RSSI: -57
I (40922) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (40932) BLUETOOTH_SERVICE: Device discovery failed, continue to discover...
I (40942) BLUETOOTH_SERVICE: Discovery started.
I (41182) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (41182) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (41182) BLUETOOTH_SERVICE: --RSSI: -53
I (41182) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (45902) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (45902) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (45902) BLUETOOTH_SERVICE: --RSSI: -57
I (45912) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s

```

- 当 source 连接上另一开发板 sink 后，如连接成功，则开始获取 HTTP 服务器的 MP3 文件并且解码发送给 sink 端，打印如下：

```c
I (71252) BLUETOOTH_SERVICE: Scanned device: c4:dd:57:7f:50:ce
I (71262) BLUETOOTH_SERVICE: --Class of Device: 0x240414
I (71262) BLUETOOTH_SERVICE: --RSSI: -37
I (71262) BLUETOOTH_SERVICE: --Name: ESP-ADF-SPEAKER
I (71272) BLUETOOTH_SERVICE: Found a target device, address c4:dd:57:7f:50:ce, name ESP-ADF-SPEAKER
I (71282) BLUETOOTH_SERVICE: Cancel device discovery ...
I (71292) BLUETOOTH_SERVICE: Device discovery stopped.
I (71292) BLUETOOTH_SERVICE: a2dp connecting to peer: ESP-ADF-SPEAKER
E (71302) BT_APPL: reset flags
I (71302) BLUETOOTH_SERVICE: bt_a2d_source_cb state 4, evt 0x0
E (72302) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
W (72492) BT_APPL: new conn_srvc id:18, app_id:0
I (72492) BLUETOOTH_SERVICE: bt_a2d_source_cb state 4, evt 0x0
I (72492) BLUETOOTH_SERVICE: a2dp connected
I (72492) BLUETOOTH_SERVICE: a2dp media ready checking ...
I (72502) BLUETOOTH_SERVICE: bt_a2d_source_cb state 5, evt 0x3
I (72512) BLUETOOTH_SERVICE: a2dp media ready, starting ...
W (72522) BT_APPL: new conn_srvc id:18, app_id:1
I (72532) BT_LOG: bta_av_link_role_ok hndl:x41 role:0 conn_audio:x1 bits:1 features:x824b

I (72532) BLUETOOTH_SERVICE: bt_a2d_source_cb state 5, evt 0x3
I (72542) BLUETOOTH_SERVICE: a2dp media start successfully.
I (72542) BLUETOOTH_SERVICE: bt_a2d_source_cb state 5, evt 0x1

```


### 日志输出

以下为本例程的完整日志。

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13664
ho 0 tail 12 room 4
load:0x40080400,len:4632
0x40080400: _init at ??:?

entry 0x400806f4
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 15:08:11
I (29) boot: chip revision: 3
I (33) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) boot.esp32: SPI Speed      : 40MHz
I (45) boot.esp32: SPI Mode       : DIO
I (49) boot.esp32: SPI Flash Size : 2MB
I (54) boot: Enabling RNG early entropy source...
I (59) boot: Partition Table:
I (63) boot: ## Label            Usage          Type ST Offset   Length
I (70) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (78) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (85) boot:  2 factory          factory app      00 00 00010000 001e8480
I (93) boot: End of partition table
I (97) boot_comm: chip revision: 3, min. application chip revision: 0
I (104) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x48854 (297044) map
I (237) esp_image: segment 1: paddr=0x0005887c vaddr=0x3ffbdb60 size=0x04124 ( 16676) load
I (245) esp_image: segment 2: paddr=0x0005c9a8 vaddr=0x40080000 size=0x03670 ( 13936) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (252) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0x13bd30 (1293616) map
0x400d0020: _stext at ??:?

I (792) esp_image: segment 4: paddr=0x0019bd58 vaddr=0x40083670 size=0x17858 ( 96344) load
0x40083670: psram_cache_init at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/spiram_psram.c:1051

I (854) boot: Loaded app from partition at offset 0x10000
I (854) boot: Disabling RNG early entropy source...
I (854) psram: This chip is ESP32-D0WD
I (860) spiram: Found 64MBit SPI RAM device
I (863) spiram: SPI RAM mode: flash 40m sram 40m
I (869) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (876) cpu_start: Pro cpu up.
I (879) cpu_start: Application information:
I (884) cpu_start: Project name:     bt_source_demo
I (890) cpu_start: App version:      v2.2-222-gd3c18cb0-dirty
I (896) cpu_start: Compile time:     Nov 11 2021 15:08:00
I (902) cpu_start: ELF file SHA256:  9cb2e8ab88bcb283...
I (908) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (914) cpu_start: Starting app cpu, entry point is 0x40081ddc
0x40081ddc: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1809) spiram: SPI SRAM memory test OK
I (1809) heap_init: Initializing. RAM available for dynamic allocation:
I (1810) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1816) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (1822) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1828) heap_init: At 3FFC7710 len 000188F0 (98 KiB): DRAM
I (1834) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1841) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1847) heap_init: At 4009AEC8 len 00005138 (20 KiB): IRAM
I (1854) cpu_start: Pro cpu start user code
I (1858) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1880) spi_flash: detected chip: gd
I (1881) spi_flash: flash io: dio
W (1881) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1891) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1902) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1932) BLUETOOTH_SOURCE_EXAMPLE: [ 1 ] Create Bluetooth service
I (1942) BTDM_INIT: BT controller compile version [ba56601]
I (1942) system_api: Base MAC address is not set
I (1942) system_api: read default base MAC address from EFUSE
I (1952) phy_init: phy_version 4660,0162888,Dec 23 2020
I (2432) BLUETOOTH_SERVICE: event: 10
I (2442) BLUETOOTH_SERVICE: Starting device discovery...
I (2442) BLUETOOTH_SOURCE_EXAMPLE: [1.1] Get Bluetooth stream
W (2442) BT_BTC: A2DP Enable with AVRC
I (2442) BLUETOOTH_SOURCE_EXAMPLE: [ 2 ] Create http stream to read data
I (2452) BLUETOOTH_SERVICE: bt_a2d_source_cb state 1, evt 0x4
I (2452) BLUETOOTH_SOURCE_EXAMPLE: [ 3 ] Create mp3 decoder to decode mp3 file
I (2462) BLUETOOTH_SERVICE: event: 10
I (2472) MP3_DECODER: MP3 init
I (2472) BLUETOOTH_SERVICE: event: 10
I (2482) BLUETOOTH_SOURCE_EXAMPLE: [ 4 ] Create audio pipeline for BT Source
I (2482) BLUETOOTH_SERVICE: event: 10
I (2492) BLUETOOTH_SOURCE_EXAMPLE: [4.1] Register all elements to audio pipeline
I (2492) BLUETOOTH_SERVICE: Discovery started.
I (2502) BLUETOOTH_SOURCE_EXAMPLE: [4.2] Link it together http_stream-->mp3_decoder-->bt_stream_writer
I (2522) AUDIO_PIPELINE: link el->rb, el:0x3f809e54, tag:http, rb:0x3f809c00
I (2522) AUDIO_PIPELINE: link el->rb, el:0x3f80a72c, tag:mp3, rb:0x3f80a81c
I (2532) BLUETOOTH_SOURCE_EXAMPLE: [4.3] Set up  uri (http as http_stream, mp3 as mp3 decoder, and default output is i2s)
I (2542) BLUETOOTH_SOURCE_EXAMPLE: [ 5 ] Start and wait for Wi-Fi network
I (2562) wifi:wifi driver task: 3ffde5fc, prio:23, stack:6656, core=0
I (2562) wifi:wifi firmware version: bb6888c
I (2562) wifi:wifi certification version: v7.0
I (2562) wifi:config NVS flash: enabled
I (2572) wifi:config nano formating: disabled
I (2572) wifi:Init data frame dynamic rx buffer num: 32
I (2582) wifi:Init management frame dynamic rx buffer num: 32
I (2582) wifi:Init management short buffer num: 32
I (2592) wifi:Init static tx buffer num: 16
I (2592) wifi:Init tx cache buffer num: 32
I (2592) wifi:Init static rx buffer size: 1600
I (2602) wifi:Init static rx buffer num: 10
I (2602) wifi:Init dynamic rx buffer num: 32
I (2612) wifi_init: rx ba win: 6
I (2612) wifi_init: tcpip mbox: 32
I (2612) wifi_init: udp mbox: 6
I (2622) wifi_init: tcp mbox: 6
I (2622) wifi_init: tcp tx win: 5744
I (2632) wifi_init: tcp rx win: 5744
I (2632) wifi_init: tcp mss: 1440
I (2642) phy_init: phy_version 4660,0162888,Dec 23 2020
I (2652) wifi:mode : sta (94:b9:7e:65:c2:44)
I (3882) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (4452) wifi:state: init -> auth (b0)
I (4472) wifi:state: auth -> assoc (0)
I (4472) wifi:state: assoc -> run (10)
I (4492) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (4492) wifi:security: WPA2-PSK, phy: bgn, rssi: -42
I (4492) wifi:pm start, type: 1

W (4492) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4502) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4692) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (4692) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (4692) BLUETOOTH_SERVICE: --RSSI: -55
I (4702) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (4892) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (4892) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (4892) BLUETOOTH_SERVICE: --RSSI: -55
I (4902) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (6432) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (6432) PERIPH_WIFI: Got ip:192.168.5.187
I (6432) BLUETOOTH_SOURCE_EXAMPLE: [5.1] Create Bluetooth peripheral
I (6442) BLUETOOTH_SOURCE_EXAMPLE: [5.2] Start Bluetooth peripheral
I (6452) BLUETOOTH_SOURCE_EXAMPLE: [ 6 ] Set up  event listener
I (6452) BLUETOOTH_SOURCE_EXAMPLE: [6.1] Listening event from all elements of pipeline
I (6462) BLUETOOTH_SOURCE_EXAMPLE: [6.2] Listening event from peripherals
I (6472) BLUETOOTH_SOURCE_EXAMPLE: [ 7 ] Start audio_pipeline
I (6482) AUDIO_ELEMENT: [http-0x3f809e54] Element task created
I (6482) AUDIO_ELEMENT: [mp3-0x3f80a72c] Element task created
I (6492) AUDIO_ELEMENT: [bt-0x3f809adc] Element task created
I (6492) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4176264 Bytes, Inter:72292 Bytes, Dram:51536 Bytes

I (6512) AUDIO_ELEMENT: [http] AEL_MSG_CMD_RESUME,state:1
I (6522) AUDIO_ELEMENT: [mp3] AEL_MSG_CMD_RESUME,state:1
I (6522) MP3_DECODER: MP3 opened
I (6522) AUDIO_PIPELINE: Pipeline started
I (6532) BLUETOOTH_SOURCE_EXAMPLE: [ 8 ] Listen for all pipeline events
I (9732) HTTP_STREAM: total_bytes=2994349
I (10392) BLUETOOTH_SOURCE_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (14972) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (14972) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (14972) BLUETOOTH_SERVICE: --RSSI: -55
I (14982) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (15072) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (15072) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (15072) BLUETOOTH_SERVICE: --RSSI: -52
I (15072) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (15282) BLUETOOTH_SERVICE: Device discovery failed, continue to discover...
I (15282) BLUETOOTH_SERVICE: Discovery started.
I (15382) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (15382) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (15382) BLUETOOTH_SERVICE: --RSSI: -56
I (15392) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (17552) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (17552) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (17552) BLUETOOTH_SERVICE: --RSSI: -63
I (17562) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (17692) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (17692) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (17692) BLUETOOTH_SERVICE: --RSSI: -57
I (17702) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (22712) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (22712) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (22712) BLUETOOTH_SERVICE: --RSSI: -70
I (22712) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (27882) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (27882) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (27882) BLUETOOTH_SERVICE: --RSSI: -52
I (27892) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (28102) BLUETOOTH_SERVICE: Device discovery failed, continue to discover...
I (28112) BLUETOOTH_SERVICE: Discovery started.
I (30452) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (30452) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (30452) BLUETOOTH_SERVICE: --RSSI: -60
I (30462) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (35582) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (35582) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (35592) BLUETOOTH_SERVICE: --RSSI: -55
I (35592) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (40752) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (40752) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (40752) BLUETOOTH_SERVICE: --RSSI: -57
I (40752) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (40922) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (40922) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (40922) BLUETOOTH_SERVICE: --RSSI: -57
I (40922) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (40932) BLUETOOTH_SERVICE: Device discovery failed, continue to discover...
I (40942) BLUETOOTH_SERVICE: Discovery started.
I (41182) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (41182) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (41182) BLUETOOTH_SERVICE: --RSSI: -53
I (41182) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (45902) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (45902) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (45902) BLUETOOTH_SERVICE: --RSSI: -57
I (45912) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (51022) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (51022) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (51022) BLUETOOTH_SERVICE: --RSSI: -55
I (51032) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (51162) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (51162) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (51172) BLUETOOTH_SERVICE: --RSSI: -54
I (51172) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (53752) BLUETOOTH_SERVICE: Device discovery failed, continue to discover...
I (53762) BLUETOOTH_SERVICE: Discovery started.
I (58782) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (58782) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (58782) BLUETOOTH_SERVICE: --RSSI: -57
I (58792) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (58862) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (58862) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (58862) BLUETOOTH_SERVICE: --RSSI: -52
I (58872) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (63932) BLUETOOTH_SERVICE: Scanned device: 7c:7a:91:ba:df:91
I (63932) BLUETOOTH_SERVICE: --Class of Device: 0xc010c
I (63932) BLUETOOTH_SERVICE: --RSSI: -58
I (63942) BLUETOOTH_SERVICE: --Name: madcow-ThinkPad-T440s
I (66582) BLUETOOTH_SERVICE: Device discovery failed, continue to discover...
I (66582) BLUETOOTH_SERVICE: Discovery started.
I (71252) BLUETOOTH_SERVICE: Scanned device: c4:dd:57:7f:50:ce
I (71262) BLUETOOTH_SERVICE: --Class of Device: 0x240414
I (71262) BLUETOOTH_SERVICE: --RSSI: -37
I (71262) BLUETOOTH_SERVICE: --Name: ESP-ADF-SPEAKER
I (71272) BLUETOOTH_SERVICE: Found a target device, address c4:dd:57:7f:50:ce, name ESP-ADF-SPEAKER
I (71282) BLUETOOTH_SERVICE: Cancel device discovery ...
I (71292) BLUETOOTH_SERVICE: Device discovery stopped.
I (71292) BLUETOOTH_SERVICE: a2dp connecting to peer: ESP-ADF-SPEAKER
E (71302) BT_APPL: reset flags
I (71302) BLUETOOTH_SERVICE: bt_a2d_source_cb state 4, evt 0x0
E (72302) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
W (72492) BT_APPL: new conn_srvc id:18, app_id:0
I (72492) BLUETOOTH_SERVICE: bt_a2d_source_cb state 4, evt 0x0
I (72492) BLUETOOTH_SERVICE: a2dp connected
I (72492) BLUETOOTH_SERVICE: a2dp media ready checking ...
I (72502) BLUETOOTH_SERVICE: bt_a2d_source_cb state 5, evt 0x3
I (72512) BLUETOOTH_SERVICE: a2dp media ready, starting ...
W (72522) BT_APPL: new conn_srvc id:18, app_id:1
I (72532) BT_LOG: bta_av_link_role_ok hndl:x41 role:0 conn_audio:x1 bits:1 features:x824b

I (72532) BLUETOOTH_SERVICE: bt_a2d_source_cb state 5, evt 0x3
I (72542) BLUETOOTH_SERVICE: a2dp media start successfully.
I (72542) BLUETOOTH_SERVICE: bt_a2d_source_cb state 5, evt 0x1
W (132722) wifi:m f null

```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
