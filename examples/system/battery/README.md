# Battery Detection Service

- [中文版本](./README.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how to configurate the battery detection service in ADF to receive notifications of battery events (full charge, loss of charge, and threshold alarm) by registering `battery_service_cb` callback function.


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default development board of this example is `ESP32-Korvo-DU1906`. Note, as this example requires GPIO to read the ADC voltage, some development boards are not compatible with this example.

```
menuconfig > Audio HAL > ESP32-Korvo-DU1906
```

**Note:**

For the development board which has a charge management chip with interfaces such as `I2C` to read battery voltage, temperature and other data, you can initialize the battery charge management chip by registering `init` for battery service. Then, register the `vol_get` interface to read the data from the battery charge management chip.

```c
    vol_monitor_param_t vol_monitor_cfg = {
        .init = adc_init,
        .deinit = adc_deinit,
        .vol_get = vol_read,
        .read_freq = 2,
        .report_freq = 2,
        .vol_full_threshold = CONFIG_VOLTAGE_OF_BATTERY_FULL,
        .vol_low_threshold = CONFIG_VOLTAGE_OF_BATTERY_LOW,
    };
```

Configure the battery voltage threshold via menuconfig.

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

### Example Functionality

- After the example starts running, it automatically reads the battery voltage data from the `ESP32-Korvo-DU1906` development board. The log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1b (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 16:37:38
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x07284 ( 29316) map
I (122) esp_image: segment 1: paddr=0x000172ac vaddr=0x3ffb0000 size=0x02088 (  8328) load
I (126) esp_image: segment 2: paddr=0x0001933c vaddr=0x40080000 size=0x06cdc ( 27868) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (143) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x15878 ( 88184) map
0x400d0020: _stext at ??:?

I (177) esp_image: segment 4: paddr=0x000358a0 vaddr=0x40086cdc size=0x03874 ( 14452) load
0x40086cdc: prvCopyDataToQueue at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/queue.c:1921

I (190) boot: Loaded app from partition at offset 0x10000
I (190) boot: Disabling RNG early entropy source...
I (190) cpu_start: Pro cpu up.
I (194) cpu_start: Application information:
I (199) cpu_start: Project name:     battery_example
I (204) cpu_start: App version:      v2.2-238-ga496ac5c
I (210) cpu_start: Compile time:     Nov 24 2021 16:37:31
I (216) cpu_start: ELF file SHA256:  870be2024ac17feb...
I (222) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (228) cpu_start: Starting app cpu, entry point is 0x40081604
0x40081604: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (239) heap_init: Initializing. RAM available for dynamic allocation:
I (245) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (252) heap_init: At 3FFB28C0 len 0002D740 (181 KiB): DRAM
I (258) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (264) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (271) heap_init: At 4008A550 len 00015AB0 (86 KiB): IRAM
I (277) cpu_start: Pro cpu start user code
I (295) spi_flash: detected chip: gd
I (296) spi_flash: flash io: dio
W (296) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (306) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (316) BATTERY_EXAMPLE: [1.0] create battery service
I (326) BATTERY_EXAMPLE: [1.1] start battery service
I (326) BATTERY_EXAMPLE: [1.2] start battery voltage report
I (4326) BATTERY_EXAMPLE: got voltage 2077
I (8326) BATTERY_EXAMPLE: got voltage 2078
I (12326) BATTERY_EXAMPLE: got voltage 2078
I (16326) BATTERY_EXAMPLE: got voltage 2077
I (20326) BATTERY_EXAMPLE: got voltage 2078
I (24326) BATTERY_EXAMPLE: got voltage 2078
I (28326) BATTERY_EXAMPLE: got voltage 2078
I (32326) BATTERY_EXAMPLE: got voltage 2078
I (36326) BATTERY_EXAMPLE: got voltage 2078
I (40326) BATTERY_EXAMPLE: got voltage 2077
I (44326) BATTERY_EXAMPLE: got voltage 2076
I (48326) BATTERY_EXAMPLE: got voltage 2078
I (52326) BATTERY_EXAMPLE: got voltage 2077
I (56326) BATTERY_EXAMPLE: got voltage 2078
I (60326) BATTERY_EXAMPLE: got voltage 2078
I (60336) BATTERY_EXAMPLE: [1.3] change battery voltage report freqency
I (62326) BATTERY_EXAMPLE: got voltage 2077
I (64326) BATTERY_EXAMPLE: got voltage 2078
I (66326) BATTERY_EXAMPLE: got voltage 2078
I (68326) BATTERY_EXAMPLE: got voltage 2078
I (70326) BATTERY_EXAMPLE: got voltage 2078
I (72326) BATTERY_EXAMPLE: got voltage 2077
I (74326) BATTERY_EXAMPLE: got voltage 2077
I (76326) BATTERY_EXAMPLE: got voltage 2077
I (78326) BATTERY_EXAMPLE: got voltage 2078
I (80326) BATTERY_EXAMPLE: got voltage 2078
I (82326) BATTERY_EXAMPLE: got voltage 2078
I (84326) BATTERY_EXAMPLE: got voltage 2078
I (86326) BATTERY_EXAMPLE: got voltage 2077
I (88326) BATTERY_EXAMPLE: got voltage 2078
I (90326) BATTERY_EXAMPLE: got voltage 2078
I (92326) BATTERY_EXAMPLE: got voltage 2078
I (94326) BATTERY_EXAMPLE: got voltage 2078
I (96326) BATTERY_EXAMPLE: got voltage 2078
I (98326) BATTERY_EXAMPLE: got voltage 2077
I (100326) BATTERY_EXAMPLE: got voltage 2079
I (102326) BATTERY_EXAMPLE: got voltage 2078
I (104326) BATTERY_EXAMPLE: got voltage 2078
I (106326) BATTERY_EXAMPLE: got voltage 2078
I (108326) BATTERY_EXAMPLE: got voltage 2077
I (110326) BATTERY_EXAMPLE: got voltage 2077
I (112326) BATTERY_EXAMPLE: got voltage 2077
I (114326) BATTERY_EXAMPLE: got voltage 2078
I (116326) BATTERY_EXAMPLE: got voltage 2077
I (118326) BATTERY_EXAMPLE: got voltage 2077
I (120326) BATTERY_EXAMPLE: got voltage 2077
I (120336) BATTERY_EXAMPLE: [2.0] stop battery voltage report
I (120336) BATTERY_EXAMPLE: [2.1] destroy battery service
I (120336) BATTERY_SERVICE: battery service destroyed
I (120336) VOL_MONITOR: vol monitor destroyed

```


### Example Log
A complete log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1b (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 16:37:38
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x07284 ( 29316) map
I (122) esp_image: segment 1: paddr=0x000172ac vaddr=0x3ffb0000 size=0x02088 (  8328) load
I (126) esp_image: segment 2: paddr=0x0001933c vaddr=0x40080000 size=0x06cdc ( 27868) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (143) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x15878 ( 88184) map
0x400d0020: _stext at ??:?

I (177) esp_image: segment 4: paddr=0x000358a0 vaddr=0x40086cdc size=0x03874 ( 14452) load
0x40086cdc: prvCopyDataToQueue at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/queue.c:1921

I (190) boot: Loaded app from partition at offset 0x10000
I (190) boot: Disabling RNG early entropy source...
I (190) cpu_start: Pro cpu up.
I (194) cpu_start: Application information:
I (199) cpu_start: Project name:     battery_example
I (204) cpu_start: App version:      v2.2-238-ga496ac5c
I (210) cpu_start: Compile time:     Nov 24 2021 16:37:31
I (216) cpu_start: ELF file SHA256:  870be2024ac17feb...
I (222) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (228) cpu_start: Starting app cpu, entry point is 0x40081604
0x40081604: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (239) heap_init: Initializing. RAM available for dynamic allocation:
I (245) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (252) heap_init: At 3FFB28C0 len 0002D740 (181 KiB): DRAM
I (258) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (264) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (271) heap_init: At 4008A550 len 00015AB0 (86 KiB): IRAM
I (277) cpu_start: Pro cpu start user code
I (295) spi_flash: detected chip: gd
I (296) spi_flash: flash io: dio
W (296) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (306) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (316) BATTERY_EXAMPLE: [1.0] create battery service
I (326) BATTERY_EXAMPLE: [1.1] start battery service
I (326) BATTERY_EXAMPLE: [1.2] start battery voltage report
I (4326) BATTERY_EXAMPLE: got voltage 2077
I (8326) BATTERY_EXAMPLE: got voltage 2078
I (12326) BATTERY_EXAMPLE: got voltage 2078
I (16326) BATTERY_EXAMPLE: got voltage 2077
I (20326) BATTERY_EXAMPLE: got voltage 2078
I (24326) BATTERY_EXAMPLE: got voltage 2078
I (28326) BATTERY_EXAMPLE: got voltage 2078
I (32326) BATTERY_EXAMPLE: got voltage 2078
I (36326) BATTERY_EXAMPLE: got voltage 2078
I (40326) BATTERY_EXAMPLE: got voltage 2077
I (44326) BATTERY_EXAMPLE: got voltage 2076
I (48326) BATTERY_EXAMPLE: got voltage 2078
I (52326) BATTERY_EXAMPLE: got voltage 2077
I (56326) BATTERY_EXAMPLE: got voltage 2078
I (60326) BATTERY_EXAMPLE: got voltage 2078
I (60336) BATTERY_EXAMPLE: [1.3] change battery voltage report freqency
I (62326) BATTERY_EXAMPLE: got voltage 2077
I (64326) BATTERY_EXAMPLE: got voltage 2078
I (66326) BATTERY_EXAMPLE: got voltage 2078
I (68326) BATTERY_EXAMPLE: got voltage 2078
I (70326) BATTERY_EXAMPLE: got voltage 2078
I (72326) BATTERY_EXAMPLE: got voltage 2077
I (74326) BATTERY_EXAMPLE: got voltage 2077
I (76326) BATTERY_EXAMPLE: got voltage 2077
I (78326) BATTERY_EXAMPLE: got voltage 2078
I (80326) BATTERY_EXAMPLE: got voltage 2078
I (82326) BATTERY_EXAMPLE: got voltage 2078
I (84326) BATTERY_EXAMPLE: got voltage 2078
I (86326) BATTERY_EXAMPLE: got voltage 2077
I (88326) BATTERY_EXAMPLE: got voltage 2078
I (90326) BATTERY_EXAMPLE: got voltage 2078
I (92326) BATTERY_EXAMPLE: got voltage 2078
I (94326) BATTERY_EXAMPLE: got voltage 2078
I (96326) BATTERY_EXAMPLE: got voltage 2078
I (98326) BATTERY_EXAMPLE: got voltage 2077
I (100326) BATTERY_EXAMPLE: got voltage 2079
I (102326) BATTERY_EXAMPLE: got voltage 2078
I (104326) BATTERY_EXAMPLE: got voltage 2078
I (106326) BATTERY_EXAMPLE: got voltage 2078
I (108326) BATTERY_EXAMPLE: got voltage 2077
I (110326) BATTERY_EXAMPLE: got voltage 2077
I (112326) BATTERY_EXAMPLE: got voltage 2077
I (114326) BATTERY_EXAMPLE: got voltage 2078
I (116326) BATTERY_EXAMPLE: got voltage 2077
I (118326) BATTERY_EXAMPLE: got voltage 2077
I (120326) BATTERY_EXAMPLE: got voltage 2077
I (120336) BATTERY_EXAMPLE: [2.0] stop battery voltage report
I (120336) BATTERY_EXAMPLE: [2.1] destroy battery service
I (120336) BATTERY_SERVICE: battery service destroyed
I (120336) VOL_MONITOR: vol monitor destroyed

```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
