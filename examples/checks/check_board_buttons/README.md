# Check Keys of Development Boards

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

ADF defines [six function keys](https://github.com/espressif/esp-adf/blob/master/components/input_key_service/include/input_key_com_user_id.h) that are frequently used in audio development boards. They are `Rec`, `Mode`, `Play`, `Set`, `Vol-`, `Vol+`. Each key has [four key events](https://github.com/espressif/esp-adf/blob/master/components/input_key_service/include/input_key_service.h), i.e., short press, short press and release, long press, long press and release. You could call `input_key_service` to implement them. Different development boards have different ways of checking keys, including [ADC analog voltage detection](https://github.com/espressif/esp-adf/blob/master/components/audio_board/esp32_s3_korvo2_v3/board_def.h), [GPIO interrupt input](https://github.com/espressif/esp-adf/blob/master/components/audio_board/lyrat_v4_3/board_def.h), and [capacitive touch](https://github.com/espressif/esp-adf/blob/master/components/audio_board/lyrat_v4_3/board_def.h).

This example calls `input_key_service` to use the six function keys and four key events, which can be used to debug and detect the development board keys.


## Environment Setup


#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash


### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.


### Configuration

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

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

- The example starts to run and is waiting for users to press keys. The log is as follows:

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
I (27) boot: compile time 14:56:47
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x07f44 ( 32580) map
I (123) esp_image: segment 1: paddr=0x00017f6c vaddr=0x3ffb0000 size=0x020a0 (  8352) load
I (127) esp_image: segment 2: paddr=0x0001a014 vaddr=0x40080000 size=0x06004 ( 24580) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x17770 ( 96112) map
0x400d0020: _stext at ??:?

I (179) esp_image: segment 4: paddr=0x00037798 vaddr=0x40086004 size=0x04538 ( 17720) load
0x40086004: pcTaskGetTaskName at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/tasks.c:2373 (discriminator 1)

I (193) boot: Loaded app from partition at offset 0x10000
I (193) boot: Disabling RNG early entropy source...
I (193) cpu_start: Pro cpu up.
I (197) cpu_start: Application information:
I (202) cpu_start: Project name:     check_board_buttons
I (208) cpu_start: App version:      v2.2-228-g838b0926-dirty
I (214) cpu_start: Compile time:     Nov 12 2021 14:56:40
I (220) cpu_start: ELF file SHA256:  dde162e03a6a6339...
I (226) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (232) cpu_start: Starting app cpu, entry point is 0x40081624
0x40081624: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (243) heap_init: Initializing. RAM available for dynamic allocation:
I (249) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (255) heap_init: At 3FFB2908 len 0002D6F8 (181 KiB): DRAM
I (262) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (268) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (274) heap_init: At 4008A53C len 00015AC4 (86 KiB): IRAM
I (281) cpu_start: Pro cpu start user code
I (299) spi_flash: detected chip: gd
I (299) spi_flash: flash io: dio
W (300) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (309) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (320) CHECK_BUTTON: [ 1 ] Initialize peripherals
I (330) CHECK_BUTTON: [ 2 ] Initialize Button peripheral with board init
I (330) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (340) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (350) CHECK_BUTTON: [ 3 ] Create and start input key service
W (370) PERIPH_TOUCH: _touch_init
W (370) CHECK_BUTTON: [ 4 ] Waiting for a button to be pressed ...

```

- As this example is running on the default developement board `ESP32-Lyrat V4.3`, you can press the [Rec], [Mode], [Play], [Set], [Vol-], [Vol+] keys on the audio board to check whether they are working properly. The log is as follows:

```c
I (24320) CHECK_BUTTON: [ * ] [Rec] KEY CLICKED
I (24490) CHECK_BUTTON: [ * ] [Rec] KEY CLICK RELEASED
I (25370) CHECK_BUTTON: [ * ] [MODE] KEY CLICKED
I (25510) CHECK_BUTTON: [ * ] [MODE] KEY CLICK RELEASED
I (26920) CHECK_BUTTON: [ * ] [Play] KEY CLICKED
I (27370) CHECK_BUTTON: [ * ] [Play] KEY CLICK RELEASED
I (28420) CHECK_BUTTON: [ * ] [SET] KEY CLICKED
I (28720) CHECK_BUTTON: [ * ] [SET] KEY CLICK RELEASED
I (29320) CHECK_BUTTON: [ * ] [Vol-] KEY CLICKED
I (29770) CHECK_BUTTON: [ * ] [Vol-] KEY CLICK RELEASED
I (31120) CHECK_BUTTON: [ * ] [Vol+] KEY CLICKED
I (31420) CHECK_BUTTON: [ * ] [Vol+] KEY CLICK RELEASED

```

- Besides, this example supports long press. Below is the log for long press and release of keys.

```c
I (43170) CHECK_BUTTON: [ * ] [Rec] KEY CLICKED
I (45200) CHECK_BUTTON: [ * ] [Rec] KEY PRESSED
I (48330) CHECK_BUTTON: [ * ] [Rec] KEY PRESS RELEASED
I (51620) CHECK_BUTTON: [ * ] [MODE] KEY CLICKED
I (53650) CHECK_BUTTON: [ * ] [MODE] KEY PRESSED
I (56260) CHECK_BUTTON: [ * ] [MODE] KEY PRESS RELEASED
I (57670) CHECK_BUTTON: [ * ] [Play] KEY CLICKED
I (59770) CHECK_BUTTON: [ * ] [Play] KEY PRESSED
I (60670) CHECK_BUTTON: [ * ] [Play] KEY PRESS RELEASED
I (62920) CHECK_BUTTON: [ * ] [SET] KEY CLICKED
I (65020) CHECK_BUTTON: [ * ] [SET] KEY PRESSED
I (67270) CHECK_BUTTON: [ * ] [SET] KEY PRESS RELEASED
I (68470) CHECK_BUTTON: [ * ] [Vol-] KEY CLICKED
I (70570) CHECK_BUTTON: [ * ] [Vol-] KEY PRESSED
I (72070) CHECK_BUTTON: [ * ] [Vol-] KEY PRESS RELEASED
I (72820) CHECK_BUTTON: [ * ] [Vol+] KEY CLICKED
I (74920) CHECK_BUTTON: [ * ] [Vol+] KEY PRESSED
I (75820) CHECK_BUTTON: [ * ] [Vol+] KEY PRESS RELEASED

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
I (27) boot: compile time 14:56:47
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x07f44 ( 32580) map
I (123) esp_image: segment 1: paddr=0x00017f6c vaddr=0x3ffb0000 size=0x020a0 (  8352) load
I (127) esp_image: segment 2: paddr=0x0001a014 vaddr=0x40080000 size=0x06004 ( 24580) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x17770 ( 96112) map
0x400d0020: _stext at ??:?

I (179) esp_image: segment 4: paddr=0x00037798 vaddr=0x40086004 size=0x04538 ( 17720) load
0x40086004: pcTaskGetTaskName at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/tasks.c:2373 (discriminator 1)

I (193) boot: Loaded app from partition at offset 0x10000
I (193) boot: Disabling RNG early entropy source...
I (193) cpu_start: Pro cpu up.
I (197) cpu_start: Application information:
I (202) cpu_start: Project name:     check_board_buttons
I (208) cpu_start: App version:      v2.2-228-g838b0926-dirty
I (214) cpu_start: Compile time:     Nov 12 2021 14:56:40
I (220) cpu_start: ELF file SHA256:  dde162e03a6a6339...
I (226) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (232) cpu_start: Starting app cpu, entry point is 0x40081624
0x40081624: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (243) heap_init: Initializing. RAM available for dynamic allocation:
I (249) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (255) heap_init: At 3FFB2908 len 0002D6F8 (181 KiB): DRAM
I (262) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (268) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (274) heap_init: At 4008A53C len 00015AC4 (86 KiB): IRAM
I (281) cpu_start: Pro cpu start user code
I (299) spi_flash: detected chip: gd
I (299) spi_flash: flash io: dio
W (300) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (309) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (320) CHECK_BUTTON: [ 1 ] Initialize peripherals
I (330) CHECK_BUTTON: [ 2 ] Initialize Button peripheral with board init
I (330) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (340) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (350) CHECK_BUTTON: [ 3 ] Create and start input key service
W (370) PERIPH_TOUCH: _touch_init
W (370) CHECK_BUTTON: [ 4 ] Waiting for a button to be pressed ...
I (24320) CHECK_BUTTON: [ * ] [Rec] KEY CLICKED
I (24490) CHECK_BUTTON: [ * ] [Rec] KEY CLICK RELEASED
I (25370) CHECK_BUTTON: [ * ] [MODE] KEY CLICKED
I (25510) CHECK_BUTTON: [ * ] [MODE] KEY CLICK RELEASED
I (26920) CHECK_BUTTON: [ * ] [Play] KEY CLICKED
I (27370) CHECK_BUTTON: [ * ] [Play] KEY CLICK RELEASED
I (28420) CHECK_BUTTON: [ * ] [SET] KEY CLICKED
I (28720) CHECK_BUTTON: [ * ] [SET] KEY CLICK RELEASED
I (29320) CHECK_BUTTON: [ * ] [Vol-] KEY CLICKED
I (29770) CHECK_BUTTON: [ * ] [Vol-] KEY CLICK RELEASED
I (31120) CHECK_BUTTON: [ * ] [Vol+] KEY CLICKED
I (31420) CHECK_BUTTON: [ * ] [Vol+] KEY CLICK RELEASED
I (43170) CHECK_BUTTON: [ * ] [Rec] KEY CLICKED
I (45200) CHECK_BUTTON: [ * ] [Rec] KEY PRESSED
I (48330) CHECK_BUTTON: [ * ] [Rec] KEY PRESS RELEASED
I (51620) CHECK_BUTTON: [ * ] [MODE] KEY CLICKED
I (53650) CHECK_BUTTON: [ * ] [MODE] KEY PRESSED
I (56260) CHECK_BUTTON: [ * ] [MODE] KEY PRESS RELEASED
I (57670) CHECK_BUTTON: [ * ] [Play] KEY CLICKED
I (59770) CHECK_BUTTON: [ * ] [Play] KEY PRESSED
I (60670) CHECK_BUTTON: [ * ] [Play] KEY PRESS RELEASED
I (62920) CHECK_BUTTON: [ * ] [SET] KEY CLICKED
I (65020) CHECK_BUTTON: [ * ] [SET] KEY PRESSED
I (67270) CHECK_BUTTON: [ * ] [SET] KEY PRESS RELEASED
I (68470) CHECK_BUTTON: [ * ] [Vol-] KEY CLICKED
I (70570) CHECK_BUTTON: [ * ] [Vol-] KEY PRESSED
I (72070) CHECK_BUTTON: [ * ] [Vol-] KEY PRESS RELEASED
I (72820) CHECK_BUTTON: [ * ] [Vol+] KEY CLICKED
I (74920) CHECK_BUTTON: [ * ] [Vol+] KEY PRESSED
I (75820) CHECK_BUTTON: [ * ] [Vol+] KEY PRESS RELEASED
```


## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
