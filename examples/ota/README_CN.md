# 空中升级服务（OTA Service）例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_regular.png "中级")


## 例程简介

本例程在 ADF 框架下演示配置空中升级服务（OTA Service）更新应用（app）和数据（data）分区的例子。

此例程可以配置从 microSD 卡中升级数据分区，例程演示了从 microSD 卡升级语音提示音数据分区。此外，此例程也演示了从 HTTP 服务器获取应用二进制固件更新升级的操作。


## 环境配置

### 硬件要求

本例程可在标有绿色复选框的开发板上运行。请记住，如下面的 *配置* 一节所述，可以在 `menuconfig` 中选择开发板。

| 开发板名称 | 开始入门 | 芯片 | 兼容性 |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |


## 编译和下载

### IDF 默认分支

本例程默认 IDF 为 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程需要先配置 Wi-Fi 连接信息，通过运行 `menuconfig > OTA App Configuration` 填写 `Wi-Fi SSID` 和 `Wi-Fi Password`。

```c
menuconfig > OTA App Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

本例程需要先配置固件升级 HTTP 服务器的 URI 地址信息，通过运行 `menuconfig > OTA App Configuration` 填写 `firmware upgrade uri`。

```c
menuconfig > OTA App Configuration > (http://192.168.5.72:8080/play_mp3_control.bin) firmware upgrade uri
```

本例程提供了从 microSD 卡中升级数据分区的功能，通过运行 `menuconfig > OTA App Configuration` 填写 `data image upgrade uri`。

```c
menuconfig > OTA App Configuration > (file://sdcard/flash_tone.bin) data image upgrade uri
```

同时，需要填写所选数据分区的名称，通过运行 `menuconfig > OTA App Configuration` 填写 `data partition label`。

```c
menuconfig > OTA App Configuration > (flash_tone) data partition label
```

本例程默认不检查固件版本号，如果需要强制检查固件版本号，则需要使能 `Forece check the firmware version number when OTA start` 选项。

```c
menuconfig > OTA App Configuration > Forece check the firmware version number when OTA start
```

本例程需要准备一张 microSD 卡，放置提示音二进制文件名为 `flash_tone.bin`，用于更新提示音分区数据。

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程

### 功能和用法

- 例程需要先运行 Python 脚本，需要 python 2.7，并且开发板和 HTTP 服务器连接在同一个 Wi-Fi 网络中，Python 脚本运行 log 如下：

```c
python -m http.server -b 192.168.5.72 8080
Serving HTTP on 192.168.5.72 port 8080 (http://192.168.5.72:8080/) ...
192.168.5.187 - - [25/Nov/2021 16:13:37] "GET /play_mp3_control.bin HTTP/1.1" 200 -

```

- 例程开始运行后，将主动链接 Wi-Fi 热点，如连接成功则去先更新 `flash_tone` 分区的数据，关键部分打印如下：

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

- 随后将开始升级应用分区（app）的固件，此例程设置升级 `$ADF_PATH/examples/get-started/play_mp3_control/build` 编译的固件，关键部分打印如下：

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

本例选取完整的从启动到初始化完成的 log，示例如下：

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
I (13206) OTA_DEFAULT: Image bytes read: 2023
I (13206) OTA_DEFAULT: Image bytes read: 2312
I (13216) OTA_DEFAULT: Image bytes read: 2601
I (13216) OTA_DEFAULT: Image bytes read: 2890
I (13226) OTA_DEFAULT: Image bytes read: 3179
I (13226) OTA_DEFAULT: Image bytes read: 3468
I (13236) OTA_DEFAULT: Image bytes read: 3757
I (13236) OTA_DEFAULT: Image bytes read: 4046
I (13246) OTA_DEFAULT: Image bytes read: 4335
I (13246) OTA_DEFAULT: Image bytes read: 4624
I (13256) OTA_DEFAULT: Image bytes read: 4913
I (13256) OTA_DEFAULT: Image bytes read: 5202
I (13266) OTA_DEFAULT: Image bytes read: 5491
I (13266) OTA_DEFAULT: Image bytes read: 5780
I (13276) OTA_DEFAULT: Image bytes read: 6069
I (13276) OTA_DEFAULT: Image bytes read: 6358
I (13286) OTA_DEFAULT: Image bytes read: 6647
I (13286) OTA_DEFAULT: Image bytes read: 6936
I (13296) OTA_DEFAULT: Image bytes read: 7225
I (13296) OTA_DEFAULT: Image bytes read: 7514
I (13306) OTA_DEFAULT: Image bytes read: 7803
I (13306) OTA_DEFAULT: Image bytes read: 8092
I (13316) OTA_DEFAULT: Image bytes read: 8381
I (13316) OTA_DEFAULT: Image bytes read: 8670
I (13326) OTA_DEFAULT: Image bytes read: 8959
I (13326) OTA_DEFAULT: Image bytes read: 9248
I (13336) OTA_DEFAULT: Image bytes read: 9537
I (13336) OTA_DEFAULT: Image bytes read: 9826
I (13346) OTA_DEFAULT: Image bytes read: 10115
I (13346) OTA_DEFAULT: Image bytes read: 10404
I (13356) OTA_DEFAULT: Image bytes read: 10693
I (13356) OTA_DEFAULT: Image bytes read: 10982
I (13366) OTA_DEFAULT: Image bytes read: 11271
I (13366) OTA_DEFAULT: Image bytes read: 11560
I (13376) OTA_DEFAULT: Image bytes read: 11849
I (13376) OTA_DEFAULT: Image bytes read: 12138
I (13386) OTA_DEFAULT: Image bytes read: 12427
I (13386) OTA_DEFAULT: Image bytes read: 12716
I (13396) OTA_DEFAULT: Image bytes read: 13005
I (13406) OTA_DEFAULT: Image bytes read: 13294
I (13406) OTA_DEFAULT: Image bytes read: 13583
I (13406) OTA_DEFAULT: Image bytes read: 13872
I (13416) OTA_DEFAULT: Image bytes read: 14161
I (13426) OTA_DEFAULT: Image bytes read: 14450
I (13426) OTA_DEFAULT: Image bytes read: 14739
I (13436) OTA_DEFAULT: Image bytes read: 15028
I (13436) OTA_DEFAULT: Image bytes read: 15317
I (13446) OTA_DEFAULT: Image bytes read: 15606
I (13446) OTA_DEFAULT: Image bytes read: 15895
I (13456) OTA_DEFAULT: Image bytes read: 16184
I (13456) OTA_DEFAULT: Image bytes read: 16473
I (13466) OTA_DEFAULT: Image bytes read: 16762
I (13466) OTA_DEFAULT: Image bytes read: 17051
I (13476) OTA_DEFAULT: Image bytes read: 17340
I (13476) OTA_DEFAULT: Image bytes read: 17629
I (13486) OTA_DEFAULT: Image bytes read: 17918
I (13486) OTA_DEFAULT: Image bytes read: 18207
I (13496) OTA_DEFAULT: Image bytes read: 18496
I (13496) OTA_DEFAULT: Image bytes read: 18785
I (13506) OTA_DEFAULT: Image bytes read: 19074
I (13506) OTA_DEFAULT: Image bytes read: 19363
I (13516) OTA_DEFAULT: Image bytes read: 19652
I (13516) OTA_DEFAULT: Image bytes read: 19941
I (13526) OTA_DEFAULT: Image bytes read: 20230
I (13526) OTA_DEFAULT: Image bytes read: 20519
I (13536) OTA_DEFAULT: Image bytes read: 20808
I (13536) OTA_DEFAULT: Image bytes read: 21097
I (13546) OTA_DEFAULT: Image bytes read: 21386
I (13546) OTA_DEFAULT: Image bytes read: 21675
I (13556) OTA_DEFAULT: Image bytes read: 21964
I (13556) OTA_DEFAULT: Image bytes read: 22253
I (13566) OTA_DEFAULT: Image bytes read: 22542
I (13566) OTA_DEFAULT: Image bytes read: 22831
I (13576) OTA_DEFAULT: Image bytes read: 23120
I (13576) OTA_DEFAULT: Image bytes read: 23409
I (13586) OTA_DEFAULT: Image bytes read: 23698
I (13586) OTA_DEFAULT: Image bytes read: 23987
I (13596) OTA_DEFAULT: Image bytes read: 24276
I (13596) OTA_DEFAULT: Image bytes read: 24565
I (13606) OTA_DEFAULT: Image bytes read: 24854
I (13606) OTA_DEFAULT: Image bytes read: 25143
I (13616) OTA_DEFAULT: Image bytes read: 25432
I (13616) OTA_DEFAULT: Image bytes read: 25721
I (13626) OTA_DEFAULT: Image bytes read: 26010
I (13636) OTA_DEFAULT: Image bytes read: 26299
I (13636) OTA_DEFAULT: Image bytes read: 26588
I (13646) OTA_DEFAULT: Image bytes read: 26877
I (13646) OTA_DEFAULT: Image bytes read: 27166
I (13656) OTA_DEFAULT: Image bytes read: 27455
I (13656) OTA_DEFAULT: Image bytes read: 27744
I (13666) OTA_DEFAULT: Image bytes read: 28033
I (13666) OTA_DEFAULT: Image bytes read: 28322
I (13676) OTA_DEFAULT: Image bytes read: 28611
I (13676) OTA_DEFAULT: Image bytes read: 28900
I (13686) OTA_DEFAULT: Image bytes read: 29189
I (13686) OTA_DEFAULT: Image bytes read: 29478
I (13696) OTA_DEFAULT: Image bytes read: 29767
I (13696) OTA_DEFAULT: Image bytes read: 30056
I (13706) OTA_DEFAULT: Image bytes read: 30345
I (13706) OTA_DEFAULT: Image bytes read: 30634
I (13716) OTA_DEFAULT: Image bytes read: 30923
I (13716) OTA_DEFAULT: Image bytes read: 31212
I (13726) OTA_DEFAULT: Image bytes read: 31501
I (13726) OTA_DEFAULT: Image bytes read: 31790
I (13736) OTA_DEFAULT: Image bytes read: 32079
I (13736) OTA_DEFAULT: Image bytes read: 32368
I (13746) OTA_DEFAULT: Image bytes read: 32657
I (13746) OTA_DEFAULT: Image bytes read: 32946
I (13756) OTA_DEFAULT: Image bytes read: 33235
I (13756) OTA_DEFAULT: Image bytes read: 33524
I (13766) OTA_DEFAULT: Image bytes read: 33813
I (13766) OTA_DEFAULT: Image bytes read: 34102
I (13776) OTA_DEFAULT: Image bytes read: 34391
I (13776) OTA_DEFAULT: Image bytes read: 34680
I (13786) OTA_DEFAULT: Image bytes read: 34969
I (13786) OTA_DEFAULT: Image bytes read: 35258
I (13796) OTA_DEFAULT: Image bytes read: 35547
I (13796) OTA_DEFAULT: Image bytes read: 35836
I (13806) OTA_DEFAULT: Image bytes read: 36125
I (13806) OTA_DEFAULT: Image bytes read: 36414
I (13816) OTA_DEFAULT: Image bytes read: 36703
I (13816) OTA_DEFAULT: Image bytes read: 36992
I (13826) OTA_DEFAULT: Image bytes read: 37281
I (13826) OTA_DEFAULT: Image bytes read: 37570
I (13836) OTA_DEFAULT: Image bytes read: 37859
I (13836) OTA_DEFAULT: Image bytes read: 38148
I (13846) OTA_DEFAULT: Image bytes read: 38437
I (13856) OTA_DEFAULT: Image bytes read: 38726
I (13856) OTA_DEFAULT: Image bytes read: 39015
I (13866) OTA_DEFAULT: Image bytes read: 39304
I (13866) OTA_DEFAULT: Image bytes read: 39593
I (13876) OTA_DEFAULT: Image bytes read: 39882
I (13876) OTA_DEFAULT: Image bytes read: 40171
I (13886) OTA_DEFAULT: Image bytes read: 40460
I (13886) OTA_DEFAULT: Image bytes read: 40749
I (13896) OTA_DEFAULT: Image bytes read: 41038
I (13896) OTA_DEFAULT: Image bytes read: 41327
I (13906) OTA_DEFAULT: Image bytes read: 41616
I (13906) OTA_DEFAULT: Image bytes read: 41905
I (13916) OTA_DEFAULT: Image bytes read: 42194
I (13916) OTA_DEFAULT: Image bytes read: 42483
I (13926) OTA_DEFAULT: Image bytes read: 42772
I (13926) OTA_DEFAULT: Image bytes read: 43061
I (13936) OTA_DEFAULT: Image bytes read: 43350
I (13936) OTA_DEFAULT: Image bytes read: 43639
I (13946) OTA_DEFAULT: Image bytes read: 43928
I (13946) OTA_DEFAULT: Image bytes read: 44217
I (13956) OTA_DEFAULT: Image bytes read: 44506
I (13956) OTA_DEFAULT: Image bytes read: 44795
I (13966) OTA_DEFAULT: Image bytes read: 45084
I (13966) OTA_DEFAULT: Image bytes read: 45373
I (13976) OTA_DEFAULT: Image bytes read: 45662
I (13976) OTA_DEFAULT: Image bytes read: 45951
I (13986) OTA_DEFAULT: Image bytes read: 46240
I (13986) OTA_DEFAULT: Image bytes read: 46529
I (13996) OTA_DEFAULT: Image bytes read: 46818
I (13996) OTA_DEFAULT: Image bytes read: 47107
I (14006) OTA_DEFAULT: Image bytes read: 47396
I (14006) OTA_DEFAULT: Image bytes read: 47685
I (14016) OTA_DEFAULT: Image bytes read: 47974
I (14026) OTA_DEFAULT: Image bytes read: 48263
I (14026) OTA_DEFAULT: Image bytes read: 48552
I (14026) OTA_DEFAULT: Image bytes read: 48841
I (14036) OTA_DEFAULT: Image bytes read: 49130
I (14036) OTA_DEFAULT: Image bytes read: 49419
I (14046) OTA_DEFAULT: Image bytes read: 49708
I (14046) OTA_DEFAULT: Image bytes read: 49997
I (14056) OTA_DEFAULT: Image bytes read: 50286
I (14066) OTA_DEFAULT: Image bytes read: 50575
I (14066) OTA_DEFAULT: Image bytes read: 50864
I (14076) OTA_DEFAULT: Image bytes read: 51153
I (14076) OTA_DEFAULT: Image bytes read: 51442
I (14086) OTA_DEFAULT: Image bytes read: 51731
I (14086) OTA_DEFAULT: Image bytes read: 52020
I (14096) OTA_DEFAULT: Image bytes read: 52309
I (14096) OTA_DEFAULT: Image bytes read: 52598
I (14106) OTA_DEFAULT: Image bytes read: 52887
I (14106) OTA_DEFAULT: Image bytes read: 53176
I (14116) OTA_DEFAULT: Image bytes read: 53465
I (14116) OTA_DEFAULT: Image bytes read: 53754
I (14126) OTA_DEFAULT: Image bytes read: 54043
I (14126) OTA_DEFAULT: Image bytes read: 54332
I (14136) OTA_DEFAULT: Image bytes read: 54621
I (14136) OTA_DEFAULT: Image bytes read: 54910
I (14146) OTA_DEFAULT: Image bytes read: 55199
I (14146) OTA_DEFAULT: Image bytes read: 55488
I (14156) OTA_DEFAULT: Image bytes read: 55777
I (14156) OTA_DEFAULT: Image bytes read: 56066
I (14166) OTA_DEFAULT: Image bytes read: 56355
I (14166) OTA_DEFAULT: Image bytes read: 56644
I (14176) OTA_DEFAULT: Image bytes read: 56933
I (14176) OTA_DEFAULT: Image bytes read: 57222
I (14186) OTA_DEFAULT: Image bytes read: 57511
I (14186) OTA_DEFAULT: Image bytes read: 57800
I (14196) OTA_DEFAULT: Image bytes read: 58089
I (14196) OTA_DEFAULT: Image bytes read: 58378
I (14206) OTA_DEFAULT: Image bytes read: 58667
I (14206) OTA_DEFAULT: Image bytes read: 58956
I (14216) OTA_DEFAULT: Image bytes read: 59245
I (14216) OTA_DEFAULT: Image bytes read: 59534
I (14226) OTA_DEFAULT: Image bytes read: 59823
I (14226) OTA_DEFAULT: Image bytes read: 60112
I (14236) OTA_DEFAULT: Image bytes read: 60401
I (14236) OTA_DEFAULT: Image bytes read: 60690
I (14246) OTA_DEFAULT: Image bytes read: 60979
I (14246) OTA_DEFAULT: Image bytes read: 61268
I (14256) OTA_DEFAULT: Image bytes read: 61557
I (14256) OTA_DEFAULT: Image bytes read: 61846
I (14266) OTA_DEFAULT: Image bytes read: 62135
I (14276) OTA_DEFAULT: Image bytes read: 62424
I (14276) OTA_DEFAULT: Image bytes read: 62713
I (14286) OTA_DEFAULT: Image bytes read: 63002
I (14286) OTA_DEFAULT: Image bytes read: 63291
I (14296) OTA_DEFAULT: Image bytes read: 63580
I (14296) OTA_DEFAULT: Image bytes read: 63869
I (14306) OTA_DEFAULT: Image bytes read: 64158
I (14306) OTA_DEFAULT: Image bytes read: 64447
I (14316) OTA_DEFAULT: Image bytes read: 64736
I (14316) OTA_DEFAULT: Image bytes read: 65025
I (14326) OTA_DEFAULT: Image bytes read: 65314
I (14326) OTA_DEFAULT: Image bytes read: 65603
I (14336) OTA_DEFAULT: Image bytes read: 65892
I (14336) OTA_DEFAULT: Image bytes read: 66181
I (14346) OTA_DEFAULT: Image bytes read: 66470
I (14346) OTA_DEFAULT: Image bytes read: 66759
I (14356) OTA_DEFAULT: Image bytes read: 67048
I (14356) OTA_DEFAULT: Image bytes read: 67337
I (14366) OTA_DEFAULT: Image bytes read: 67626
I (14366) OTA_DEFAULT: Image bytes read: 67915
I (14376) OTA_DEFAULT: Image bytes read: 68204
I (14376) OTA_DEFAULT: Image bytes read: 68493
I (14386) OTA_DEFAULT: Image bytes read: 68782
I (14386) OTA_DEFAULT: Image bytes read: 69071
I (14396) OTA_DEFAULT: Image bytes read: 69360
I (14396) OTA_DEFAULT: Image bytes read: 69649
I (14406) OTA_DEFAULT: Image bytes read: 69938
I (14406) OTA_DEFAULT: Image bytes read: 70227
I (14416) OTA_DEFAULT: Image bytes read: 70516
I (14416) OTA_DEFAULT: Image bytes read: 70805
I (14426) OTA_DEFAULT: Image bytes read: 71094
I (14426) OTA_DEFAULT: Image bytes read: 71383
I (14436) OTA_DEFAULT: Image bytes read: 71672
I (14436) OTA_DEFAULT: Image bytes read: 71961
I (14446) OTA_DEFAULT: Image bytes read: 72250
I (14446) OTA_DEFAULT: Image bytes read: 72539
I (14456) OTA_DEFAULT: Image bytes read: 72828
I (14456) OTA_DEFAULT: Image bytes read: 73117
I (14466) OTA_DEFAULT: Image bytes read: 73406
I (14466) OTA_DEFAULT: Image bytes read: 73695
I (14476) OTA_DEFAULT: Image bytes read: 73984
I (14486) OTA_DEFAULT: Image bytes read: 74273
I (14486) OTA_DEFAULT: Image bytes read: 74562
I (14496) OTA_DEFAULT: Image bytes read: 74851
I (14496) OTA_DEFAULT: Image bytes read: 75140
I (14506) OTA_DEFAULT: Image bytes read: 75429
I (14506) OTA_DEFAULT: Image bytes read: 75718
I (14516) OTA_DEFAULT: Image bytes read: 76007
I (14516) OTA_DEFAULT: Image bytes read: 76296
I (14526) OTA_DEFAULT: Image bytes read: 76585
I (14526) OTA_DEFAULT: Image bytes read: 76874
I (14536) OTA_DEFAULT: Image bytes read: 77163
I (14536) OTA_DEFAULT: Image bytes read: 77452
I (14546) OTA_DEFAULT: Image bytes read: 77741
I (14546) OTA_DEFAULT: Image bytes read: 78030
I (14556) OTA_DEFAULT: Image bytes read: 78319
I (14556) OTA_DEFAULT: Image bytes read: 78608
I (14566) OTA_DEFAULT: Image bytes read: 78897
I (14566) OTA_DEFAULT: Image bytes read: 79186
I (14576) OTA_DEFAULT: Image bytes read: 79475
I (14576) OTA_DEFAULT: Image bytes read: 79764
I (14586) OTA_DEFAULT: Image bytes read: 80053
I (14586) OTA_DEFAULT: Image bytes read: 80342
I (14596) OTA_DEFAULT: Image bytes read: 80631
I (14596) OTA_DEFAULT: Image bytes read: 80920
I (14606) OTA_DEFAULT: Image bytes read: 81209
I (14606) OTA_DEFAULT: Image bytes read: 81498
I (14616) OTA_DEFAULT: Image bytes read: 81787
I (14616) OTA_DEFAULT: Image bytes read: 82076
I (14626) OTA_DEFAULT: Image bytes read: 82365
I (14626) OTA_DEFAULT: Image bytes read: 82654
I (14636) OTA_DEFAULT: Image bytes read: 82943
I (14636) OTA_DEFAULT: Image bytes read: 83232
I (14646) OTA_DEFAULT: Image bytes read: 83521
I (14646) OTA_DEFAULT: Image bytes read: 83810
I (14656) OTA_DEFAULT: Image bytes read: 84099
I (14656) OTA_DEFAULT: Image bytes read: 84388
I (14666) OTA_DEFAULT: Image bytes read: 84677
I (14666) OTA_DEFAULT: Image bytes read: 84966
I (14676) OTA_DEFAULT: Image bytes read: 85255
I (14686) OTA_DEFAULT: Image bytes read: 85544
I (14686) OTA_DEFAULT: Image bytes read: 85833
I (14686) OTA_DEFAULT: Image bytes read: 86122
I (14696) OTA_DEFAULT: Image bytes read: 86411
I (14706) OTA_DEFAULT: Image bytes read: 86700
I (14706) OTA_DEFAULT: Image bytes read: 86989
I (14716) OTA_DEFAULT: Image bytes read: 87278
I (14716) OTA_DEFAULT: Image bytes read: 87567
I (14726) OTA_DEFAULT: Image bytes read: 87856
I (14726) OTA_DEFAULT: Image bytes read: 88145
I (14736) OTA_DEFAULT: Image bytes read: 88434
I (14736) OTA_DEFAULT: Image bytes read: 88723
I (14746) OTA_DEFAULT: Image bytes read: 89012
I (14746) OTA_DEFAULT: Image bytes read: 89301
I (14756) OTA_DEFAULT: Image bytes read: 89590
I (14756) OTA_DEFAULT: Image bytes read: 89879
I (14766) OTA_DEFAULT: Image bytes read: 90168
I (14766) OTA_DEFAULT: Image bytes read: 90457
I (14776) OTA_DEFAULT: Image bytes read: 90746
I (14776) OTA_DEFAULT: Image bytes read: 91035
I (14786) OTA_DEFAULT: Image bytes read: 91324
I (14786) OTA_DEFAULT: Image bytes read: 91613
I (14796) OTA_DEFAULT: Image bytes read: 91902
I (14796) OTA_DEFAULT: Image bytes read: 92191
I (14806) OTA_DEFAULT: Image bytes read: 92480
I (14806) OTA_DEFAULT: Image bytes read: 92769
I (14816) OTA_DEFAULT: Image bytes read: 93058
I (14816) OTA_DEFAULT: Image bytes read: 93347
I (14826) OTA_DEFAULT: Image bytes read: 93636
I (14826) OTA_DEFAULT: Image bytes read: 93925
I (14836) OTA_DEFAULT: Image bytes read: 94214
I (14836) OTA_DEFAULT: Image bytes read: 94503
I (14846) OTA_DEFAULT: Image bytes read: 94792
I (14846) OTA_DEFAULT: Image bytes read: 95081
I (14856) OTA_DEFAULT: Image bytes read: 95370
I (14856) OTA_DEFAULT: Image bytes read: 95659
I (14866) OTA_DEFAULT: Image bytes read: 95948
I (14866) OTA_DEFAULT: Image bytes read: 96237
I (14876) OTA_DEFAULT: Image bytes read: 96526
I (14876) OTA_DEFAULT: Image bytes read: 96815
I (14886) OTA_DEFAULT: Image bytes read: 97104
I (14896) OTA_DEFAULT: Image bytes read: 97393
I (14896) OTA_DEFAULT: Image bytes read: 97682
I (14906) OTA_DEFAULT: Image bytes read: 97971
I (14906) OTA_DEFAULT: Image bytes read: 98260
I (14916) OTA_DEFAULT: Image bytes read: 98549
I (14916) OTA_DEFAULT: Image bytes read: 98838
I (14926) OTA_DEFAULT: Image bytes read: 99127
I (14926) OTA_DEFAULT: Image bytes read: 99416
I (14936) OTA_DEFAULT: Image bytes read: 99705
I (14936) OTA_DEFAULT: Image bytes read: 99994
I (14946) OTA_DEFAULT: Image bytes read: 100283
I (14946) OTA_DEFAULT: Image bytes read: 100572
I (14956) OTA_DEFAULT: Image bytes read: 100861
I (14956) OTA_DEFAULT: Image bytes read: 101150
I (14966) OTA_DEFAULT: Image bytes read: 101439
I (14966) OTA_DEFAULT: Image bytes read: 101728
I (14976) OTA_DEFAULT: Image bytes read: 102017
I (14976) OTA_DEFAULT: Image bytes read: 102306
I (14986) OTA_DEFAULT: Image bytes read: 102595
I (14986) OTA_DEFAULT: Image bytes read: 102884
I (14996) OTA_DEFAULT: Image bytes read: 103173
I (14996) OTA_DEFAULT: Image bytes read: 103462
I (15006) OTA_DEFAULT: Image bytes read: 103751
I (15006) OTA_DEFAULT: Image bytes read: 104040
I (15016) OTA_DEFAULT: Image bytes read: 104329
I (15016) OTA_DEFAULT: Image bytes read: 104618
I (15026) OTA_DEFAULT: Image bytes read: 104907
I (15026) OTA_DEFAULT: Image bytes read: 105196
I (15036) OTA_DEFAULT: Image bytes read: 105485
I (15046) OTA_DEFAULT: Image bytes read: 105774
I (15046) OTA_DEFAULT: Image bytes read: 106063
I (15056) OTA_DEFAULT: Image bytes read: 106352
I (15056) OTA_DEFAULT: Image bytes read: 106641
I (15066) OTA_DEFAULT: Image bytes read: 106930
I (15066) OTA_DEFAULT: Image bytes read: 107219
I (15076) OTA_DEFAULT: Image bytes read: 107508
I (15076) OTA_DEFAULT: Image bytes read: 107797
I (15086) OTA_DEFAULT: Image bytes read: 108086
I (15086) OTA_DEFAULT: Image bytes read: 108375
I (15096) OTA_DEFAULT: Image bytes read: 108664
I (15096) OTA_DEFAULT: Image bytes read: 108953
I (15106) OTA_DEFAULT: Image bytes read: 109242
I (15106) OTA_DEFAULT: Image bytes read: 109531
I (15116) OTA_DEFAULT: Image bytes read: 109820
I (15116) OTA_DEFAULT: Image bytes read: 110109
I (15126) OTA_DEFAULT: Image bytes read: 110398
I (15126) OTA_DEFAULT: Image bytes read: 110687
I (15136) OTA_DEFAULT: Image bytes read: 110976
I (15136) OTA_DEFAULT: Image bytes read: 111265
I (15146) OTA_DEFAULT: Image bytes read: 111554
I (15146) OTA_DEFAULT: Image bytes read: 111843
I (15156) OTA_DEFAULT: Image bytes read: 112132
I (15156) OTA_DEFAULT: Image bytes read: 112421
I (15166) OTA_DEFAULT: Image bytes read: 112710
I (15176) OTA_DEFAULT: Image bytes read: 112999
I (15176) OTA_DEFAULT: Image bytes read: 113288
I (15186) OTA_DEFAULT: Image bytes read: 113577
I (15186) OTA_DEFAULT: Image bytes read: 113866
I (15196) OTA_DEFAULT: Image bytes read: 114155
I (15196) OTA_DEFAULT: Image bytes read: 114444
I (15206) OTA_DEFAULT: Image bytes read: 114733
I (15206) OTA_DEFAULT: Image bytes read: 115022
I (15216) OTA_DEFAULT: Image bytes read: 115311
I (15216) OTA_DEFAULT: Image bytes read: 115600
I (15226) OTA_DEFAULT: Image bytes read: 115889
I (15226) OTA_DEFAULT: Image bytes read: 116178
I (15236) OTA_DEFAULT: Image bytes read: 116467
I (15236) OTA_DEFAULT: Image bytes read: 116756
I (15246) OTA_DEFAULT: Image bytes read: 117045
I (15246) OTA_DEFAULT: Image bytes read: 117334
I (15256) OTA_DEFAULT: Image bytes read: 117623
I (15256) OTA_DEFAULT: Image bytes read: 117912
I (15266) OTA_DEFAULT: Image bytes read: 118201
I (15266) OTA_DEFAULT: Image bytes read: 118490
I (15276) OTA_DEFAULT: Image bytes read: 118779
I (15276) OTA_DEFAULT: Image bytes read: 119068
I (15286) OTA_DEFAULT: Image bytes read: 119357
I (15286) OTA_DEFAULT: Image bytes read: 119646
I (15296) OTA_DEFAULT: Image bytes read: 119935
I (15306) OTA_DEFAULT: Image bytes read: 120224
I (15306) OTA_DEFAULT: Image bytes read: 120513
I (15316) OTA_DEFAULT: Image bytes read: 120802
I (15316) OTA_DEFAULT: Image bytes read: 121091
I (15326) OTA_DEFAULT: Image bytes read: 121380
I (15326) OTA_DEFAULT: Image bytes read: 121669
I (15336) OTA_DEFAULT: Image bytes read: 121958
I (15336) OTA_DEFAULT: Image bytes read: 122247
I (15346) OTA_DEFAULT: Image bytes read: 122536
I (15346) OTA_DEFAULT: Image bytes read: 122825
I (15356) OTA_DEFAULT: Image bytes read: 123114
I (15356) OTA_DEFAULT: Image bytes read: 123403
I (15366) OTA_DEFAULT: Image bytes read: 123692
I (15366) OTA_DEFAULT: Image bytes read: 123981
I (15376) OTA_DEFAULT: Image bytes read: 124270
I (15376) OTA_DEFAULT: Image bytes read: 124559
I (15386) OTA_DEFAULT: Image bytes read: 124848
I (15386) OTA_DEFAULT: Image bytes read: 125137
I (15396) OTA_DEFAULT: Image bytes read: 125426
I (15396) OTA_DEFAULT: Image bytes read: 125715
I (15406) OTA_DEFAULT: Image bytes read: 126004
I (15406) OTA_DEFAULT: Image bytes read: 126293
I (15416) OTA_DEFAULT: Image bytes read: 126582
I (15426) OTA_DEFAULT: Image bytes read: 126871
I (15426) OTA_DEFAULT: Image bytes read: 127160
I (15436) OTA_DEFAULT: Image bytes read: 127449
I (15436) OTA_DEFAULT: Image bytes read: 127738
I (15446) OTA_DEFAULT: Image bytes read: 128027
I (15446) OTA_DEFAULT: Image bytes read: 128316
I (15456) OTA_DEFAULT: Image bytes read: 128605
I (15456) OTA_DEFAULT: Image bytes read: 128894
I (15466) OTA_DEFAULT: Image bytes read: 129183
I (15466) OTA_DEFAULT: Image bytes read: 129472
I (15476) OTA_DEFAULT: Image bytes read: 129761
I (15476) OTA_DEFAULT: Image bytes read: 130050
I (15486) OTA_DEFAULT: Image bytes read: 130339
I (15486) OTA_DEFAULT: Image bytes read: 130628
I (15496) OTA_DEFAULT: Image bytes read: 130917
I (15496) OTA_DEFAULT: Image bytes read: 131206
I (15506) OTA_DEFAULT: Image bytes read: 131495
I (15506) OTA_DEFAULT: Image bytes read: 131784
I (15516) OTA_DEFAULT: Image bytes read: 132073
I (15516) OTA_DEFAULT: Image bytes read: 132362
I (15526) OTA_DEFAULT: Image bytes read: 132651
I (15526) OTA_DEFAULT: Image bytes read: 132940
I (15536) OTA_DEFAULT: Image bytes read: 133229
I (15546) OTA_DEFAULT: Image bytes read: 133518
I (15546) OTA_DEFAULT: Image bytes read: 133807
I (15556) OTA_DEFAULT: Image bytes read: 134096
I (15556) OTA_DEFAULT: Image bytes read: 134385
I (15566) OTA_DEFAULT: Image bytes read: 134674
I (15566) OTA_DEFAULT: Image bytes read: 134963
I (15576) OTA_DEFAULT: Image bytes read: 135252
I (15576) OTA_DEFAULT: Image bytes read: 135541
I (15586) OTA_DEFAULT: Image bytes read: 135830
I (15586) OTA_DEFAULT: Image bytes read: 136119
I (15596) OTA_DEFAULT: Image bytes read: 136408
I (15596) OTA_DEFAULT: Image bytes read: 136697
I (15606) OTA_DEFAULT: Image bytes read: 136986
I (15606) OTA_DEFAULT: Image bytes read: 137275
I (15616) OTA_DEFAULT: Image bytes read: 137564
I (15616) OTA_DEFAULT: Image bytes read: 137853
I (15626) OTA_DEFAULT: Image bytes read: 138142
I (15626) OTA_DEFAULT: Image bytes read: 138431
I (15636) OTA_DEFAULT: Image bytes read: 138720
I (15636) OTA_DEFAULT: Image bytes read: 139009
I (15646) OTA_DEFAULT: Image bytes read: 139298
I (15646) OTA_DEFAULT: Image bytes read: 139587
I (15656) OTA_DEFAULT: Image bytes read: 139876
I (15656) OTA_DEFAULT: Image bytes read: 140165
I (15666) OTA_DEFAULT: Image bytes read: 140454
I (15676) OTA_DEFAULT: Image bytes read: 140743
I (15676) OTA_DEFAULT: Image bytes read: 141032
I (15686) OTA_DEFAULT: Image bytes read: 141321
I (15686) OTA_DEFAULT: Image bytes read: 141610
I (15696) OTA_DEFAULT: Image bytes read: 141899
I (15696) OTA_DEFAULT: Image bytes read: 142188
I (15706) OTA_DEFAULT: Image bytes read: 142477
I (15706) OTA_DEFAULT: Image bytes read: 142766
I (15716) OTA_DEFAULT: Image bytes read: 143055
I (15716) OTA_DEFAULT: Image bytes read: 143344
I (15726) OTA_DEFAULT: Image bytes read: 143633
I (15726) OTA_DEFAULT: Image bytes read: 143922
I (15736) OTA_DEFAULT: Image bytes read: 144211
I (15736) OTA_DEFAULT: Image bytes read: 144500
I (15746) OTA_DEFAULT: Image bytes read: 144789
I (15746) OTA_DEFAULT: Image bytes read: 145078
I (15756) OTA_DEFAULT: Image bytes read: 145367
I (15756) OTA_DEFAULT: Image bytes read: 145656
I (15766) OTA_DEFAULT: Image bytes read: 145945
I (15766) OTA_DEFAULT: Image bytes read: 146234
I (15776) OTA_DEFAULT: Image bytes read: 146523
I (15776) OTA_DEFAULT: Image bytes read: 146812
I (15786) OTA_DEFAULT: Image bytes read: 147101
I (15786) OTA_DEFAULT: Image bytes read: 147390
I (15796) OTA_DEFAULT: Image bytes read: 147679
I (15806) OTA_DEFAULT: Image bytes read: 147968
I (15806) OTA_DEFAULT: Image bytes read: 148257
I (15816) OTA_DEFAULT: Image bytes read: 148546
I (15816) OTA_DEFAULT: Image bytes read: 148835
I (15826) OTA_DEFAULT: Image bytes read: 149124
I (15826) OTA_DEFAULT: Image bytes read: 149413
I (15836) OTA_DEFAULT: Image bytes read: 149702
I (15836) OTA_DEFAULT: Image bytes read: 149991
I (15846) OTA_DEFAULT: Image bytes read: 150280
I (15846) OTA_DEFAULT: Image bytes read: 150569
I (15856) OTA_DEFAULT: Image bytes read: 150858
I (15856) OTA_DEFAULT: Image bytes read: 151147
I (15866) OTA_DEFAULT: Image bytes read: 151436
I (15866) OTA_DEFAULT: Image bytes read: 151725
I (15876) OTA_DEFAULT: Image bytes read: 152014
I (15876) OTA_DEFAULT: Image bytes read: 152303
I (15886) OTA_DEFAULT: Image bytes read: 152592
I (15886) OTA_DEFAULT: Image bytes read: 152881
I (15896) OTA_DEFAULT: Image bytes read: 153170
I (15896) OTA_DEFAULT: Image bytes read: 153459
I (15906) OTA_DEFAULT: Image bytes read: 153748
I (15906) OTA_DEFAULT: Image bytes read: 154037
I (15916) OTA_DEFAULT: Image bytes read: 154326
I (15926) OTA_DEFAULT: Image bytes read: 154615
I (15926) OTA_DEFAULT: Image bytes read: 154904
I (15936) OTA_DEFAULT: Image bytes read: 155193
I (15936) OTA_DEFAULT: Image bytes read: 155482
I (15946) OTA_DEFAULT: Image bytes read: 155771
I (15946) OTA_DEFAULT: Image bytes read: 156060
I (15956) OTA_DEFAULT: Image bytes read: 156349
I (15956) OTA_DEFAULT: Image bytes read: 156638
I (15966) OTA_DEFAULT: Image bytes read: 156927
I (15966) OTA_DEFAULT: Image bytes read: 157216
I (15976) OTA_DEFAULT: Image bytes read: 157505
I (15976) OTA_DEFAULT: Image bytes read: 157794
I (15986) OTA_DEFAULT: Image bytes read: 158083
I (15986) OTA_DEFAULT: Image bytes read: 158372
I (15996) OTA_DEFAULT: Image bytes read: 158661
I (15996) OTA_DEFAULT: Image bytes read: 158950
I (16006) OTA_DEFAULT: Image bytes read: 159239
I (16006) OTA_DEFAULT: Image bytes read: 159528
I (16016) OTA_DEFAULT: Image bytes read: 159817
I (16016) OTA_DEFAULT: Image bytes read: 160106
I (16026) OTA_DEFAULT: Image bytes read: 160395
I (16036) OTA_DEFAULT: Image bytes read: 160684
I (16036) OTA_DEFAULT: Image bytes read: 160973
I (16036) OTA_DEFAULT: Image bytes read: 161262
I (16046) OTA_DEFAULT: Image bytes read: 161551
I (16056) OTA_DEFAULT: Image bytes read: 161840
I (16056) OTA_DEFAULT: Image bytes read: 162129
I (16066) OTA_DEFAULT: Image bytes read: 162418
I (16066) OTA_DEFAULT: Image bytes read: 162707
I (16076) OTA_DEFAULT: Image bytes read: 162996
I (16076) OTA_DEFAULT: Image bytes read: 163285
I (16086) OTA_DEFAULT: Image bytes read: 163574
I (16086) OTA_DEFAULT: Image bytes read: 163863
I (16096) OTA_DEFAULT: Image bytes read: 164152
I (16096) OTA_DEFAULT: Image bytes read: 164441
I (16106) OTA_DEFAULT: Image bytes read: 164730
I (16106) OTA_DEFAULT: Image bytes read: 165019
I (16116) OTA_DEFAULT: Image bytes read: 165308
I (16116) OTA_DEFAULT: Image bytes read: 165597
I (16126) OTA_DEFAULT: Image bytes read: 165886
I (16126) OTA_DEFAULT: Image bytes read: 166175
I (16136) OTA_DEFAULT: Image bytes read: 166464
I (16136) OTA_DEFAULT: Image bytes read: 166753
I (16146) OTA_DEFAULT: Image bytes read: 167042
I (16146) OTA_DEFAULT: Image bytes read: 167331
I (16156) OTA_DEFAULT: Image bytes read: 167620
I (16166) OTA_DEFAULT: Image bytes read: 167909
I (16166) OTA_DEFAULT: Image bytes read: 168198
I (16176) OTA_DEFAULT: Image bytes read: 168487
I (16176) OTA_DEFAULT: Image bytes read: 168776
I (16186) OTA_DEFAULT: Image bytes read: 169065
I (16186) OTA_DEFAULT: Image bytes read: 169354
I (16196) OTA_DEFAULT: Image bytes read: 169643
I (16196) OTA_DEFAULT: Image bytes read: 169932
I (16206) OTA_DEFAULT: Image bytes read: 170221
I (16206) OTA_DEFAULT: Image bytes read: 170510
I (16216) OTA_DEFAULT: Image bytes read: 170799
I (16216) OTA_DEFAULT: Image bytes read: 171088
I (16226) OTA_DEFAULT: Image bytes read: 171377
I (16226) OTA_DEFAULT: Image bytes read: 171666
I (16236) OTA_DEFAULT: Image bytes read: 171955
I (16236) OTA_DEFAULT: Image bytes read: 172244
I (16246) OTA_DEFAULT: Image bytes read: 172533
I (16246) OTA_DEFAULT: Image bytes read: 172822
I (16256) OTA_DEFAULT: Image bytes read: 173111
I (16256) OTA_DEFAULT: Image bytes read: 173400
I (16266) OTA_DEFAULT: Image bytes read: 173689
I (16266) OTA_DEFAULT: Image bytes read: 173978
I (16276) OTA_DEFAULT: Image bytes read: 174267
I (16276) OTA_DEFAULT: Image bytes read: 174556
I (16286) OTA_DEFAULT: Image bytes read: 174845
I (16296) OTA_DEFAULT: Image bytes read: 175134
I (16296) OTA_DEFAULT: Image bytes read: 175423
I (16306) OTA_DEFAULT: Image bytes read: 175712
I (16306) OTA_DEFAULT: Image bytes read: 176001
I (16316) OTA_DEFAULT: Image bytes read: 176290
I (16316) OTA_DEFAULT: Image bytes read: 176579
I (16326) OTA_DEFAULT: Image bytes read: 176868
I (16326) OTA_DEFAULT: Image bytes read: 177157
I (16336) OTA_DEFAULT: Image bytes read: 177446
I (16336) OTA_DEFAULT: Image bytes read: 177735
I (16346) OTA_DEFAULT: Image bytes read: 178024
I (16346) OTA_DEFAULT: Image bytes read: 178313
I (16356) OTA_DEFAULT: Image bytes read: 178602
I (16356) OTA_DEFAULT: Image bytes read: 178891
I (16366) OTA_DEFAULT: Image bytes read: 179180
I (16366) OTA_DEFAULT: Image bytes read: 179469
I (16376) OTA_DEFAULT: Image bytes read: 179758
I (16376) OTA_DEFAULT: Image bytes read: 180047
I (16386) OTA_DEFAULT: Image bytes read: 180336
I (16386) OTA_DEFAULT: Image bytes read: 180625
I (16396) OTA_DEFAULT: Image bytes read: 180914
I (16396) OTA_DEFAULT: Image bytes read: 181203
I (16406) OTA_DEFAULT: Image bytes read: 181492
I (16406) OTA_DEFAULT: Image bytes read: 181781
I (16416) OTA_DEFAULT: Image bytes read: 182070
I (16426) OTA_DEFAULT: Image bytes read: 182359
I (16426) OTA_DEFAULT: Image bytes read: 182648
I (16436) OTA_DEFAULT: Image bytes read: 182937
I (16436) OTA_DEFAULT: Image bytes read: 183226
I (16446) OTA_DEFAULT: Image bytes read: 183515
I (16446) OTA_DEFAULT: Image bytes read: 183804
I (16456) OTA_DEFAULT: Image bytes read: 184093
I (16456) OTA_DEFAULT: Image bytes read: 184382
I (16466) OTA_DEFAULT: Image bytes read: 184671
I (16466) OTA_DEFAULT: Image bytes read: 184960
I (16476) OTA_DEFAULT: Image bytes read: 185249
I (16476) OTA_DEFAULT: Image bytes read: 185538
I (16486) OTA_DEFAULT: Image bytes read: 185827
I (16486) OTA_DEFAULT: Image bytes read: 186116
I (16496) OTA_DEFAULT: Image bytes read: 186405
I (16496) OTA_DEFAULT: Image bytes read: 186694
I (16506) OTA_DEFAULT: Image bytes read: 186983
I (16506) OTA_DEFAULT: Image bytes read: 187272
I (16516) OTA_DEFAULT: Image bytes read: 187561
I (16516) OTA_DEFAULT: Image bytes read: 187850
I (16526) OTA_DEFAULT: Image bytes read: 188139
I (16526) OTA_DEFAULT: Image bytes read: 188428
I (16536) OTA_DEFAULT: Image bytes read: 188717
I (16536) OTA_DEFAULT: Image bytes read: 189006
I (16546) OTA_DEFAULT: Image bytes read: 189295
I (16556) OTA_DEFAULT: Image bytes read: 189584
I (16556) OTA_DEFAULT: Image bytes read: 189873
I (16566) OTA_DEFAULT: Image bytes read: 190162
I (16566) OTA_DEFAULT: Image bytes read: 190451
I (16576) OTA_DEFAULT: Image bytes read: 190740
I (16576) OTA_DEFAULT: Image bytes read: 191029
I (16586) OTA_DEFAULT: Image bytes read: 191318
I (16586) OTA_DEFAULT: Image bytes read: 191607
I (16596) OTA_DEFAULT: Image bytes read: 191896
I (16596) OTA_DEFAULT: Image bytes read: 192185
I (16606) OTA_DEFAULT: Image bytes read: 192474
I (16606) OTA_DEFAULT: Image bytes read: 192763
I (16616) OTA_DEFAULT: Image bytes read: 193052
I (16616) OTA_DEFAULT: Image bytes read: 193341
I (16626) OTA_DEFAULT: Image bytes read: 193630
I (16626) OTA_DEFAULT: Image bytes read: 193919
I (16636) OTA_DEFAULT: Image bytes read: 194208
I (16636) OTA_DEFAULT: Image bytes read: 194497
I (16646) OTA_DEFAULT: Image bytes read: 194786
I (16646) OTA_DEFAULT: Image bytes read: 195075
I (16656) OTA_DEFAULT: Image bytes read: 195364
I (16666) OTA_DEFAULT: Image bytes read: 195653
I (16666) OTA_DEFAULT: Image bytes read: 195942
I (16676) OTA_DEFAULT: Image bytes read: 196231
I (16676) OTA_DEFAULT: Image bytes read: 196520
I (16686) OTA_DEFAULT: Image bytes read: 196809
I (16686) OTA_DEFAULT: Image bytes read: 197098
I (16696) OTA_DEFAULT: Image bytes read: 197387
I (16696) OTA_DEFAULT: Image bytes read: 197676
I (16706) OTA_DEFAULT: Image bytes read: 197965
I (16706) OTA_DEFAULT: Image bytes read: 198254
I (16716) OTA_DEFAULT: Image bytes read: 198543
I (16716) OTA_DEFAULT: Image bytes read: 198832
I (16726) OTA_DEFAULT: Image bytes read: 199121
I (16726) OTA_DEFAULT: Image bytes read: 199410
I (16736) OTA_DEFAULT: Image bytes read: 199699
I (16736) OTA_DEFAULT: Image bytes read: 199988
I (16746) OTA_DEFAULT: Image bytes read: 200277
I (16746) OTA_DEFAULT: Image bytes read: 200566
I (16756) OTA_DEFAULT: Image bytes read: 200855
I (16756) OTA_DEFAULT: Image bytes read: 201144
I (16766) OTA_DEFAULT: Image bytes read: 201433
I (16766) OTA_DEFAULT: Image bytes read: 201722
I (16776) OTA_DEFAULT: Image bytes read: 202011
I (16776) OTA_DEFAULT: Image bytes read: 202300
I (16786) OTA_DEFAULT: Image bytes read: 202589
I (16796) OTA_DEFAULT: Image bytes read: 202878
I (16796) OTA_DEFAULT: Image bytes read: 203167
I (16806) OTA_DEFAULT: Image bytes read: 203456
I (16806) OTA_DEFAULT: Image bytes read: 203745
I (16816) OTA_DEFAULT: Image bytes read: 204034
I (16816) OTA_DEFAULT: Image bytes read: 204323
I (16826) OTA_DEFAULT: Image bytes read: 204612
I (16826) OTA_DEFAULT: Image bytes read: 204901
I (16836) OTA_DEFAULT: Image bytes read: 205190
I (16836) OTA_DEFAULT: Image bytes read: 205479
I (16846) OTA_DEFAULT: Image bytes read: 205768
I (16846) OTA_DEFAULT: Image bytes read: 206057
I (16856) OTA_DEFAULT: Image bytes read: 206346
I (16856) OTA_DEFAULT: Image bytes read: 206635
I (16866) OTA_DEFAULT: Image bytes read: 206924
I (16866) OTA_DEFAULT: Image bytes read: 207213
I (16876) OTA_DEFAULT: Image bytes read: 207502
I (16876) OTA_DEFAULT: Image bytes read: 207791
I (16886) OTA_DEFAULT: Image bytes read: 208080
I (16886) OTA_DEFAULT: Image bytes read: 208369
I (16896) OTA_DEFAULT: Image bytes read: 208658
I (16896) OTA_DEFAULT: Image bytes read: 208947
I (16906) OTA_DEFAULT: Image bytes read: 209236
I (16906) OTA_DEFAULT: Image bytes read: 209525
I (16916) OTA_DEFAULT: Image bytes read: 209814
I (16926) OTA_DEFAULT: Image bytes read: 210103
I (16926) OTA_DEFAULT: Image bytes read: 210392
I (16936) OTA_DEFAULT: Image bytes read: 210681
I (16936) OTA_DEFAULT: Image bytes read: 210970
I (16946) OTA_DEFAULT: Image bytes read: 211259
I (16946) OTA_DEFAULT: Image bytes read: 211548
I (16956) OTA_DEFAULT: Image bytes read: 211837
I (16956) OTA_DEFAULT: Image bytes read: 212126
I (16966) OTA_DEFAULT: Image bytes read: 212415
I (16966) OTA_DEFAULT: Image bytes read: 212704
I (16976) OTA_DEFAULT: Image bytes read: 212993
I (16976) OTA_DEFAULT: Image bytes read: 213282
I (16986) OTA_DEFAULT: Image bytes read: 213571
I (16986) OTA_DEFAULT: Image bytes read: 213860
I (16996) OTA_DEFAULT: Image bytes read: 214149
I (16996) OTA_DEFAULT: Image bytes read: 214438
I (17006) OTA_DEFAULT: Image bytes read: 214727
I (17006) OTA_DEFAULT: Image bytes read: 215016
I (17016) OTA_DEFAULT: Image bytes read: 215305
I (17016) OTA_DEFAULT: Image bytes read: 215594
I (17026) OTA_DEFAULT: Image bytes read: 215883
I (17026) OTA_DEFAULT: Image bytes read: 216172
I (17036) OTA_DEFAULT: Image bytes read: 216461
I (17036) OTA_DEFAULT: Image bytes read: 216750
I (17046) OTA_DEFAULT: Image bytes read: 217039
I (17056) OTA_DEFAULT: Image bytes read: 217328
I (17056) OTA_DEFAULT: Image bytes read: 217617
I (17066) OTA_DEFAULT: Image bytes read: 217906
I (17066) OTA_DEFAULT: Image bytes read: 218195
I (17076) OTA_DEFAULT: Image bytes read: 218484
I (17076) OTA_DEFAULT: Image bytes read: 218773
I (17086) OTA_DEFAULT: Image bytes read: 219062
I (17086) OTA_DEFAULT: Image bytes read: 219351
I (17096) OTA_DEFAULT: Image bytes read: 219640
I (17096) OTA_DEFAULT: Image bytes read: 219929
I (17106) OTA_DEFAULT: Image bytes read: 220218
I (17106) OTA_DEFAULT: Image bytes read: 220507
I (17116) OTA_DEFAULT: Image bytes read: 220796
I (17116) OTA_DEFAULT: Image bytes read: 221085
I (17126) OTA_DEFAULT: Image bytes read: 221374
I (17126) OTA_DEFAULT: Image bytes read: 221663
I (17136) OTA_DEFAULT: Image bytes read: 221952
I (17136) OTA_DEFAULT: Image bytes read: 222241
I (17146) OTA_DEFAULT: Image bytes read: 222530
I (17146) OTA_DEFAULT: Image bytes read: 222819
I (17156) OTA_DEFAULT: Image bytes read: 223108
I (17156) OTA_DEFAULT: Image bytes read: 223397
I (17166) OTA_DEFAULT: Image bytes read: 223686
I (17176) OTA_DEFAULT: Image bytes read: 223975
I (17176) OTA_DEFAULT: Image bytes read: 224264
I (17186) OTA_DEFAULT: Image bytes read: 224553
I (17186) OTA_DEFAULT: Image bytes read: 224842
I (17196) OTA_DEFAULT: Image bytes read: 225131
I (17196) OTA_DEFAULT: Image bytes read: 225420
I (17206) OTA_DEFAULT: Image bytes read: 225709
I (17206) OTA_DEFAULT: Image bytes read: 225998
I (17216) OTA_DEFAULT: Image bytes read: 226287
I (17216) OTA_DEFAULT: Image bytes read: 226576
I (17226) OTA_DEFAULT: Image bytes read: 226865
I (17226) OTA_DEFAULT: Image bytes read: 227154
I (17236) OTA_DEFAULT: Image bytes read: 227443
I (17236) OTA_DEFAULT: Image bytes read: 227732
I (17246) OTA_DEFAULT: Image bytes read: 228021
I (17246) OTA_DEFAULT: Image bytes read: 228310
I (17256) OTA_DEFAULT: Image bytes read: 228599
I (17256) OTA_DEFAULT: Image bytes read: 228888
I (17266) OTA_DEFAULT: Image bytes read: 229177
I (17266) OTA_DEFAULT: Image bytes read: 229466
I (17276) OTA_DEFAULT: Image bytes read: 229755
I (17286) OTA_DEFAULT: Image bytes read: 230044
I (17286) OTA_DEFAULT: Image bytes read: 230333
I (17286) OTA_DEFAULT: Image bytes read: 230622
I (17296) OTA_DEFAULT: Image bytes read: 230911
I (17306) OTA_DEFAULT: Image bytes read: 231200
I (17306) OTA_DEFAULT: Image bytes read: 231489
I (17316) OTA_DEFAULT: Image bytes read: 231778
I (17316) OTA_DEFAULT: Image bytes read: 232067
I (17326) OTA_DEFAULT: Image bytes read: 232356
I (17326) OTA_DEFAULT: Image bytes read: 232645
I (17336) OTA_DEFAULT: Image bytes read: 232934
I (17336) OTA_DEFAULT: Image bytes read: 233223
I (17346) OTA_DEFAULT: Image bytes read: 233512
I (17346) OTA_DEFAULT: Image bytes read: 233801
I (17356) OTA_DEFAULT: Image bytes read: 234090
I (17356) OTA_DEFAULT: Image bytes read: 234379
I (17366) OTA_DEFAULT: Image bytes read: 234668
I (17366) OTA_DEFAULT: Image bytes read: 234957
I (17376) OTA_DEFAULT: Image bytes read: 235246
I (17376) OTA_DEFAULT: Image bytes read: 235535
I (17386) OTA_DEFAULT: Image bytes read: 235824
I (17386) OTA_DEFAULT: Image bytes read: 236113
I (17396) OTA_DEFAULT: Image bytes read: 236402
I (17396) OTA_DEFAULT: Image bytes read: 236691
I (17406) OTA_DEFAULT: Image bytes read: 236980
I (17416) OTA_DEFAULT: Image bytes read: 237269
I (17416) OTA_DEFAULT: Image bytes read: 237558
I (17426) OTA_DEFAULT: Image bytes read: 237847
I (17426) OTA_DEFAULT: Image bytes read: 238136
I (17436) OTA_DEFAULT: Image bytes read: 238425
I (17436) OTA_DEFAULT: Image bytes read: 238714
I (17446) OTA_DEFAULT: Image bytes read: 239003
I (17446) OTA_DEFAULT: Image bytes read: 239292
I (17456) OTA_DEFAULT: Image bytes read: 239581
I (17456) OTA_DEFAULT: Image bytes read: 239870
I (17466) OTA_DEFAULT: Image bytes read: 240159
I (17466) OTA_DEFAULT: Image bytes read: 240448
I (17476) OTA_DEFAULT: Image bytes read: 240737
I (17476) OTA_DEFAULT: Image bytes read: 241026
I (17486) OTA_DEFAULT: Image bytes read: 241315
I (17486) OTA_DEFAULT: Image bytes read: 241604
I (17496) OTA_DEFAULT: Image bytes read: 241893
I (17496) OTA_DEFAULT: Image bytes read: 242182
I (17506) OTA_DEFAULT: Image bytes read: 242471
I (17506) OTA_DEFAULT: Image bytes read: 242760
I (17516) OTA_DEFAULT: Image bytes read: 243049
I (17516) OTA_DEFAULT: Image bytes read: 243338
I (17526) OTA_DEFAULT: Image bytes read: 243627
I (17526) OTA_DEFAULT: Image bytes read: 243916
I (17536) OTA_DEFAULT: Image bytes read: 244205
I (17546) OTA_DEFAULT: Image bytes read: 244494
I (17546) OTA_DEFAULT: Image bytes read: 244783
I (17556) OTA_DEFAULT: Image bytes read: 245072
I (17556) OTA_DEFAULT: Image bytes read: 245361
I (17566) OTA_DEFAULT: Image bytes read: 245650
I (17566) OTA_DEFAULT: Image bytes read: 245939
I (17576) OTA_DEFAULT: Image bytes read: 246228
I (17576) OTA_DEFAULT: Image bytes read: 246517
I (17586) OTA_DEFAULT: Image bytes read: 246806
I (17586) OTA_DEFAULT: Image bytes read: 247095
I (17596) OTA_DEFAULT: Image bytes read: 247384
I (17596) OTA_DEFAULT: Image bytes read: 247673
I (17606) OTA_DEFAULT: Image bytes read: 247962
I (17606) OTA_DEFAULT: Image bytes read: 248251
I (17616) OTA_DEFAULT: Image bytes read: 248540
I (17616) OTA_DEFAULT: Image bytes read: 248829
I (17626) OTA_DEFAULT: Image bytes read: 249118
I (17626) OTA_DEFAULT: Image bytes read: 249407
I (17636) OTA_DEFAULT: Image bytes read: 249696
I (17636) OTA_DEFAULT: Image bytes read: 249985
I (17646) OTA_DEFAULT: Image bytes read: 250274
I (17646) OTA_DEFAULT: Image bytes read: 250563
I (17656) OTA_DEFAULT: Image bytes read: 250852
I (17656) OTA_DEFAULT: Image bytes read: 251141
I (17666) OTA_DEFAULT: Image bytes read: 251430
I (17676) OTA_DEFAULT: Image bytes read: 251719
I (17676) OTA_DEFAULT: Image bytes read: 252008
I (17686) OTA_DEFAULT: Image bytes read: 252297
I (17686) OTA_DEFAULT: Image bytes read: 252586
I (17696) OTA_DEFAULT: Image bytes read: 252875
I (17696) OTA_DEFAULT: Image bytes read: 253164
I (17706) OTA_DEFAULT: Image bytes read: 253453
I (17706) OTA_DEFAULT: Image bytes read: 253742
I (17716) OTA_DEFAULT: Image bytes read: 254031
I (17716) OTA_DEFAULT: Image bytes read: 254320
I (17726) OTA_DEFAULT: Image bytes read: 254609
I (17726) OTA_DEFAULT: Image bytes read: 254898
I (17736) OTA_DEFAULT: Image bytes read: 255187
I (17736) OTA_DEFAULT: Image bytes read: 255476
I (17746) OTA_DEFAULT: Image bytes read: 255765
I (17746) OTA_DEFAULT: Image bytes read: 256054
I (17756) OTA_DEFAULT: Image bytes read: 256343
I (17756) OTA_DEFAULT: Image bytes read: 256632
I (17766) OTA_DEFAULT: Image bytes read: 256921
I (17766) OTA_DEFAULT: Image bytes read: 257210
I (17776) OTA_DEFAULT: Image bytes read: 257499
I (17776) OTA_DEFAULT: Image bytes read: 257788
I (17786) OTA_DEFAULT: Image bytes read: 258077
I (17786) OTA_DEFAULT: Image bytes read: 258366
I (17796) OTA_DEFAULT: Image bytes read: 258655
I (17806) OTA_DEFAULT: Image bytes read: 258944
I (17806) OTA_DEFAULT: Image bytes read: 259233
I (17816) OTA_DEFAULT: Image bytes read: 259522
I (17816) OTA_DEFAULT: Image bytes read: 259811
I (17826) OTA_DEFAULT: Image bytes read: 260100
I (17826) OTA_DEFAULT: Image bytes read: 260389
I (17836) OTA_DEFAULT: Image bytes read: 260678
I (17836) OTA_DEFAULT: Image bytes read: 260967
I (17846) OTA_DEFAULT: Image bytes read: 261256
I (17846) OTA_DEFAULT: Image bytes read: 261545
I (17856) OTA_DEFAULT: Image bytes read: 261834
I (17856) OTA_DEFAULT: Image bytes read: 262123
I (17866) OTA_DEFAULT: Image bytes read: 262412
I (17866) OTA_DEFAULT: Image bytes read: 262701
I (17876) OTA_DEFAULT: Image bytes read: 262990
I (17876) OTA_DEFAULT: Image bytes read: 263279
I (17886) OTA_DEFAULT: Image bytes read: 263568
I (17886) OTA_DEFAULT: Image bytes read: 263857
I (17896) OTA_DEFAULT: Image bytes read: 264146
I (17896) OTA_DEFAULT: Image bytes read: 264435
I (17906) OTA_DEFAULT: Image bytes read: 264724
I (17906) OTA_DEFAULT: Image bytes read: 265013
I (17916) OTA_DEFAULT: Image bytes read: 265302
I (17926) OTA_DEFAULT: Image bytes read: 265591
I (17926) OTA_DEFAULT: Image bytes read: 265880
I (17936) OTA_DEFAULT: Image bytes read: 266169
I (17936) OTA_DEFAULT: Image bytes read: 266458
I (17946) OTA_DEFAULT: Image bytes read: 266747
I (17946) OTA_DEFAULT: Image bytes read: 267036
I (17956) OTA_DEFAULT: Image bytes read: 267325
I (17956) OTA_DEFAULT: Image bytes read: 267614
I (17966) OTA_DEFAULT: Image bytes read: 267903
I (17966) OTA_DEFAULT: Image bytes read: 268192
I (17976) OTA_DEFAULT: Image bytes read: 268481
I (17976) OTA_DEFAULT: Image bytes read: 268770
I (17986) OTA_DEFAULT: Image bytes read: 269059
I (17986) OTA_DEFAULT: Image bytes read: 269348
I (17996) OTA_DEFAULT: Image bytes read: 269637
I (17996) OTA_DEFAULT: Image bytes read: 269926
I (18006) OTA_DEFAULT: Image bytes read: 270215
I (18006) OTA_DEFAULT: Image bytes read: 270504
I (18016) OTA_DEFAULT: Image bytes read: 270793
I (18016) OTA_DEFAULT: Image bytes read: 271082
I (18026) OTA_DEFAULT: Image bytes read: 271371
I (18026) OTA_DEFAULT: Image bytes read: 271660
I (18036) OTA_DEFAULT: Image bytes read: 271949
I (18036) OTA_DEFAULT: Image bytes read: 272238
I (18046) OTA_DEFAULT: Image bytes read: 272527
I (18056) OTA_DEFAULT: Image bytes read: 272816
I (18056) OTA_DEFAULT: Image bytes read: 273105
I (18066) OTA_DEFAULT: Image bytes read: 273394
I (18066) OTA_DEFAULT: Image bytes read: 273683
I (18076) OTA_DEFAULT: Image bytes read: 273972
I (18076) OTA_DEFAULT: Image bytes read: 274261
I (18086) OTA_DEFAULT: Image bytes read: 274550
I (18086) OTA_DEFAULT: Image bytes read: 274839
I (18096) OTA_DEFAULT: Image bytes read: 275128
I (18096) OTA_DEFAULT: Image bytes read: 275417
I (18106) OTA_DEFAULT: Image bytes read: 275706
I (18106) OTA_DEFAULT: Image bytes read: 275995
I (18116) OTA_DEFAULT: Image bytes read: 276284
I (18116) OTA_DEFAULT: Image bytes read: 276573
I (18126) OTA_DEFAULT: Image bytes read: 276862
I (18126) OTA_DEFAULT: Image bytes read: 277151
I (18136) OTA_DEFAULT: Image bytes read: 277440
I (18136) OTA_DEFAULT: Image bytes read: 277729
I (18146) OTA_DEFAULT: Image bytes read: 278018
I (18146) OTA_DEFAULT: Image bytes read: 278307
I (18156) OTA_DEFAULT: Image bytes read: 278596
I (18156) OTA_DEFAULT: Image bytes read: 278885
I (18166) OTA_DEFAULT: Image bytes read: 279174
I (18176) OTA_DEFAULT: Image bytes read: 279463
I (18176) OTA_DEFAULT: Image bytes read: 279752
I (18186) OTA_DEFAULT: Image bytes read: 280041
I (18186) OTA_DEFAULT: Image bytes read: 280330
I (18196) OTA_DEFAULT: Image bytes read: 280619
I (18196) OTA_DEFAULT: Image bytes read: 280908
I (18206) OTA_DEFAULT: Image bytes read: 281197
I (18206) OTA_DEFAULT: Image bytes read: 281486
I (18216) OTA_DEFAULT: Image bytes read: 281775
I (18216) OTA_DEFAULT: Image bytes read: 282064
I (18226) OTA_DEFAULT: Image bytes read: 282353
I (18226) OTA_DEFAULT: Image bytes read: 282642
I (18236) OTA_DEFAULT: Image bytes read: 282931
I (18236) OTA_DEFAULT: Image bytes read: 283220
I (18246) OTA_DEFAULT: Image bytes read: 283509
I (18246) OTA_DEFAULT: Image bytes read: 283798
I (18256) OTA_DEFAULT: Image bytes read: 284087
I (18256) OTA_DEFAULT: Image bytes read: 284376
I (18266) OTA_DEFAULT: Image bytes read: 284665
I (18266) OTA_DEFAULT: Image bytes read: 284954
I (18276) OTA_DEFAULT: Image bytes read: 285243
I (18276) OTA_DEFAULT: Image bytes read: 285532
I (18286) OTA_DEFAULT: Image bytes read: 285821
I (18296) OTA_DEFAULT: Image bytes read: 286110
I (18296) OTA_DEFAULT: Image bytes read: 286399
I (18306) OTA_DEFAULT: Image bytes read: 286688
I (18306) OTA_DEFAULT: Image bytes read: 286977
I (18316) OTA_DEFAULT: Image bytes read: 287266
I (18316) OTA_DEFAULT: Image bytes read: 287555
I (18326) OTA_DEFAULT: Image bytes read: 287844
I (18326) OTA_DEFAULT: Image bytes read: 288133
I (18336) OTA_DEFAULT: Image bytes read: 288422
I (18336) OTA_DEFAULT: Image bytes read: 288711
I (18346) OTA_DEFAULT: Image bytes read: 289000
I (18346) OTA_DEFAULT: Image bytes read: 289289
I (18356) OTA_DEFAULT: Image bytes read: 289578
I (18356) OTA_DEFAULT: Image bytes read: 289867
I (18366) OTA_DEFAULT: Image bytes read: 290156
I (18366) OTA_DEFAULT: Image bytes read: 290445
I (18376) OTA_DEFAULT: Image bytes read: 290734
I (18376) OTA_DEFAULT: Image bytes read: 291023
I (18386) OTA_DEFAULT: Image bytes read: 291312
I (18386) OTA_DEFAULT: Image bytes read: 291601
I (18396) OTA_DEFAULT: Image bytes read: 291890
I (18396) OTA_DEFAULT: Image bytes read: 292179
I (18406) OTA_DEFAULT: Image bytes read: 292468
I (18406) OTA_DEFAULT: Image bytes read: 292757
I (18416) OTA_DEFAULT: Image bytes read: 293046
I (18426) OTA_DEFAULT: Image bytes read: 293335
I (18426) OTA_DEFAULT: Image bytes read: 293624
I (18436) OTA_DEFAULT: Image bytes read: 293913
I (18436) OTA_DEFAULT: Image bytes read: 294202
I (18446) OTA_DEFAULT: Image bytes read: 294491
I (18446) OTA_DEFAULT: Image bytes read: 294780
I (18456) OTA_DEFAULT: Image bytes read: 295069
I (18456) OTA_DEFAULT: Image bytes read: 295358
I (18466) OTA_DEFAULT: Image bytes read: 295647
I (18466) OTA_DEFAULT: Image bytes read: 295936
I (18476) OTA_DEFAULT: Image bytes read: 296225
I (18476) OTA_DEFAULT: Image bytes read: 296514
I (18486) OTA_DEFAULT: Image bytes read: 296803
I (18486) OTA_DEFAULT: Image bytes read: 297092
I (18496) OTA_DEFAULT: Image bytes read: 297381
I (18496) OTA_DEFAULT: Image bytes read: 297670
I (18506) OTA_DEFAULT: Image bytes read: 297959
I (18506) OTA_DEFAULT: Image bytes read: 298248
I (18516) OTA_DEFAULT: Image bytes read: 298537
I (18516) OTA_DEFAULT: Image bytes read: 298826
I (18526) OTA_DEFAULT: Image bytes read: 299115
I (18526) OTA_DEFAULT: Image bytes read: 299404
I (18536) OTA_DEFAULT: Image bytes read: 299693
I (18536) OTA_DEFAULT: Image bytes read: 299982
I (18546) OTA_DEFAULT: Image bytes read: 300271
I (18556) OTA_DEFAULT: Image bytes read: 300560
I (18556) OTA_DEFAULT: Image bytes read: 300849
I (18566) OTA_DEFAULT: Image bytes read: 301138
I (18566) OTA_DEFAULT: Image bytes read: 301427
I (18576) OTA_DEFAULT: Image bytes read: 301716
I (18576) OTA_DEFAULT: Image bytes read: 302005
I (18586) OTA_DEFAULT: Image bytes read: 302294
I (18586) OTA_DEFAULT: Image bytes read: 302583
I (18596) OTA_DEFAULT: Image bytes read: 302872
I (18596) OTA_DEFAULT: Image bytes read: 303161
I (18606) OTA_DEFAULT: Image bytes read: 303450
I (18606) OTA_DEFAULT: Image bytes read: 303739
I (18616) OTA_DEFAULT: Image bytes read: 304028
I (18616) OTA_DEFAULT: Image bytes read: 304317
I (18626) OTA_DEFAULT: Image bytes read: 304606
I (18626) OTA_DEFAULT: Image bytes read: 304895
I (18636) OTA_DEFAULT: Image bytes read: 305184
I (18636) OTA_DEFAULT: Image bytes read: 305473
I (18646) OTA_DEFAULT: Image bytes read: 305762
I (18646) OTA_DEFAULT: Image bytes read: 306051
I (18656) OTA_DEFAULT: Image bytes read: 306340
I (18666) OTA_DEFAULT: Image bytes read: 306629
I (18666) OTA_DEFAULT: Image bytes read: 306918
I (18676) OTA_DEFAULT: Image bytes read: 307207
I (18676) OTA_DEFAULT: Image bytes read: 307496
I (18686) OTA_DEFAULT: Image bytes read: 307785
I (18686) OTA_DEFAULT: Image bytes read: 308074
I (18696) OTA_DEFAULT: Image bytes read: 308363
I (18696) OTA_DEFAULT: Image bytes read: 308652
I (18706) OTA_DEFAULT: Image bytes read: 308941
I (18706) OTA_DEFAULT: Image bytes read: 309230
I (18716) OTA_DEFAULT: Image bytes read: 309519
I (18716) OTA_DEFAULT: Image bytes read: 309808
I (18726) OTA_DEFAULT: Image bytes read: 310097
I (18726) OTA_DEFAULT: Image bytes read: 310386
I (18736) OTA_DEFAULT: Image bytes read: 310675
I (18736) OTA_DEFAULT: Image bytes read: 310964
I (18746) OTA_DEFAULT: Image bytes read: 311253
I (18746) OTA_DEFAULT: Image bytes read: 311542
I (18756) OTA_DEFAULT: Image bytes read: 311831
I (18756) OTA_DEFAULT: Image bytes read: 312120
I (18766) OTA_DEFAULT: Image bytes read: 312409
I (18766) OTA_DEFAULT: Image bytes read: 312698
I (18776) OTA_DEFAULT: Image bytes read: 312987
I (18776) OTA_DEFAULT: Image bytes read: 313276
I (18786) OTA_DEFAULT: Image bytes read: 313565
I (18796) OTA_DEFAULT: Image bytes read: 313854
I (18796) OTA_DEFAULT: Image bytes read: 314143
I (18806) OTA_DEFAULT: Image bytes read: 314432
I (18806) OTA_DEFAULT: Image bytes read: 314721
I (18816) OTA_DEFAULT: Image bytes read: 315010
I (18816) OTA_DEFAULT: Image bytes read: 315299
I (18826) OTA_DEFAULT: Image bytes read: 315588
I (18826) OTA_DEFAULT: Image bytes read: 315877
I (18836) OTA_DEFAULT: Image bytes read: 316166
I (18836) OTA_DEFAULT: Image bytes read: 316455
I (18846) OTA_DEFAULT: Image bytes read: 316744
I (18846) OTA_DEFAULT: Image bytes read: 317033
I (18856) OTA_DEFAULT: Image bytes read: 317322
I (18856) OTA_DEFAULT: Image bytes read: 317611
I (18866) OTA_DEFAULT: Image bytes read: 317900
I (18866) OTA_DEFAULT: Image bytes read: 318189
I (18876) OTA_DEFAULT: Image bytes read: 318478
I (18876) OTA_DEFAULT: Image bytes read: 318767
I (18886) OTA_DEFAULT: Image bytes read: 319056
I (18886) OTA_DEFAULT: Image bytes read: 319345
I (18896) OTA_DEFAULT: Image bytes read: 319634
I (18896) OTA_DEFAULT: Image bytes read: 319923
I (18906) OTA_DEFAULT: Image bytes read: 320212
I (18906) OTA_DEFAULT: Image bytes read: 320501
I (18916) OTA_DEFAULT: Image bytes read: 320790
I (18926) OTA_DEFAULT: Image bytes read: 321079
I (18926) OTA_DEFAULT: Image bytes read: 321368
I (18936) OTA_DEFAULT: Image bytes read: 321657
I (18936) OTA_DEFAULT: Image bytes read: 321946
I (18946) OTA_DEFAULT: Image bytes read: 322235
I (18946) OTA_DEFAULT: Image bytes read: 322524
I (18956) OTA_DEFAULT: Image bytes read: 322813
I (18956) OTA_DEFAULT: Image bytes read: 323102
I (18966) OTA_DEFAULT: Image bytes read: 323391
I (18966) OTA_DEFAULT: Image bytes read: 323680
I (18976) OTA_DEFAULT: Image bytes read: 323969
I (18976) OTA_DEFAULT: Image bytes read: 324258
I (18986) OTA_DEFAULT: Image bytes read: 324547
I (18986) OTA_DEFAULT: Image bytes read: 324836
I (18996) OTA_DEFAULT: Image bytes read: 325125
I (18996) OTA_DEFAULT: Image bytes read: 325414
I (19006) OTA_DEFAULT: Image bytes read: 325703
I (19006) OTA_DEFAULT: Image bytes read: 325992
I (19016) OTA_DEFAULT: Image bytes read: 326281
I (19016) OTA_DEFAULT: Image bytes read: 326570
I (19026) OTA_DEFAULT: Image bytes read: 326859
I (19026) OTA_DEFAULT: Image bytes read: 327148
I (19036) OTA_DEFAULT: Image bytes read: 327437
I (19046) OTA_DEFAULT: Image bytes read: 327726
I (19046) OTA_DEFAULT: Image bytes read: 328015
I (19056) OTA_DEFAULT: Image bytes read: 328304
I (19056) OTA_DEFAULT: Image bytes read: 328593
I (19066) OTA_DEFAULT: Image bytes read: 328882
I (19066) OTA_DEFAULT: Image bytes read: 329171
I (19076) OTA_DEFAULT: Image bytes read: 329460
I (19076) OTA_DEFAULT: Image bytes read: 329749
I (19086) OTA_DEFAULT: Image bytes read: 330038
I (19086) OTA_DEFAULT: Image bytes read: 330327
I (19096) OTA_DEFAULT: Image bytes read: 330616
I (19096) OTA_DEFAULT: Image bytes read: 330905
I (19106) OTA_DEFAULT: Image bytes read: 331194
I (19106) OTA_DEFAULT: Image bytes read: 331483
I (19116) OTA_DEFAULT: Image bytes read: 331772
I (19116) OTA_DEFAULT: Image bytes read: 332061
I (19126) OTA_DEFAULT: Image bytes read: 332350
I (19126) OTA_DEFAULT: Image bytes read: 332639
I (19136) OTA_DEFAULT: Image bytes read: 332928
I (19136) OTA_DEFAULT: Image bytes read: 333217
I (19146) OTA_DEFAULT: Image bytes read: 333506
I (19146) OTA_DEFAULT: Image bytes read: 333795
I (19156) OTA_DEFAULT: Image bytes read: 334084
I (19166) OTA_DEFAULT: Image bytes read: 334373
I (19166) OTA_DEFAULT: Image bytes read: 334662
I (19176) OTA_DEFAULT: Image bytes read: 334951
I (19176) OTA_DEFAULT: Image bytes read: 335240
I (19186) OTA_DEFAULT: Image bytes read: 335529
I (19186) OTA_DEFAULT: Image bytes read: 335818
I (19196) OTA_DEFAULT: Image bytes read: 336107
I (19196) OTA_DEFAULT: Image bytes read: 336396
I (19206) OTA_DEFAULT: Image bytes read: 336685
I (19206) OTA_DEFAULT: Image bytes read: 336974
I (19216) OTA_DEFAULT: Image bytes read: 337263
I (19216) OTA_DEFAULT: Image bytes read: 337552
I (19226) OTA_DEFAULT: Image bytes read: 337841
I (19226) OTA_DEFAULT: Image bytes read: 338130
I (19236) OTA_DEFAULT: Image bytes read: 338419
I (19236) OTA_DEFAULT: Image bytes read: 338708
I (19246) OTA_DEFAULT: Image bytes read: 338997
I (19246) OTA_DEFAULT: Image bytes read: 339286
I (19256) OTA_DEFAULT: Image bytes read: 339575
I (19256) OTA_DEFAULT: Image bytes read: 339864
I (19266) OTA_DEFAULT: Image bytes read: 340153
I (19266) OTA_DEFAULT: Image bytes read: 340442
I (19276) OTA_DEFAULT: Image bytes read: 340731
I (19276) OTA_DEFAULT: Image bytes read: 341020
I (19286) OTA_DEFAULT: Image bytes read: 341309
I (19296) OTA_DEFAULT: Image bytes read: 341598
I (19296) OTA_DEFAULT: Image bytes read: 341887
I (19306) OTA_DEFAULT: Image bytes read: 342176
I (19306) OTA_DEFAULT: Image bytes read: 342465
I (19316) OTA_DEFAULT: Image bytes read: 342754
I (19316) OTA_DEFAULT: Image bytes read: 343043
I (19326) OTA_DEFAULT: Image bytes read: 343332
I (19326) OTA_DEFAULT: Image bytes read: 343621
I (19336) OTA_DEFAULT: Image bytes read: 343910
I (19336) OTA_DEFAULT: Image bytes read: 344199
I (19346) OTA_DEFAULT: Image bytes read: 344488
I (19346) OTA_DEFAULT: Image bytes read: 344777
I (19356) OTA_DEFAULT: Image bytes read: 345066
I (19356) OTA_DEFAULT: Image bytes read: 345355
I (19366) OTA_DEFAULT: Image bytes read: 345644
I (19366) OTA_DEFAULT: Image bytes read: 345933
I (19376) OTA_DEFAULT: Image bytes read: 346222
I (19376) OTA_DEFAULT: Image bytes read: 346511
I (19386) OTA_DEFAULT: Image bytes read: 346800
I (19386) OTA_DEFAULT: Image bytes read: 347089
I (19396) OTA_DEFAULT: Image bytes read: 347378
I (19396) OTA_DEFAULT: Image bytes read: 347667
I (19406) OTA_DEFAULT: Image bytes read: 347956
I (19406) OTA_DEFAULT: Image bytes read: 348245
I (19416) OTA_DEFAULT: Image bytes read: 348534
I (19426) OTA_DEFAULT: Image bytes read: 348823
I (19426) OTA_DEFAULT: Image bytes read: 349112
I (19436) OTA_DEFAULT: Image bytes read: 349401
I (19436) OTA_DEFAULT: Image bytes read: 349690
I (19446) OTA_DEFAULT: Image bytes read: 349979
I (19446) OTA_DEFAULT: Image bytes read: 350268
I (19456) OTA_DEFAULT: Image bytes read: 350557
I (19456) OTA_DEFAULT: Image bytes read: 350846
I (19466) OTA_DEFAULT: Image bytes read: 351135
I (19466) OTA_DEFAULT: Image bytes read: 351424
I (19476) OTA_DEFAULT: Image bytes read: 351713
I (19476) OTA_DEFAULT: Image bytes read: 352002
I (19486) OTA_DEFAULT: Image bytes read: 352291
I (19486) OTA_DEFAULT: Image bytes read: 352580
I (19496) OTA_DEFAULT: Image bytes read: 352869
I (19496) OTA_DEFAULT: Image bytes read: 353158
I (19506) OTA_DEFAULT: Image bytes read: 353447
I (19506) OTA_DEFAULT: Image bytes read: 353736
I (19516) OTA_DEFAULT: Image bytes read: 354025
I (19516) OTA_DEFAULT: Image bytes read: 354314
I (19526) OTA_DEFAULT: Image bytes read: 354603
I (19526) OTA_DEFAULT: Image bytes read: 354892
I (19536) OTA_DEFAULT: Image bytes read: 355181
I (19536) OTA_DEFAULT: Image bytes read: 355470
I (19546) OTA_DEFAULT: Image bytes read: 355759
I (19556) OTA_DEFAULT: Image bytes read: 356048
I (19556) OTA_DEFAULT: Image bytes read: 356337
I (19566) OTA_DEFAULT: Image bytes read: 356626
I (19566) OTA_DEFAULT: Image bytes read: 356915
I (19576) OTA_DEFAULT: Image bytes read: 357204
I (19576) OTA_DEFAULT: Image bytes read: 357493
I (19586) OTA_DEFAULT: Image bytes read: 357782
I (19586) OTA_DEFAULT: Image bytes read: 358071
I (19596) OTA_DEFAULT: Image bytes read: 358360
I (19596) OTA_DEFAULT: Image bytes read: 358649
I (19606) OTA_DEFAULT: Image bytes read: 358938
I (19606) OTA_DEFAULT: Image bytes read: 359227
I (19616) OTA_DEFAULT: Image bytes read: 359516
I (19616) OTA_DEFAULT: Image bytes read: 359805
I (19626) OTA_DEFAULT: Image bytes read: 360094
I (19626) OTA_DEFAULT: Image bytes read: 360383
I (19636) OTA_DEFAULT: Image bytes read: 360672
I (19636) OTA_DEFAULT: Image bytes read: 360961
I (19646) OTA_DEFAULT: Image bytes read: 361250
I (19646) OTA_DEFAULT: Image bytes read: 361539
I (19656) OTA_DEFAULT: Image bytes read: 361828
I (19656) OTA_DEFAULT: Image bytes read: 362117
I (19666) OTA_DEFAULT: Image bytes read: 362406
I (19676) OTA_DEFAULT: Image bytes read: 362695
I (19676) OTA_DEFAULT: Image bytes read: 362984
I (19686) OTA_DEFAULT: Image bytes read: 363273
I (19686) OTA_DEFAULT: Image bytes read: 363562
I (19696) OTA_DEFAULT: Image bytes read: 363851
I (19696) OTA_DEFAULT: Image bytes read: 364140
I (19706) OTA_DEFAULT: Image bytes read: 364429
I (19706) OTA_DEFAULT: Image bytes read: 364718
I (19716) OTA_DEFAULT: Image bytes read: 365007
I (19716) OTA_DEFAULT: Image bytes read: 365296
I (19726) OTA_DEFAULT: Image bytes read: 365585
I (19726) OTA_DEFAULT: Image bytes read: 365874
I (19736) OTA_DEFAULT: Image bytes read: 366163
I (19736) OTA_DEFAULT: Image bytes read: 366452
I (19746) OTA_DEFAULT: Image bytes read: 366741
I (19746) OTA_DEFAULT: Image bytes read: 367030
I (19756) OTA_DEFAULT: Image bytes read: 367319
I (19756) OTA_DEFAULT: Image bytes read: 367608
I (19766) OTA_DEFAULT: Image bytes read: 367897
I (19766) OTA_DEFAULT: Image bytes read: 368186
I (19776) OTA_DEFAULT: Image bytes read: 368475
I (19776) OTA_DEFAULT: Image bytes read: 368764
I (19786) OTA_DEFAULT: Image bytes read: 369053
I (19796) OTA_DEFAULT: Image bytes read: 369342
I (19796) OTA_DEFAULT: Image bytes read: 369631
I (19806) OTA_DEFAULT: Image bytes read: 369920
I (19806) OTA_DEFAULT: Image bytes read: 370209
I (19816) OTA_DEFAULT: Image bytes read: 370498
I (19816) OTA_DEFAULT: Image bytes read: 370787
I (19826) OTA_DEFAULT: Image bytes read: 371076
I (19826) OTA_DEFAULT: Image bytes read: 371365
I (19836) OTA_DEFAULT: Image bytes read: 371654
I (19836) OTA_DEFAULT: Image bytes read: 371943
I (19846) OTA_DEFAULT: Image bytes read: 372232
I (19846) OTA_DEFAULT: Image bytes read: 372521
I (19856) OTA_DEFAULT: Image bytes read: 372810
I (19856) OTA_DEFAULT: Image bytes read: 373099
I (19866) OTA_DEFAULT: Image bytes read: 373388
I (19866) OTA_DEFAULT: Image bytes read: 373677
I (19876) OTA_DEFAULT: Image bytes read: 373966
I (19876) OTA_DEFAULT: Image bytes read: 374255
I (19886) OTA_DEFAULT: Image bytes read: 374544
I (19886) OTA_DEFAULT: Image bytes read: 374833
I (19896) OTA_DEFAULT: Image bytes read: 375122
I (19896) OTA_DEFAULT: Image bytes read: 375411
I (19906) OTA_DEFAULT: Image bytes read: 375700
I (19916) OTA_DEFAULT: Image bytes read: 375989
I (19916) OTA_DEFAULT: Image bytes read: 376278
I (19926) OTA_DEFAULT: Image bytes read: 376567
I (19926) OTA_DEFAULT: Image bytes read: 376856
I (19936) OTA_DEFAULT: Image bytes read: 377145
I (19936) OTA_DEFAULT: Image bytes read: 377434
I (19946) OTA_DEFAULT: Image bytes read: 377723
I (19946) OTA_DEFAULT: Image bytes read: 378012
I (19956) OTA_DEFAULT: Image bytes read: 378301
I (19956) OTA_DEFAULT: Image bytes read: 378590
I (19966) OTA_DEFAULT: Image bytes read: 378879
I (19966) OTA_DEFAULT: Image bytes read: 379168
I (19976) OTA_DEFAULT: Image bytes read: 379457
I (19976) OTA_DEFAULT: Image bytes read: 379746
I (19986) OTA_DEFAULT: Image bytes read: 380035
I (19986) OTA_DEFAULT: Image bytes read: 380324
I (19996) OTA_DEFAULT: Image bytes read: 380613
I (19996) OTA_DEFAULT: Image bytes read: 380902
I (20006) OTA_DEFAULT: Image bytes read: 381191
I (20006) OTA_DEFAULT: Image bytes read: 381480
I (20016) OTA_DEFAULT: Image bytes read: 381769
I (20016) OTA_DEFAULT: Image bytes read: 382058
I (20026) OTA_DEFAULT: Image bytes read: 382347
I (20026) OTA_DEFAULT: Image bytes read: 382636
I (20036) OTA_DEFAULT: Image bytes read: 382925
I (20046) OTA_DEFAULT: Image bytes read: 383214
I (20046) OTA_DEFAULT: Image bytes read: 383503
I (20056) OTA_DEFAULT: Image bytes read: 383792
I (20056) OTA_DEFAULT: Image bytes read: 384081
I (20066) OTA_DEFAULT: Image bytes read: 384370
I (20066) OTA_DEFAULT: Image bytes read: 384659
I (20076) OTA_DEFAULT: Image bytes read: 384948
I (20076) OTA_DEFAULT: Image bytes read: 385237
I (20086) OTA_DEFAULT: Image bytes read: 385526
I (20086) OTA_DEFAULT: Image bytes read: 385815
I (20096) OTA_DEFAULT: Image bytes read: 386104
I (20096) OTA_DEFAULT: Image bytes read: 386393
I (20106) OTA_DEFAULT: Image bytes read: 386682
I (20106) OTA_DEFAULT: Image bytes read: 386971
I (20116) OTA_DEFAULT: Image bytes read: 387260
I (20116) OTA_DEFAULT: Image bytes read: 387549
I (20126) OTA_DEFAULT: Image bytes read: 387838
I (20126) OTA_DEFAULT: Image bytes read: 388127
I (20136) OTA_DEFAULT: Image bytes read: 388416
I (20136) OTA_DEFAULT: Image bytes read: 388705
I (20146) OTA_DEFAULT: Image bytes read: 388994
I (20156) OTA_DEFAULT: Image bytes read: 389283
I (20156) OTA_DEFAULT: Image bytes read: 389572
I (20156) OTA_DEFAULT: Image bytes read: 389861
I (20166) OTA_DEFAULT: Image bytes read: 390150
I (20176) OTA_DEFAULT: Image bytes read: 390439
I (20176) OTA_DEFAULT: Image bytes read: 390728
I (20186) OTA_DEFAULT: Image bytes read: 391017
I (20186) OTA_DEFAULT: Image bytes read: 391306
I (20196) OTA_DEFAULT: Image bytes read: 391595
I (20196) OTA_DEFAULT: Image bytes read: 391884
I (20206) OTA_DEFAULT: Image bytes read: 392173
I (20206) OTA_DEFAULT: Image bytes read: 392462
I (20216) OTA_DEFAULT: Image bytes read: 392751
I (20216) OTA_DEFAULT: Image bytes read: 393040
I (20226) OTA_DEFAULT: Image bytes read: 393329
I (20226) OTA_DEFAULT: Image bytes read: 393618
I (20236) OTA_DEFAULT: Image bytes read: 393907
I (20236) OTA_DEFAULT: Image bytes read: 394196
I (20246) OTA_DEFAULT: Image bytes read: 394485
I (20246) OTA_DEFAULT: Image bytes read: 394774
I (20256) OTA_DEFAULT: Image bytes read: 395063
I (20256) OTA_DEFAULT: Image bytes read: 395352
I (20266) OTA_DEFAULT: Image bytes read: 395641
I (20266) OTA_DEFAULT: Image bytes read: 395930
I (20276) OTA_DEFAULT: Image bytes read: 396219
I (20286) OTA_DEFAULT: Image bytes read: 396508
I (20286) OTA_DEFAULT: Image bytes read: 396797
I (20286) OTA_DEFAULT: Image bytes read: 397086
I (20296) OTA_DEFAULT: Image bytes read: 397375
I (20306) OTA_DEFAULT: Image bytes read: 397664
I (20306) OTA_DEFAULT: Image bytes read: 397953
I (20316) OTA_DEFAULT: Image bytes read: 398242
I (20316) OTA_DEFAULT: Image bytes read: 398531
I (20326) OTA_DEFAULT: Image bytes read: 398820
I (20326) OTA_DEFAULT: Image bytes read: 399109
I (20336) OTA_DEFAULT: Image bytes read: 399398
I (20336) OTA_DEFAULT: Image bytes read: 399687
I (20346) OTA_DEFAULT: Image bytes read: 399976
I (20346) OTA_DEFAULT: Image bytes read: 400265
I (20356) OTA_DEFAULT: Image bytes read: 400554
I (20356) OTA_DEFAULT: Image bytes read: 400843
I (20366) OTA_DEFAULT: Image bytes read: 401132
I (20366) OTA_DEFAULT: Image bytes read: 401421
I (20376) OTA_DEFAULT: Image bytes read: 401710
I (20376) OTA_DEFAULT: Image bytes read: 401999
I (20386) OTA_DEFAULT: Image bytes read: 402288
I (20386) OTA_DEFAULT: Image bytes read: 402577
I (20396) OTA_DEFAULT: Image bytes read: 402866
I (20396) OTA_DEFAULT: Image bytes read: 403155
I (20406) OTA_DEFAULT: Image bytes read: 403444
I (20406) OTA_DEFAULT: Image bytes read: 403733
I (20416) OTA_DEFAULT: Image bytes read: 404022
I (20426) OTA_DEFAULT: Image bytes read: 404311
I (20426) OTA_DEFAULT: Image bytes read: 404600
I (20436) OTA_DEFAULT: Image bytes read: 404889
I (20436) OTA_DEFAULT: Image bytes read: 405178
I (20446) OTA_DEFAULT: Image bytes read: 405467
I (20446) OTA_DEFAULT: Image bytes read: 405756
I (20456) OTA_DEFAULT: Image bytes read: 406045
I (20456) OTA_DEFAULT: Image bytes read: 406334
I (20466) OTA_DEFAULT: Image bytes read: 406623
I (20466) OTA_DEFAULT: Image bytes read: 406912
I (20476) OTA_DEFAULT: Image bytes read: 407201
I (20476) OTA_DEFAULT: Image bytes read: 407490
I (20486) OTA_DEFAULT: Image bytes read: 407779
I (20486) OTA_DEFAULT: Image bytes read: 408068
I (20496) OTA_DEFAULT: Image bytes read: 408357
I (20496) OTA_DEFAULT: Image bytes read: 408646
I (20506) OTA_DEFAULT: Image bytes read: 408935
I (20506) OTA_DEFAULT: Image bytes read: 409224
I (20516) OTA_DEFAULT: Image bytes read: 409513
I (20516) OTA_DEFAULT: Image bytes read: 409802
I (20526) OTA_DEFAULT: Image bytes read: 410091
I (20526) OTA_DEFAULT: Image bytes read: 410380
I (20536) OTA_DEFAULT: Image bytes read: 410669
I (20546) OTA_DEFAULT: Image bytes read: 410958
I (20546) OTA_DEFAULT: Image bytes read: 411247
I (20556) OTA_DEFAULT: Image bytes read: 411536
I (20556) OTA_DEFAULT: Image bytes read: 411825
I (20566) OTA_DEFAULT: Image bytes read: 412114
I (20566) OTA_DEFAULT: Image bytes read: 412403
I (20576) OTA_DEFAULT: Image bytes read: 412692
I (20576) OTA_DEFAULT: Image bytes read: 412981
I (20586) OTA_DEFAULT: Image bytes read: 413270
I (20586) OTA_DEFAULT: Image bytes read: 413559
I (20596) OTA_DEFAULT: Image bytes read: 413848
I (20596) OTA_DEFAULT: Image bytes read: 414137
I (20606) OTA_DEFAULT: Image bytes read: 414426
I (20606) OTA_DEFAULT: Image bytes read: 414715
I (20616) OTA_DEFAULT: Image bytes read: 415004
I (20616) OTA_DEFAULT: Image bytes read: 415293
I (20626) OTA_DEFAULT: Image bytes read: 415582
I (20626) OTA_DEFAULT: Image bytes read: 415871
I (20636) OTA_DEFAULT: Image bytes read: 416160
I (20636) OTA_DEFAULT: Image bytes read: 416449
I (20646) OTA_DEFAULT: Image bytes read: 416738
I (20646) OTA_DEFAULT: Image bytes read: 417027
I (20656) OTA_DEFAULT: Image bytes read: 417316
I (20656) OTA_DEFAULT: Image bytes read: 417605
I (20666) OTA_DEFAULT: Image bytes read: 417894
I (20676) OTA_DEFAULT: Image bytes read: 418183
I (20676) OTA_DEFAULT: Image bytes read: 418472
I (20686) OTA_DEFAULT: Image bytes read: 418761
I (20686) OTA_DEFAULT: Image bytes read: 419050
I (20696) OTA_DEFAULT: Image bytes read: 419339
I (20696) OTA_DEFAULT: Image bytes read: 419628
I (20706) OTA_DEFAULT: Image bytes read: 419917
I (20706) OTA_DEFAULT: Image bytes read: 420206
I (20716) OTA_DEFAULT: Image bytes read: 420495
I (20716) OTA_DEFAULT: Image bytes read: 420784
I (20726) OTA_DEFAULT: Image bytes read: 421073
I (20726) OTA_DEFAULT: Image bytes read: 421362
I (20736) OTA_DEFAULT: Image bytes read: 421651
I (20736) OTA_DEFAULT: Image bytes read: 421940
I (20746) OTA_DEFAULT: Image bytes read: 422229
I (20746) OTA_DEFAULT: Image bytes read: 422518
I (20756) OTA_DEFAULT: Image bytes read: 422807
I (20756) OTA_DEFAULT: Image bytes read: 423096
I (20766) OTA_DEFAULT: Image bytes read: 423385
I (20766) OTA_DEFAULT: Image bytes read: 423674
I (20776) OTA_DEFAULT: Image bytes read: 423963
I (20776) OTA_DEFAULT: Image bytes read: 424252
I (20786) OTA_DEFAULT: Image bytes read: 424541
I (20786) OTA_DEFAULT: Image bytes read: 424830
I (20796) OTA_DEFAULT: Image bytes read: 425119
I (20806) OTA_DEFAULT: Image bytes read: 425408
I (20806) OTA_DEFAULT: Image bytes read: 425697
I (20816) OTA_DEFAULT: Image bytes read: 425986
I (20816) OTA_DEFAULT: Image bytes read: 426275
I (20826) OTA_DEFAULT: Image bytes read: 426564
I (20826) OTA_DEFAULT: Image bytes read: 426853
I (20836) OTA_DEFAULT: Image bytes read: 427142
I (20836) OTA_DEFAULT: Image bytes read: 427431
I (20846) OTA_DEFAULT: Image bytes read: 427720
I (20846) OTA_DEFAULT: Image bytes read: 428009
I (20856) OTA_DEFAULT: Image bytes read: 428298
I (20856) OTA_DEFAULT: Image bytes read: 428587
I (20866) OTA_DEFAULT: Image bytes read: 428876
I (20866) OTA_DEFAULT: Image bytes read: 429165
I (20876) OTA_DEFAULT: Image bytes read: 429454
I (20876) OTA_DEFAULT: Image bytes read: 429743
I (20886) OTA_DEFAULT: Image bytes read: 430032
I (20886) OTA_DEFAULT: Image bytes read: 430321
I (20896) OTA_DEFAULT: Image bytes read: 430610
I (20896) OTA_DEFAULT: Image bytes read: 430899
I (20906) OTA_DEFAULT: Image bytes read: 431188
I (20906) OTA_DEFAULT: Image bytes read: 431477
I (20916) OTA_DEFAULT: Image bytes read: 431766
I (20926) OTA_DEFAULT: Image bytes read: 432055
I (20926) OTA_DEFAULT: Image bytes read: 432344
I (20936) OTA_DEFAULT: Image bytes read: 432633
I (20936) OTA_DEFAULT: Image bytes read: 432922
I (20946) OTA_DEFAULT: Image bytes read: 433211
I (20946) OTA_DEFAULT: Image bytes read: 433500
I (20956) OTA_DEFAULT: Image bytes read: 433789
I (20956) OTA_DEFAULT: Image bytes read: 434078
I (20966) OTA_DEFAULT: Image bytes read: 434367
I (20966) OTA_DEFAULT: Image bytes read: 434656
I (20976) OTA_DEFAULT: Image bytes read: 434945
I (20976) OTA_DEFAULT: Image bytes read: 435234
I (20986) OTA_DEFAULT: Image bytes read: 435523
I (20986) OTA_DEFAULT: Image bytes read: 435812
I (20996) OTA_DEFAULT: Image bytes read: 436101
I (20996) OTA_DEFAULT: Image bytes read: 436390
I (21006) OTA_DEFAULT: Image bytes read: 436679
I (21006) OTA_DEFAULT: Image bytes read: 436968
I (21016) OTA_DEFAULT: Image bytes read: 437257
I (21016) OTA_DEFAULT: Image bytes read: 437546
I (21026) OTA_DEFAULT: Image bytes read: 437835
I (21036) OTA_DEFAULT: Image bytes read: 438124
I (21036) OTA_DEFAULT: Image bytes read: 438413
I (21036) OTA_DEFAULT: Image bytes read: 438702
I (21046) OTA_DEFAULT: Image bytes read: 438991
I (21056) OTA_DEFAULT: Image bytes read: 439280
I (21056) OTA_DEFAULT: Image bytes read: 439569
I (21066) OTA_DEFAULT: Image bytes read: 439858
I (21066) OTA_DEFAULT: Image bytes read: 440147
I (21076) OTA_DEFAULT: Image bytes read: 440436
I (21076) OTA_DEFAULT: Image bytes read: 440725
I (21086) OTA_DEFAULT: Image bytes read: 441014
I (21086) OTA_DEFAULT: Image bytes read: 441303
I (21096) OTA_DEFAULT: Image bytes read: 441592
I (21096) OTA_DEFAULT: Image bytes read: 441881
I (21106) OTA_DEFAULT: Image bytes read: 442170
I (21106) OTA_DEFAULT: Image bytes read: 442459
I (21116) OTA_DEFAULT: Image bytes read: 442748
I (21116) OTA_DEFAULT: Image bytes read: 443037
I (21126) OTA_DEFAULT: Image bytes read: 443326
I (21126) OTA_DEFAULT: Image bytes read: 443615
I (21136) OTA_DEFAULT: Image bytes read: 443904
I (21136) OTA_DEFAULT: Image bytes read: 444193
I (21146) OTA_DEFAULT: Image bytes read: 444482
I (21146) OTA_DEFAULT: Image bytes read: 444771
I (21156) OTA_DEFAULT: Image bytes read: 445060
I (21166) OTA_DEFAULT: Image bytes read: 445349
I (21166) OTA_DEFAULT: Image bytes read: 445638
I (21176) OTA_DEFAULT: Image bytes read: 445927
I (21176) OTA_DEFAULT: Image bytes read: 446216
I (21186) OTA_DEFAULT: Image bytes read: 446505
I (21186) OTA_DEFAULT: Image bytes read: 446794
I (21196) OTA_DEFAULT: Image bytes read: 447083
I (21196) OTA_DEFAULT: Image bytes read: 447372
I (21206) OTA_DEFAULT: Image bytes read: 447661
I (21206) OTA_DEFAULT: Image bytes read: 447950
I (21216) OTA_DEFAULT: Image bytes read: 448239
I (21216) OTA_DEFAULT: Image bytes read: 448528
I (21226) OTA_DEFAULT: Image bytes read: 448817
I (21226) OTA_DEFAULT: Image bytes read: 449106
I (21236) OTA_DEFAULT: Image bytes read: 449395
I (21236) OTA_DEFAULT: Image bytes read: 449684
I (21246) OTA_DEFAULT: Image bytes read: 449973
I (21246) OTA_DEFAULT: Image bytes read: 450262
I (21256) OTA_DEFAULT: Image bytes read: 450551
I (21256) OTA_DEFAULT: Image bytes read: 450840
I (21266) OTA_DEFAULT: Image bytes read: 451129
I (21266) OTA_DEFAULT: Image bytes read: 451418
I (21276) OTA_DEFAULT: Image bytes read: 451707
I (21276) OTA_DEFAULT: Image bytes read: 451996
I (21286) OTA_DEFAULT: Image bytes read: 452285
I (21296) OTA_DEFAULT: Image bytes read: 452574
I (21296) OTA_DEFAULT: Image bytes read: 452863
I (21306) OTA_DEFAULT: Image bytes read: 453152
I (21306) OTA_DEFAULT: Image bytes read: 453441
I (21316) OTA_DEFAULT: Image bytes read: 453730
I (21316) OTA_DEFAULT: Image bytes read: 454019
I (21326) OTA_DEFAULT: Image bytes read: 454308
I (21326) OTA_DEFAULT: Image bytes read: 454597
I (21336) OTA_DEFAULT: Image bytes read: 454886
I (21336) OTA_DEFAULT: Image bytes read: 455175
I (21346) OTA_DEFAULT: Image bytes read: 455464
I (21346) OTA_DEFAULT: Image bytes read: 455753
I (21356) OTA_DEFAULT: Image bytes read: 456042
I (21356) OTA_DEFAULT: Image bytes read: 456331
I (21366) OTA_DEFAULT: Image bytes read: 456620
I (21366) OTA_DEFAULT: Image bytes read: 456909
I (21376) OTA_DEFAULT: Image bytes read: 457198
I (21376) OTA_DEFAULT: Image bytes read: 457487
I (21386) OTA_DEFAULT: Image bytes read: 457776
I (21386) OTA_DEFAULT: Image bytes read: 458065
I (21396) OTA_DEFAULT: Image bytes read: 458354
I (21396) OTA_DEFAULT: Image bytes read: 458643
I (21406) OTA_DEFAULT: Image bytes read: 458932
I (21406) OTA_DEFAULT: Image bytes read: 459221
I (21416) OTA_DEFAULT: Image bytes read: 459510
I (21426) OTA_DEFAULT: Image bytes read: 459799
I (21426) OTA_DEFAULT: Image bytes read: 460088
I (21436) OTA_DEFAULT: Image bytes read: 460377
I (21436) OTA_DEFAULT: Image bytes read: 460666
I (21446) OTA_DEFAULT: Image bytes read: 460955
I (21446) OTA_DEFAULT: Image bytes read: 461244
I (21456) OTA_DEFAULT: Image bytes read: 461533
I (21456) OTA_DEFAULT: Image bytes read: 461822
I (21466) OTA_DEFAULT: Image bytes read: 462111
I (21466) OTA_DEFAULT: Image bytes read: 462400
I (21476) OTA_DEFAULT: Image bytes read: 462689
I (21476) OTA_DEFAULT: Image bytes read: 462978
I (21486) OTA_DEFAULT: Image bytes read: 463267
I (21486) OTA_DEFAULT: Image bytes read: 463556
I (21496) OTA_DEFAULT: Image bytes read: 463845
I (21496) OTA_DEFAULT: Image bytes read: 464134
I (21506) OTA_DEFAULT: Image bytes read: 464423
I (21506) OTA_DEFAULT: Image bytes read: 464712
I (21516) OTA_DEFAULT: Image bytes read: 465001
I (21516) OTA_DEFAULT: Image bytes read: 465290
I (21526) OTA_DEFAULT: Image bytes read: 465579
I (21526) OTA_DEFAULT: Image bytes read: 465868
I (21536) OTA_DEFAULT: Image bytes read: 466157
I (21536) OTA_DEFAULT: Image bytes read: 466446
I (21546) OTA_DEFAULT: Image bytes read: 466735
I (21556) OTA_DEFAULT: Image bytes read: 467024
I (21556) OTA_DEFAULT: Image bytes read: 467313
I (21566) OTA_DEFAULT: Image bytes read: 467602
I (21566) OTA_DEFAULT: Image bytes read: 467891
I (21576) OTA_DEFAULT: Image bytes read: 468180
I (21576) OTA_DEFAULT: Image bytes read: 468469
I (21586) OTA_DEFAULT: Image bytes read: 468758
I (21586) OTA_DEFAULT: Image bytes read: 469047
I (21596) OTA_DEFAULT: Image bytes read: 469336
I (21596) OTA_DEFAULT: Image bytes read: 469625
I (21606) OTA_DEFAULT: Image bytes read: 469914
I (21606) OTA_DEFAULT: Image bytes read: 470203
I (21616) OTA_DEFAULT: Image bytes read: 470492
I (21616) OTA_DEFAULT: Image bytes read: 470781
I (21626) OTA_DEFAULT: Image bytes read: 471070
I (21626) OTA_DEFAULT: Image bytes read: 471359
I (21636) OTA_DEFAULT: Image bytes read: 471648
I (21636) OTA_DEFAULT: Image bytes read: 471937
I (21646) OTA_DEFAULT: Image bytes read: 472226
I (21646) OTA_DEFAULT: Image bytes read: 472515
I (21656) OTA_DEFAULT: Image bytes read: 472804
I (21656) OTA_DEFAULT: Image bytes read: 473093
I (21666) OTA_DEFAULT: Image bytes read: 473382
I (21676) OTA_DEFAULT: Image bytes read: 473671
I (21676) OTA_DEFAULT: Image bytes read: 473960
I (21686) OTA_DEFAULT: Image bytes read: 474249
I (21686) OTA_DEFAULT: Image bytes read: 474538
I (21696) OTA_DEFAULT: Image bytes read: 474827
I (21696) OTA_DEFAULT: Image bytes read: 475116
I (21706) OTA_DEFAULT: Image bytes read: 475405
I (21706) OTA_DEFAULT: Image bytes read: 475694
I (21716) OTA_DEFAULT: Image bytes read: 475983
I (21716) OTA_DEFAULT: Image bytes read: 476272
I (21726) OTA_DEFAULT: Image bytes read: 476561
I (21726) OTA_DEFAULT: Image bytes read: 476850
I (21736) OTA_DEFAULT: Image bytes read: 477139
I (21736) OTA_DEFAULT: Image bytes read: 477428
I (21746) OTA_DEFAULT: Image bytes read: 477717
I (21746) OTA_DEFAULT: Image bytes read: 478006
I (21756) OTA_DEFAULT: Image bytes read: 478295
I (21756) OTA_DEFAULT: Image bytes read: 478584
I (21766) OTA_DEFAULT: Image bytes read: 478873
I (21766) OTA_DEFAULT: Image bytes read: 479162
I (21776) OTA_DEFAULT: Image bytes read: 479451
I (21776) OTA_DEFAULT: Image bytes read: 479740
I (21786) OTA_DEFAULT: Image bytes read: 480029
I (21786) OTA_DEFAULT: Image bytes read: 480318
I (21796) OTA_DEFAULT: Image bytes read: 480607
I (21806) OTA_DEFAULT: Image bytes read: 480896
I (21806) OTA_DEFAULT: Image bytes read: 481185
I (21816) OTA_DEFAULT: Image bytes read: 481474
I (21816) OTA_DEFAULT: Image bytes read: 481763
I (21826) OTA_DEFAULT: Image bytes read: 482052
I (21826) OTA_DEFAULT: Image bytes read: 482341
I (21836) OTA_DEFAULT: Image bytes read: 482630
I (21836) OTA_DEFAULT: Image bytes read: 482919
I (21846) OTA_DEFAULT: Image bytes read: 483208
I (21846) OTA_DEFAULT: Image bytes read: 483497
I (21856) OTA_DEFAULT: Image bytes read: 483786
I (21856) OTA_DEFAULT: Image bytes read: 484075
I (21866) OTA_DEFAULT: Image bytes read: 484364
I (21866) OTA_DEFAULT: Image bytes read: 484653
I (21876) OTA_DEFAULT: Image bytes read: 484942
I (21876) OTA_DEFAULT: Image bytes read: 485231
I (21886) OTA_DEFAULT: Image bytes read: 485520
I (21886) OTA_DEFAULT: Image bytes read: 485809
I (21896) OTA_DEFAULT: Image bytes read: 486098
I (21896) OTA_DEFAULT: Image bytes read: 486387
I (21906) OTA_DEFAULT: Image bytes read: 486676
I (21906) OTA_DEFAULT: Image bytes read: 486965
I (21916) OTA_DEFAULT: Image bytes read: 487254
I (21926) OTA_DEFAULT: Image bytes read: 487543
I (21926) OTA_DEFAULT: Image bytes read: 487832
I (21936) OTA_DEFAULT: Image bytes read: 488121
I (21936) OTA_DEFAULT: Image bytes read: 488410
I (21946) OTA_DEFAULT: Image bytes read: 488699
I (21946) OTA_DEFAULT: Image bytes read: 488988
I (21956) OTA_DEFAULT: Image bytes read: 489277
I (21956) OTA_DEFAULT: Image bytes read: 489566
I (21966) OTA_DEFAULT: Image bytes read: 489855
I (21966) OTA_DEFAULT: Image bytes read: 490144
I (21976) OTA_DEFAULT: Image bytes read: 490433
I (21976) OTA_DEFAULT: Image bytes read: 490722
I (21986) OTA_DEFAULT: Image bytes read: 491011
I (21986) OTA_DEFAULT: Image bytes read: 491300
I (21996) OTA_DEFAULT: Image bytes read: 491589
I (21996) OTA_DEFAULT: Image bytes read: 491878
I (22006) OTA_DEFAULT: Image bytes read: 492167
I (22006) OTA_DEFAULT: Image bytes read: 492456
I (22016) OTA_DEFAULT: Image bytes read: 492745
I (22016) OTA_DEFAULT: Image bytes read: 493034
I (22026) OTA_DEFAULT: Image bytes read: 493323
I (22026) OTA_DEFAULT: Image bytes read: 493612
I (22036) OTA_DEFAULT: Image bytes read: 493901
I (22046) OTA_DEFAULT: Image bytes read: 494190
I (22046) OTA_DEFAULT: Image bytes read: 494479
I (22056) OTA_DEFAULT: Image bytes read: 494768
I (22056) OTA_DEFAULT: Image bytes read: 495057
I (22066) OTA_DEFAULT: Image bytes read: 495346
I (22066) OTA_DEFAULT: Image bytes read: 495635
I (22076) OTA_DEFAULT: Image bytes read: 495924
I (22076) OTA_DEFAULT: Image bytes read: 496213
I (22086) OTA_DEFAULT: Image bytes read: 496502
I (22086) OTA_DEFAULT: Image bytes read: 496791
I (22096) OTA_DEFAULT: Image bytes read: 497080
I (22096) OTA_DEFAULT: Image bytes read: 497369
I (22106) OTA_DEFAULT: Image bytes read: 497658
I (22106) OTA_DEFAULT: Image bytes read: 497947
I (22116) OTA_DEFAULT: Image bytes read: 498236
I (22116) OTA_DEFAULT: Image bytes read: 498525
I (22126) OTA_DEFAULT: Image bytes read: 498814
I (22126) OTA_DEFAULT: Image bytes read: 499103
I (22136) OTA_DEFAULT: Image bytes read: 499392
I (22136) OTA_DEFAULT: Image bytes read: 499681
I (22146) OTA_DEFAULT: Image bytes read: 499970
I (22146) OTA_DEFAULT: Image bytes read: 500259
I (22156) OTA_DEFAULT: Image bytes read: 500548
I (22156) OTA_DEFAULT: Image bytes read: 500837
I (22166) OTA_DEFAULT: Image bytes read: 501126
I (22176) OTA_DEFAULT: Image bytes read: 501415
I (22176) OTA_DEFAULT: Image bytes read: 501704
I (22186) OTA_DEFAULT: Image bytes read: 501993
I (22186) OTA_DEFAULT: Image bytes read: 502282
I (22196) OTA_DEFAULT: Image bytes read: 502571
I (22196) OTA_DEFAULT: Image bytes read: 502860
I (22206) OTA_DEFAULT: Image bytes read: 503149
I (22206) OTA_DEFAULT: Image bytes read: 503438
I (22216) OTA_DEFAULT: Image bytes read: 503727
I (22216) OTA_DEFAULT: Image bytes read: 504016
I (22226) OTA_DEFAULT: Image bytes read: 504305
I (22226) OTA_DEFAULT: Image bytes read: 504594
I (22236) OTA_DEFAULT: Image bytes read: 504883
I (22236) OTA_DEFAULT: Image bytes read: 505172
I (22246) OTA_DEFAULT: Image bytes read: 505461
I (22246) OTA_DEFAULT: Image bytes read: 505750
I (22256) OTA_DEFAULT: Image bytes read: 506039
I (22256) OTA_DEFAULT: Image bytes read: 506328
I (22266) OTA_DEFAULT: Image bytes read: 506617
I (22266) OTA_DEFAULT: Image bytes read: 506906
I (22276) OTA_DEFAULT: Image bytes read: 507195
I (22286) OTA_DEFAULT: Image bytes read: 507484
I (22286) OTA_DEFAULT: Image bytes read: 507773
I (22286) OTA_DEFAULT: Image bytes read: 508062
I (22296) OTA_DEFAULT: Image bytes read: 508351
I (22306) OTA_DEFAULT: Image bytes read: 508640
I (22306) OTA_DEFAULT: Image bytes read: 508929
I (22316) OTA_DEFAULT: Image bytes read: 509218
I (22316) OTA_DEFAULT: Image bytes read: 509507
I (22326) OTA_DEFAULT: Image bytes read: 509796
I (22326) OTA_DEFAULT: Image bytes read: 510085
I (22336) OTA_DEFAULT: Image bytes read: 510374
I (22336) OTA_DEFAULT: Image bytes read: 510663
I (22346) OTA_DEFAULT: Image bytes read: 510952
I (22346) OTA_DEFAULT: Image bytes read: 511241
I (22356) OTA_DEFAULT: Image bytes read: 511530
I (22356) OTA_DEFAULT: Image bytes read: 511819
I (22366) OTA_DEFAULT: Image bytes read: 512108
I (22366) OTA_DEFAULT: Image bytes read: 512397
I (22376) OTA_DEFAULT: Image bytes read: 512686
I (22376) OTA_DEFAULT: Image bytes read: 512975
I (22386) OTA_DEFAULT: Image bytes read: 513264
I (22386) OTA_DEFAULT: Image bytes read: 513553
I (22396) OTA_DEFAULT: Image bytes read: 513842
I (22396) OTA_DEFAULT: Image bytes read: 514131
I (22406) OTA_DEFAULT: Image bytes read: 514420
I (22416) OTA_DEFAULT: Image bytes read: 514709
I (22416) OTA_DEFAULT: Image bytes read: 514998
I (22426) OTA_DEFAULT: Image bytes read: 515287
I (22426) OTA_DEFAULT: Image bytes read: 515576
I (22436) OTA_DEFAULT: Image bytes read: 515865
I (22436) OTA_DEFAULT: Image bytes read: 516154
I (22446) OTA_DEFAULT: Image bytes read: 516443
I (22446) OTA_DEFAULT: Image bytes read: 516732
I (22456) OTA_DEFAULT: Image bytes read: 517021
I (22456) OTA_DEFAULT: Image bytes read: 517310
I (22466) OTA_DEFAULT: Image bytes read: 517599
I (22466) OTA_DEFAULT: Image bytes read: 517888
I (22476) OTA_DEFAULT: Image bytes read: 518177
I (22476) OTA_DEFAULT: Image bytes read: 518466
I (22486) OTA_DEFAULT: Image bytes read: 518755
I (22486) OTA_DEFAULT: Image bytes read: 519044
I (22496) OTA_DEFAULT: Image bytes read: 519333
I (22496) OTA_DEFAULT: Image bytes read: 519622
I (22506) OTA_DEFAULT: Image bytes read: 519911
I (22506) OTA_DEFAULT: Image bytes read: 520200
I (22516) OTA_DEFAULT: Image bytes read: 520489
I (22516) OTA_DEFAULT: Image bytes read: 520778
I (22526) OTA_DEFAULT: Image bytes read: 521067
I (22526) OTA_DEFAULT: Image bytes read: 521356
I (22536) OTA_DEFAULT: Image bytes read: 521645
I (22546) OTA_DEFAULT: Image bytes read: 521934
I (22546) OTA_DEFAULT: Image bytes read: 522223
I (22556) OTA_DEFAULT: Image bytes read: 522512
I (22556) OTA_DEFAULT: Image bytes read: 522801
I (22566) OTA_DEFAULT: Image bytes read: 523090
I (22566) OTA_DEFAULT: Image bytes read: 523379
I (22576) OTA_DEFAULT: Image bytes read: 523668
I (22576) OTA_DEFAULT: Image bytes read: 523957
I (22586) OTA_DEFAULT: Image bytes read: 524246
I (22586) OTA_DEFAULT: Image bytes read: 524535
I (22596) OTA_DEFAULT: Image bytes read: 524824
I (22596) OTA_DEFAULT: Image bytes read: 525113
I (22606) OTA_DEFAULT: Image bytes read: 525402
I (22606) OTA_DEFAULT: Image bytes read: 525691
I (22616) OTA_DEFAULT: Image bytes read: 525980
I (22616) OTA_DEFAULT: Image bytes read: 526269
I (22626) OTA_DEFAULT: Image bytes read: 526558
I (22626) OTA_DEFAULT: Image bytes read: 526847
I (22636) OTA_DEFAULT: Image bytes read: 527136
I (22636) OTA_DEFAULT: Image bytes read: 527425
I (22646) OTA_DEFAULT: Image bytes read: 527714
I (22656) OTA_DEFAULT: Image bytes read: 528003
I (22656) OTA_DEFAULT: Image bytes read: 528292
I (22656) OTA_DEFAULT: Image bytes read: 528581
I (22666) OTA_DEFAULT: Image bytes read: 528870
I (22676) OTA_DEFAULT: Image bytes read: 529159
I (22676) OTA_DEFAULT: Image bytes read: 529448
I (22686) OTA_DEFAULT: Image bytes read: 529737
I (22686) OTA_DEFAULT: Image bytes read: 530026
I (22696) OTA_DEFAULT: Image bytes read: 530315
I (22696) OTA_DEFAULT: Image bytes read: 530604
I (22706) OTA_DEFAULT: Image bytes read: 530893
I (22706) OTA_DEFAULT: Image bytes read: 531182
I (22716) OTA_DEFAULT: Image bytes read: 531471
I (22716) OTA_DEFAULT: Image bytes read: 531760
I (22726) OTA_DEFAULT: Image bytes read: 532049
I (22726) OTA_DEFAULT: Image bytes read: 532338
I (22736) OTA_DEFAULT: Image bytes read: 532627
I (22736) OTA_DEFAULT: Image bytes read: 532916
I (22746) OTA_DEFAULT: Image bytes read: 533205
I (22746) OTA_DEFAULT: Image bytes read: 533494
I (22756) OTA_DEFAULT: Image bytes read: 533783
I (22756) OTA_DEFAULT: Image bytes read: 534072
I (22766) OTA_DEFAULT: Image bytes read: 534361
I (22766) OTA_DEFAULT: Image bytes read: 534650
I (22776) OTA_DEFAULT: Image bytes read: 534939
I (22776) OTA_DEFAULT: Image bytes read: 535228
I (22786) OTA_DEFAULT: Image bytes read: 535517
I (22796) OTA_DEFAULT: Image bytes read: 535806
I (22796) OTA_DEFAULT: Image bytes read: 536095
I (22806) OTA_DEFAULT: Image bytes read: 536384
I (22806) OTA_DEFAULT: Image bytes read: 536673
I (22816) OTA_DEFAULT: Image bytes read: 536962
I (22816) OTA_DEFAULT: Image bytes read: 537251
I (22826) OTA_DEFAULT: Image bytes read: 537540
I (22826) OTA_DEFAULT: Image bytes read: 537829
I (22836) OTA_DEFAULT: Image bytes read: 538118
I (22836) OTA_DEFAULT: Image bytes read: 538407
I (22846) OTA_DEFAULT: Image bytes read: 538696
I (22846) OTA_DEFAULT: Image bytes read: 538985
I (22856) OTA_DEFAULT: Image bytes read: 539274
I (22856) OTA_DEFAULT: Image bytes read: 539563
I (22866) OTA_DEFAULT: Image bytes read: 539852
I (22866) OTA_DEFAULT: Image bytes read: 540141
I (22876) OTA_DEFAULT: Image bytes read: 540430
I (22876) OTA_DEFAULT: Image bytes read: 540719
I (22886) OTA_DEFAULT: Image bytes read: 541008
I (22886) OTA_DEFAULT: Image bytes read: 541297
I (22896) OTA_DEFAULT: Image bytes read: 541586
I (22896) OTA_DEFAULT: Image bytes read: 541875
I (22906) OTA_DEFAULT: Image bytes read: 542164
I (22906) OTA_DEFAULT: Image bytes read: 542453
I (22916) OTA_DEFAULT: Image bytes read: 542742
I (22926) OTA_DEFAULT: Image bytes read: 543031
I (22926) OTA_DEFAULT: Image bytes read: 543320
I (22936) OTA_DEFAULT: Image bytes read: 543609
I (22936) OTA_DEFAULT: Image bytes read: 543898
I (22946) OTA_DEFAULT: Image bytes read: 544187
I (22946) OTA_DEFAULT: Image bytes read: 544476
I (22956) OTA_DEFAULT: Image bytes read: 544765
I (22956) OTA_DEFAULT: Image bytes read: 545054
I (22966) OTA_DEFAULT: Image bytes read: 545343
I (22966) OTA_DEFAULT: Image bytes read: 545632
I (22976) OTA_DEFAULT: Image bytes read: 545921
I (22976) OTA_DEFAULT: Image bytes read: 546210
I (22986) OTA_DEFAULT: Image bytes read: 546499
I (22986) OTA_DEFAULT: Image bytes read: 546788
I (22996) OTA_DEFAULT: Image bytes read: 547077
I (22996) OTA_DEFAULT: Image bytes read: 547366
I (23006) OTA_DEFAULT: Image bytes read: 547655
I (23006) OTA_DEFAULT: Image bytes read: 547944
I (23016) OTA_DEFAULT: Image bytes read: 548233
I (23016) OTA_DEFAULT: Image bytes read: 548522
I (23026) OTA_DEFAULT: Image bytes read: 548811
I (23026) OTA_DEFAULT: Image bytes read: 549100
I (23036) OTA_DEFAULT: Image bytes read: 549389
I (23036) OTA_DEFAULT: Image bytes read: 549678
I (23046) OTA_DEFAULT: Image bytes read: 549967
I (23056) OTA_DEFAULT: Image bytes read: 550256
I (23056) OTA_DEFAULT: Image bytes read: 550545
I (23066) OTA_DEFAULT: Image bytes read: 550834
I (23066) OTA_DEFAULT: Image bytes read: 551123
I (23076) OTA_DEFAULT: Image bytes read: 551412
I (23076) OTA_DEFAULT: Image bytes read: 551701
I (23086) OTA_DEFAULT: Image bytes read: 551990
I (23086) OTA_DEFAULT: Image bytes read: 552279
I (23096) OTA_DEFAULT: Image bytes read: 552568
I (23096) OTA_DEFAULT: Image bytes read: 552857
I (23106) OTA_DEFAULT: Image bytes read: 553146
I (23106) OTA_DEFAULT: Image bytes read: 553435
I (23116) OTA_DEFAULT: Image bytes read: 553724
I (23116) OTA_DEFAULT: Image bytes read: 554013
I (23126) OTA_DEFAULT: Image bytes read: 554302
I (23126) OTA_DEFAULT: Image bytes read: 554591
I (23136) OTA_DEFAULT: Image bytes read: 554880
I (23136) OTA_DEFAULT: Image bytes read: 555169
I (23146) OTA_DEFAULT: Image bytes read: 555458
I (23146) OTA_DEFAULT: Image bytes read: 555747
I (23156) OTA_DEFAULT: Image bytes read: 556036
I (23156) OTA_DEFAULT: Image bytes read: 556325
I (23166) OTA_DEFAULT: Image bytes read: 556614
I (23176) OTA_DEFAULT: Image bytes read: 556903
I (23176) OTA_DEFAULT: Image bytes read: 557192
I (23186) OTA_DEFAULT: Image bytes read: 557481
I (23186) OTA_DEFAULT: Image bytes read: 557770
I (23196) OTA_DEFAULT: Image bytes read: 558059
I (23196) OTA_DEFAULT: Image bytes read: 558348
I (23206) OTA_DEFAULT: Image bytes read: 558637
I (23206) OTA_DEFAULT: Image bytes read: 558926
I (23216) OTA_DEFAULT: Image bytes read: 559215
I (23216) OTA_DEFAULT: Image bytes read: 559504
I (23226) OTA_DEFAULT: Image bytes read: 559793
I (23226) OTA_DEFAULT: Image bytes read: 560082
I (23236) OTA_DEFAULT: Image bytes read: 560371
I (23236) OTA_DEFAULT: Image bytes read: 560660
I (23246) OTA_DEFAULT: Image bytes read: 560949
I (23246) OTA_DEFAULT: Image bytes read: 561238
I (23256) OTA_DEFAULT: Image bytes read: 561527
I (23256) OTA_DEFAULT: Image bytes read: 561816
I (23266) OTA_DEFAULT: Image bytes read: 562105
I (23266) OTA_DEFAULT: Image bytes read: 562394
I (23276) OTA_DEFAULT: Image bytes read: 562683
I (23276) OTA_DEFAULT: Image bytes read: 562972
I (23286) OTA_DEFAULT: Image bytes read: 563261
I (23286) OTA_DEFAULT: Image bytes read: 563550
I (23296) OTA_DEFAULT: Image bytes read: 563839
I (23306) OTA_DEFAULT: Image bytes read: 564128
I (23306) OTA_DEFAULT: Image bytes read: 564417
I (23316) OTA_DEFAULT: Image bytes read: 564706
I (23316) OTA_DEFAULT: Image bytes read: 564995
I (23326) OTA_DEFAULT: Image bytes read: 565284
I (23326) OTA_DEFAULT: Image bytes read: 565573
I (23336) OTA_DEFAULT: Image bytes read: 565862
I (23336) OTA_DEFAULT: Image bytes read: 566151
I (23346) OTA_DEFAULT: Image bytes read: 566440
I (23346) OTA_DEFAULT: Image bytes read: 566729
I (23356) OTA_DEFAULT: Image bytes read: 567018
I (23356) OTA_DEFAULT: Image bytes read: 567307
I (23366) OTA_DEFAULT: Image bytes read: 567596
I (23366) OTA_DEFAULT: Image bytes read: 567885
I (23376) OTA_DEFAULT: Image bytes read: 568174
I (23376) OTA_DEFAULT: Image bytes read: 568463
I (23386) OTA_DEFAULT: Image bytes read: 568752
I (23386) OTA_DEFAULT: Image bytes read: 569041
I (23396) OTA_DEFAULT: Image bytes read: 569330
I (23396) OTA_DEFAULT: Image bytes read: 569619
I (23406) OTA_DEFAULT: Image bytes read: 569908
I (23406) OTA_DEFAULT: Image bytes read: 570197
I (23416) OTA_DEFAULT: Image bytes read: 570486
I (23416) OTA_DEFAULT: Image bytes read: 570775
I (23426) OTA_DEFAULT: Image bytes read: 571064
I (23436) OTA_DEFAULT: Image bytes read: 571353
I (23436) OTA_DEFAULT: Image bytes read: 571642
I (23446) OTA_DEFAULT: Image bytes read: 571931
I (23446) OTA_DEFAULT: Image bytes read: 572220
I (23456) OTA_DEFAULT: Image bytes read: 572509
I (23456) OTA_DEFAULT: Image bytes read: 572798
I (23466) OTA_DEFAULT: Image bytes read: 573087
I (23466) OTA_DEFAULT: Image bytes read: 573376
I (23476) OTA_DEFAULT: Image bytes read: 573665
I (23476) OTA_DEFAULT: Image bytes read: 573954
I (23486) OTA_DEFAULT: Image bytes read: 574243
I (23486) OTA_DEFAULT: Image bytes read: 574532
I (23496) OTA_DEFAULT: Image bytes read: 574821
I (23496) OTA_DEFAULT: Image bytes read: 575110
I (23506) OTA_DEFAULT: Image bytes read: 575399
I (23506) OTA_DEFAULT: Image bytes read: 575688
I (23516) OTA_DEFAULT: Image bytes read: 575977
I (23516) OTA_DEFAULT: Image bytes read: 576266
I (23526) OTA_DEFAULT: Image bytes read: 576555
I (23536) OTA_DEFAULT: Image bytes read: 576844
I (23536) OTA_DEFAULT: Image bytes read: 577133
I (23536) OTA_DEFAULT: Image bytes read: 577422
I (23546) OTA_DEFAULT: Image bytes read: 577711
I (23556) OTA_DEFAULT: Image bytes read: 578000
I (23556) OTA_DEFAULT: Image bytes read: 578289
I (23566) OTA_DEFAULT: Image bytes read: 578578
I (23566) OTA_DEFAULT: Image bytes read: 578867
I (23576) OTA_DEFAULT: Image bytes read: 579156
I (23576) OTA_DEFAULT: Image bytes read: 579445
I (23586) OTA_DEFAULT: Image bytes read: 579734
I (23586) OTA_DEFAULT: Image bytes read: 580023
I (23596) OTA_DEFAULT: Image bytes read: 580312
I (23596) OTA_DEFAULT: Image bytes read: 580601
I (23606) OTA_DEFAULT: Image bytes read: 580890
I (23606) OTA_DEFAULT: Image bytes read: 581179
I (23616) OTA_DEFAULT: Image bytes read: 581468
I (23616) OTA_DEFAULT: Image bytes read: 581757
I (23626) OTA_DEFAULT: Image bytes read: 582046
I (23626) OTA_DEFAULT: Image bytes read: 582335
I (23636) OTA_DEFAULT: Image bytes read: 582624
I (23636) OTA_DEFAULT: Image bytes read: 582913
I (23646) OTA_DEFAULT: Image bytes read: 583202
I (23646) OTA_DEFAULT: Image bytes read: 583491
I (23656) OTA_DEFAULT: Image bytes read: 583780
I (23656) OTA_DEFAULT: Image bytes read: 584069
I (23666) OTA_DEFAULT: Image bytes read: 584358
I (23676) OTA_DEFAULT: Image bytes read: 584647
I (23676) OTA_DEFAULT: Image bytes read: 584936
I (23686) OTA_DEFAULT: Image bytes read: 585225
I (23686) OTA_DEFAULT: Image bytes read: 585514
I (23696) OTA_DEFAULT: Image bytes read: 585803
I (23696) OTA_DEFAULT: Image bytes read: 586092
I (23706) OTA_DEFAULT: Image bytes read: 586381
I (23706) OTA_DEFAULT: Image bytes read: 586670
I (23716) OTA_DEFAULT: Image bytes read: 586959
I (23716) OTA_DEFAULT: Image bytes read: 587248
I (23726) OTA_DEFAULT: Image bytes read: 587537
I (23726) OTA_DEFAULT: Image bytes read: 587826
I (23736) OTA_DEFAULT: Image bytes read: 588115
I (23736) OTA_DEFAULT: Image bytes read: 588404
I (23746) OTA_DEFAULT: Image bytes read: 588693
I (23746) OTA_DEFAULT: Image bytes read: 588982
I (23756) OTA_DEFAULT: Image bytes read: 589271
I (23756) OTA_DEFAULT: Image bytes read: 589560
I (23766) OTA_DEFAULT: Image bytes read: 589849
I (23766) OTA_DEFAULT: Image bytes read: 590138
I (23776) OTA_DEFAULT: Image bytes read: 590427
I (23776) OTA_DEFAULT: Image bytes read: 590716
I (23786) OTA_DEFAULT: Image bytes read: 591005
I (23796) OTA_DEFAULT: Image bytes read: 591294
I (23796) OTA_DEFAULT: Image bytes read: 591583
I (23806) OTA_DEFAULT: Image bytes read: 591872
I (23806) OTA_DEFAULT: Image bytes read: 592161
I (23816) OTA_DEFAULT: Image bytes read: 592450
I (23816) OTA_DEFAULT: Image bytes read: 592739
I (23826) OTA_DEFAULT: Image bytes read: 593028
I (23826) OTA_DEFAULT: Image bytes read: 593317
I (23836) OTA_DEFAULT: Image bytes read: 593606
I (23836) OTA_DEFAULT: Image bytes read: 593895
I (23846) OTA_DEFAULT: Image bytes read: 594184
I (23846) OTA_DEFAULT: Image bytes read: 594473
I (23856) OTA_DEFAULT: Image bytes read: 594762
I (23856) OTA_DEFAULT: Image bytes read: 595051
I (23866) OTA_DEFAULT: Image bytes read: 595340
I (23866) OTA_DEFAULT: Image bytes read: 595629
I (23876) OTA_DEFAULT: Image bytes read: 595918
I (23876) OTA_DEFAULT: Image bytes read: 596207
I (23886) OTA_DEFAULT: Image bytes read: 596496
I (23886) OTA_DEFAULT: Image bytes read: 596785
I (23896) OTA_DEFAULT: Image bytes read: 597074
I (23896) OTA_DEFAULT: Image bytes read: 597363
I (23906) OTA_DEFAULT: Image bytes read: 597652
I (23906) OTA_DEFAULT: Image bytes read: 597941
I (23916) OTA_DEFAULT: Image bytes read: 598230
I (23926) OTA_DEFAULT: Image bytes read: 598519
I (23926) OTA_DEFAULT: Image bytes read: 598808
I (23936) OTA_DEFAULT: Image bytes read: 599097
I (23936) OTA_DEFAULT: Image bytes read: 599386
I (23946) OTA_DEFAULT: Image bytes read: 599675
I (23946) OTA_DEFAULT: Image bytes read: 599964
I (23956) OTA_DEFAULT: Image bytes read: 600253
I (23956) OTA_DEFAULT: Image bytes read: 600542
I (23966) OTA_DEFAULT: Image bytes read: 600831
I (23966) OTA_DEFAULT: Image bytes read: 601120
I (23976) OTA_DEFAULT: Image bytes read: 601409
I (23976) OTA_DEFAULT: Image bytes read: 601698
I (23986) OTA_DEFAULT: Image bytes read: 601987
I (23986) OTA_DEFAULT: Image bytes read: 602276
I (23996) OTA_DEFAULT: Image bytes read: 602565
I (23996) OTA_DEFAULT: Image bytes read: 602854
I (24006) OTA_DEFAULT: Image bytes read: 603143
I (24006) OTA_DEFAULT: Image bytes read: 603432
I (24016) OTA_DEFAULT: Image bytes read: 603721
I (24016) OTA_DEFAULT: Image bytes read: 604010
I (24026) OTA_DEFAULT: Image bytes read: 604299
I (24026) OTA_DEFAULT: Image bytes read: 604588
I (24036) OTA_DEFAULT: Image bytes read: 604877
I (24036) OTA_DEFAULT: Image bytes read: 605166
I (24046) OTA_DEFAULT: Image bytes read: 605455
I (24056) OTA_DEFAULT: Image bytes read: 605744
I (24056) OTA_DEFAULT: Image bytes read: 606033
I (24066) OTA_DEFAULT: Image bytes read: 606322
I (24066) OTA_DEFAULT: Image bytes read: 606611
I (24076) OTA_DEFAULT: Image bytes read: 606900
I (24076) OTA_DEFAULT: Image bytes read: 607189
I (24086) OTA_DEFAULT: Image bytes read: 607478
I (24086) OTA_DEFAULT: Image bytes read: 607767
I (24096) OTA_DEFAULT: Image bytes read: 608056
I (24096) OTA_DEFAULT: Image bytes read: 608345
I (24106) OTA_DEFAULT: Image bytes read: 608634
I (24106) OTA_DEFAULT: Image bytes read: 608923
I (24116) OTA_DEFAULT: Image bytes read: 609212
I (24116) OTA_DEFAULT: Image bytes read: 609501
I (24126) OTA_DEFAULT: Image bytes read: 609790
I (24126) OTA_DEFAULT: Image bytes read: 610079
I (24136) OTA_DEFAULT: Image bytes read: 610368
I (24136) OTA_DEFAULT: Image bytes read: 610657
I (24146) OTA_DEFAULT: Image bytes read: 610946
I (24146) OTA_DEFAULT: Image bytes read: 611235
I (24156) OTA_DEFAULT: Image bytes read: 611524
I (24156) OTA_DEFAULT: Image bytes read: 611813
I (24166) OTA_DEFAULT: Image bytes read: 612102
I (24166) OTA_DEFAULT: Image bytes read: 612391
I (24176) OTA_DEFAULT: Image bytes read: 612680
I (24186) OTA_DEFAULT: Image bytes read: 612969
I (24186) OTA_DEFAULT: Image bytes read: 613258
I (24196) OTA_DEFAULT: Image bytes read: 613547
I (24196) OTA_DEFAULT: Image bytes read: 613836
I (24206) OTA_DEFAULT: Image bytes read: 614125
I (24206) OTA_DEFAULT: Image bytes read: 614414
I (24216) OTA_DEFAULT: Image bytes read: 614703
I (24216) OTA_DEFAULT: Image bytes read: 614992
I (24226) OTA_DEFAULT: Image bytes read: 615281
I (24226) OTA_DEFAULT: Image bytes read: 615570
I (24236) OTA_DEFAULT: Image bytes read: 615859
I (24236) OTA_DEFAULT: Image bytes read: 616148
I (24246) OTA_DEFAULT: Image bytes read: 616437
I (24246) OTA_DEFAULT: Image bytes read: 616726
I (24256) OTA_DEFAULT: Image bytes read: 617015
I (24256) OTA_DEFAULT: Image bytes read: 617304
I (24266) OTA_DEFAULT: Image bytes read: 617593
I (24266) OTA_DEFAULT: Image bytes read: 617882
I (24276) OTA_DEFAULT: Image bytes read: 618171
I (24276) OTA_DEFAULT: Image bytes read: 618460
I (24286) OTA_DEFAULT: Image bytes read: 618749
I (24296) OTA_DEFAULT: Image bytes read: 619038
I (24296) OTA_DEFAULT: Image bytes read: 619327
I (24306) OTA_DEFAULT: Image bytes read: 619616
I (24306) OTA_DEFAULT: Image bytes read: 619905
I (24316) OTA_DEFAULT: Image bytes read: 620194
I (24316) OTA_DEFAULT: Image bytes read: 620483
I (24326) OTA_DEFAULT: Image bytes read: 620772
I (24326) OTA_DEFAULT: Image bytes read: 621061
I (24336) OTA_DEFAULT: Image bytes read: 621350
I (24336) OTA_DEFAULT: Image bytes read: 621639
I (24346) OTA_DEFAULT: Image bytes read: 621928
I (24346) OTA_DEFAULT: Image bytes read: 622217
I (24356) OTA_DEFAULT: Image bytes read: 622506
I (24356) OTA_DEFAULT: Image bytes read: 622795
I (24366) OTA_DEFAULT: Image bytes read: 623084
I (24366) OTA_DEFAULT: Image bytes read: 623373
I (24376) OTA_DEFAULT: Image bytes read: 623662
I (24376) OTA_DEFAULT: Image bytes read: 623951
I (24386) OTA_DEFAULT: Image bytes read: 624240
I (24386) OTA_DEFAULT: Image bytes read: 624529
I (24396) OTA_DEFAULT: Image bytes read: 624818
I (24396) OTA_DEFAULT: Image bytes read: 625107
I (24406) OTA_DEFAULT: Image bytes read: 625396
I (24406) OTA_DEFAULT: Image bytes read: 625685
I (24416) OTA_DEFAULT: Image bytes read: 625974
I (24426) OTA_DEFAULT: Image bytes read: 626263
I (24426) OTA_DEFAULT: Image bytes read: 626552
I (24436) OTA_DEFAULT: Image bytes read: 626841
I (24436) OTA_DEFAULT: Image bytes read: 627130
I (24446) OTA_DEFAULT: Image bytes read: 627419
I (24446) OTA_DEFAULT: Image bytes read: 627708
I (24456) OTA_DEFAULT: Image bytes read: 627997
I (24456) OTA_DEFAULT: Image bytes read: 628286
I (24466) OTA_DEFAULT: Image bytes read: 628575
I (24466) OTA_DEFAULT: Image bytes read: 628864
I (24476) OTA_DEFAULT: Image bytes read: 629153
I (24476) OTA_DEFAULT: Image bytes read: 629442
I (24486) OTA_DEFAULT: Image bytes read: 629731
I (24486) OTA_DEFAULT: Image bytes read: 630020
I (24496) OTA_DEFAULT: Image bytes read: 630309
I (24496) OTA_DEFAULT: Image bytes read: 630598
I (24506) OTA_DEFAULT: Image bytes read: 630887
I (24506) OTA_DEFAULT: Image bytes read: 631176
I (24516) OTA_DEFAULT: Image bytes read: 631465
I (24516) OTA_DEFAULT: Image bytes read: 631754
I (24526) OTA_DEFAULT: Image bytes read: 632043
I (24526) OTA_DEFAULT: Image bytes read: 632332
I (24536) OTA_DEFAULT: Image bytes read: 632621
I (24536) OTA_DEFAULT: Image bytes read: 632910
I (24546) OTA_DEFAULT: Image bytes read: 633199
I (24556) OTA_DEFAULT: Image bytes read: 633488
I (24556) OTA_DEFAULT: Image bytes read: 633777
I (24566) OTA_DEFAULT: Image bytes read: 634066
I (24566) OTA_DEFAULT: Image bytes read: 634355
I (24576) OTA_DEFAULT: Image bytes read: 634644
I (24576) OTA_DEFAULT: Image bytes read: 634933
I (24586) OTA_DEFAULT: Image bytes read: 635222
I (24586) OTA_DEFAULT: Image bytes read: 635511
I (24596) OTA_DEFAULT: Image bytes read: 635800
I (24596) OTA_DEFAULT: Image bytes read: 636089
I (24606) OTA_DEFAULT: Image bytes read: 636378
I (24606) OTA_DEFAULT: Image bytes read: 636667
I (24616) OTA_DEFAULT: Image bytes read: 636956
I (24616) OTA_DEFAULT: Image bytes read: 637245
I (24626) OTA_DEFAULT: Image bytes read: 637534
I (24626) OTA_DEFAULT: Image bytes read: 637823
I (24636) OTA_DEFAULT: Image bytes read: 638112
I (24636) OTA_DEFAULT: Image bytes read: 638401
I (24646) OTA_DEFAULT: Image bytes read: 638690
I (24646) OTA_DEFAULT: Image bytes read: 638979
I (24656) OTA_DEFAULT: Image bytes read: 639268
I (24656) OTA_DEFAULT: Image bytes read: 639557
I (24666) OTA_DEFAULT: Image bytes read: 639846
I (24666) OTA_DEFAULT: Image bytes read: 640135
I (24676) OTA_DEFAULT: Image bytes read: 640424
I (24686) OTA_DEFAULT: Image bytes read: 640713
I (24686) OTA_DEFAULT: Image bytes read: 641002
I (24696) OTA_DEFAULT: Image bytes read: 641291
I (24696) OTA_DEFAULT: Image bytes read: 641580
I (24706) OTA_DEFAULT: Image bytes read: 641869
I (24706) OTA_DEFAULT: Image bytes read: 642158
I (24716) OTA_DEFAULT: Image bytes read: 642447
I (24716) OTA_DEFAULT: Image bytes read: 642736
I (24726) OTA_DEFAULT: Image bytes read: 643025
I (24726) OTA_DEFAULT: Image bytes read: 643314
I (24736) OTA_DEFAULT: Image bytes read: 643603
I (24736) OTA_DEFAULT: Image bytes read: 643892
I (24746) OTA_DEFAULT: Image bytes read: 644181
I (24746) OTA_DEFAULT: Image bytes read: 644470
I (24756) OTA_DEFAULT: Image bytes read: 644759
I (24756) OTA_DEFAULT: Image bytes read: 645048
I (24766) OTA_DEFAULT: Image bytes read: 645337
I (24766) OTA_DEFAULT: Image bytes read: 645626
I (24776) OTA_DEFAULT: Image bytes read: 645915
I (24776) OTA_DEFAULT: Image bytes read: 646204
I (24786) OTA_DEFAULT: Image bytes read: 646493
I (24786) OTA_DEFAULT: Image bytes read: 646782
I (24796) OTA_DEFAULT: Image bytes read: 647071
I (24806) OTA_DEFAULT: Image bytes read: 647360
I (24806) OTA_DEFAULT: Image bytes read: 647649
I (24816) OTA_DEFAULT: Image bytes read: 647938
I (24816) OTA_DEFAULT: Image bytes read: 648227
I (24826) OTA_DEFAULT: Image bytes read: 648516
I (24826) OTA_DEFAULT: Image bytes read: 648805
I (24836) OTA_DEFAULT: Image bytes read: 649094
I (24836) OTA_DEFAULT: Image bytes read: 649383
I (24846) OTA_DEFAULT: Image bytes read: 649672
I (24846) OTA_DEFAULT: Image bytes read: 649961
I (24856) OTA_DEFAULT: Image bytes read: 650250
I (24856) OTA_DEFAULT: Image bytes read: 650539
I (24866) OTA_DEFAULT: Image bytes read: 650828
I (24866) OTA_DEFAULT: Image bytes read: 651117
I (24876) OTA_DEFAULT: Image bytes read: 651406
I (24876) OTA_DEFAULT: Image bytes read: 651695
I (24886) OTA_DEFAULT: Image bytes read: 651984
I (24886) OTA_DEFAULT: Image bytes read: 652273
I (24896) OTA_DEFAULT: Image bytes read: 652562
I (24896) OTA_DEFAULT: Image bytes read: 652851
I (24906) OTA_DEFAULT: Image bytes read: 653140
I (24906) OTA_DEFAULT: Image bytes read: 653429
I (24916) OTA_DEFAULT: Image bytes read: 653718
I (24926) OTA_DEFAULT: Image bytes read: 654007
I (24926) OTA_DEFAULT: Image bytes read: 654296
I (24936) OTA_DEFAULT: Image bytes read: 654585
I (24936) OTA_DEFAULT: Image bytes read: 654874
I (24946) OTA_DEFAULT: Image bytes read: 655163
I (24946) OTA_DEFAULT: Image bytes read: 655452
I (24956) OTA_DEFAULT: Image bytes read: 655741
I (24956) OTA_DEFAULT: Image bytes read: 656030
I (24966) OTA_DEFAULT: Image bytes read: 656319
I (24966) OTA_DEFAULT: Image bytes read: 656608
I (24976) OTA_DEFAULT: Image bytes read: 656897
I (24976) OTA_DEFAULT: Image bytes read: 657186
I (24986) OTA_DEFAULT: Image bytes read: 657475
I (24986) OTA_DEFAULT: Image bytes read: 657764
I (24996) OTA_DEFAULT: Image bytes read: 658053
I (24996) OTA_DEFAULT: Image bytes read: 658342
I (25006) OTA_DEFAULT: Image bytes read: 658631
I (25006) OTA_DEFAULT: Image bytes read: 658920
I (25016) OTA_DEFAULT: Image bytes read: 659209
I (25016) OTA_DEFAULT: Image bytes read: 659498
I (25026) OTA_DEFAULT: Image bytes read: 659787
I (25026) OTA_DEFAULT: Image bytes read: 660076
I (25036) OTA_DEFAULT: Image bytes read: 660365
I (25046) OTA_DEFAULT: Image bytes read: 660654
I (25046) OTA_DEFAULT: Image bytes read: 660943
I (25056) OTA_DEFAULT: Image bytes read: 661232
I (25056) OTA_DEFAULT: Image bytes read: 661521
I (25066) OTA_DEFAULT: Image bytes read: 661810
I (25066) OTA_DEFAULT: Image bytes read: 662099
I (25076) OTA_DEFAULT: Image bytes read: 662388
I (25076) OTA_DEFAULT: Image bytes read: 662677
I (25086) OTA_DEFAULT: Image bytes read: 662966
I (25086) OTA_DEFAULT: Image bytes read: 663255
I (25096) OTA_DEFAULT: Image bytes read: 663544
I (25096) OTA_DEFAULT: Image bytes read: 663833
I (25106) OTA_DEFAULT: Image bytes read: 664122
I (25106) OTA_DEFAULT: Image bytes read: 664411
I (25116) OTA_DEFAULT: Image bytes read: 664700
I (25116) OTA_DEFAULT: Image bytes read: 664989
I (25126) OTA_DEFAULT: Image bytes read: 665278
I (25126) OTA_DEFAULT: Image bytes read: 665567
I (25136) OTA_DEFAULT: Image bytes read: 665856
I (25136) OTA_DEFAULT: Image bytes read: 666145
I (25146) OTA_DEFAULT: Image bytes read: 666434
I (25146) OTA_DEFAULT: Image bytes read: 666723
I (25156) OTA_DEFAULT: Image bytes read: 667012
I (25156) OTA_DEFAULT: Image bytes read: 667301
I (25166) OTA_DEFAULT: Image bytes read: 667590
I (25176) OTA_DEFAULT: Image bytes read: 667879
I (25176) OTA_DEFAULT: Image bytes read: 668168
I (25186) OTA_DEFAULT: Image bytes read: 668457
I (25186) OTA_DEFAULT: Image bytes read: 668746
I (25196) OTA_DEFAULT: Image bytes read: 669035
I (25196) OTA_DEFAULT: Image bytes read: 669324
I (25206) OTA_DEFAULT: Image bytes read: 669613
I (25206) OTA_DEFAULT: Image bytes read: 669902
I (25216) OTA_DEFAULT: Image bytes read: 670191
I (25216) OTA_DEFAULT: Image bytes read: 670480
I (25226) OTA_DEFAULT: Image bytes read: 670769
I (25226) OTA_DEFAULT: Image bytes read: 671058
I (25236) OTA_DEFAULT: Image bytes read: 671347
I (25236) OTA_DEFAULT: Image bytes read: 671636
I (25246) OTA_DEFAULT: Image bytes read: 671925
I (25246) OTA_DEFAULT: Image bytes read: 672214
I (25256) OTA_DEFAULT: Image bytes read: 672503
I (25256) OTA_DEFAULT: Image bytes read: 672792
I (25266) OTA_DEFAULT: Image bytes read: 673081
I (25266) OTA_DEFAULT: Image bytes read: 673370
I (25276) OTA_DEFAULT: Image bytes read: 673659
I (25276) OTA_DEFAULT: Image bytes read: 673948
I (25286) OTA_DEFAULT: Image bytes read: 674237
I (25286) OTA_DEFAULT: Image bytes read: 674526
I (25296) OTA_DEFAULT: Image bytes read: 674815
I (25306) OTA_DEFAULT: Image bytes read: 675104
I (25306) OTA_DEFAULT: Image bytes read: 675393
I (25316) OTA_DEFAULT: Image bytes read: 675682
I (25316) OTA_DEFAULT: Image bytes read: 675971
I (25326) OTA_DEFAULT: Image bytes read: 676260
I (25326) OTA_DEFAULT: Image bytes read: 676549
I (25336) OTA_DEFAULT: Image bytes read: 676838
I (25336) OTA_DEFAULT: Image bytes read: 677127
I (25346) OTA_DEFAULT: Image bytes read: 677416
I (25346) OTA_DEFAULT: Image bytes read: 677705
I (25356) OTA_DEFAULT: Image bytes read: 677994
I (25356) OTA_DEFAULT: Image bytes read: 678283
I (25366) OTA_DEFAULT: Image bytes read: 678572
I (25366) OTA_DEFAULT: Image bytes read: 678861
I (25376) OTA_DEFAULT: Image bytes read: 679150
I (25376) OTA_DEFAULT: Image bytes read: 679439
I (25386) OTA_DEFAULT: Image bytes read: 679728
I (25386) OTA_DEFAULT: Image bytes read: 680017
I (25396) OTA_DEFAULT: Image bytes read: 680306
I (25396) OTA_DEFAULT: Image bytes read: 680595
I (25406) OTA_DEFAULT: Image bytes read: 680884
I (25406) OTA_DEFAULT: Image bytes read: 681173
I (25416) OTA_DEFAULT: Image bytes read: 681462
I (25416) OTA_DEFAULT: Image bytes read: 681751
I (25426) OTA_DEFAULT: Image bytes read: 682040
I (25436) OTA_DEFAULT: Image bytes read: 682329
I (25436) OTA_DEFAULT: Image bytes read: 682618
I (25446) OTA_DEFAULT: Image bytes read: 682907
I (25446) OTA_DEFAULT: Image bytes read: 683196
I (25456) OTA_DEFAULT: Image bytes read: 683485
I (25456) OTA_DEFAULT: Image bytes read: 683774
I (25466) OTA_DEFAULT: Image bytes read: 684063
I (25466) OTA_DEFAULT: Image bytes read: 684352
I (25476) OTA_DEFAULT: Image bytes read: 684641
I (25476) OTA_DEFAULT: Image bytes read: 684930
I (25486) OTA_DEFAULT: Image bytes read: 685219
I (25486) OTA_DEFAULT: Image bytes read: 685508
I (25496) OTA_DEFAULT: Image bytes read: 685797
I (25496) OTA_DEFAULT: Image bytes read: 686086
I (25506) OTA_DEFAULT: Image bytes read: 686375
I (25506) OTA_DEFAULT: Image bytes read: 686664
I (25516) OTA_DEFAULT: Image bytes read: 686953
I (25516) OTA_DEFAULT: Image bytes read: 687242
I (25526) OTA_DEFAULT: Image bytes read: 687531
I (25526) OTA_DEFAULT: Image bytes read: 687820
I (25536) OTA_DEFAULT: Image bytes read: 688109
I (25536) OTA_DEFAULT: Image bytes read: 688398
I (25546) OTA_DEFAULT: Image bytes read: 688687
I (25556) OTA_DEFAULT: Image bytes read: 688976
I (25556) OTA_DEFAULT: Image bytes read: 689265
I (25566) OTA_DEFAULT: Image bytes read: 689554
I (25566) OTA_DEFAULT: Image bytes read: 689843
I (25576) OTA_DEFAULT: Image bytes read: 690132
I (25576) OTA_DEFAULT: Image bytes read: 690421
I (25586) OTA_DEFAULT: Image bytes read: 690710
I (25586) OTA_DEFAULT: Image bytes read: 690999
I (25596) OTA_DEFAULT: Image bytes read: 691288
I (25596) OTA_DEFAULT: Image bytes read: 691577
I (25606) OTA_DEFAULT: Image bytes read: 691866
I (25606) OTA_DEFAULT: Image bytes read: 692155
I (25616) OTA_DEFAULT: Image bytes read: 692444
I (25616) OTA_DEFAULT: Image bytes read: 692733
I (25626) OTA_DEFAULT: Image bytes read: 693022
I (25626) OTA_DEFAULT: Image bytes read: 693311
I (25636) OTA_DEFAULT: Image bytes read: 693600
I (25636) OTA_DEFAULT: Image bytes read: 693889
I (25646) OTA_DEFAULT: Image bytes read: 694178
I (25646) OTA_DEFAULT: Image bytes read: 694467
I (25656) OTA_DEFAULT: Image bytes read: 694756
I (25656) OTA_DEFAULT: Image bytes read: 695045
I (25666) OTA_DEFAULT: Image bytes read: 695334
I (25676) OTA_DEFAULT: Image bytes read: 695623
I (25676) OTA_DEFAULT: Image bytes read: 695912
I (25686) OTA_DEFAULT: Image bytes read: 696201
I (25686) OTA_DEFAULT: Image bytes read: 696490
I (25696) OTA_DEFAULT: Image bytes read: 696779
I (25696) OTA_DEFAULT: Image bytes read: 697068
I (25706) OTA_DEFAULT: Image bytes read: 697357
I (25706) OTA_DEFAULT: Image bytes read: 697646
I (25716) OTA_DEFAULT: Image bytes read: 697935
I (25716) OTA_DEFAULT: Image bytes read: 698224
I (25726) OTA_DEFAULT: Image bytes read: 698513
I (25726) OTA_DEFAULT: Image bytes read: 698802
I (25736) OTA_DEFAULT: Image bytes read: 699091
I (25736) OTA_DEFAULT: Image bytes read: 699380
I (25746) OTA_DEFAULT: Image bytes read: 699669
I (25746) OTA_DEFAULT: Image bytes read: 699958
I (25756) OTA_DEFAULT: Image bytes read: 700247
I (25756) OTA_DEFAULT: Image bytes read: 700536
I (25766) OTA_DEFAULT: Image bytes read: 700825
I (25766) OTA_DEFAULT: Image bytes read: 701114
I (25776) OTA_DEFAULT: Image bytes read: 701403
I (25776) OTA_DEFAULT: Image bytes read: 701692
I (25786) OTA_DEFAULT: Image bytes read: 701981
I (25786) OTA_DEFAULT: Image bytes read: 702270
I (25796) OTA_DEFAULT: Image bytes read: 702559
I (25806) OTA_DEFAULT: Image bytes read: 702848
I (25806) OTA_DEFAULT: Image bytes read: 703137
I (25816) OTA_DEFAULT: Image bytes read: 703426
I (25816) OTA_DEFAULT: Image bytes read: 703715
I (25826) OTA_DEFAULT: Image bytes read: 704004
I (25826) OTA_DEFAULT: Image bytes read: 704293
I (25836) OTA_DEFAULT: Image bytes read: 704582
I (25836) OTA_DEFAULT: Image bytes read: 704871
I (25846) OTA_DEFAULT: Image bytes read: 705160
I (25846) OTA_DEFAULT: Image bytes read: 705449
I (25856) OTA_DEFAULT: Image bytes read: 705738
I (25856) OTA_DEFAULT: Image bytes read: 706027
I (25866) OTA_DEFAULT: Image bytes read: 706316
I (25866) OTA_DEFAULT: Image bytes read: 706605
I (25876) OTA_DEFAULT: Image bytes read: 706894
I (25876) OTA_DEFAULT: Image bytes read: 707183
I (25886) OTA_DEFAULT: Image bytes read: 707472
I (25886) OTA_DEFAULT: Image bytes read: 707761
I (25896) OTA_DEFAULT: Image bytes read: 708050
I (25896) OTA_DEFAULT: Image bytes read: 708339
I (25906) OTA_DEFAULT: Image bytes read: 708628
I (25906) OTA_DEFAULT: Image bytes read: 708917
I (25916) OTA_DEFAULT: Image bytes read: 709206
I (25926) OTA_DEFAULT: Image bytes read: 709495
I (25926) OTA_DEFAULT: Image bytes read: 709784
I (25936) OTA_DEFAULT: Image bytes read: 710073
I (25936) OTA_DEFAULT: Image bytes read: 710362
I (25946) OTA_DEFAULT: Image bytes read: 710651
I (25946) OTA_DEFAULT: Image bytes read: 710940
I (25956) OTA_DEFAULT: Image bytes read: 711229
I (25956) OTA_DEFAULT: Image bytes read: 711518
I (25966) OTA_DEFAULT: Image bytes read: 711807
I (25966) OTA_DEFAULT: Image bytes read: 712096
I (25976) OTA_DEFAULT: Image bytes read: 712385
I (25976) OTA_DEFAULT: Image bytes read: 712674
I (25986) OTA_DEFAULT: Image bytes read: 712963
I (25986) OTA_DEFAULT: Image bytes read: 713252
I (25996) OTA_DEFAULT: Image bytes read: 713541
I (25996) OTA_DEFAULT: Image bytes read: 713830
I (26006) OTA_DEFAULT: Image bytes read: 714119
I (26006) OTA_DEFAULT: Image bytes read: 714408
I (26016) OTA_DEFAULT: Image bytes read: 714697
I (26016) OTA_DEFAULT: Image bytes read: 714986
I (26026) OTA_DEFAULT: Image bytes read: 715275
I (26026) OTA_DEFAULT: Image bytes read: 715564
I (26036) OTA_DEFAULT: Image bytes read: 715853
I (26036) OTA_DEFAULT: Image bytes read: 716142
I (26046) OTA_DEFAULT: Image bytes read: 716431
I (26056) OTA_DEFAULT: Image bytes read: 716720
I (26056) OTA_DEFAULT: Image bytes read: 717009
I (26066) OTA_DEFAULT: Image bytes read: 717298
I (26066) OTA_DEFAULT: Image bytes read: 717587
I (26076) OTA_DEFAULT: Image bytes read: 717876
I (26076) OTA_DEFAULT: Image bytes read: 718165
I (26086) OTA_DEFAULT: Image bytes read: 718454
I (26086) OTA_DEFAULT: Image bytes read: 718743
I (26096) OTA_DEFAULT: Image bytes read: 719032
I (26096) OTA_DEFAULT: Image bytes read: 719321
I (26106) OTA_DEFAULT: Image bytes read: 719610
I (26106) OTA_DEFAULT: Image bytes read: 719899
I (26116) OTA_DEFAULT: Image bytes read: 720188
I (26116) OTA_DEFAULT: Image bytes read: 720477
I (26126) OTA_DEFAULT: Image bytes read: 720766
I (26126) OTA_DEFAULT: Image bytes read: 721055
I (26136) OTA_DEFAULT: Image bytes read: 721344
I (26136) OTA_DEFAULT: Image bytes read: 721633
I (26146) OTA_DEFAULT: Image bytes read: 721922
I (26146) OTA_DEFAULT: Image bytes read: 722211
I (26156) OTA_DEFAULT: Image bytes read: 722500
I (26166) OTA_DEFAULT: Image bytes read: 722789
I (26166) OTA_DEFAULT: Image bytes read: 723078
I (26176) OTA_DEFAULT: Image bytes read: 723367
I (26176) OTA_DEFAULT: Image bytes read: 723656
I (26186) OTA_DEFAULT: Image bytes read: 723945
I (26186) OTA_DEFAULT: Image bytes read: 724234
I (26196) OTA_DEFAULT: Image bytes read: 724523
I (26196) OTA_DEFAULT: Image bytes read: 724812
I (26206) OTA_DEFAULT: Image bytes read: 725101
I (26206) OTA_DEFAULT: Image bytes read: 725390
I (26216) OTA_DEFAULT: Image bytes read: 725679
I (26216) OTA_DEFAULT: Image bytes read: 725968
I (26226) OTA_DEFAULT: Image bytes read: 726257
I (26226) OTA_DEFAULT: Image bytes read: 726546
I (26236) OTA_DEFAULT: Image bytes read: 726835
I (26236) OTA_DEFAULT: Image bytes read: 727124
I (26246) OTA_DEFAULT: Image bytes read: 727413
I (26246) OTA_DEFAULT: Image bytes read: 727702
I (26256) OTA_DEFAULT: Image bytes read: 727991
I (26256) OTA_DEFAULT: Image bytes read: 728280
I (26266) OTA_DEFAULT: Image bytes read: 728569
I (26266) OTA_DEFAULT: Image bytes read: 728858
I (26276) OTA_DEFAULT: Image bytes read: 729147
I (26276) OTA_DEFAULT: Image bytes read: 729436
I (26286) OTA_DEFAULT: Image bytes read: 729725
I (26296) OTA_DEFAULT: Image bytes read: 730014
I (26296) OTA_DEFAULT: Image bytes read: 730303
I (26306) OTA_DEFAULT: Image bytes read: 730592
I (26306) OTA_DEFAULT: Image bytes read: 730881
I (26316) OTA_DEFAULT: Image bytes read: 731170
I (26316) OTA_DEFAULT: Image bytes read: 731459
I (26326) OTA_DEFAULT: Image bytes read: 731748
I (26326) OTA_DEFAULT: Image bytes read: 732037
I (26336) OTA_DEFAULT: Image bytes read: 732326
I (26336) OTA_DEFAULT: Image bytes read: 732615
I (26346) OTA_DEFAULT: Image bytes read: 732904
I (26346) OTA_DEFAULT: Image bytes read: 733193
I (26356) OTA_DEFAULT: Image bytes read: 733482
I (26356) OTA_DEFAULT: Image bytes read: 733771
I (26366) OTA_DEFAULT: Image bytes read: 734060
I (26366) OTA_DEFAULT: Image bytes read: 734349
I (26376) OTA_DEFAULT: Image bytes read: 734638
I (26376) OTA_DEFAULT: Image bytes read: 734927
I (26386) OTA_DEFAULT: Image bytes read: 735216
I (26386) OTA_DEFAULT: Image bytes read: 735505
I (26396) OTA_DEFAULT: Image bytes read: 735794
I (26396) OTA_DEFAULT: Image bytes read: 736083
I (26406) OTA_DEFAULT: Image bytes read: 736372
I (26406) OTA_DEFAULT: Image bytes read: 736661
I (26416) OTA_DEFAULT: Image bytes read: 736950
I (26426) OTA_DEFAULT: Image bytes read: 737239
I (26426) OTA_DEFAULT: Image bytes read: 737528
I (26436) OTA_DEFAULT: Image bytes read: 737817
I (26436) OTA_DEFAULT: Image bytes read: 738106
I (26446) OTA_DEFAULT: Image bytes read: 738395
I (26446) OTA_DEFAULT: Image bytes read: 738684
I (26456) OTA_DEFAULT: Image bytes read: 738973
I (26456) OTA_DEFAULT: Image bytes read: 739262
I (26466) OTA_DEFAULT: Image bytes read: 739551
I (26466) OTA_DEFAULT: Image bytes read: 739840
I (26476) OTA_DEFAULT: Image bytes read: 740129
I (26476) OTA_DEFAULT: Image bytes read: 740418
I (26486) OTA_DEFAULT: Image bytes read: 740707
I (26486) OTA_DEFAULT: Image bytes read: 740996
I (26496) OTA_DEFAULT: Image bytes read: 741285
I (26496) OTA_DEFAULT: Image bytes read: 741574
I (26506) OTA_DEFAULT: Image bytes read: 741863
I (26506) OTA_DEFAULT: Image bytes read: 742152
I (26516) OTA_DEFAULT: Image bytes read: 742441
I (26516) OTA_DEFAULT: Image bytes read: 742730
I (26526) OTA_DEFAULT: Image bytes read: 743019
I (26526) OTA_DEFAULT: Image bytes read: 743308
I (26536) OTA_DEFAULT: Image bytes read: 743597
I (26536) OTA_DEFAULT: Image bytes read: 743886
I (26546) OTA_DEFAULT: Image bytes read: 744175
I (26556) OTA_DEFAULT: Image bytes read: 744464
I (26556) OTA_DEFAULT: Image bytes read: 744753
I (26566) OTA_DEFAULT: Image bytes read: 745042
I (26566) OTA_DEFAULT: Image bytes read: 745331
I (26576) OTA_DEFAULT: Image bytes read: 745620
I (26576) OTA_DEFAULT: Image bytes read: 745909
I (26586) OTA_DEFAULT: Image bytes read: 746198
I (26586) OTA_DEFAULT: Image bytes read: 746487
I (26596) OTA_DEFAULT: Image bytes read: 746776
I (26596) OTA_DEFAULT: Image bytes read: 747065
I (26606) OTA_DEFAULT: Image bytes read: 747354
I (26606) OTA_DEFAULT: Image bytes read: 747643
I (26616) OTA_DEFAULT: Image bytes read: 747932
I (26616) OTA_DEFAULT: Image bytes read: 748221
I (26626) OTA_DEFAULT: Image bytes read: 748510
I (26626) OTA_DEFAULT: Image bytes read: 748799
I (26636) OTA_DEFAULT: Image bytes read: 749088
I (26636) OTA_DEFAULT: Image bytes read: 749377
I (26646) OTA_DEFAULT: Image bytes read: 749666
I (26646) OTA_DEFAULT: Image bytes read: 749955
I (26656) OTA_DEFAULT: Image bytes read: 750244
I (26666) OTA_DEFAULT: Image bytes read: 750533
I (26666) OTA_DEFAULT: Image bytes read: 750822
I (26666) OTA_DEFAULT: Image bytes read: 751111
I (26676) OTA_DEFAULT: Image bytes read: 751400
I (26686) OTA_DEFAULT: Image bytes read: 751689
I (26686) OTA_DEFAULT: Image bytes read: 751978
I (26696) OTA_DEFAULT: Image bytes read: 752267
I (26696) OTA_DEFAULT: Image bytes read: 752556
I (26706) OTA_DEFAULT: Image bytes read: 752845
I (26706) OTA_DEFAULT: Image bytes read: 753134
I (26716) OTA_DEFAULT: Image bytes read: 753423
I (26716) OTA_DEFAULT: Image bytes read: 753712
I (26726) OTA_DEFAULT: Image bytes read: 754001
I (26726) OTA_DEFAULT: Image bytes read: 754290
I (26736) OTA_DEFAULT: Image bytes read: 754579
I (26736) OTA_DEFAULT: Image bytes read: 754868
I (26746) OTA_DEFAULT: Image bytes read: 755157
I (26746) OTA_DEFAULT: Image bytes read: 755446
I (26756) OTA_DEFAULT: Image bytes read: 755735
I (26756) OTA_DEFAULT: Image bytes read: 756024
I (26766) OTA_DEFAULT: Image bytes read: 756313
I (26766) OTA_DEFAULT: Image bytes read: 756602
I (26776) OTA_DEFAULT: Image bytes read: 756891
I (26776) OTA_DEFAULT: Image bytes read: 757180
I (26786) OTA_DEFAULT: Image bytes read: 757469
I (26796) OTA_DEFAULT: Image bytes read: 757758
I (26796) OTA_DEFAULT: Image bytes read: 758047
I (26806) OTA_DEFAULT: Image bytes read: 758336
I (26806) OTA_DEFAULT: Image bytes read: 758625
I (26816) OTA_DEFAULT: Image bytes read: 758914
I (26816) OTA_DEFAULT: Image bytes read: 759203
I (26826) OTA_DEFAULT: Image bytes read: 759492
I (26826) OTA_DEFAULT: Image bytes read: 759781
I (26836) OTA_DEFAULT: Image bytes read: 760070
I (26836) OTA_DEFAULT: Image bytes read: 760359
I (26846) OTA_DEFAULT: Image bytes read: 760648
I (26846) OTA_DEFAULT: Image bytes read: 760937
I (26856) OTA_DEFAULT: Image bytes read: 761226
I (26856) OTA_DEFAULT: Image bytes read: 761515
I (26866) OTA_DEFAULT: Image bytes read: 761804
I (26866) OTA_DEFAULT: Image bytes read: 762093
I (26876) OTA_DEFAULT: Image bytes read: 762382
I (26876) OTA_DEFAULT: Image bytes read: 762671
I (26886) OTA_DEFAULT: Image bytes read: 762960
I (26886) OTA_DEFAULT: Image bytes read: 763249
I (26896) OTA_DEFAULT: Image bytes read: 763538
I (26896) OTA_DEFAULT: Image bytes read: 763827
I (26906) OTA_DEFAULT: Image bytes read: 764116
I (26906) OTA_DEFAULT: Image bytes read: 764405
I (26916) OTA_DEFAULT: Image bytes read: 764694
I (26926) OTA_DEFAULT: Image bytes read: 764983
I (26926) OTA_DEFAULT: Image bytes read: 765272
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

- 本例 HTTP 服务器端完整的 log，示例如下：

```c
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
