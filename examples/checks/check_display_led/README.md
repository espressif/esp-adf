# Check LED Display of Development Boards

- [中文版本](./README.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

ADF defines a series of common [display patterns](https://github.com/espressif/esp-adf/blob/master/components/display_service/include/display_service.h) from a functional point of view. In different hardware environments, you can use the interface `display_service_set_pattern` to set display patterns. Currently, ADF allows you to drive LEDs with [PWM](https://github.com/espressif/esp-adf/blob/master/components/display_service/led_indicator/led_indicator.c), [AW2013](https://github.com/espressif/esp-adf/blob/master/components/display_service/led_bar/led_bar_aw2013.c), [IS3x](https://github.com/espressif/esp-adf/blob/master/components/display_service/led_bar/led_bar_is31x.c), and [WS2812](https://github.com/espressif/esp-adf/blob/master/components/display_service/led_bar/led_bar_ws2812.c).

This example demonstrates how to set LED mode and checks whether all LEDs are working properly. For more information, please refer to [ADF Get Started](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html).


## Environment Setup


### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Build and Flash


### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.


### Configuration

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```c
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```


### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project. 


## How to Use the Example


### Example Functionality

After the example starts to run, the LEDs loop through different display patterns in order. The log is as follows: 

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
I (27) boot: compile time 15:01:00
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
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x06c00 ( 27648) map
I (122) esp_image: segment 1: paddr=0x00016c28 vaddr=0x3ffb0000 size=0x020ac (  8364) load
I (125) esp_image: segment 2: paddr=0x00018cdc vaddr=0x40080000 size=0x0733c ( 29500) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x16d48 ( 93512) map
0x400d0020: _stext at ??:?

I (178) esp_image: segment 4: paddr=0x00036d70 vaddr=0x4008733c size=0x03118 ( 12568) load
0x4008733c: xthal_window_spill_nw at /Users/igrokhotkov/e/esp32/hal/hal/windowspill_asm.S:212

I (189) boot: Loaded app from partition at offset 0x10000
I (189) boot: Disabling RNG early entropy source...
I (190) cpu_start: Pro cpu up.
I (193) cpu_start: Application information:
I (198) cpu_start: Project name:     check_display_led
I (204) cpu_start: App version:      v2.2-228-g07116fa5-dirty
I (210) cpu_start: Compile time:     Nov 12 2021 16:20:42
I (217) cpu_start: ELF file SHA256:  b5d39a251613418c...
I (223) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (229) cpu_start: Starting app cpu, entry point is 0x40081618
0x40081618: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (239) heap_init: Initializing. RAM available for dynamic allocation:
I (246) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (252) heap_init: At 3FFB2930 len 0002D6D0 (181 KiB): DRAM
I (258) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (264) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (271) heap_init: At 4008A454 len 00015BAC (86 KiB): IRAM
I (277) cpu_start: Pro cpu start user code
I (295) spi_flash: detected chip: gd
I (296) spi_flash: flash io: dio
W (296) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (306) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (317) CHECK_DISPLAY_LED: [ 1 ] Create display service instance
I (327) CHECK_DISPLAY_LED: LED pattern index is 0
W (327) LED_INDI: The led mode is invalid
I (2337) CHECK_DISPLAY_LED: LED pattern index is 1
I (4337) CHECK_DISPLAY_LED: LED pattern index is 2
W (4337) LED_INDI: The led mode is invalid
I (6337) CHECK_DISPLAY_LED: LED pattern index is 3
I (8337) CHECK_DISPLAY_LED: LED pattern index is 4
I (10337) CHECK_DISPLAY_LED: LED pattern index is 5
W (10337) LED_INDI: The led mode is invalid
I (12337) CHECK_DISPLAY_LED: LED pattern index is 6
W (12337) LED_INDI: The led mode is invalid
I (14337) CHECK_DISPLAY_LED: LED pattern index is 7
W (14337) LED_INDI: The led mode is invalid
I (16337) CHECK_DISPLAY_LED: LED pattern index is 8
W (16337) LED_INDI: The led mode is invalid
I (18337) CHECK_DISPLAY_LED: LED pattern index is 9
W (18337) LED_INDI: The led mode is invalid
I (20337) CHECK_DISPLAY_LED: LED pattern index is 10
W (20337) LED_INDI: The led mode is invalid
I (22337) CHECK_DISPLAY_LED: LED pattern index is 11
W (22337) LED_INDI: The led mode is invalid
I (24337) CHECK_DISPLAY_LED: LED pattern index is 12
W (24337) LED_INDI: The led mode is invalid
I (26337) CHECK_DISPLAY_LED: LED pattern index is 13
I (28337) CHECK_DISPLAY_LED: LED pattern index is 14
I (30337) CHECK_DISPLAY_LED: LED pattern index is 15
W (30337) LED_INDI: The led mode is invalid
I (32337) CHECK_DISPLAY_LED: LED pattern index is 16
W (32337) LED_INDI: The led mode is invalid
I (34337) CHECK_DISPLAY_LED: LED pattern index is 17
W (34337) LED_INDI: The led mode is invalid
I (36337) CHECK_DISPLAY_LED: LED pattern index is 18
W (36337) LED_INDI: The led mode is invalid
I (38337) CHECK_DISPLAY_LED: LED pattern index is 19
W (38337) LED_INDI: The led mode is invalid
I (40337) CHECK_DISPLAY_LED: LED pattern index is 20
I (42337) CHECK_DISPLAY_LED: LED pattern index is 21
I (44337) CHECK_DISPLAY_LED: LED pattern index is 22
W (44337) LED_INDI: The led mode is invalid
I (46337) CHECK_DISPLAY_LED: LED pattern index is 23
W (46337) LED_INDI: The led mode is invalid
I (48337) CHECK_DISPLAY_LED: LED pattern index is 24
W (48337) LED_INDI: The led mode is invalid
I (50337) CHECK_DISPLAY_LED: LED pattern index is 25
W (50337) LED_INDI: The led mode is invalid
I (52337) CHECK_DISPLAY_LED: LED pattern index is 26
W (52337) LED_INDI: The led mode is invalid
I (54337) CHECK_DISPLAY_LED: LED pattern index is 27
I (56337) CHECK_DISPLAY_LED: LED pattern index is 28
I (58337) CHECK_DISPLAY_LED: LED pattern index is 0
W (58337) LED_INDI: The led mode is invalid
I (60337) CHECK_DISPLAY_LED: LED pattern index is 1
I (62337) CHECK_DISPLAY_LED: LED pattern index is 2
W (62337) LED_INDI: The led mode is invalid
I (64337) CHECK_DISPLAY_LED: LED pattern index is 3
I (66337) CHECK_DISPLAY_LED: LED pattern index is 4
I (68337) CHECK_DISPLAY_LED: LED pattern index is 5
W (68337) LED_INDI: The led mode is invalid
I (70337) CHECK_DISPLAY_LED: LED pattern index is 6
W (70337) LED_INDI: The led mode is invalid
I (72337) CHECK_DISPLAY_LED: LED pattern index is 7
W (72337) LED_INDI: The led mode is invalid
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
I (27) boot: compile time 15:01:00
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
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x06c00 ( 27648) map
I (122) esp_image: segment 1: paddr=0x00016c28 vaddr=0x3ffb0000 size=0x020ac (  8364) load
I (125) esp_image: segment 2: paddr=0x00018cdc vaddr=0x40080000 size=0x0733c ( 29500) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x16d48 ( 93512) map
0x400d0020: _stext at ??:?

I (178) esp_image: segment 4: paddr=0x00036d70 vaddr=0x4008733c size=0x03118 ( 12568) load
0x4008733c: xthal_window_spill_nw at /Users/igrokhotkov/e/esp32/hal/hal/windowspill_asm.S:212

I (189) boot: Loaded app from partition at offset 0x10000
I (189) boot: Disabling RNG early entropy source...
I (190) cpu_start: Pro cpu up.
I (193) cpu_start: Application information:
I (198) cpu_start: Project name:     check_display_led
I (204) cpu_start: App version:      v2.2-228-g07116fa5-dirty
I (210) cpu_start: Compile time:     Nov 12 2021 16:20:42
I (217) cpu_start: ELF file SHA256:  b5d39a251613418c...
I (223) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (229) cpu_start: Starting app cpu, entry point is 0x40081618
0x40081618: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (239) heap_init: Initializing. RAM available for dynamic allocation:
I (246) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (252) heap_init: At 3FFB2930 len 0002D6D0 (181 KiB): DRAM
I (258) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (264) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (271) heap_init: At 4008A454 len 00015BAC (86 KiB): IRAM
I (277) cpu_start: Pro cpu start user code
I (295) spi_flash: detected chip: gd
I (296) spi_flash: flash io: dio
W (296) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (306) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (317) CHECK_DISPLAY_LED: [ 1 ] Create display service instance
I (327) CHECK_DISPLAY_LED: LED pattern index is 0
W (327) LED_INDI: The led mode is invalid
I (2337) CHECK_DISPLAY_LED: LED pattern index is 1
I (4337) CHECK_DISPLAY_LED: LED pattern index is 2
W (4337) LED_INDI: The led mode is invalid
I (6337) CHECK_DISPLAY_LED: LED pattern index is 3
I (8337) CHECK_DISPLAY_LED: LED pattern index is 4
I (10337) CHECK_DISPLAY_LED: LED pattern index is 5
W (10337) LED_INDI: The led mode is invalid
I (12337) CHECK_DISPLAY_LED: LED pattern index is 6
W (12337) LED_INDI: The led mode is invalid
I (14337) CHECK_DISPLAY_LED: LED pattern index is 7
W (14337) LED_INDI: The led mode is invalid
I (16337) CHECK_DISPLAY_LED: LED pattern index is 8
W (16337) LED_INDI: The led mode is invalid
I (18337) CHECK_DISPLAY_LED: LED pattern index is 9
W (18337) LED_INDI: The led mode is invalid
I (20337) CHECK_DISPLAY_LED: LED pattern index is 10
W (20337) LED_INDI: The led mode is invalid
I (22337) CHECK_DISPLAY_LED: LED pattern index is 11
W (22337) LED_INDI: The led mode is invalid
I (24337) CHECK_DISPLAY_LED: LED pattern index is 12
W (24337) LED_INDI: The led mode is invalid
I (26337) CHECK_DISPLAY_LED: LED pattern index is 13
I (28337) CHECK_DISPLAY_LED: LED pattern index is 14
I (30337) CHECK_DISPLAY_LED: LED pattern index is 15
W (30337) LED_INDI: The led mode is invalid
I (32337) CHECK_DISPLAY_LED: LED pattern index is 16
W (32337) LED_INDI: The led mode is invalid
I (34337) CHECK_DISPLAY_LED: LED pattern index is 17
W (34337) LED_INDI: The led mode is invalid
I (36337) CHECK_DISPLAY_LED: LED pattern index is 18
W (36337) LED_INDI: The led mode is invalid
I (38337) CHECK_DISPLAY_LED: LED pattern index is 19
W (38337) LED_INDI: The led mode is invalid
I (40337) CHECK_DISPLAY_LED: LED pattern index is 20
I (42337) CHECK_DISPLAY_LED: LED pattern index is 21
I (44337) CHECK_DISPLAY_LED: LED pattern index is 22
W (44337) LED_INDI: The led mode is invalid
I (46337) CHECK_DISPLAY_LED: LED pattern index is 23
W (46337) LED_INDI: The led mode is invalid
I (48337) CHECK_DISPLAY_LED: LED pattern index is 24
W (48337) LED_INDI: The led mode is invalid
I (50337) CHECK_DISPLAY_LED: LED pattern index is 25
W (50337) LED_INDI: The led mode is invalid
I (52337) CHECK_DISPLAY_LED: LED pattern index is 26
W (52337) LED_INDI: The led mode is invalid
I (54337) CHECK_DISPLAY_LED: LED pattern index is 27
I (56337) CHECK_DISPLAY_LED: LED pattern index is 28
I (58337) CHECK_DISPLAY_LED: LED pattern index is 0
W (58337) LED_INDI: The led mode is invalid
I (60337) CHECK_DISPLAY_LED: LED pattern index is 1
I (62337) CHECK_DISPLAY_LED: LED pattern index is 2
W (62337) LED_INDI: The led mode is invalid
I (64337) CHECK_DISPLAY_LED: LED pattern index is 3
I (66337) CHECK_DISPLAY_LED: LED pattern index is 4
I (68337) CHECK_DISPLAY_LED: LED pattern index is 5
W (68337) LED_INDI: The led mode is invalid
I (70337) CHECK_DISPLAY_LED: LED pattern index is 6
W (70337) LED_INDI: The led mode is invalid
I (72337) CHECK_DISPLAY_LED: LED pattern index is 7
W (72337) LED_INDI: The led mode is invalid
```


## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
