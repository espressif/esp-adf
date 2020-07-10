# Example of COREDUMP_UPLOAD_SERVICE

This example shows how to configure the `coredump_upload_service` to upload core dump raw data to the backend server.
Please refer to `https://docs.espressif.com/projects/esp-idf/en/stable/api-guides/core_dump.html` for more information about `Core Dump`.

## How to use example

### Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |

### Configure the project

1. Run `make menuconfig` or `idf.py menuconfig`.
2. Select the board type in `Audio HAL`.
3. Configure `WiFi SSID` and `WiFi Password` in `Core dump upload configuration`.
4. Configure `core dump upload uri` in `Core dump upload configuration` to the images, the `path` must be `/upload`.

### Build and flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
make -j4 flash monitor
```

Or, for CMake based build system (replace PORT with serial port name):

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

### Startup `coredump_http_server.py`

This example provide a HTTP server located in `$ADF_PATH/examples/coredump/tools`.
The server can receive the core dump raw data from device and parse it with 'espcoredump.py'.

* Setup IDF_PATH by `export $ADF_PATH/esp-idf`.
* Download [esp32_rom.elf](https://dl.espressif.com/dl/esp32_rom.elf) to `$ADF_PATH/examples/coredump/tools`.
* Get help with `python2 $ADF_PATH/examples/coredump/tools/coredump_http_server.py -h`.
* Startup server with `python2 $ADF_PATH/examples/coredump/tools/coredump_http_server.py -e $ADF_PATH/examples/coredump/build/coredump_example.elf -r $ADF_PATH/examples/coredump/tools/esp32_rom.elf`.

## Example output

Here is an example console output.

Device boot up and check the reset reason, if no need to upload the core dump data, it will run an illegal instruction to cause a panic, otherwise upload the core dump data.

```
rst:0x1 (POWERON_RESET),boot:0x3f (SPI_FAST_FLASH_BOOT)
flash read err, 1000
ets_main.c 371
ets Jun  8 2016 00:22:57

rst:0x10 (RTCWDT_RTC_RESET),boot:0x3f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0018,len:4
load:0x3fff001c,len:6836
load:0x40078000,len:13232
load:0x40080400,len:7200
entry 0x40080778
I (65) boot: Chip Revision: 1
I (65) boot_comm: chip revision: 1, min. bootloader chip revision: 0
I (39) boot: ESP-IDF v3.3.2-107-g722043f73 2nd stage bootloader
I (39) boot: compile time 10:45:21
I (47) boot: Enabling RNG early entropy source...
I (47) boot: SPI Speed      : 80MHz
I (49) boot: SPI Mode       : DIO
I (53) boot: SPI Flash Size : 4MB
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 otadata          OTA data         01 00 0000d000 00002000
I (83) boot:  2 phy_init         RF data          01 01 0000f000 00001000
I (90) boot:  3 ota_0            OTA app          00 10 00010000 00100000
I (98) boot:  4 ota_1            OTA app          00 11 00110000 00100000
I (105) boot:  5 coredump         Unknown data     01 03 00210000 00100000
I (113) boot: End of partition table
I (117) boot_comm: chip revision: 1, min. application chip revision: 0
I (124) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2a40c (173068) map
I (184) esp_image: segment 1: paddr=0x0003a434 vaddr=0x3ffb0000 size=0x03d64 ( 15716) load
I (190) esp_image: segment 2: paddr=0x0003e1a0 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (191) esp_image: segment 3: paddr=0x0003e5a8 vaddr=0x40080400 size=0x01a68 (  6760) load
I (202) esp_image: segment 4: paddr=0x00040018 vaddr=0x400d0018 size=0x90140 (590144) map
0x400d0018: _stext at ??:?

I (381) esp_image: segment 5: paddr=0x000d0160 vaddr=0x40081e68 size=0x181f8 ( 98808) load
0x40081e68: timer_count_reload at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/esp_timer_esp32.c:351
 (inlined by) timer_alarm_isr at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/esp_timer_esp32.c:264

