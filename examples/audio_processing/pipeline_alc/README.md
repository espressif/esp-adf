# Adjust WAV Volume with ALC

- [中文版本](./README_CN.md)
- Baisc Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how to use the automatic level control (ALC) of ADF. It detects the amplitude of the voice signal and determines whether it exceeds the preset Max/Min value. If it exceeds the range, the amplitude of the audio signal will be adjusted according to the set gain to make it within a reasonable range, so as to achieve the purpose of controlling the volume.

ADF provides the following two ways to adjust volume with ALC:

- For pipelines where `USE_ALONE_ALC` is enabled:

  ```
  sdcard ---> fatfs_stream ---> wav_decoder ---> ALC ---> i2s_stream ---> codec_chip ---> speaker
  ```

- For pipelines where `USE_ALONE_ALC` is not enabled:

  ```
  sdcard ---> fatfs_stream ---> wav_decoder ---> i2s_stream ---> codec_chip ---> speaker
  ```


## Environment Setup

#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the table below. Please remember to select the board in menuconfig as discussed in Section *Configuration* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Incompatible") |


## Build and Flash


### Default IDF Branch

The default IDF branch of this example is ADF's built-in branch `$ADF_PATH/esp-idf`.


### Configuration

In this example, you need to prepare an microSD card with an audio file in WAV format named `test.wav`, and insert the card into the development board.

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

If you need to modify the audio file name, please enable FatFs long file name support.

```
menuconfig > Component config > FAT Filesystem support > Long filename support
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.


## How to Use the Example


### Example Functionality

In the program, `USE_ALONE_ALC` is not enabled by default.

```
// #define USE_ALONE_ALC
```

That is, the volume amplitude is controlled by the function `i2s_alc_volume_set` in `i2s_stream.h`.


- After the example starts running, it will automatically process the excessively high amplitude of the audio data. For the related log, please refer to [Example Log](###Example Log).


### Example Log

A complete log is as follows:

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 12:05:31
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 2MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00100000
I (90) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0f750 ( 63312) map
I (134) esp_image: segment 1: paddr=0x0001f778 vaddr=0x3ffb0000 size=0x008a0 (  2208) load
I (135) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x2bea8 (179880) map
0x400d0020: _stext at ??:?

I (209) esp_image: segment 3: paddr=0x0004bed0 vaddr=0x3ffb08a0 size=0x01960 (  6496) load
I (212) esp_image: segment 4: paddr=0x0004d838 vaddr=0x40080000 size=0x0daf0 ( 56048) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (248) boot: Loaded app from partition at offset 0x10000
I (248) boot: Disabling RNG early entropy source...
I (248) cpu_start: Pro cpu up.
I (252) cpu_start: Application information:
I (257) cpu_start: Project name:     use_alc_set_volume_app
I (263) cpu_start: App version:      v2.2-202-g020b5c49-dirty
I (270) cpu_start: Compile time:     Sep 30 2021 12:05:25
I (276) cpu_start: ELF file SHA256:  3b7b0211f9ca2f5f...
I (282) cpu_start: ESP-IDF:          v4.2.2
I (287) cpu_start: Starting app cpu, entry point is 0x40081988
0x40081988: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (297) heap_init: Initializing. RAM available for dynamic allocation:
I (304) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (310) heap_init: At 3FFB2B48 len 0002D4B8 (181 KiB): DRAM
I (316) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (322) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (329) heap_init: At 4008DAF0 len 00012510 (73 KiB): IRAM
I (335) cpu_start: Pro cpu start user code
I (353) spi_flash: detected chip: gd
I (354) spi_flash: flash io: dio
W (354) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (364) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (375) USE_ALC_EXAMPLE: [ 1 ] Mount sdcard
I (885) USE_ALC_EXAMPLE: [ 2 ] Start codec chip
E (885) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (905) USE_ALC_EXAMPLE: [3.0] Create audio pipeline for playback
I (905) USE_ALC_EXAMPLE: [3.1] Create fatfs stream to read data from sdcard
I (905) USE_ALC_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (945) USE_ALC_EXAMPLE: [3.3] Create wav decoder to decode wav file
I (945) USE_ALC_EXAMPLE: [3.4] Register all elements to audio pipeline
I (945) USE_ALC_EXAMPLE: [3.5] Link it together [sdcard]-->fatfs_stream-->wav_decoder-->i2s_stream-->[codec_chip]
I (955) USE_ALC_EXAMPLE: [3.6] Set up  uri (file as fatfs_stream, wav as wav decoder, and default output is i2s)
I (965) USE_ALC_EXAMPLE: [ 4 ] Set up  event listener
I (975) USE_ALC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (975) USE_ALC_EXAMPLE: [4.2] Listening event from peripherals
I (985) USE_ALC_EXAMPLE: [ 5 ] Start audio_pipeline
I (1025) USE_ALC_EXAMPLE: [ 6 ] Listen for all pipeline events
I (1035) USE_ALC_EXAMPLE: [ * ] Receive music info from wav decoder, sample_rates=48000, bits=16, ch=2
W (91795) FATFS_STREAM: No more data, ret:0
W (432735) HEADPHONE: Headphone jack inserted

```


## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
