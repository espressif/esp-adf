# Play MP3 Files from SPI Flash File System (SPIFFS)

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief


This example demonstrates how to play MP3 files from SPI Flash File System (SPIFFS) in the ESP-ADF.


The pipeline is as follows:

```
[flash] ---> spiffs_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
```

### Prerequisites

Create the SPIFFS file.

- Clone SPIFFS repository from [Github/spiffs](https://github.com/igrr/mkspiffs.git).

```
    git clone https://github.com/igrr/mkspiffs.git
```

- Build SPIFFS.

```
    cd mkspiffs
    make clean
    make dist CPPFLAGS="-DSPIFFS_OBJ_META_LEN=4"
```

- Copy the user audio file to the `tools` folder under the example (the `adf_music.mp3` file is already prepared for this example).

- Run the following command to zip the `adf_music.mp3` file into the `adf_music.bin` binary file, and then flash the file to the assigned partition in `partition`. The created file is already prepared under the `tools` folder for this example.

```
    ./mkspiffs -c ./tools -b 4096 -p 256 -s 0x100000 ./tools/adf_music.bin
```

- Create the partition table as follows:

  ```
    nvs,      data, nvs,     ,        0x6000,
    phy_init, data, phy,     ,        0x1000,
    factory,  app,  factory, ,        1M,
    storage,  data, spiffs,  0x110000,1M,
  ```

- Download the SPIFFS bin. Now the `./tools/adf_music.bin` includes `adf_music.mp3` only (all MP3 files will eventually generate a bin file).

For more `spiffs` descriptions, please refer to [SPIFFS Filesystem](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/spiffs.html).


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.
If you select `CONFIG_ESP32_C3_LYRA_V2_BOARD`, you need to apply `idf_v4.4_i2s_c3_pdm_tx.patch` in the `$ADF_PATH/esp-idf` directory.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

Run the command below, flash the `./tools/adf_music.bin` file to the `storage` in `partition`.

```
python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 115200 write_flash -z 0x110000 ./tools/adf_music.bin
```


> Note: the `download` command is only for the `esp32` chip modules. If other chip hardware is used, the `-- chip` needs to be modified. For example, if the `esp32s3` chip is used, the command would be `python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 115200 write_flash -z 0x110000 ./tools/adf_music.bin`.

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

### Example Functionality

- After the example starts running, it reads the audio file `/spiffs/adf_music.mp3` in SPIFFS for playback. The log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7556
load:0x40078000,len:13904
ho 0 tail 12 room 4
load:0x40080400,len:5296
0x40080400: _init at ??:?

entry 0x40080710
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 17:32:32
I (29) boot: chip revision: 3
I (33) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) qio_mode: Enabling default flash chip QIO
I (45) boot.esp32: SPI Speed      : 80MHz
I (50) boot.esp32: SPI Mode       : QIO
I (55) boot.esp32: SPI Flash Size : 4MB
I (59) boot: Enabling RNG early entropy source...
I (65) boot: Partition Table:
I (68) boot: ## Label            Usage          Type ST Offset   Length
I (75) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (83) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (90) boot:  2 factory          factory app      00 00 00010000 00100000
I (98) boot:  3 storage          Unknown data     01 82 00110000 00100000
I (105) boot: End of partition table
I (110) boot_comm: chip revision: 3, min. application chip revision: 0
I (117) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0eab4 ( 60084) map
I (143) esp_image: segment 1: paddr=0x0001eadc vaddr=0x3ffb0000 size=0x0153c (  5436) load
I (146) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x2cd40 (183616) map
0x400d0020: _stext at ??:?