I (433) boot: Loaded app from partition at offset 0x10000
I (433) boot: Disabling RNG early entropy source...
I (434) psram: This chip is ESP32-D0WD
I (439) spiram: Found 32MBit SPI RAM device
I (443) spiram: SPI RAM mode: flash 80m sram 40m
I (448) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (455) cpu_start: Pro cpu up.
I (459) cpu_start: Application information:
I (464) cpu_start: Project name:     esp-idf
I (469) cpu_start: App version:      v1.0-513-g65eaad6b
I (475) cpu_start: Compile time:     Jun 29 2020 15:09:54
I (481) cpu_start: ELF file SHA256:  0ee0dea91e06cdc8...
I (487) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (493) cpu_start: Starting app cpu, entry point is 0x40081564
0x40081564: call_start_cpu1 at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (1393) spiram: SPI SRAM memory test OK
I (1395) heap_init: Initializing. RAM available for dynamic allocation:
I (1395) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1400) heap_init: At 3FFB5798 len 0002A868 (170 KiB): DRAM
I (1406) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1413) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1419) heap_init: At 4009A060 len 00005FA0 (23 KiB): IRAM
I (1425) cpu_start: Pro cpu start user code
I (1430) spiram: Adding pool of 4077K of external SPI memory to heap allocator
I (109) esp_core_dump_flash: Init core dump to flash
I (119) esp_core_dump_flash: Found partition 'coredump' @ 210000 1048576 bytes
I (140) cpu_start: Found core dump 8992 bytes in flash @ 0x210000
I (140) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (146) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (186) coredump_example: [1.0] Initialize peripherals management
I (186) coredump_example: [1.1] Start and wait for Wi-Fi network
I (196) wifi:wifi driver task: 3ffc6988, prio:23, stack:3584, core=0
I (196) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (206) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (246) wifi:wifi firmware version: 5f8804c
I (246) wifi:config NVS flash: enabled
I (246) wifi:config nano formating: disabled
I (246) wifi:Init dynamic tx buffer num: 32
I (256) wifi:Init data frame dynamic rx buffer num: 32
I (256) wifi:Init management frame dynamic rx buffer num: 32
I (266) wifi:Init management short buffer num: 32
I (266) wifi:Init static tx buffer num: 16
I (276) wifi:Init static rx buffer size: 1600
I (276) wifi:Init static rx buffer num: 10
I (276) wifi:Init dynamic rx buffer num: 32
I (376) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 0
I (376) wifi:mode : sta (24:0a:c4:9c:fe:a4)
I (1706) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2836) wifi:state: init -> auth (b0)
I (2846) wifi:state: auth -> assoc (0)
I (2856) wifi:state: assoc -> run (10)
I (2876) wifi:connected with ESP-Audio, aid = 4, channel 11, BW20, bssid = 18:31:bf:4b:8b:68
I (2876) wifi:security type: 3, phy: bgn, rssi: -42
I (2876) wifi:pm start, type: 1

I (2876) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (3686) event: sta ip: 192.168.50.47, mask: 255.255.255.0, gw: 192.168.50.1
I (3686) coredump_upload: reset reason is 7
I (3686) coredump_example: result -1
Guru Meditation Error: Core  0 panic'ed (IllegalInstruction). Exception was unhandled.
Memory dump at 0x400d5b4c: 0019653a 00020266 f01d0000
0x400d5b4c: app_main at /home/joseph/esp/esp-adf-internal/examples/coredump/main/coredump_example.c:70 (discriminator 9)

Core 0 register dump:
PC      : 0x400d5b53  PS      : 0x00060830  A0      : 0x800d13f3  A1      : 0x3ffb7670
0x400d5b53: app_main at /home/joseph/esp/esp-adf-internal/examples/coredump/main/coredump_example.c:72

A2      : 0xffffffff  A3      : 0x3f804930  A4      : 0x00000001  A5      : 0x3ffb8f60
A6      : 0x00000000  A7      : 0x3ffb90b4  A8      : 0x800d5b50  A9      : 0x3ffb7620
A10     : 0x00000003  A11     : 0x3f4084ac  A12     : 0x3f408578  A13     : 0x00000e66
A14     : 0x3f4084ac  A15     : 0xffffffff  SAR     : 0x00000004  EXCCAUSE: 0x00000000
EXCVADDR: 0x00000000  LBEG    : 0x40092121  LEND    : 0x40092131  LCOUNT  : 0xfffffffb
0x40092121: strlen at /home/jeroen/esp8266/esp32/newlib_xtensa-2.2.0-bin/newlib_xtensa-2.2.0/xtensa-esp32-elf/newlib/libc/machine/xtensa/../../../../.././newlib/libc/machine/xtensa/strlen.S:84

0x40092131: strlen at /home/jeroen/esp8266/esp32/newlib_xtensa-2.2.0-bin/newlib_xtensa-2.2.0/xtensa-esp32-elf/newlib/libc/machine/xtensa/../../../../.././newlib/libc/machine/xtensa/strlen.S:96


