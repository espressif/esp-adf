# 核心转储服务 (Core Dump Service) 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了如何在 ADF 框架下配置核心转储（core dump）服务。通过引用一个不存在的汇编指令制造一个 crash，把核心转储的数据上传到 HTTP 服务器上，并由 HTTP 服务器接收并解析这些核心转储信息。


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

本例程需要先配置 Wi-Fi 连接信息，通过运行 `menuconfig > Example Configuration` 填写 `Wi-Fi SSID` 和 `Wi-Fi Password`。

```
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

本例程还需要配置接收核心转储数据的 HTTP 服务器（个人 PC）的 URI，请确保开发板和 HTTP 服务器处在同一个 Wi-Fi 局域网中，如 HTTP 服务器的 LAN IP 地址是 `192.168.5.72`，那么在 `menuconfig > Example Configuration` 中则配置为 `http://192.168.5.72:8000/upload`。

```
menuconfig > Core dump upload configuration > core dump upload uri
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

- 例程需要先在 Python 2.7 版本下运行 Python 脚本，且开发板和 HTTP 服务器要连接在同一个 Wi-Fi 网络下。

- 请预先设定 Python 脚本所需的 `$IDF_PATH` 和 `$ADF_PATH` 环境变量值。

- 下载 [esp32_rom.elf](https://dl.espressif.com/dl/esp32_rom.elf) 到 `$ADF_PATH/examples/system/coredump/tools` 目录下。

- 在 tools 目录下，运行接收和解析核心转储 HTTP 服务器的 Python 脚本 `coredump_http_server.py`，Python 脚本运行日志如下：

```
python2 $ADF_PATH/examples/system/coredump/tools/coredump_http_server.py -e $ADF_PATH/examples/system/coredump/build/coredump_example.elf -r $ADF_PATH/examples/system/coredump/tools/esp32_rom.elf
Serving HTTP on 0.0.0.0 port 8000
```

- 例程开始运行后，会先连接 Wi-Fi，Wi-Fi 连接成功后，如果是第一次运行，则将故意引用一条不存在的汇编指令自行制造 crash，核心转储服务会把此次的 crash 数据写入 flash 保存然后重启，重启后会检查上一次 crash，最后把这些数据上传到 HTTP 服务器。核心步骤日志打印如下：

```c
I (2921) wifi:AP's beacon interval = 102400 us, DTIM period = 3
W (2931) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4681) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (4681) PERIPH_WIFI: Got ip:192.168.5.187
I (4681) COREDUMP_UPLOAD: reset reason is 1
I (4691) coredump_example: result -1
Guru Meditation Error: Core  0 panic'ed (IllegalInstruction). Exception was unhandled.
Memory dump at 0x400d6b58: b4de653a 00020266 f01d0000
0x400d6b58: app_main at /repo/adfs/bugfix/esp-adf-internal/examples/system/coredump/build/../main/coredump_example.c:126 (discriminator 13)

Core  0 register dump:
PC      : 0x400d6b5f  PS      : 0x00060e30  A0      : 0x800d559d  A1      : 0x3ffbb800
0x400d6b5f: app_main at /repo/adfs/bugfix/esp-adf-internal/examples/system/coredump/build/../main/coredump_example.c:128

A2      : 0xffffffff  A3      : 0x3ffbeb2c  A4      : 0x00000001  A5      : 0x00060023
A6      : 0x00000001  A7      : 0x00060023  A8      : 0x800d6b5c  A9      : 0x3ffbb7b0
A10     : 0x00000003  A11     : 0x3f4035b8  A12     : 0x3f403730  A13     : 0x00001253
A14     : 0x3f4035b8  A15     : 0xffffffff  SAR     : 0x00000004  EXCCAUSE: 0x00000000
EXCVADDR: 0x00000000  LBEG    : 0x400014fd  LEND    : 0x4000150d  LCOUNT  : 0xfffffffb

Backtrace:0x400d6b5c:0x3ffbb800 0x400d559a:0x3ffbb890 0x40087a51:0x3ffbb8c0
0x400d6b5c: app_main at /repo/adfs/bugfix/esp-adf-internal/examples/system/coredump/build/../main/coredump_example.c:127 (discriminator 13)

