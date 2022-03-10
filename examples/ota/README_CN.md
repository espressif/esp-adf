# 空中升级服务 (OTA Service) 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_regular.png "中级")


## 例程简介

本例程在 ADF 框架下演示配置空中升级服务 (OTA Service) 更新应用 (app) 和数据 (data) 分区的例子。

此例程可以配置从 microSD 卡中升级数据分区，例程演示了从 microSD 卡升级语音提示音数据分区。此外，此例程也演示了从 HTTP 服务器获取应用二进制固件更新升级的操作。


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程需要先配置 Wi-Fi 连接信息，通过运行 `menuconfig > OTA App Configuration` 填写 `Wi-Fi SSID` 和 `Wi-Fi Password`。

```
menuconfig > OTA App Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

本例程需要先配置固件升级 HTTP 服务器的 URI 地址信息，通过运行 `menuconfig > OTA App Configuration` 填写 `firmware upgrade uri`。

```
menuconfig > OTA App Configuration > (http://192.168.5.72:8080/play_mp3_control.bin) firmware upgrade uri
```

本例程提供了从 microSD 卡中升级数据分区的功能，通过运行 `menuconfig > OTA App Configuration` 填写 `data image upgrade uri`。

```
menuconfig > OTA App Configuration > (file://sdcard/flash_tone.bin) data image upgrade uri
```

同时，需要填写所选数据分区的名称，通过运行 `menuconfig > OTA App Configuration` 填写 `data partition label`。

```
menuconfig > OTA App Configuration > (flash_tone) data partition label
```

本例程默认不检查固件版本号，如果需要强制检查固件版本号，则需要使能 `Forece check the firmware version number when OTA start` 选项。

```
menuconfig > OTA App Configuration > Forece check the firmware version number when OTA start
```

本例程需要准备一张 microSD 卡，放置提示音二进制文件名为 `flash_tone.bin`，用于更新提示音分区数据。

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程

### 功能和用法

- 例程需要先运行 Python 脚本，需要 python 2.7，并且开发板和 HTTP 服务器连接在同一个 Wi-Fi 网络中，Python 脚本运行日志如下：

```
python -m http.server -b 192.168.5.72 8080
Serving HTTP on 192.168.5.72 port 8080 (http://192.168.5.72:8080/) ...
192.168.5.187 - - [25/Nov/2021 16:13:37] "GET /play_mp3_control.bin HTTP/1.1" 200 -
```

- 例程开始运行后，将主动连接 Wi-Fi 热点，如连接成功则去先更新 `flash_tone` 分区的数据，关键部分打印如下：

```c
I (5246) OTA_DEFAULT: data upgrade uri file://sdcard/flash_tone.bin
I (5256) FATFS_STREAM: File size: 434756 byte, file position: 0
I (5256) TONE_PARTITION: tone partition format 0, total 21
I (5266) HTTPS_OTA_EXAMPLE: format 0 : 0
I (7136) OTA_DEFAULT: write_offset 0, size 8
I (8216) OTA_DEFAULT: write_offset 8, r_size 2048
I (8226) OTA_DEFAULT: write_offset 2056, r_size 2048
I (8226) OTA_DEFAULT: write_offset 4104, r_size 2048
I (8236) OTA_DEFAULT: write_offset 6152, r_size 2048
I (8246) OTA_DEFAULT: write_offset 8200, r_size 2048
I (8256) OTA_DEFAULT: write_offset 10248, r_size 2048
I (8256) OTA_DEFAULT: write_offset 12296, r_size 2048
I (8266) OTA_DEFAULT: write_offset 14344, r_size 2048
I (8276) OTA_DEFAULT: write_offset 16392, r_size 2048
I (8286) OTA_DEFAULT: write_offset 18440, r_size 2048
I (8296) OTA_DEFAULT: write_offset 20488, r_size 2048

...

I (9756) OTA_DEFAULT: write_offset 409608, r_size 2048
I (9756) OTA_DEFAULT: write_offset 411656, r_size 2048
I (9766) OTA_DEFAULT: write_offset 413704, r_size 2048
I (9776) OTA_DEFAULT: write_offset 415752, r_size 2048
I (9786) OTA_DEFAULT: write_offset 417800, r_size 2048
I (9786) OTA_DEFAULT: write_offset 419848, r_size 2048
I (9796) OTA_DEFAULT: write_offset 421896, r_size 2048
I (9806) OTA_DEFAULT: write_offset 423944, r_size 2048
I (9816) OTA_DEFAULT: write_offset 425992, r_size 2048
I (9816) OTA_DEFAULT: write_offset 428040, r_size 2048
I (9826) OTA_DEFAULT: write_offset 430088, r_size 2048
I (9836) OTA_DEFAULT: write_offset 432136, r_size 2048
I (9846) OTA_DEFAULT: write_offset 434184, r_size 572
W (9846) FATFS_STREAM: No more data, ret:0
I (9846) AUDIO_ELEMENT: IN-[file] AEL_IO_DONE,0
I (9846) OTA_DEFAULT: partition flash_tone upgrade successes
W (9856) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
```

- 随后将开始升级应用分区 (app) 的固件，此例程设置升级 `$ADF_PATH/examples/get-started/play_mp3_control/build` 编译的固件，关键部分打印如下：

```c
I (9866) HTTPS_OTA_EXAMPLE: List id: 0, OTA success
I (9886) esp_https_ota: Starting OTA...
I (9896) esp_https_ota: Writing to partition subtype 17 at offset 0x110000
I (9896) OTA_DEFAULT: Running firmware version: v2.2-252-g62edbd26-dirty, the incoming firmware version v2.2-237-g6532b89f
I (13186) OTA_DEFAULT: Image bytes read: 578
I (13186) OTA_DEFAULT: Image bytes read: 867
I (13196) OTA_DEFAULT: Image bytes read: 1156
I (13196) OTA_DEFAULT: Image bytes read: 1445
I (13196) OTA_DEFAULT: Image bytes read: 1734
I (13206) OTA_DEFAULT: Image bytes read: 2023
I (13206) OTA_DEFAULT: Image bytes read: 2312
I (13216) OTA_DEFAULT: Image bytes read: 2601
I (13216) OTA_DEFAULT: Image bytes read: 2890
I (13226) OTA_DEFAULT: Image bytes read: 3179
I (13226) OTA_DEFAULT: Image bytes read: 3468
I (13236) OTA_DEFAULT: Image bytes read: 3757
I (13236) OTA_DEFAULT: Image bytes read: 4046

...

I (26936) OTA_DEFAULT: Image bytes read: 765561
I (26936) OTA_DEFAULT: Image bytes read: 765850
I (26946) OTA_DEFAULT: Image bytes read: 766139
I (26946) OTA_DEFAULT: Image bytes read: 766428
I (26956) OTA_DEFAULT: Image bytes read: 766717
I (26956) OTA_DEFAULT: Image bytes read: 767006
I (26966) OTA_DEFAULT: Image bytes read: 767295
I (26966) OTA_DEFAULT: Image bytes read: 767584
I (26976) OTA_DEFAULT: Image bytes read: 767873
I (26976) OTA_DEFAULT: Image bytes read: 768162
I (26986) OTA_DEFAULT: Image bytes read: 768209
I (26986) esp_https_ota: Connection closed
I (26996) esp_image: segment 0: paddr=0x00110020 vaddr=0x3f400020 size=0x85590 (546192) map
I (27186) esp_image: segment 1: paddr=0x001955b8 vaddr=0x3ffb0000 size=0x02100 (  8448)
I (27196) esp_image: segment 2: paddr=0x001976c0 vaddr=0x40080000 size=0x08958 ( 35160)
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (27206) esp_image: segment 3: paddr=0x001a0020 vaddr=0x400d0020 size=0x27e70 (163440) map
0x400d0020: _stext at ??:?

I (27266) esp_image: segment 4: paddr=0x001c7e98 vaddr=0x40088958 size=0x038f0 ( 14576)
0x40088958: print_backtrace_entry at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_system/port/panic_handler.c:144

I (27276) esp_image: segment 0: paddr=0x00110020 vaddr=0x3f400020 size=0x85590 (546192) map
I (27466) esp_image: segment 1: paddr=0x001955b8 vaddr=0x3ffb0000 size=0x02100 (  8448)
I (27466) esp_image: segment 2: paddr=0x001976c0 vaddr=0x40080000 size=0x08958 ( 35160)
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (27486) esp_image: segment 3: paddr=0x001a0020 vaddr=0x400d0020 size=0x27e70 (163440) map
0x400d0020: _stext at ??:?

I (27546) esp_image: segment 4: paddr=0x001c7e98 vaddr=0x40088958 size=0x038f0 ( 14576)
0x40088958: print_backtrace_entry at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_system/port/panic_handler.c:144

I (27566) HTTPS_OTA_EXAMPLE: List id: 1, OTA success
W (27566) OTA_SERVICE: OTA_END!
W (27566) OTA_SERVICE: restart!

```


### 日志输出

以下为本例程的完整日志。

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
I (27) boot: compile time 14:59:43
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 otadata          OTA data         01 00 0000d000 00002000
I (83) boot:  2 phy_init         RF data          01 01 0000f000 00001000
I (91) boot:  3 ota_0            OTA app          00 10 00010000 00100000
I (98) boot:  4 ota_1            OTA app          00 11 00110000 00100000
I (106) boot:  5 flash_tone       Unknown data     01 ff 00210000 00100000
I (113) boot: End of partition table
I (118) boot: No factory image, trying OTA 0
I (122) boot_comm: chip revision: 3, min. application chip revision: 0
I (130) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x272e4 (160484) map
I (200) esp_image: segment 1: paddr=0x0003730c vaddr=0x3ffb0000 size=0x03700 ( 14080) load
I (206) esp_image: segment 2: paddr=0x0003aa14 vaddr=0x40080000 size=0x05604 ( 22020) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (216) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xa7b40 (686912) map
0x400d0020: _stext at ??:?

I (478) esp_image: segment 4: paddr=0x000e7b68 vaddr=0x40085604 size=0x1223c ( 74300) load
0x40085604: acquire_end_core at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/driver/spi_bus_lock.c:414
 (inlined by) spi_bus_lock_acquire_end at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/driver/spi_bus_lock.c:703

I (523) boot: Loaded app from partition at offset 0x10000
I (539) boot: Set actual ota_seq=1 in otadata[0]
I (539) boot: Disabling RNG early entropy source...
I (539) cpu_start: Pro cpu up.
I (542) cpu_start: Application information:
I (547) cpu_start: Project name:     ota_example
I (552) cpu_start: App version:      v2.2-252-g62edbd26-dirty
I (559) cpu_start: Compile time:     Nov 25 2021 16:38:27
I (565) cpu_start: ELF file SHA256:  4acd1ee4e347ff83...
I (571) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (577) cpu_start: Starting app cpu, entry point is 0x40081a50
0x40081a50: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (587) heap_init: Initializing. RAM available for dynamic allocation:
I (594) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (600) heap_init: At 3FFB7EE0 len 00028120 (160 KiB): DRAM
I (606) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (613) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (619) heap_init: At 40097840 len 000087C0 (33 KiB): IRAM
I (625) cpu_start: Pro cpu start user code
I (644) spi_flash: detected chip: gd
I (644) spi_flash: flash io: dio
W (644) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (654) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (726) HTTPS_OTA_EXAMPLE: [1.0] Initialize peripherals management
I (726) HTTPS_OTA_EXAMPLE: [1.1] Start and wait for Wi-Fi network
I (746) wifi:wifi driver task: 3ffc20a4, prio:23, stack:6656, core=0
I (746) system_api: Base MAC address is not set
I (746) system_api: read default base MAC address from EFUSE
I (766) wifi:wifi firmware version: bb6888c
I (766) wifi:wifi certification version: v7.0
I (766) wifi:config NVS flash: enabled
I (766) wifi:config nano formating: disabled
I (776) wifi:Init data frame dynamic rx buffer num: 32
I (776) wifi:Init management frame dynamic rx buffer num: 32
I (786) wifi:Init management short buffer num: 32
I (786) wifi:Init dynamic tx buffer num: 32
I (796) wifi:Init static rx buffer size: 1600
I (796) wifi:Init static rx buffer num: 10
I (796) wifi:Init dynamic rx buffer num: 32
I (806) wifi_init: rx ba win: 6
I (806) wifi_init: tcpip mbox: 32
I (816) wifi_init: udp mbox: 6
I (816) wifi_init: tcp mbox: 6
I (816) wifi_init: tcp tx win: 5744
I (826) wifi_init: tcp rx win: 5744
I (826) wifi_init: tcp mss: 1436
I (836) wifi_init: WiFi IRAM OP enabled
I (836) wifi_init: WiFi RX IRAM OP enabled
I (846) phy_init: phy_version 4660,0162888,Dec 23 2020
I (956) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2166) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2946) wifi:state: init -> auth (b0)
I (2956) wifi:state: auth -> assoc (0)
I (2966) wifi:state: assoc -> run (10)
I (2976) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (2976) wifi:security: WPA2-PSK, phy: bgn, rssi: -34
I (2986) wifi:pm start, type: 1

W (2986) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (3036) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4726) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (4726) PERIPH_WIFI: Got ip:192.168.5.187
I (4726) HTTPS_OTA_EXAMPLE: [1.2] Mount SDCard
I (4746) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (4776) SDCARD: CID name SA16G!

I (5236) HTTPS_OTA_EXAMPLE: [2.0] Create OTA service
I (5236) HTTPS_OTA_EXAMPLE: [2.1] Set upgrade list
I (5236) HTTPS_OTA_EXAMPLE: [2.2] Start OTA service
I (5236) HTTPS_OTA_EXAMPLE: Func:app_main, Line:224, MEM Total:193760 Bytes

I (5246) OTA_DEFAULT: data upgrade uri file://sdcard/flash_tone.bin
I (5256) FATFS_STREAM: File size: 434756 byte, file position: 0
I (5256) TONE_PARTITION: tone partition format 0, total 21
I (5266) HTTPS_OTA_EXAMPLE: format 0 : 0
I (7136) OTA_DEFAULT: write_offset 0, size 8
I (8216) OTA_DEFAULT: write_offset 8, r_size 2048
I (8226) OTA_DEFAULT: write_offset 2056, r_size 2048
I (8226) OTA_DEFAULT: write_offset 4104, r_size 2048
I (8236) OTA_DEFAULT: write_offset 6152, r_size 2048
I (8246) OTA_DEFAULT: write_offset 8200, r_size 2048
I (8256) OTA_DEFAULT: write_offset 10248, r_size 2048
I (8256) OTA_DEFAULT: write_offset 12296, r_size 2048
I (8266) OTA_DEFAULT: write_offset 14344, r_size 2048
I (8276) OTA_DEFAULT: write_offset 16392, r_size 2048
I (8286) OTA_DEFAULT: write_offset 18440, r_size 2048
I (8296) OTA_DEFAULT: write_offset 20488, r_size 2048
I (8296) OTA_DEFAULT: write_offset 22536, r_size 2048
I (8306) OTA_DEFAULT: write_offset 24584, r_size 2048
I (8316) OTA_DEFAULT: write_offset 26632, r_size 2048
I (8326) OTA_DEFAULT: write_offset 28680, r_size 2048
I (8326) OTA_DEFAULT: write_offset 30728, r_size 2048
I (8336) OTA_DEFAULT: write_offset 32776, r_size 2048
I (8346) OTA_DEFAULT: write_offset 34824, r_size 2048
I (8356) OTA_DEFAULT: write_offset 36872, r_size 2048
I (8356) OTA_DEFAULT: write_offset 38920, r_size 2048
I (8366) OTA_DEFAULT: write_offset 40968, r_size 2048
I (8376) OTA_DEFAULT: write_offset 43016, r_size 2048
I (8386) OTA_DEFAULT: write_offset 45064, r_size 2048
I (8396) OTA_DEFAULT: write_offset 47112, r_size 2048
I (8396) OTA_DEFAULT: write_offset 49160, r_size 2048
I (8406) OTA_DEFAULT: write_offset 51208, r_size 2048
I (8416) OTA_DEFAULT: write_offset 53256, r_size 2048
I (8426) OTA_DEFAULT: write_offset 55304, r_size 2048
I (8426) OTA_DEFAULT: write_offset 57352, r_size 2048
I (8436) OTA_DEFAULT: write_offset 59400, r_size 2048
I (8446) OTA_DEFAULT: write_offset 61448, r_size 2048
I (8456) OTA_DEFAULT: write_offset 63496, r_size 2048
I (8466) OTA_DEFAULT: write_offset 65544, r_size 2048
I (8466) OTA_DEFAULT: write_offset 67592, r_size 2048
I (8476) OTA_DEFAULT: write_offset 69640, r_size 2048
I (8486) OTA_DEFAULT: write_offset 71688, r_size 2048
I (8496) OTA_DEFAULT: write_offset 73736, r_size 2048
I (8496) OTA_DEFAULT: write_offset 75784, r_size 2048
I (8506) OTA_DEFAULT: write_offset 77832, r_size 2048
I (8516) OTA_DEFAULT: write_offset 79880, r_size 2048
I (8526) OTA_DEFAULT: write_offset 81928, r_size 2048
I (8526) OTA_DEFAULT: write_offset 83976, r_size 2048
I (8536) OTA_DEFAULT: write_offset 86024, r_size 2048
I (8546) OTA_DEFAULT: write_offset 88072, r_size 2048
I (8556) OTA_DEFAULT: write_offset 90120, r_size 2048
I (8566) OTA_DEFAULT: write_offset 92168, r_size 2048
I (8566) OTA_DEFAULT: write_offset 94216, r_size 2048
I (8576) OTA_DEFAULT: write_offset 96264, r_size 2048
I (8586) OTA_DEFAULT: write_offset 98312, r_size 2048
I (8596) OTA_DEFAULT: write_offset 100360, r_size 2048
I (8596) OTA_DEFAULT: write_offset 102408, r_size 2048
I (8606) OTA_DEFAULT: write_offset 104456, r_size 2048
I (8616) OTA_DEFAULT: write_offset 106504, r_size 2048
I (8626) OTA_DEFAULT: write_offset 108552, r_size 2048
I (8626) OTA_DEFAULT: write_offset 110600, r_size 2048
I (8636) OTA_DEFAULT: write_offset 112648, r_size 2048
I (8646) OTA_DEFAULT: write_offset 114696, r_size 2048
I (8656) OTA_DEFAULT: write_offset 116744, r_size 2048
I (8666) OTA_DEFAULT: write_offset 118792, r_size 2048
I (8666) OTA_DEFAULT: write_offset 120840, r_size 2048
I (8676) OTA_DEFAULT: write_offset 122888, r_size 2048
I (8686) OTA_DEFAULT: write_offset 124936, r_size 2048
I (8696) OTA_DEFAULT: write_offset 126984, r_size 2048
I (8696) OTA_DEFAULT: write_offset 129032, r_size 2048
I (8706) OTA_DEFAULT: write_offset 131080, r_size 2048
I (8716) OTA_DEFAULT: write_offset 133128, r_size 2048
I (8726) OTA_DEFAULT: write_offset 135176, r_size 2048
I (8726) OTA_DEFAULT: write_offset 137224, r_size 2048
I (8736) OTA_DEFAULT: write_offset 139272, r_size 2048
I (8746) OTA_DEFAULT: write_offset 141320, r_size 2048
I (8756) OTA_DEFAULT: write_offset 143368, r_size 2048
I (8756) OTA_DEFAULT: write_offset 145416, r_size 2048
I (8766) OTA_DEFAULT: write_offset 147464, r_size 2048
I (8776) OTA_DEFAULT: write_offset 149512, r_size 2048
I (8786) OTA_DEFAULT: write_offset 151560, r_size 2048
I (8796) OTA_DEFAULT: write_offset 153608, r_size 2048
I (8796) OTA_DEFAULT: write_offset 155656, r_size 2048
I (8806) OTA_DEFAULT: write_offset 157704, r_size 2048
I (8816) OTA_DEFAULT: write_offset 159752, r_size 2048
I (8826) OTA_DEFAULT: write_offset 161800, r_size 2048
I (8826) OTA_DEFAULT: write_offset 163848, r_size 2048
I (8836) OTA_DEFAULT: write_offset 165896, r_size 2048
I (8846) OTA_DEFAULT: write_offset 167944, r_size 2048
I (8856) OTA_DEFAULT: write_offset 169992, r_size 2048
I (8856) OTA_DEFAULT: write_offset 172040, r_size 2048
I (8866) OTA_DEFAULT: write_offset 174088, r_size 2048
I (8876) OTA_DEFAULT: write_offset 176136, r_size 2048
I (8886) OTA_DEFAULT: write_offset 178184, r_size 2048
I (8896) OTA_DEFAULT: write_offset 180232, r_size 2048
I (8896) OTA_DEFAULT: write_offset 182280, r_size 2048
I (8906) OTA_DEFAULT: write_offset 184328, r_size 2048
I (8916) OTA_DEFAULT: write_offset 186376, r_size 2048
I (8926) OTA_DEFAULT: write_offset 188424, r_size 2048
I (8926) OTA_DEFAULT: write_offset 190472, r_size 2048
I (8936) OTA_DEFAULT: write_offset 192520, r_size 2048
I (8946) OTA_DEFAULT: write_offset 194568, r_size 2048
I (8956) OTA_DEFAULT: write_offset 196616, r_size 2048
I (8956) OTA_DEFAULT: write_offset 198664, r_size 2048
I (8966) OTA_DEFAULT: write_offset 200712, r_size 2048
I (8976) OTA_DEFAULT: write_offset 202760, r_size 2048
I (8986) OTA_DEFAULT: write_offset 204808, r_size 2048
I (8996) OTA_DEFAULT: write_offset 206856, r_size 2048
I (8996) OTA_DEFAULT: write_offset 208904, r_size 2048
I (9006) OTA_DEFAULT: write_offset 210952, r_size 2048
I (9016) OTA_DEFAULT: write_offset 213000, r_size 2048
I (9026) OTA_DEFAULT: write_offset 215048, r_size 2048
I (9026) OTA_DEFAULT: write_offset 217096, r_size 2048
I (9036) OTA_DEFAULT: write_offset 219144, r_size 2048
I (9046) OTA_DEFAULT: write_offset 221192, r_size 2048
I (9056) OTA_DEFAULT: write_offset 223240, r_size 2048
I (9056) OTA_DEFAULT: write_offset 225288, r_size 2048
I (9066) OTA_DEFAULT: write_offset 227336, r_size 2048
I (9076) OTA_DEFAULT: write_offset 229384, r_size 2048
I (9086) OTA_DEFAULT: write_offset 231432, r_size 2048
I (9086) OTA_DEFAULT: write_offset 233480, r_size 2048
I (9096) OTA_DEFAULT: write_offset 235528, r_size 2048
I (9106) OTA_DEFAULT: write_offset 237576, r_size 2048
I (9116) OTA_DEFAULT: write_offset 239624, r_size 2048
I (9126) OTA_DEFAULT: write_offset 241672, r_size 2048
I (9126) OTA_DEFAULT: write_offset 243720, r_size 2048
I (9136) OTA_DEFAULT: write_offset 245768, r_size 2048
I (9146) OTA_DEFAULT: write_offset 247816, r_size 2048
I (9156) OTA_DEFAULT: write_offset 249864, r_size 2048
I (9156) OTA_DEFAULT: write_offset 251912, r_size 2048
I (9166) OTA_DEFAULT: write_offset 253960, r_size 2048
I (9176) OTA_DEFAULT: write_offset 256008, r_size 2048
I (9186) OTA_DEFAULT: write_offset 258056, r_size 2048
I (9186) OTA_DEFAULT: write_offset 260104, r_size 2048
I (9196) OTA_DEFAULT: write_offset 262152, r_size 2048
I (9206) OTA_DEFAULT: write_offset 264200, r_size 2048
I (9216) OTA_DEFAULT: write_offset 266248, r_size 2048
I (9226) OTA_DEFAULT: write_offset 268296, r_size 2048
I (9226) OTA_DEFAULT: write_offset 270344, r_size 2048
I (9236) OTA_DEFAULT: write_offset 272392, r_size 2048
I (9246) OTA_DEFAULT: write_offset 274440, r_size 2048
I (9256) OTA_DEFAULT: write_offset 276488, r_size 2048
I (9256) OTA_DEFAULT: write_offset 278536, r_size 2048
I (9266) OTA_DEFAULT: write_offset 280584, r_size 2048
I (9276) OTA_DEFAULT: write_offset 282632, r_size 2048
I (9286) OTA_DEFAULT: write_offset 284680, r_size 2048
I (9286) OTA_DEFAULT: write_offset 286728, r_size 2048
I (9296) OTA_DEFAULT: write_offset 288776, r_size 2048
I (9306) OTA_DEFAULT: write_offset 290824, r_size 2048
I (9316) OTA_DEFAULT: write_offset 292872, r_size 2048
I (9316) OTA_DEFAULT: write_offset 294920, r_size 2048
I (9326) OTA_DEFAULT: write_offset 296968, r_size 2048
I (9336) OTA_DEFAULT: write_offset 299016, r_size 2048
I (9346) OTA_DEFAULT: write_offset 301064, r_size 2048
I (9356) OTA_DEFAULT: write_offset 303112, r_size 2048
I (9356) OTA_DEFAULT: write_offset 305160, r_size 2048
I (9366) OTA_DEFAULT: write_offset 307208, r_size 2048
I (9376) OTA_DEFAULT: write_offset 309256, r_size 2048
I (9386) OTA_DEFAULT: write_offset 311304, r_size 2048
I (9386) OTA_DEFAULT: write_offset 313352, r_size 2048
I (9396) OTA_DEFAULT: write_offset 315400, r_size 2048
I (9406) OTA_DEFAULT: write_offset 317448, r_size 2048
I (9416) OTA_DEFAULT: write_offset 319496, r_size 2048
I (9416) OTA_DEFAULT: write_offset 321544, r_size 2048
I (9426) OTA_DEFAULT: write_offset 323592, r_size 2048
I (9436) OTA_DEFAULT: write_offset 325640, r_size 2048
I (9446) OTA_DEFAULT: write_offset 327688, r_size 2048
I (9456) OTA_DEFAULT: write_offset 329736, r_size 2048
I (9456) OTA_DEFAULT: write_offset 331784, r_size 2048
I (9466) OTA_DEFAULT: write_offset 333832, r_size 2048
I (9476) OTA_DEFAULT: write_offset 335880, r_size 2048
I (9486) OTA_DEFAULT: write_offset 337928, r_size 2048
I (9486) OTA_DEFAULT: write_offset 339976, r_size 2048
I (9496) OTA_DEFAULT: write_offset 342024, r_size 2048
I (9506) OTA_DEFAULT: write_offset 344072, r_size 2048
I (9516) OTA_DEFAULT: write_offset 346120, r_size 2048
I (9516) OTA_DEFAULT: write_offset 348168, r_size 2048
I (9526) OTA_DEFAULT: write_offset 350216, r_size 2048
I (9536) OTA_DEFAULT: write_offset 352264, r_size 2048
I (9546) OTA_DEFAULT: write_offset 354312, r_size 2048
I (9546) OTA_DEFAULT: write_offset 356360, r_size 2048
I (9556) OTA_DEFAULT: write_offset 358408, r_size 2048
I (9566) OTA_DEFAULT: write_offset 360456, r_size 2048
I (9576) OTA_DEFAULT: write_offset 362504, r_size 2048
I (9586) OTA_DEFAULT: write_offset 364552, r_size 2048
I (9586) OTA_DEFAULT: write_offset 366600, r_size 2048
I (9596) OTA_DEFAULT: write_offset 368648, r_size 2048
I (9606) OTA_DEFAULT: write_offset 370696, r_size 2048
I (9616) OTA_DEFAULT: write_offset 372744, r_size 2048
I (9616) OTA_DEFAULT: write_offset 374792, r_size 2048
I (9626) OTA_DEFAULT: write_offset 376840, r_size 2048
I (9636) OTA_DEFAULT: write_offset 378888, r_size 2048
I (9646) OTA_DEFAULT: write_offset 380936, r_size 2048
I (9646) OTA_DEFAULT: write_offset 382984, r_size 2048
I (9656) OTA_DEFAULT: write_offset 385032, r_size 2048
I (9666) OTA_DEFAULT: write_offset 387080, r_size 2048
I (9676) OTA_DEFAULT: write_offset 389128, r_size 2048
I (9686) OTA_DEFAULT: write_offset 391176, r_size 2048
I (9686) OTA_DEFAULT: write_offset 393224, r_size 2048
I (9696) OTA_DEFAULT: write_offset 395272, r_size 2048
I (9706) OTA_DEFAULT: write_offset 397320, r_size 2048
I (9716) OTA_DEFAULT: write_offset 399368, r_size 2048
I (9716) OTA_DEFAULT: write_offset 401416, r_size 2048
I (9726) OTA_DEFAULT: write_offset 403464, r_size 2048
I (9736) OTA_DEFAULT: write_offset 405512, r_size 2048
I (9746) OTA_DEFAULT: write_offset 407560, r_size 2048
I (9756) OTA_DEFAULT: write_offset 409608, r_size 2048
I (9756) OTA_DEFAULT: write_offset 411656, r_size 2048
I (9766) OTA_DEFAULT: write_offset 413704, r_size 2048
I (9776) OTA_DEFAULT: write_offset 415752, r_size 2048
I (9786) OTA_DEFAULT: write_offset 417800, r_size 2048
I (9786) OTA_DEFAULT: write_offset 419848, r_size 2048
I (9796) OTA_DEFAULT: write_offset 421896, r_size 2048
I (9806) OTA_DEFAULT: write_offset 423944, r_size 2048
I (9816) OTA_DEFAULT: write_offset 425992, r_size 2048
I (9816) OTA_DEFAULT: write_offset 428040, r_size 2048
I (9826) OTA_DEFAULT: write_offset 430088, r_size 2048
I (9836) OTA_DEFAULT: write_offset 432136, r_size 2048
I (9846) OTA_DEFAULT: write_offset 434184, r_size 572
W (9846) FATFS_STREAM: No more data, ret:0
I (9846) AUDIO_ELEMENT: IN-[file] AEL_IO_DONE,0
I (9846) OTA_DEFAULT: partition flash_tone upgrade successes
W (9856) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
I (9866) HTTPS_OTA_EXAMPLE: List id: 0, OTA success
I (9886) esp_https_ota: Starting OTA...
I (9896) esp_https_ota: Writing to partition subtype 17 at offset 0x110000
I (9896) OTA_DEFAULT: Running firmware version: v2.2-252-g62edbd26-dirty, the incoming firmware version v2.2-237-g6532b89f
I (13186) OTA_DEFAULT: Image bytes read: 578
I (13186) OTA_DEFAULT: Image bytes read: 867
I (13196) OTA_DEFAULT: Image bytes read: 1156
I (13196) OTA_DEFAULT: Image bytes read: 1445
I (13196) OTA_DEFAULT: Image bytes read: 1734
... until downloading finished


I (26976) OTA_DEFAULT: Image bytes read: 768162
I (26986) OTA_DEFAULT: Image bytes read: 768209
I (26986) esp_https_ota: Connection closed
I (26996) esp_image: segment 0: paddr=0x00110020 vaddr=0x3f400020 size=0x85590 (546192) map
I (27186) esp_image: segment 1: paddr=0x001955b8 vaddr=0x3ffb0000 size=0x02100 (  8448)
I (27196) esp_image: segment 2: paddr=0x001976c0 vaddr=0x40080000 size=0x08958 ( 35160)
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (27206) esp_image: segment 3: paddr=0x001a0020 vaddr=0x400d0020 size=0x27e70 (163440) map
0x400d0020: _stext at ??:?

I (27266) esp_image: segment 4: paddr=0x001c7e98 vaddr=0x40088958 size=0x038f0 ( 14576)
0x40088958: print_backtrace_entry at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_system/port/panic_handler.c:144

I (27276) esp_image: segment 0: paddr=0x00110020 vaddr=0x3f400020 size=0x85590 (546192) map
I (27466) esp_image: segment 1: paddr=0x001955b8 vaddr=0x3ffb0000 size=0x02100 (  8448)
I (27466) esp_image: segment 2: paddr=0x001976c0 vaddr=0x40080000 size=0x08958 ( 35160)
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (27486) esp_image: segment 3: paddr=0x001a0020 vaddr=0x400d0020 size=0x27e70 (163440) map
0x400d0020: _stext at ??:?

I (27546) esp_image: segment 4: paddr=0x001c7e98 vaddr=0x40088958 size=0x038f0 ( 14576)
0x40088958: print_backtrace_entry at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_system/port/panic_handler.c:144

I (27566) HTTPS_OTA_EXAMPLE: List id: 1, OTA success
W (27566) OTA_SERVICE: OTA_END!
W (27566) OTA_SERVICE: restart!
I (27566) wifi:state: run -> init (0)
I (27576) wifi:pm stop, total sleep time: 6364543 us / 24586906 us

I (27576) wifi:new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (27586) wifi:hmac tx: stop, discard
W (27586) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect enabled, reconnect after 1000 ms
W (27596) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3
I (27636) wifi:flush txq
I (27636) wifi:stop sw txq
I (27636) wifi:lmac stop hw txq
ets Jul 29 2019 12:21:46

rst:0xc (SW_CPU_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 14:59:43
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 otadata          OTA data         01 00 0000d000 00002000
I (83) boot:  2 phy_init         RF data          01 01 0000f000 00001000
I (91) boot:  3 ota_0            OTA app          00 10 00010000 00100000
I (98) boot:  4 ota_1            OTA app          00 11 00110000 00100000
I (105) boot:  5 flash_tone       Unknown data     01 ff 00210000 00100000
I (113) boot: End of partition table
I (117) esp_image: segment 0: paddr=0x00110020 vaddr=0x3f400020 size=0x85590 (546192) map
I (334) esp_image: segment 1: paddr=0x001955b8 vaddr=0x3ffb0000 size=0x02100 (  8448) load
I (338) esp_image: segment 2: paddr=0x001976c0 vaddr=0x40080000 size=0x08958 ( 35160) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (356) esp_image: segment 3: paddr=0x001a0020 vaddr=0x400d0020 size=0x27e70 (163440) map
0x400d0020: _stext at ??:?

I (419) esp_image: segment 4: paddr=0x001c7e98 vaddr=0x40088958 size=0x038f0 ( 14576) load
0x40088958: print_backtrace_entry at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_system/port/panic_handler.c:144

I (432) boot: Loaded app from partition at offset 0x110000
I (432) boot: Disabling RNG early entropy source...
I (433) cpu_start: Pro cpu up.
I (437) cpu_start: Application information:
I (441) cpu_start: Project name:     play_mp3_control
I (447) cpu_start: App version:      v2.2-237-g6532b89f
I (453) cpu_start: Compile time:     Nov 22 2021 15:35:26
I (459) cpu_start: ELF file SHA256:  25546313465b7114...
I (465) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (471) cpu_start: Starting app cpu, entry point is 0x40081704
0x40081704: heap_caps_realloc_default at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/heap/heap_caps.c:206

I (464) cpu_start: App cpu up.
I (482) heap_init: Initializing. RAM available for dynamic allocation:
I (489) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (495) heap_init: At 3FFB29B8 len 0002D648 (181 KiB): DRAM
I (501) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (507) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (514) heap_init: At 4008C248 len 00013DB8 (79 KiB): IRAM
I (520) cpu_start: Pro cpu start user code
I (538) spi_flash: detected chip: gd
I (538) spi_flash: flash io: dio
W (538) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (548) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (559) PLAY_FLASH_MP3_CONTROL: [ 1 ] Start audio codec chip
I (1129) PLAY_FLASH_MP3_CONTROL: [ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event
I (1129) PLAY_FLASH_MP3_CONTROL: [2.1] Create mp3 decoder to decode mp3 file and set custom read callback
I (1139) PLAY_FLASH_MP3_CONTROL: [2.2] Create i2s stream to write data to codec chip
I (1159) PLAY_FLASH_MP3_CONTROL: [2.3] Register all elements to audio pipeline
I (1159) PLAY_FLASH_MP3_CONTROL: [2.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]
I (1169) PLAY_FLASH_MP3_CONTROL: [ 3 ] Initialize peripherals
E (1179) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1179) PLAY_FLASH_MP3_CONTROL: [3.1] Initialize keys on board
I (1189) PLAY_FLASH_MP3_CONTROL: [ 4 ] Set up  event listener
W (1209) PERIPH_TOUCH: _touch_init
I (1209) PLAY_FLASH_MP3_CONTROL: [4.1] Listening event from all elements of pipeline
I (1209) PLAY_FLASH_MP3_CONTROL: [4.2] Listening event from peripherals
W (1219) PLAY_FLASH_MP3_CONTROL: [ 5 ] Tap touch buttons to control music player:
W (1229) PLAY_FLASH_MP3_CONTROL:       [Play] to start, pause and resume, [Set] to stop.
W (1239) PLAY_FLASH_MP3_CONTROL:       [Vol-] or [Vol+] to adjust volume.
I (1259) PLAY_FLASH_MP3_CONTROL: [ 5.1 ] Start audio_pipeline
I (1269) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2

Done
```

- 以下为本例程 HTTP 服务器端的完整日志。

```
python -m http.server -b 192.168.5.72 8080
Serving HTTP on 192.168.5.72 port 8080 (http://192.168.5.72:8080/) ...
192.168.5.187 - - [25/Nov/2021 16:13:37] "GET /play_mp3_control.bin HTTP/1.1" 200 -
192.168.5.187 - - [25/Nov/2021 16:39:36] "GET /play_mp3_control.bin HTTP/1.1" 200 -
192.168.5.72 - - [25/Nov/2021 17:09:46] "GET /play_mp3_control.bin HTTP/1.1" 200 -
```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