ELF file SHA256: 0ee0dea91e06cdc8

Backtrace: 0x400d5b53:0x3ffb7670 0x400d13f0:0x3ffb76d0 0x4008b0ba:0x3ffb76f0
0x400d5b53: app_main at /home/joseph/esp/esp-adf-internal/examples/coredump/main/coredump_example.c:72

0x400d13f0: main_task at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:551

0x4008b0ba: vPortTaskWrapper at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/port.c:435


I (3762) esp_core_dump_flash: Save core dump to flash...
I (3767) esp_core_dump_common: Found tasks: (11)!
I (4076) esp_core_dump_flash: Core dump has been saved to flash.
Rebooting...
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x3f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0018,len:4
load:0x3fff001c,len:6836
load:0x40078000,len:13232
load:0x40080400,len:7200
entry 0x40080778
I (65) boot: Chip Revision: 1
I (65) boot_comm: chip revision: 1, min. bootloader chip revision: 0
I (39) boot: ESP-IDF v3.3.2-107-g722043f73 2nd stage bootloader
I (39) boot: compile time 10:45:21
I (48) boot: Enabling RNG early entropy source...
I (48) boot: SPI Speed      : 80MHz
I (50) boot: SPI Mode       : DIO
I (54) boot: SPI Flash Size : 4MB
I (58) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (69) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 otadata          OTA data         01 00 0000d000 00002000
I (84) boot:  2 phy_init         RF data          01 01 0000f000 00001000
I (91) boot:  3 ota_0            OTA app          00 10 00010000 00100000
I (99) boot:  4 ota_1            OTA app          00 11 00110000 00100000
I (106) boot:  5 coredump         Unknown data     01 03 00210000 00100000
I (114) boot: End of partition table
I (118) boot_comm: chip revision: 1, min. application chip revision: 0
I (125) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2a40c (173068) map
I (185) esp_image: segment 1: paddr=0x0003a434 vaddr=0x3ffb0000 size=0x03d64 ( 15716) load
I (190) esp_image: segment 2: paddr=0x0003e1a0 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (192) esp_image: segment 3: paddr=0x0003e5a8 vaddr=0x40080400 size=0x01a68 (  6760) load
I (203) esp_image: segment 4: paddr=0x00040018 vaddr=0x400d0018 size=0x90140 (590144) map
0x400d0018: _stext at ??:?

I (381) esp_image: segment 5: paddr=0x000d0160 vaddr=0x40081e68 size=0x181f8 ( 98808) load
0x40081e68: timer_count_reload at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/esp_timer_esp32.c:351
 (inlined by) timer_alarm_isr at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/esp_timer_esp32.c:264

I (434) boot: Loaded app from partition at offset 0x10000
I (434) boot: Disabling RNG early entropy source...
I (434) psram: This chip is ESP32-D0WD
I (440) spiram: Found 32MBit SPI RAM device
I (444) spiram: SPI RAM mode: flash 80m sram 40m
I (449) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (456) cpu_start: Pro cpu up.
I (460) cpu_start: Application information:
I (465) cpu_start: Project name:     esp-idf
I (470) cpu_start: App version:      v1.0-513-g65eaad6b
I (475) cpu_start: Compile time:     Jun 29 2020 15:09:54
I (482) cpu_start: ELF file SHA256:  0ee0dea91e06cdc8...
I (488) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (494) cpu_start: Starting app cpu, entry point is 0x40081564
0x40081564: call_start_cpu1 at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (481) cpu_start: App cpu up.
I (1394) spiram: SPI SRAM memory test OK
I (1396) heap_init: Initializing. RAM available for dynamic allocation:
I (1396) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1401) heap_init: At 3FFB5798 len 0002A868 (170 KiB): DRAM
I (1407) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1413) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1420) heap_init: At 4009A060 len 00005FA0 (23 KiB): IRAM
I (1426) cpu_start: Pro cpu start user code
I (1431) spiram: Adding pool of 4077K of external SPI memory to heap allocator
I (110) esp_core_dump_flash: Init core dump to flash
I (120) esp_core_dump_flash: Found partition 'coredump' @ 210000 1048576 bytes
I (140) cpu_start: Found core dump 8992 bytes in flash @ 0x210000
I (141) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (147) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (187) coredump_example: [1.0] Initialize peripherals management
I (187) coredump_example: [1.1] Start and wait for Wi-Fi network
I (197) wifi:wifi driver task: 3ffc6988, prio:23, stack:3584, core=0
I (197) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (207) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (347) wifi:wifi firmware version: 5f8804c
I (347) wifi:config NVS flash: enabled
I (347) wifi:config nano formating: disabled
I (347) wifi:Init dynamic tx buffer num: 32
I (357) wifi:Init data frame dynamic rx buffer num: 32
I (357) wifi:Init management frame dynamic rx buffer num: 32
I (367) wifi:Init management short buffer num: 32
I (367) wifi:Init static tx buffer num: 16
I (377) wifi:Init static rx buffer size: 1600
I (377) wifi:Init static rx buffer num: 10
I (377) wifi:Init dynamic rx buffer num: 32
I (477) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 0
I (477) wifi:mode : sta (24:0a:c4:9c:fe:a4)
I (1807) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2937) wifi:state: init -> auth (b0)
I (2937) wifi:state: auth -> assoc (0)
I (2947) wifi:state: assoc -> run (10)
I (2967) wifi:connected with ESP-Audio, aid = 4, channel 11, BW20, bssid = 18:31:bf:4b:8b:68
I (2967) wifi:security type: 3, phy: bgn, rssi: -43
I (2967) wifi:pm start, type: 1