0x400d559a: main_task at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:609 (discriminator 2)

0x40087a51: vPortTaskWrapper at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143



ELF file SHA256: 08bc5f71a7e35774

I (4748) esp_core_dump_flash: Save core dump to flash...
I (4754) esp_core_dump_elf: Found tasks: 11
I (4759) esp_core_dump_flash: Erase flash 20480 bytes @ 0x210000
I (5050) esp_core_dump_flash: Write end offset 0x41c4, check sum length 4
I (5051) esp_core_dump_flash: Core dump has been saved to flash.
Rebooting...
ets Jul 29 2019 12:21:46

rst:0xc (SW_CPU_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?


W (2905) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (2975) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (5165) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (5165) PERIPH_WIFI: Got ip:192.168.5.187
I (5165) COREDUMP_UPLOAD: reset reason is 4
I (5545) coredump_example: HTTP GET Status = 200, content_length = -1
I (5545) COREDUMP_UPLOAD: core dump upload success
W (5545) COREDUMP_UPLOAD: coredump uploader destroyed
I (5545) coredump_example: result 0

Done
```


### 日志输出

- 以下为本例程开发板端从启动到初始化的完整日志。

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
I (27) boot: compile time 19:09:15
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
I (106) boot:  5 coredump         Unknown data     01 03 00210000 00100000
I (113) boot: End of partition table
I (118) boot: No factory image, trying OTA 0
I (122) boot_comm: chip revision: 3, min. application chip revision: 0
I (130) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1f37c (127868) map
I (187) esp_image: segment 1: paddr=0x0002f3a4 vaddr=0x3ffb0000 size=0x00c74 (  3188) load
I (189) esp_image: segment 2: paddr=0x00030020 vaddr=0x400d0020 size=0x95c54 (613460) map
0x400d0020: _stext at ??:?

I (428) esp_image: segment 3: paddr=0x000c5c7c vaddr=0x3ffb0c74 size=0x03d70 ( 15728) load
I (434) esp_image: segment 4: paddr=0x000c99f4 vaddr=0x40080000 size=0x15a9c ( 88732) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (486) boot: Loaded app from partition at offset 0x10000
I (505) boot: Set actual ota_seq=1 in otadata[0]
I (506) boot: Disabling RNG early entropy source...
I (506) cpu_start: Pro cpu up.
I (509) cpu_start: Application information:
I (514) cpu_start: Project name:     coredump_example
I (519) cpu_start: App version:      v2.2-238-ga496ac5c
I (525) cpu_start: Compile time:     Nov 24 2021 17:41:45
I (531) cpu_start: ELF file SHA256:  08bc5f71a7e35774...
I (537) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (543) cpu_start: Starting app cpu, entry point is 0x40081914
0x40081914: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (554) heap_init: Initializing. RAM available for dynamic allocation:
I (561) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (567) heap_init: At 3FFB9888 len 00026778 (153 KiB): DRAM
I (573) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (579) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (586) heap_init: At 40095A9C len 0000A564 (41 KiB): IRAM
I (592) cpu_start: Pro cpu start user code
I (610) spi_flash: detected chip: gd
I (611) spi_flash: flash io: dio
W (611) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (621) esp_core_dump_flash: Init core dump to flash
I (627) esp_core_dump_flash: Found partition 'coredump' @ 210000 1048576 bytes
I (635) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (681) coredump_example: [1.0] Initialize peripherals management
I (681) coredump_example: [1.1] Start and wait for Wi-Fi network
I (691) wifi:wifi driver task: 3ffc3648, prio:23, stack:6656, core=0
I (691) system_api: Base MAC address is not set
I (691) system_api: read default base MAC address from EFUSE
I (701) wifi:wifi firmware version: bb6888c
I (701) wifi:wifi certification version: v7.0
I (701) wifi:config NVS flash: enabled
I (711) wifi:config nano formating: disabled
I (711) wifi:Init data frame dynamic rx buffer num: 32
I (721) wifi:Init management frame dynamic rx buffer num: 32
I (721) wifi:Init management short buffer num: 32
I (731) wifi:Init dynamic tx buffer num: 32
I (731) wifi:Init static rx buffer size: 1600
I (741) wifi:Init static rx buffer num: 10
I (741) wifi:Init dynamic rx buffer num: 32
I (741) wifi_init: rx ba win: 6
I (751) wifi_init: tcpip mbox: 32
I (751) wifi_init: udp mbox: 6
I (751) wifi_init: tcp mbox: 6
I (761) wifi_init: tcp tx win: 5744
I (761) wifi_init: tcp rx win: 5744
I (771) wifi_init: tcp mss: 1440
I (771) wifi_init: WiFi IRAM OP enabled
I (771) wifi_init: WiFi RX IRAM OP enabled
I (781) phy_init: phy_version 4660,0162888,Dec 23 2020
I (891) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2111) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2891) wifi:state: init -> auth (b0)
I (2901) wifi:state: auth -> assoc (0)
I (2911) wifi:state: assoc -> run (10)
I (2921) wifi:connected with esp32, aid = 3, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (2921) wifi:security: WPA2-PSK, phy: bgn, rssi: -35
I (2921) wifi:pm start, type: 1

I (2921) wifi:AP's beacon interval = 102400 us, DTIM period = 3
W (2931) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4681) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (4681) PERIPH_WIFI: Got ip:192.168.5.187
I (4681) COREDUMP_UPLOAD: reset reason is 1
I (4691) coredump_example: result -1
Guru Meditation Error: Core  0 panic'ed (IllegalInstruction). Exception was unhandled.
Memory dump at 0x400d6b58: b4de653a 00020266 f01d0000
0x400d6b58: app_main at /repo/adfs/bugfix/esp-adf-internal/examples/system/coredump/build/../main/coredump_example.c:126 (discriminator 13)

Core  0 register dump:
PC      : 0x400d6b5f  PS      : 0x00060e30  A0      : 0x800d559d  A1      : 0x3ffbb800
0x400d6b5f: app_main at /repo/adfs/bugfix/esp-adf-internal/examples/system/coredump/build/../main/coredump_example.c:128

A2      : 0xffffffff  A3      : 0x3ffbeb2c  A4      : 0x00000001  A5      : 0x00060023
A6      : 0x00000001  A7      : 0x00060023  A8      : 0x800d6b5c  A9      : 0x3ffbb7b0
A10     : 0x00000003  A11     : 0x3f4035b8  A12     : 0x3f403730  A13     : 0x00001253
A14     : 0x3f4035b8  A15     : 0xffffffff  SAR     : 0x00000004  EXCCAUSE: 0x00000000
EXCVADDR: 0x00000000  LBEG    : 0x400014fd  LEND    : 0x4000150d  LCOUNT  : 0xfffffffb

Backtrace:0x400d6b5c:0x3ffbb800 0x400d559a:0x3ffbb890 0x40087a51:0x3ffbb8c0
0x400d6b5c: app_main at /repo/adfs/bugfix/esp-adf-internal/examples/system/coredump/build/../main/coredump_example.c:127 (discriminator 13)

0x400d559a: main_task at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:609 (discriminator 2)

0x40087a51: vPortTaskWrapper at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143



ELF file SHA256: 08bc5f71a7e35774

I (4748) esp_core_dump_flash: Save core dump to flash...
I (4754) esp_core_dump_elf: Found tasks: 11
I (4759) esp_core_dump_flash: Erase flash 20480 bytes @ 0x210000
I (5050) esp_core_dump_flash: Write end offset 0x41c4, check sum length 4
I (5051) esp_core_dump_flash: Core dump has been saved to flash.
Rebooting...
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
I (27) boot: compile time 19:09:15
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
I (105) boot:  5 coredump         Unknown data     01 03 00210000 00100000
I (113) boot: End of partition table
I (117) boot_comm: chip revision: 3, min. application chip revision: 0
I (125) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1f37c (127868) map
I (182) esp_image: segment 1: paddr=0x0002f3a4 vaddr=0x3ffb0000 size=0x00c74 (  3188) load
I (184) esp_image: segment 2: paddr=0x00030020 vaddr=0x400d0020 size=0x95c54 (613460) map
0x400d0020: _stext at ??:?

I (422) esp_image: segment 3: paddr=0x000c5c7c vaddr=0x3ffb0c74 size=0x03d70 ( 15728) load
I (429) esp_image: segment 4: paddr=0x000c99f4 vaddr=0x40080000 size=0x15a9c ( 88732) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (481) boot: Loaded app from partition at offset 0x10000
I (481) boot: Disabling RNG early entropy source...
I (481) cpu_start: Pro cpu up.
I (485) cpu_start: Application information:
I (490) cpu_start: Project name:     coredump_example
I (495) cpu_start: App version:      v2.2-238-ga496ac5c
I (501) cpu_start: Compile time:     Nov 24 2021 17:41:45
I (507) cpu_start: ELF file SHA256:  08bc5f71a7e35774...
I (513) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (519) cpu_start: Starting app cpu, entry point is 0x40081914
0x40081914: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (513) cpu_start: App cpu up.
I (530) heap_init: Initializing. RAM available for dynamic allocation:
I (537) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (543) heap_init: At 3FFB9888 len 00026778 (153 KiB): DRAM
I (549) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (555) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (562) heap_init: At 40095A9C len 0000A564 (41 KiB): IRAM
I (568) cpu_start: Pro cpu start user code
I (586) spi_flash: detected chip: gd
I (587) spi_flash: flash io: dio
W (587) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (597) esp_core_dump_flash: Init core dump to flash
I (604) esp_core_dump_flash: Found partition 'coredump' @ 210000 1048576 bytes
I (615) esp_core_dump_flash: Found core dump 16836 bytes in flash @ 0x210000
I (618) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (665) coredump_example: [1.0] Initialize peripherals management
I (665) coredump_example: [1.1] Start and wait for Wi-Fi network
I (675) wifi:wifi driver task: 3ffc3648, prio:23, stack:6656, core=0
I (675) system_api: Base MAC address is not set
I (675) system_api: read default base MAC address from EFUSE
I (685) wifi:wifi firmware version: bb6888c
I (685) wifi:wifi certification version: v7.0
I (685) wifi:config NVS flash: enabled
I (695) wifi:config nano formating: disabled
I (695) wifi:Init data frame dynamic rx buffer num: 32
I (705) wifi:Init management frame dynamic rx buffer num: 32
I (705) wifi:Init management short buffer num: 32
I (715) wifi:Init dynamic tx buffer num: 32
I (715) wifi:Init static rx buffer size: 1600
I (725) wifi:Init static rx buffer num: 10
I (725) wifi:Init dynamic rx buffer num: 32
I (725) wifi_init: rx ba win: 6
I (735) wifi_init: tcpip mbox: 32
I (735) wifi_init: udp mbox: 6
I (735) wifi_init: tcp mbox: 6
I (745) wifi_init: tcp tx win: 5744
I (745) wifi_init: tcp rx win: 5744
I (755) wifi_init: tcp mss: 1440
I (755) wifi_init: WiFi IRAM OP enabled
I (755) wifi_init: WiFi RX IRAM OP enabled
I (765) phy_init: phy_version 4660,0162888,Dec 23 2020
I (875) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2095) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2865) wifi:state: init -> auth (b0)
I (2885) wifi:state: auth -> assoc (0)
I (2885) wifi:state: assoc -> run (10)
I (2905) wifi:connected with esp32, aid = 3, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (2905) wifi:security: WPA2-PSK, phy: bgn, rssi: -36
I (2905) wifi:pm start, type: 1

W (2905) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (2975) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (5165) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (5165) PERIPH_WIFI: Got ip:192.168.5.187
I (5165) COREDUMP_UPLOAD: reset reason is 4
I (5545) coredump_example: HTTP GET Status = 200, content_length = -1
I (5545) COREDUMP_UPLOAD: core dump upload success
W (5545) COREDUMP_UPLOAD: coredump uploader destroyed
I (5545) coredump_example: result 0

Done
```


- 以下为 HTTP 服务器端从接收核心转储数据到解析核心转储数据的完整日志。

```c
(py27)  hengyongchao@ubuntu-18-04-5-lts  /repo/adfs/bugfix/esp-adf-internal  python2 $ADF_PATH/examples/system/coredump/tools/coredump_http_server.py -e $ADF_PATH/examples/system/coredump/build/coredump_example.elf -r $ADF_PATH/examples/system/coredump/tools/esp32_rom.elf
Serving HTTP on 0.0.0.0 port 8000
===============================================================
==================== ESP32 CORE DUMP START ====================

Crashed task handle: 0x3ffbb954, name: 'main', GDB name: 'process 1073461588'

================== CURRENT THREAD REGISTERS ===================
exccause       0x0 (IllegalInstructionCause)
excvaddr       0x0
epc1           0x0
epc2           0x0
epc3           0x0
epc4           0x0
epc5           0x0
epc6           0x0
epc7           0x0
eps2           0x0
eps3           0x0
eps4           0x0
eps5           0x0
eps6           0x0
eps7           0x4016232f
pc             0x400d6b5f   0x400d6b5f <app_main+267>
lbeg           0x400014fd   1073747197
lend           0x4000150d   1073747213
lcount         0xfffffffb   4294967291
sar            0x4  4
ps             0x60e20  396832
threadptr      <unavailable>
br             <unavailable>
scompare1      <unavailable>
acclo          <unavailable>
acchi          <unavailable>
m0             <unavailable>
m1             <unavailable>
m2             <unavailable>
m3             <unavailable>
expstate       <unavailable>
f64r_lo        <unavailable>
f64r_hi        <unavailable>
f64s           <unavailable>
fcr            <unavailable>
fsr            <unavailable>
a0             0x800d559d   -2146609763
a1             0x3ffbb800   1073461248
a2             0xffffffff   -1
a3             0x3ffbeb2c   1073474348
a4             0x1  1
a5             0x60023  393251
a6             0x1  1
a7             0x60023  393251
a8             0x800d6b5c   -2146604196
a9             0x3ffbb7b0   1073461168
a10            0x3  3
a11            0x3f4035b8   1061172664
a12            0x3f403730   1061173040
a13            0x1253   4691
a14            0x3f4035b8   1061172664
a15            0xffffffff   -1

==================== CURRENT THREAD STACK =====================
#0  app_main () at ../main/coredump_example.c:128
#1  0x400d559d in main_task (args=<optimized out>) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:609
#2  0x40087a54 in vPortTaskWrapper (pxCode=0x400d5520 <main_task>, pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

======================== THREADS INFO =========================
  Id   Target Id         Frame
  11   process 1073413692 0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
  10   process 1073493576 0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
  9    process 1073483528 0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
  8    process 1073412788 0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
  7    process 1073455444 0x4008ab1c in xQueueGenericReceive (xQueue=0x3ffb9cfc, pvBuffer=0x0, xTicksToWait=<optimized out>, xJustPeeking=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
  6    process 1073468020 0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
  5    process 1073479648 0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
  4    process 1073473660 0x4008ab1c in xQueueGenericReceive (xQueue=0x3ffbd9a4, pvBuffer=0x3ffbe7c0, xTicksToWait=<optimized out>, xJustPeeking=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
  3    process 1073463480 0x40163402 in esp_pm_impl_waiti () at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/pm_esp32.c:501
  2    process 1073465372 0x40163402 in esp_pm_impl_waiti () at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/pm_esp32.c:501
* 1    process 1073461588 app_main () at ../main/coredump_example.c:128

==================== THREAD 11 (TCB: 0x3ffafe3c, name: 'ipc0') =====================
#0  0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
#1  0x4008ab1c in xQueueGenericReceive (xQueue=0x3ffafde8 <r_modules_funcs+124>, pvBuffer=0x0, xTicksToWait=<optimized out>, xJustPeeking=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
#2  0x400832cb in ipc_task (arg=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_ipc/ipc.c:51
#3  0x40087a54 in vPortTaskWrapper (pxCode=0x40083298 <ipc_task>, pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 10 (TCB: 0x3ffc3648, name: 'wifi') =====================
#0  0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
#1  0x4008ab1c in xQueueGenericReceive (xQueue=0x3ffc155c, pvBuffer=0x3ffc3590, xTicksToWait=<optimized out>, xJustPeeking=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
#2  0x400e9a94 in queue_recv_wrapper (queue=0x3ffc155c, item=0x3ffc3590, block_time_tick=4294967295) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_wifi/esp32/esp_adapter.c:350
#3  0x4009012c in ppTask () at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:125
#4  0x40087a54 in vPortTaskWrapper (pxCode=0x400900f8 <ppTask>, pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 9 (TCB: 0x3ffc0f08, name: 'sys_evt') =====================
#0  0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
#1  0x4008ab1c in xQueueGenericReceive (xQueue=0x3ffc015c, pvBuffer=0x3ffc0e20, xTicksToWait=<optimized out>, xJustPeeking=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
#2  0x4016542c in esp_event_loop_run (event_loop=0x3ffc0140, ticks_to_run=4294967295) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_event/esp_event.c:624
#3  0x40165444 in esp_event_loop_run_task (args=0x3ffc0140) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_event/esp_event.c:115
#4  0x40087a54 in vPortTaskWrapper (pxCode=0x40165438 <esp_event_loop_run_task>, pvParameters=0x3ffc0140) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 8 (TCB: 0x3ffafab4, name: 'esp_timer') =====================
#0  0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
#1  0x4008ab1c in xQueueGenericReceive (xQueue=0x3ffaea5c <r_ip_funcs+844>, pvBuffer=0x0, xTicksToWait=<optimized out>, xJustPeeking=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
#2  0x400d6410 in timer_task (arg=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_timer/src/esp_timer.c:329
#3  0x40087a54 in vPortTaskWrapper (pxCode=0x400d63fc <timer_task>, pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 7 (TCB: 0x3ffba154, name: 'ipc1') =====================
#0  0x4008ab1c in xQueueGenericReceive (xQueue=0x3ffb9cfc, pvBuffer=0x0, xTicksToWait=<optimized out>, xJustPeeking=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
#1  0x400832cb in ipc_task (arg=0x1) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_ipc/ipc.c:51
#2  0x40087a54 in vPortTaskWrapper (pxCode=0x40083298 <ipc_task>, pvParameters=0x1) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 6 (TCB: 0x3ffbd274, name: 'Tmr Svc') =====================
#0  0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
#1  0x40089e12 in prvProcessTimerOrBlockTask (xNextExpireTime=<optimized out>, xListWasEmpty=<optimized out>) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
#2  0x40089eff in prvTimerTask (pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/timers.c:544
#3  0x40087a54 in vPortTaskWrapper (pxCode=0x40089ef0 <prvTimerTask>, pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 5 (TCB: 0x3ffbffe0, name: 'esp_periph') =====================
#0  0x40081bd8 in esp_crosscore_int_send_yield (core_id=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/crosscore_int.c:115
#1  0x4008ab1c in xQueueGenericReceive (xQueue=0x3ffbed6c, pvBuffer=0x3ffbfef0, xTicksToWait=<optimized out>, xJustPeeking=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
#2  0x40103f5c in audio_event_iface_waiting_cmd_msg (evt=0x3ffbed38) at /repo/adfs/bugfix/esp-adf-internal/components/audio_pipeline/audio_event_iface.c:244
#3  0x400d712c in esp_periph_task (pv=0x3ffbeb2c) at /repo/adfs/bugfix/esp-adf-internal/components/esp_peripherals/esp_peripherals.c:112
#4  0x40087a54 in vPortTaskWrapper (pxCode=0x400d70d0 <esp_periph_task>, pvParameters=0x3ffbeb2c) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 4 (TCB: 0x3ffbe87c, name: 'tiT') =====================
#0  0x4008ab1c in xQueueGenericReceive (xQueue=0x3ffbd9a4, pvBuffer=0x3ffbe7c0, xTicksToWait=<optimized out>, xJustPeeking=0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/cpu_ll.h:39
#1  0x400fec10 in sys_arch_mbox_fetch (mbox=<optimized out>, msg=0x3ffbe7c0, timeout=490) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/lwip/port/esp32/freertos/sys_arch.c:325
#2  0x400ed7af in tcpip_timeouts_mbox_fetch (mbox=0x3ffb5ecc <tcpip_mbox>, msg=0x3ffbe7c0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/lwip/lwip/src/api/tcpip.c:110
#3  0x400ed867 in tcpip_thread (arg=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/lwip/lwip/src/api/tcpip.c:148
#4  0x40087a54 in vPortTaskWrapper (pxCode=0x400ed84c <tcpip_thread>, pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 3 (TCB: 0x3ffbc0b8, name: 'IDLE0') =====================
#0  0x40163402 in esp_pm_impl_waiti () at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/pm_esp32.c:501
#1  0x400d6285 in esp_vApplicationIdleHook () at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_common/src/freertos_hooks.c:63
#2  0x40088438 in prvIdleTask (pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/tasks.c:3427
#3  0x40087a54 in vPortTaskWrapper (pxCode=0x4008842c <prvIdleTask>, pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 2 (TCB: 0x3ffbc81c, name: 'IDLE1') =====================
#0  0x40163402 in esp_pm_impl_waiti () at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/pm_esp32.c:501
#1  0x400d6285 in esp_vApplicationIdleHook () at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp_common/src/freertos_hooks.c:63
#2  0x40088438 in prvIdleTask (pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/tasks.c:3427
#3  0x40087a54 in vPortTaskWrapper (pxCode=0x4008842c <prvIdleTask>, pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143

==================== THREAD 1 (TCB: 0x3ffbb954, name: 'main') =====================
#0  app_main () at ../main/coredump_example.c:128
#1  0x400d559d in main_task (args=<optimized out>) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:609
#2  0x40087a54 in vPortTaskWrapper (pxCode=0x400d5520 <main_task>, pvParameters=0x0) at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:143


======================= ALL MEMORY REGIONS ========================
Name   Address   Size   Attrs
.rtc.text 0x400c0000 0x0 RW
.rtc.dummy 0x3ff80000 0x0 RW
.rtc.force_fast 0x3ff80000 0x0 RW
.rtc_noinit 0x50000000 0x0 RW
.rtc.force_slow 0x50000000 0x0 RW
.iram0.vectors 0x40080000 0x404 R XA
.iram0.text 0x40080404 0x15698 R XA
.dram0.data 0x3ffb0000 0x49e4 RW A
.noinit 0x3ffb49e4 0x0 RW
.flash.appdesc 0x3f400020 0x100 R  A
.flash.rodata 0x3f400120 0x1f27c RW A
.flash.text 0x400d0020 0x95c54 R XA
.iram0.data 0x40095a9c 0x0 RW
.iram0.bss 0x40095a9c 0x0 RW
.dram0.heap_start 0x3ffb9888 0x0 RW
.coredump.tasks.data 0x3ffbb954 0x15c RW
.coredump.tasks.data 0x3ffbb740 0x20c RW
.coredump.tasks.data 0x3ffbc81c 0x15c RW
.coredump.tasks.data 0x3ffbc670 0x1a4 RW
.coredump.tasks.data 0x3ffbc0b8 0x15c RW
.coredump.tasks.data 0x3ffbbf10 0x1a0 RW
.coredump.tasks.data 0x3ffbe87c 0x15c RW
.coredump.tasks.data 0x3ffbe670 0x204 RW
.coredump.tasks.data 0x3ffbffe0 0x15c RW
.coredump.tasks.data 0x3ffbfdd0 0x208 RW
.coredump.tasks.data 0x3ffbd274 0x15c RW
.coredump.tasks.data 0x3ffbd0a0 0x1cc RW
.coredump.tasks.data 0x3ffba154 0x15c RW
.coredump.tasks.data 0x3ffb9fa0 0x1ac RW
.coredump.tasks.data 0x3ffafab4 0x15c RW
.coredump.tasks.data 0x3ffaf8e0 0x1cc RW
.coredump.tasks.data 0x3ffc0f08 0x15c RW
.coredump.tasks.data 0x3ffc0d00 0x200 RW
.coredump.tasks.data 0x3ffc3648 0x15c RW
.coredump.tasks.data 0x3ffc3450 0x1f0 RW
.coredump.tasks.data 0x3ffafe3c 0x15c RW
.coredump.tasks.data 0x3ffb9ae0 0x1c0 RW

===================== ESP32 CORE DUMP END =====================
===============================================================
Done!
192.168.5.187 - - [24/Nov/2021 17:42:34] "POST /upload?app_version=v2.2-238-ga496ac5c&proj_name=coredump_example&idf_version=v4.2.2-1-g379ca2123 HTTP/1.1" 200 -

```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
