# GUI Display
- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example demonstrates how to use the LVGL graphics library to draw a music player user interface that supports touch screen control. This example refers to the original LVGL lv_demo_music project.

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Build and Flash


### Default IDF Branch

This example supports IDF release/v4.4 and later branches. By default, it runs on IDF release/v4.4.

### IDF Branch

- The command to switch to IDF release/v4.4 branch is as follows:

```
cd $IDF_PATH
git checkout master
git pull
git checkout release/v4.4
git submodule update --init --recursive
```

### Configuration

The default board for this example is `ESP32-S3-Korvo-2 v3`.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

For full steps to configure and build an ESP-IDF project, please go to [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) and select the chip and version in the upper left corner of the page.

## How to Use the Example


### Example Functionality

After the example starts running, it first presents the welcome page. Then, after entering the play page, please swipe up `ALL TRACKS` to see the playlist. Click on a piece of analog music, then slide down the collapsed play page to see the music being played.

### Example Log

A complete log is as follows:

```c
I (25) boot: ESP-IDF v4.4-dev-3930-g214d62b9ad-dirty 2nd stage bootloader
I (25) boot: compile time 21:30:04
I (25) boot: chip revision: 0
I (29) boot.esp32s3: Boot SPI Speed : 80MHz
I (33) boot.esp32s3: SPI Mode       : DIO
I (38) boot.esp32s3: SPI Flash Size : 2MB
I (43) boot: Enabling RNG early entropy source...
W (48) bootloader_random: RNG for ESP32-S3 not currently supported
I (55) boot: Partition Table:
I (59) boot: ## Label            Usage          Type ST Offset   Length
I (66) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (73) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (81) boot:  2 factory          factory app      00 00 00010000 00100000
I (88) boot: End of partition table
I (93) esp_image: segment 0: paddr=00010020 vaddr=3c050020 size=702c8h (459464) map
I (183) esp_image: segment 1: paddr=000802f0 vaddr=3fc93f90 size=0290ch ( 10508) load
I (186) esp_image: segment 2: paddr=00082c04 vaddr=40374000 size=0d414h ( 54292) load
I (201) esp_image: segment 3: paddr=00090020 vaddr=42000020 size=4906ch (299116) map
I (255) esp_image: segment 4: paddr=000d9094 vaddr=40381414 size=02b78h ( 11128) load
I (258) esp_image: segment 5: paddr=000dbc14 vaddr=50000000 size=00010h (    16) load
I (267) boot: Loaded app from partition at offset 0x10000
I (267) boot: Disabling RNG early entropy source...
W (273) bootloader_random: RNG for ESP32-S3 not currently supported
I (291) cpu_start: Pro cpu up.
I (291) cpu_start: Starting app cpu, entry point is 0x4037534c
0x4037534c: call_start_cpu1 at /Users/maojianxin/workspace/esp-adf-internal-dev/esp-idf/components/esp_system/port/cpu_start.c:156

I (0) cpu_start: App cpu up.
I (305) cpu_start: Pro cpu start user code
I (305) cpu_start: cpu freq: 160000000
I (305) cpu_start: Application information:
I (308) cpu_start: Project name:     lv_demos
I (313) cpu_start: App version:      v2.2-296-g830d5002-dirty
I (319) cpu_start: Compile time:     Dec 28 2021 21:39:20
I (325) cpu_start: ELF file SHA256:  db1ef929fed12d0e...
I (331) cpu_start: ESP-IDF:          v4.4-dev-3930-g214d62b9ad-dirty
I (338) heap_init: Initializing. RAM available for dynamic allocation:
I (346) heap_init: At 3FCAA8A8 len 00035758 (213 KiB): D/IRAM
I (352) heap_init: At 3FCE0000 len 0000EE34 (59 KiB): STACK/DRAM
I (359) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (365) heap_init: At 600FE000 len 00002000 (8 KiB): RTCRAM
I (372) spi_flash: detected chip: gd
I (376) spi_flash: flash io: dio
W (379) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (393) sleep: Configure to isolate all GPIO pins in sleep state
I (400) sleep: Enable automatic switching of GPIO sleep configuration
I (407) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (468) gpio: GPIO[2]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (468) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
```

## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