I (2987) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (3737) event: sta ip: 192.168.50.47, mask: 255.255.255.0, gw: 192.168.50.1
I (3737) coredump_upload: reset reason is 4
I (4017) coredump_upload: HTTP GET Status = 200, content_length = -1
I (4017) coredump_upload: core dump upload success
W (4017) coredump_upload: coredump uploader destroyed
I (4027) coredump_example: result 0
```

### Server output

Here is the server output when receive the core dump data.

```
Serving HTTP on 0.0.0.0 port 8000
===============================================================
==================== ESP32 CORE DUMP START ====================

================== CURRENT THREAD REGISTERS ===================
pc             0x400d5b53       0x400d5b53 <app_main+263>
lbeg           0x40092121       1074340129
lend           0x40092131       1074340145
lcount         0xfffffffb       4294967291
sar            0x4      4
ps             0x60820  395296
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
a0             0x400d13f3       1074598899
a1             0x3ffb7670       1073444464
a2             0xffffffff       -1
a3             0x3f804930       1065371952
a4             0x1      1
a5             0x3ffb8f60       1073450848
a6             0x0      0
a7             0x3ffb90b4       1073451188
a8             0x800d5b50       -2146608304
a9             0x3ffb7620       1073444384
a10            0x3      3
a11            0x3f4084ac       1061192876
a12            0x3f408578       1061193080
a13            0xe95    3733
a14            0x3f4084ac       1061192876
a15            0xffffffff       -1

==================== CURRENT THREAD STACK =====================
#0  app_main () at /home/joseph/esp/esp-adf-internal/examples/coredump/main/coredump_example.c:72
#1  0x400d13f3 in main_task (args=<optimized out>) at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:551
#2  0x4008b0bd in vPortTaskWrapper (pxCode=0x400d136c <main_task>, pvParameters=0x0) at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/port.c:143

======================== THREADS INFO =========================
  Id   Target Id         Frame
  11   process 10        0x4008c0c9 in xQueueGenericReceive (xQueue=0x3ffafda4 <r_modules_funcs+56>, pvBuffer=0x0, xTicksToWait=4294967295, xJustPeeking=0) at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/queue.c:1592
  10   process 9         0x4008c0c9 in xQueueGenericReceive (xQueue=0x3ffc5494, pvBuffer=0x3ffc68d0, xTicksToWait=4294967295, xJustPeeking=0) at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/queue.c:1592
  9    process 8         0x4008c0c9 in xQueueGenericReceive (xQueue=0x3ffc40b0, pvBuffer=0x3ffc5130, xTicksToWait=4294967295, xJustPeeking=0) at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/queue.c:1592
  8    process 7         0x4008c0c9 in xQueueGenericReceive (xQueue=0x3ffb45bc <s_timer_semaphore_memory>, pvBuffer=0x0, xTicksToWait=4294967295, xJustPeeking=0) at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/queue.c:1592
  7    process 6         0x4008c0c9 in xQueueGenericReceive (xQueue=0x3ffaff64, pvBuffer=0x0, xTicksToWait=4294967295, xJustPeeking=0) at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/queue.c:1592
  6    process 5         0x4008e0f6 in prvProcessTimerOrBlockTask (xNextExpireTime=<optimized out>, xListWasEmpty=<optimized out>) at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/timers.c:589
  5    process 4         0x4008c0c9 in xQueueGenericReceive (xQueue=0x3ffc2d7c, pvBuffer=0x3ffc3e60, xTicksToWait=1, xJustPeeking=0) at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/queue.c:1592
  4    process 3         0x4008c0c9 in xQueueGenericReceive (xQueue=0x3ffc19f8, pvBuffer=0x3ffc2810, xTicksToWait=10, xJustPeeking=0) at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/queue.c:1592
  3    process 2         0x4015d71e in esp_pm_impl_waiti () at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/pm_esp32.c:492
  2    process 1         0x4015d71e in esp_pm_impl_waiti () at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/pm_esp32.c:492
