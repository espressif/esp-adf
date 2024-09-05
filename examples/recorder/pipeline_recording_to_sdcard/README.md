
# Multi-format recording routine

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example mainly demonstrates the use of WAV, OPUS, AMRNB and AMRWB encoders pipeline. Voice data is read from the i2s stream, encoded by the encoder, and finally written into the microSD card by the fatfs stream.

This example supports four audio encoders: WAV, OPUS, AMRNB and AMRWB. The WAV encoder is selected by default to encode the recording, and save the recording file to the microSD card.

## Environment Setup

### Hardware Required

A microSD card needs to be prepared and inserted into the development board in advance for this example.

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Example Set Up

### Default IDF Branch
This example supports IDF release/v5.0 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration
The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, you need to select the configuration of the development board in menuconfig, for example, select `ESP32-Lyrat-Mini V1.1`.

```c
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

This example needs to enable FATFS long file name support also. For example, file names like `rec.opus` are beyond the default `8.3 naming` specification.

```c
menuconfig > Component config > FAT Filesystem support > Long filename support
```

The default encoder of this example is WAV. If you change to another encoder, you need to select the encoder in the `menuconfig`, such as the OPUS encoder.

```c
menuconfig > Example configuration > Encoding type of recording file > opus encoder
```

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the Getting Started Guide for full steps to configure and use  [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/index.html) to build projects.

## How to use the Example

### Example Functionality
After the routine starts to run, it will print the following countdown prompt and prompt that the recording has started.

```c
I (655) RECORDING_TO_SDCARD: [ 5 ] Start audio_pipeline
I (665) RECORDING_TO_SDCARD: [ 6 ] Listen for all pipeline events, record for 10 Seconds
I (1725) RECORDING_TO_SDCARD: [ * ] Recording ... 1
I (2725) RECORDING_TO_SDCARD: [ * ] Recording ... 2
I (3735) RECORDING_TO_SDCARD: [ * ] Recording ... 3
```

At the end of the routine, the following is printed. At this time, you can remove the microSD card and use a computer player to play the audio file. The audio file is named like `rec.wav` or `rec.opus`.

```c
W (10975) RECORDING_TO_SDCARD: [ * ] Stop event received
I (10975) RECORDING_TO_SDCARD: [ 7 ] Stop audio_pipeline
E (10975) AUDIO_ELEMENT: [opus] Element already stopped
E (10975) AUDIO_ELEMENT: [res] Element already stopped
E (10985) AUDIO_ELEMENT: [file] Element already stopped
W (10995) AUDIO_PIPELINE: There are no listener registered
W (10995) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11005) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11015) AUDIO_ELEMENT: [res] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11025) AUDIO_ELEMENT: [opus] Element has not create when AUDIO_ELEMENT_TERMINATE
```

### Example Logs
A complete log is as follows:

```c
--- idf_monitor on /dev/ttyUSB0 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
I (13) boot: ESP-IDF v3.3.2-107-g722043f73 2nd stage bootloader
I (13) boot: compile time 17:12:08
I (13) boot: Enabling RNG early entropy source...
I (18) boot: SPI Speed      : 40MHz
I (22) boot: SPI Mode       : DIO
I (26) boot: SPI Flash Size : 8MB
I (30) boot: Partition Table:
I (33) boot: ## Label            Usage          Type ST Offset   Length
I (41) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (48) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (56) boot:  2 factory          factory app      00 00 00010000 00100000
I (63) boot: End of partition table
I (67) boot_comm: chip revision: 1, min. application chip revision: 0
I (74) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1bc98 (113816) map
I (123) esp_image: segment 1: paddr=0x0002bcc0 vaddr=0x3ffb0000 size=0x01f7c (  8060) load
I (127) esp_image: segment 2: paddr=0x0002dc44 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (130) esp_image: segment 3: paddr=0x0002e04c vaddr=0x40080400 size=0x01fc4 (  8132) load
I (142) esp_image: segment 4: paddr=0x00030018 vaddr=0x400d0018 size=0x4e4b4 (320692) map
0x400d0018: _flash_cache_start at ??:?

