# Save and Play Back Recordings in SPIFFS

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how to use ADF SPIFFS stream to save recording files and then read and play them. When the audio is recorded, the I2S parameters are 48000 Hz, 16 bits, and double channels. The example resamples it to the 8000 Hz, 16-bit, and single-channel data, encodes it into the AMR-NB format, and writes it in SPIFFS. During playback, the example reads the AMR-NB file in SPIFFS, decodes and resamples it to the 48000 Hz, 16-bit, and double-channel data, and then plays it through the I2S peripheral.

1. For the recording process:

  - Set up I2S and obtain the audio at 48000 Hz, 16 bits, and the stereo sampling rate.
  - Use the resampling filter to convert the data to 8000 Hz, 16 bits, and single channel.
  - Use the ARM-NB encoder to encode the data.
  - Write to SPIFFS.

  The pipeline to resample the recording and save it to SPIFFS is as follows:

  ```
    [mic] ---> codec_chip ---> i2s_stream ---> resample_filter ---> amrnb_encoder ---> spiffs_stream ---> [flash]
  ```

2. For the recording playback process:

  - Read the file recorded in SPIFFS. The sampling rate is 8000 Hz, 16 bits, and single channel.
  - Use the AMR-NB decoder to decode the file data.
  - Use the resampling filter to convert the data to 48000 Hz, 16 bits, and double channels.
  - Write the data to the I2S peripheral for playback.

  The pipeline to read the data from SPIFFS, resample it, and play it through I2S is as follows:

  ```
    [flash] ---> spiffs_stream ---> amrnb_decoder ---> resample_filter ---> i2s_stream ---> codec_chip ---> [speaker]
  ```


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.0 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

### Example Functionality

- The example starts running. If `SPIFFS` is mounted for the first time, it will format. The log is as follows:

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
I (27) boot: compile time 16:55:50
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
I (91) boot:  3 storage          Unknown data     01 82 00110000 000f0000
I (98) boot: End of partition table
I (102) boot_comm: chip revision: 3, min. application chip revision: 0
I (110) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1b0ec (110828) map
I (161) esp_image: segment 1: paddr=0x0002b114 vaddr=0x3ffb0000 size=0x0240c (  9228) load
I (165) esp_image: segment 2: paddr=0x0002d528 vaddr=0x40080000 size=0x02af0 ( 10992) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (172) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x45584 (284036) map
0x400d0020: _stext at ??:?

I (285) esp_image: segment 4: paddr=0x000755ac vaddr=0x40082af0 size=0x0a2e8 ( 41704) load
0x40082af0: _lock_try_acquire at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/newlib/locks.c:175

I (310) boot: Loaded app from partition at offset 0x10000
I (310) boot: Disabling RNG early entropy source...
I (311) cpu_start: Pro cpu up.
I (314) cpu_start: Application information:
I (319) cpu_start: Project name:     spiffs_amr_resample_app
I (326) cpu_start: App version:      v2.2-216-g29c606a4
I (331) cpu_start: Compile time:     Nov  8 2021 16:55:43
I (338) cpu_start: ELF file SHA256:  6a240ffe175924a2...
I (343) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (349) cpu_start: Starting app cpu, entry point is 0x40081900
0x40081900: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (360) heap_init: Initializing. RAM available for dynamic allocation:
I (367) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (373) heap_init: At 3FFB2CD0 len 0002D330 (180 KiB): DRAM
I (379) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (385) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (392) heap_init: At 4008CDD8 len 00013228 (76 KiB): IRAM
I (398) cpu_start: Pro cpu start user code
I (416) spi_flash: detected chip: gd
I (417) spi_flash: flash io: dio
W (417) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (427) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
W (440) SPIFFS: mount failed, -10025. formatting...
I (11690) PERIPH_SPIFFS: Partition size: total: 896321, used: 0
E (11690) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (11710) PERIPH_TOUCH: _touch_init
I (11730) SPIFFS_AMR_RESAMPLE_EXAMPLE: [1.1] Initialize recorder pipeline
I (11730) SPIFFS_AMR_RESAMPLE_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (11770) SPIFFS_AMR_RESAMPLE_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (11770) SPIFFS_AMR_RESAMPLE_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (11780) I2S: I2S driver already installed
I (11800) SPIFFS_AMR_RESAMPLE_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (11800) SPIFFS_AMR_RESAMPLE_EXAMPLE: [ 3 ] Set up  event listener

```

- After you press the [REC] key on the audio board, the example starts recording, resamples the data, encodes it into an AMR-NB file, and finally writes it to the SPIFFS for storage. The log is as follows:

```c
E (1882690) SPIFFS_AMR_RESAMPLE_EXAMPLE: STOP playback and START recording
W (1882690) AUDIO_PIPELINE: Without stop, st:1
W (1882690) AUDIO_PIPELINE: Without wait stop, st:1
I (1882690) SPIFFS_AMR_RESAMPLE_EXAMPLE: Link audio elements to make recorder pipeline ready
I (1882700) SPIFFS_AMR_RESAMPLE_EXAMPLE: Setup file path to save recorded audio

