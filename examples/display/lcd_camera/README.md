# Display on LCD from Camera
- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example demonstrates calling the esp32-camera API to get a picture and then display it on the LCD. The sample image resolution is 320 * 240.


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

- The camera model tested in this example is OV3660, for other models please refer to [esp32-camera](https://github.com/espressif/esp32-camera).

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

The default board for this example is `ESP32-S3-Korvo-2`.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

For full steps to configure and build an ESP-IDF project, please go to [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) and select the chip and version in the upper left corner of the page.

## How to Use the Example

### Example Functionality

- After the example starts running, it shows the object the camera is pointed at on the display.

### Example Log

A complete log is as follows:

```c
I (25) boot: ESP-IDF v4.4-dev-3930-g214d62b9ad 2nd stage bootloader
I (25) boot: compile time 21:36:10
I (25) boot: chip revision: 0
I (28) boot.esp32s3: Boot SPI Speed : 80MHz
I (33) boot.esp32s3: SPI Mode       : DIO
I (38) boot.esp32s3: SPI Flash Size : 2MB
I (42) boot: Enabling RNG early entropy source...
W (48) bootloader_random: RNG for ESP32-S3 not currently supported
I (55) boot: Partition Table:
I (58) boot: ## Label            Usage          Type ST Offset   Length
I (65) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (73) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (80) boot:  2 factory          factory app      00 00 00010000 00100000
I (88) boot: End of partition table
I (92) esp_image: segment 0: paddr=00010020 vaddr=3c030020 size=0e950h ( 59728) map
I (111) esp_image: segment 1: paddr=0001e978 vaddr=3fc94fd0 size=016a0h (  5792) load
I (113) esp_image: segment 2: paddr=00020020 vaddr=42000020 size=27aa0h (162464) map
I (146) esp_image: segment 3: paddr=00047ac8 vaddr=3fc96670 size=025bch (  9660) load
I (149) esp_image: segment 4: paddr=0004a08c vaddr=40374000 size=10fd0h ( 69584) load
I (167) esp_image: segment 5: paddr=0005b064 vaddr=50000000 size=00010h (    16) load
I (175) boot: Loaded app from partition at offset 0x10000
I (175) boot: Disabling RNG early entropy source...
W (176) bootloader_random: RNG for ESP32-S3 not currently supported
I (195) opi psram: vendor id : 0x0d (AP)
I (195) opi psram: dev id    : 0x02 (generation 3)
I (195) opi psram: density   : 0x03 (64 Mbit)
I (198) opi psram: good-die  : 0x01 (Pass)
I (203) opi psram: Latency   : 0x01 (Fixed)
I (208) opi psram: VCC       : 0x01 (3V)
I (213) opi psram: SRF       : 0x01 (Fast Refresh)
I (218) opi psram: BurstType : 0x01 (Hybrid Wrap)
I (224) opi psram: BurstLen  : 0x01 (32 Byte)
I (229) opi psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (235) opi psram: DriveStrength: 0x00 (1/1)
W (240) PSRAM: DO NOT USE FOR MASS PRODUCTION! Timing parameters will be updated in future IDF version.
I (250) spiram: Found 64MBit SPI RAM device
I (254) spiram: SPI RAM mode: sram 80m
I (259) spiram: PSRAM initialized, cache is in normal (1-core) mode.
I (266) cpu_start: Pro cpu up.
I (270) cpu_start: Starting app cpu, entry point is 0x403754a4
0x403754a4: call_start_cpu1 at /Users/maojianxin/workspace/esp-adf-internal-dev/esp-idf/components/esp_system/port/cpu_start.c:156

I (0) cpu_start: App cpu up.
I (696) spiram: SPI SRAM memory test OK
I (706) cpu_start: Pro cpu start user code
I (706) cpu_start: cpu freq: 240000000
I (706) cpu_start: Application information:
I (709) cpu_start: Project name:     lcd_camera
I (714) cpu_start: App version:      v2.2-304-ge5bbb3b2-dirty
I (720) cpu_start: Compile time:     Jan  4 2022 21:36:23
I (726) cpu_start: ELF file SHA256:  ca651eefe556a1e8...
I (732) cpu_start: ESP-IDF:          v4.4-dev-3930-g214d62b9ad
I (739) heap_init: Initializing. RAM available for dynamic allocation:
I (746) heap_init: At 3FC997A8 len 00046858 (282 KiB): D/IRAM
I (752) heap_init: At 3FCE0000 len 0000EE34 (59 KiB): STACK/DRAM
I (759) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (765) heap_init: At 600FE000 len 00002000 (8 KiB): RTCRAM
I (772) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (780) spi_flash: detected chip: gd
I (784) spi_flash: flash io: dio
W (788) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (801) sleep: Configure to isolate all GPIO pins in sleep state
I (808) sleep: Enable automatic switching of GPIO sleep configuration
I (815) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (835) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (865) gpio: GPIO[2]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (865) s3 ll_cam: DMA Channel=4
I (865) cam_hal: cam init ok
I (865) sccb: pin_sda 17 pin_scl 18
I (905) camera: Detected camera at address=0x3c
I (905) camera: Detected OV3660 camera
I (905) camera: Camera PID=0x3660 VER=0x00 MIDL=0x00 MIDH=0x00
I (1205) s3 ll_cam: node_size: 3840, nodes_per_line: 1, lines_per_node: 6
I (1205) s3 ll_cam: dma_half_buffer_min:  3840, dma_half_buffer: 15360, lines_per_half_buffer: 24, dma_buffer_size: 30720
I (1205) cam_hal: buffer_size: 30720, half_buffer_size: 15360, node_buffer_size: 3840, node_cnt: 8, total_cnt: 10
I (1215) cam_hal: Allocating 153600 Byte frame buffer in PSRAM
I (1225) cam_hal: cam config ok
I (1245) ov3660: Calculated VCO: 320000000 Hz, PLLCLK: 160000000 Hz, SYSCLK: 40000000 Hz, PCLK: 20000000 Hz
I (1255) LCD_Camera: Taking picture...
I (1395) LCD_Camera: Picture taken! The size was: 153600 bytes, w:320, h:240
I (1425) LCD_Camera: Taking picture...
I (1485) LCD_Camera: Picture taken! The size was: 153600 bytes, w:320, h:240

```

## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