I (260) esp_image: segment 5: paddr=0x0007e4d4 vaddr=0x400823c4 size=0x0910c ( 37132) load
0x400823c4: _xt_coproc_exc at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1019

I (283) boot: Loaded app from partition at offset 0x10000
I (283) boot: Disabling RNG early entropy source...
I (283) cpu_start: Pro cpu up.
I (287) cpu_start: Application information:
I (292) cpu_start: Project name:     recording_to_sdcard
I (298) cpu_start: App version:      v2.2-110-gb3efcc37-dirty
I (304) cpu_start: Compile time:     Apr 22 2021 17:12:07
I (310) cpu_start: ELF file SHA256:  ee3b208c350a5270...
I (316) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (322) cpu_start: Starting app cpu, entry point is 0x400811f8
0x400811f8: call_start_cpu1 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (333) heap_init: Initializing. RAM available for dynamic allocation:
I (340) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (346) heap_init: At 3FFB30D8 len 0002CF28 (179 KiB): DRAM
I (352) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (358) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (365) heap_init: At 4008B4D0 len 00014B30 (82 KiB): IRAM
I (371) cpu_start: Pro cpu start user code
I (54) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (55) RECORDING_TO_SDCARD: [ 1 ] Mount sdcard
I (555) RECORDING_TO_SDCARD: [ 2 ] Start codec chip
W (575) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
I (585) RECORDING_TO_SDCARD: [3.0] Create audio pipeline for recording
I (585) RECORDING_TO_SDCARD: [3.1] Create fatfs stream to write data to sdcard
I (595) RECORDING_TO_SDCARD: [3.2] Create i2s stream to read audio data from codec chip
I (605) RECORDING_TO_SDCARD: [3.3] Create audio encoder to handle data
I (605) RECORDING_TO_SDCARD: [3.4] Register all elements to audio pipeline
I (615) RECORDING_TO_SDCARD: [3.5] Link it together [codec_chip]-->i2s_stream-->audio_encoder-->fatfs_stream-->[sdcard]
I (625) RECORDING_TO_SDCARD: [3.6] Set up  uri
I (635) RECORDING_TO_SDCARD: [ 4 ] Set up  event listener
I (635) RECORDING_TO_SDCARD: [4.1] Listening event from pipeline
I (645) RECORDING_TO_SDCARD: [4.2] Listening event from peripherals
I (655) RECORDING_TO_SDCARD: [ 5 ] Start audio_pipeline
I (665) RECORDING_TO_SDCARD: [ 6 ] Listen for all pipeline events, record for 10 Seconds
I (1725) RECORDING_TO_SDCARD: [ * ] Recording ... 1
I (2725) RECORDING_TO_SDCARD: [ * ] Recording ... 2
I (3735) RECORDING_TO_SDCARD: [ * ] Recording ... 3
I (4735) RECORDING_TO_SDCARD: [ * ] Recording ... 4
I (5735) RECORDING_TO_SDCARD: [ * ] Recording ... 5
I (6735) RECORDING_TO_SDCARD: [ * ] Recording ... 6
I (7745) RECORDING_TO_SDCARD: [ * ] Recording ... 7
I (8885) RECORDING_TO_SDCARD: [ * ] Recording ... 8
I (9885) RECORDING_TO_SDCARD: [ * ] Recording ... 9
I (10915) RECORDING_TO_SDCARD: [ * ] Recording ... 10
W (10975) RECORDING_TO_SDCARD: [ * ] Stop event received
I (10975) RECORDING_TO_SDCARD: [ 7 ] Stop audio_pipeline
E (10975) AUDIO_ELEMENT: [opus] Element already stopped
E (10975) AUDIO_ELEMENT: [res] Element already stopped
E (10985) AUDIO_ELEMENT: [file] Element already stopped
W (10995) AUDIO_PIPELINE: There are no listener registered
W (10995) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11005) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11015) AUDIO_ELEMENT: [res] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11025) AUDIO_ELEMENT: [opus] Element has not create when AUDIO_ELEMENT_TERMINATE
```

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