* 1    <main task>       app_main () at /home/joseph/esp/esp-adf-internal/examples/coredump/main/coredump_example.c:72

======================= ALL MEMORY REGIONS ========================
Name   Address   Size   Attrs
.rtc.text 0x400c0000 0x0 RW
.rtc.dummy 0x3ff80000 0x0 RW
.rtc.force_fast 0x3ff80000 0x0 RW
.rtc_noinit 0x50000000 0x0 RW
.rtc.force_slow 0x50000000 0x0 RW
.iram0.vectors 0x40080000 0x400 R XA
.iram0.text 0x40080400 0x19c60 RWXA
.dram0.data 0x3ffb0000 0x3d64 RW A
.noinit 0x3ffb3d64 0x0 RW
.flash.rodata 0x3f400020 0x2a40c RW A
.flash.text 0x400d0018 0x90140 R XA
.coredump.tasks.data 0x3ffb7778 0x164 RW
.coredump.tasks.data 0x3ffb75b0 0x1c0 RW
.coredump.tasks.data 0x3ffb8650 0x164 RW
.coredump.tasks.data 0x3ffb84a0 0x1a8 RW
.coredump.tasks.data 0x3ffb7ee4 0x164 RW
.coredump.tasks.data 0x3ffb7d30 0x1ac RW
.coredump.tasks.data 0x3ffc28d4 0x164 RW
.coredump.tasks.data 0x3ffc26c0 0x20c RW
.coredump.tasks.data 0x3ffc3f48 0x164 RW
.coredump.tasks.data 0x3ffc3d60 0x1e0 RW
.coredump.tasks.data 0x3ffb90b4 0x164 RW
.coredump.tasks.data 0x3ffb8f00 0x1ac RW
.coredump.tasks.data 0x3ffb5fbc 0x164 RW
.coredump.tasks.data 0x3ffb5e10 0x1a4 RW
.coredump.tasks.data 0x3ffafa80 0x164 RW
.coredump.tasks.data 0x3ffaf8d0 0x1a8 RW
.coredump.tasks.data 0x3ffc520c 0x164 RW
.coredump.tasks.data 0x3ffc5030 0x1d4 RW
.coredump.tasks.data 0x3ffc6988 0x164 RW
.coredump.tasks.data 0x3ffc67b0 0x1d0 RW
.coredump.tasks.data 0x3ffafdfc 0x164 RW
.coredump.tasks.data 0x3ffb5a10 0x1a0 RW
.coredump.rom.text 0x40000000 0x170 R X
.coredump.rom.text 0x40000180 0x8 R X
.coredump.rom.text 0x400001c0 0x8 R X
.coredump.rom.text 0x40000200 0x8 R X
.coredump.rom.text 0x40000240 0x8 R X
.coredump.rom.text 0x40000280 0xc R X
.coredump.rom.text 0x400002c0 0x4 R X
.coredump.rom.text 0x40000300 0x8 R X
.coredump.rom.text 0x40000340 0x18 R X
.coredump.rom.text 0x400003c0 0x8 R X
.coredump.rom.text 0x40000400 0x160 R X
.coredump.rom.text 0x40000560 0xd10c R X
.coredump.rom.text 0x40010000 0x54ef4 R X

===================== ESP32 CORE DUMP END =====================
===============================================================
Done!
192.168.1.2 - - [29/Jun/2020 15:11:35] "POST /upload HTTP/1.1" 200 -
```

## Troubleshooting

### Failure to startup the server

Make sure the `$ADF_PATH/esp-idf/requirements.txt` are installed, and python2 is used.

### Post failed

```
E (3747) HTTP_CLIENT: Connection failed, sock < 0
E (3747) COREDUMP_UPLOAD: Post failed
E (3747) COREDUMP_UPLOAD: core dump upload failed
```

Check the `core dump upload uri` and make sure the device and server are on the same network.