```

- When you release the [REC] key, the example will read the AMR-NB file from SPIFFS, resample it, and send it to the I2S interface for playback. The log is as follows:

```c
I (1885720) SPIFFS_AMR_RESAMPLE_EXAMPLE: STOP recording and START playback
W (1885720) AUDIO_ELEMENT: IN-[filter_downsample] AEL_IO_ABORT
E (1885730) AUDIO_ELEMENT: [filter_downsample] Element already stopped
W (1885730) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
W (1885740) AUDIO_ELEMENT: IN-[amrnb_encoder] AEL_IO_ABORT
E (1885740) AUDIO_ELEMENT: [file_writer] Element already stopped
I (1885750) SPIFFS_AMR_RESAMPLE_EXAMPLE: Link audio elements to make playback pipeline ready
I (1885770) SPIFFS_AMR_RESAMPLE_EXAMPLE: Setup file path to read the amr audio to play
W (1885800) SPIFFS_STREAM: No more data, ret:0
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
I (27) boot: compile time 16:55:50
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
I (91) boot:  3 storage          Unknown data     01 82 00110000 000f0000
I (98) boot: End of partition table
I (102) boot_comm: chip revision: 3, min. application chip revision: 0
I (110) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1b0ec (110828) map
I (161) esp_image: segment 1: paddr=0x0002b114 vaddr=0x3ffb0000 size=0x0240c (  9228) load
I (165) esp_image: segment 2: paddr=0x0002d528 vaddr=0x40080000 size=0x02af0 ( 10992) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (172) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x45584 (284036) map
0x400d0020: _stext at ??:?

I (285) esp_image: segment 4: paddr=0x000755ac vaddr=0x40082af0 size=0x0a2e8 ( 41704) load
0x40082af0: _lock_try_acquire at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/newlib/locks.c:175

I (310) boot: Loaded app from partition at offset 0x10000
I (310) boot: Disabling RNG early entropy source...
I (311) cpu_start: Pro cpu up.
I (314) cpu_start: Application information:
I (319) cpu_start: Project name:     spiffs_amr_resample_app
I (326) cpu_start: App version:      v2.2-216-g29c606a4
I (331) cpu_start: Compile time:     Nov  8 2021 16:55:43
I (338) cpu_start: ELF file SHA256:  6a240ffe175924a2...
I (343) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (349) cpu_start: Starting app cpu, entry point is 0x40081900
0x40081900: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (360) heap_init: Initializing. RAM available for dynamic allocation:
I (367) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (373) heap_init: At 3FFB2CD0 len 0002D330 (180 KiB): DRAM
I (379) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (385) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (392) heap_init: At 4008CDD8 len 00013228 (76 KiB): IRAM
I (398) cpu_start: Pro cpu start user code
I (416) spi_flash: detected chip: gd
I (417) spi_flash: flash io: dio
W (417) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (427) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
W (440) SPIFFS: mount failed, -10025. formatting...
I (11690) PERIPH_SPIFFS: Partition size: total: 896321, used: 0
E (11690) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (11710) PERIPH_TOUCH: _touch_init
I (11730) SPIFFS_AMR_RESAMPLE_EXAMPLE: [1.1] Initialize recorder pipeline
I (11730) SPIFFS_AMR_RESAMPLE_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (11770) SPIFFS_AMR_RESAMPLE_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (11770) SPIFFS_AMR_RESAMPLE_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (11780) I2S: I2S driver already installed
I (11800) SPIFFS_AMR_RESAMPLE_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (11800) SPIFFS_AMR_RESAMPLE_EXAMPLE: [ 3 ] Set up  event listener
E (1882690) SPIFFS_AMR_RESAMPLE_EXAMPLE: STOP playback and START recording
W (1882690) AUDIO_PIPELINE: Without stop, st:1
W (1882690) AUDIO_PIPELINE: Without wait stop, st:1
I (1882690) SPIFFS_AMR_RESAMPLE_EXAMPLE: Link audio elements to make recorder pipeline ready
I (1882700) SPIFFS_AMR_RESAMPLE_EXAMPLE: Setup file path to save recorded audio
I (1885720) SPIFFS_AMR_RESAMPLE_EXAMPLE: STOP recording and START playback
W (1885720) AUDIO_ELEMENT: IN-[filter_downsample] AEL_IO_ABORT
E (1885730) AUDIO_ELEMENT: [filter_downsample] Element already stopped
W (1885730) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
W (1885740) AUDIO_ELEMENT: IN-[amrnb_encoder] AEL_IO_ABORT
E (1885740) AUDIO_ELEMENT: [file_writer] Element already stopped
I (1885750) SPIFFS_AMR_RESAMPLE_EXAMPLE: Link audio elements to make playback pipeline ready
I (1885770) SPIFFS_AMR_RESAMPLE_EXAMPLE: Setup file path to read the amr audio to play
W (1885800) SPIFFS_STREAM: No more data, ret:0
```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