I (204) esp_image: segment 3: paddr=0x0004cd68 vaddr=0x3ffb153c size=0x00ed0 (  3792) load
I (206) esp_image: segment 4: paddr=0x0004dc40 vaddr=0x40080000 size=0x0d144 ( 53572) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (237) boot: Loaded app from partition at offset 0x10000
I (237) boot: Disabling RNG early entropy source...
I (237) cpu_start: Pro cpu up.
I (241) cpu_start: Application information:
I (246) cpu_start: Project name:     play_spiffs_mp3
I (252) cpu_start: App version:      v2.2-230-g83bfd722-dirty
I (258) cpu_start: Compile time:     Nov 16 2021 17:32:27
I (264) cpu_start: ELF file SHA256:  d7ecb4c9ba527922...
I (270) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (276) cpu_start: Starting app cpu, entry point is 0x400818d4
0x400818d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (286) heap_init: Initializing. RAM available for dynamic allocation:
I (293) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (299) heap_init: At 3FFB2C90 len 0002D370 (180 KiB): DRAM
I (306) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (312) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (318) heap_init: At 4008D144 len 00012EBC (75 KiB): IRAM
I (325) cpu_start: Pro cpu start user code
I (342) spi_flash: detected chip: gd
I (342) spi_flash: flash io: qio
W (342) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (353) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (364) SPIFFS_MP3_EXAMPLE: [ 1 ] Mount spiffs
I (464) SPIFFS_MP3_EXAMPLE: [ 2 ] Start codec chip
E (464) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (494) SPIFFS_MP3_EXAMPLE: [3.0] Create audio pipeline for playback
I (494) SPIFFS_MP3_EXAMPLE: [3.1] Create spiffs stream to read data from sdcard
I (494) SPIFFS_MP3_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (524) SPIFFS_MP3_EXAMPLE: [3.3] Create mp3 decoder to decode mp3 file
I (524) SPIFFS_MP3_EXAMPLE: [3.4] Register all elements to audio pipeline
I (534) SPIFFS_MP3_EXAMPLE: [3.5] Link it together [flash]-->spiffs-->mp3_decoder-->i2s_stream-->[codec_chip]
I (544) SPIFFS_MP3_EXAMPLE: [3.6] Set up  uri (file as spiffs, mp3 as mp3 decoder, and default output is i2s)
I (554) SPIFFS_MP3_EXAMPLE: [ 4 ] Set up  event listener
I (554) SPIFFS_MP3_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (564) SPIFFS_MP3_EXAMPLE: [4.2] Listening event from peripherals
I (574) SPIFFS_MP3_EXAMPLE: [ 5 ] Start audio_pipeline
I (604) SPIFFS_MP3_EXAMPLE: [ 6 ] Listen for all pipeline events
I (604) SPIFFS_MP3_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (6364) SPIFFS_STREAM: No more data, ret:0
W (7384) SPIFFS_MP3_EXAMPLE: [ * ] Stop event received
I (7384) SPIFFS_MP3_EXAMPLE: [ 7 ] Stop audio_pipeline
E (7384) AUDIO_ELEMENT: [spiffs] Element already stopped
E (7394) AUDIO_ELEMENT: [mp3] Element already stopped
E (7394) AUDIO_ELEMENT: [i2s] Element already stopped
W (7404) AUDIO_PIPELINE: There are no listener registered
W (7404) AUDIO_ELEMENT: [spiffs] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7414) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7424) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE

```


### Example Log
A complete log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7556
load:0x40078000,len:13904
ho 0 tail 12 room 4
load:0x40080400,len:5296
0x40080400: _init at ??:?

entry 0x40080710
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 17:32:32
I (29) boot: chip revision: 3
I (33) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) qio_mode: Enabling default flash chip QIO
I (45) boot.esp32: SPI Speed      : 80MHz
I (50) boot.esp32: SPI Mode       : QIO
I (55) boot.esp32: SPI Flash Size : 4MB
I (59) boot: Enabling RNG early entropy source...
I (65) boot: Partition Table:
I (68) boot: ## Label            Usage          Type ST Offset   Length
I (75) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (83) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (90) boot:  2 factory          factory app      00 00 00010000 00100000
I (98) boot:  3 storage          Unknown data     01 82 00110000 00100000
I (105) boot: End of partition table
I (110) boot_comm: chip revision: 3, min. application chip revision: 0
I (117) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0eab4 ( 60084) map
I (143) esp_image: segment 1: paddr=0x0001eadc vaddr=0x3ffb0000 size=0x0153c (  5436) load
I (146) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x2cd40 (183616) map
0x400d0020: _stext at ??:?

I (204) esp_image: segment 3: paddr=0x0004cd68 vaddr=0x3ffb153c size=0x00ed0 (  3792) load
I (206) esp_image: segment 4: paddr=0x0004dc40 vaddr=0x40080000 size=0x0d144 ( 53572) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (237) boot: Loaded app from partition at offset 0x10000
I (237) boot: Disabling RNG early entropy source...
I (237) cpu_start: Pro cpu up.
I (241) cpu_start: Application information:
I (246) cpu_start: Project name:     play_spiffs_mp3
I (252) cpu_start: App version:      v2.2-230-g83bfd722-dirty
I (258) cpu_start: Compile time:     Nov 16 2021 17:32:27
I (264) cpu_start: ELF file SHA256:  d7ecb4c9ba527922...
I (270) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (276) cpu_start: Starting app cpu, entry point is 0x400818d4
0x400818d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (286) heap_init: Initializing. RAM available for dynamic allocation:
I (293) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (299) heap_init: At 3FFB2C90 len 0002D370 (180 KiB): DRAM
I (306) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (312) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (318) heap_init: At 4008D144 len 00012EBC (75 KiB): IRAM
I (325) cpu_start: Pro cpu start user code
I (342) spi_flash: detected chip: gd
I (342) spi_flash: flash io: qio
W (342) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (353) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (364) SPIFFS_MP3_EXAMPLE: [ 1 ] Mount spiffs
I (464) SPIFFS_MP3_EXAMPLE: [ 2 ] Start codec chip
E (464) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (494) SPIFFS_MP3_EXAMPLE: [3.0] Create audio pipeline for playback
I (494) SPIFFS_MP3_EXAMPLE: [3.1] Create spiffs stream to read data from sdcard
I (494) SPIFFS_MP3_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (524) SPIFFS_MP3_EXAMPLE: [3.3] Create mp3 decoder to decode mp3 file
I (524) SPIFFS_MP3_EXAMPLE: [3.4] Register all elements to audio pipeline
I (534) SPIFFS_MP3_EXAMPLE: [3.5] Link it together [flash]-->spiffs-->mp3_decoder-->i2s_stream-->[codec_chip]
I (544) SPIFFS_MP3_EXAMPLE: [3.6] Set up  uri (file as spiffs, mp3 as mp3 decoder, and default output is i2s)
I (554) SPIFFS_MP3_EXAMPLE: [ 4 ] Set up  event listener
I (554) SPIFFS_MP3_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (564) SPIFFS_MP3_EXAMPLE: [4.2] Listening event from peripherals
I (574) SPIFFS_MP3_EXAMPLE: [ 5 ] Start audio_pipeline
I (604) SPIFFS_MP3_EXAMPLE: [ 6 ] Listen for all pipeline events
I (604) SPIFFS_MP3_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (6364) SPIFFS_STREAM: No more data, ret:0
W (7384) SPIFFS_MP3_EXAMPLE: [ * ] Stop event received
I (7384) SPIFFS_MP3_EXAMPLE: [ 7 ] Stop audio_pipeline
E (7384) AUDIO_ELEMENT: [spiffs] Element already stopped
E (7394) AUDIO_ELEMENT: [mp3] Element already stopped
E (7394) AUDIO_ELEMENT: [i2s] Element already stopped
W (7404) AUDIO_PIPELINE: There are no listener registered
W (7404) AUDIO_ELEMENT: [spiffs] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7414) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7424) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE

```


## Troubleshooting

```c
I (364) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (376) SPIFFS_MP3_EXAMPLE: [ 1 ] Mount spiffs
E (386) SPIFFS: spiffs partition could not be found
root /spiffs E (386) PERIPH_SPIFFS: Failed to find SPIFFS partition

```
If the error message in the log above is reported, please follow the instruction in [Configuration](#configuration) and flash the `./tools/adf_music.bin` file to `storage` in `partition`.



## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
