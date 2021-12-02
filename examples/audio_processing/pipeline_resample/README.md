# Record and Playback with Resample

- [中文版本](./README_CN.md)
- Baisc Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates the use of ADF's resample. When recording, the 48000 Hz, 16-bit, and 2-channel audio data is resampled to 16000 Hz, 16-bit, and 1-channel data, then encoded into WAV format and written to the microSD card. When playing back, the device reads the 16000 Hz, 16-bit, and 1-channel WAV file from the microSD card, resamples it to the 48000 Hz, 16-bit, and 2-channel data, and then plays the recorded data through the I2S peripheral.


1. For the recording process:

   - Set up I2S and obtain the audio at 48000 Hz, 16 bits, and the stereo sampling rate.
   - Use the resampling filter to convert the data to 16000 Hz, 16 bits, 1 channel.
   - Use WAV encoder to encode the data.
   - Write to the microSD card.

   The pipeline to resample the recording and save it to the microSD card is as follows:

   ```
   mic ---> codec_chip ---> i2s_stream ---> resample_filter ---> wav_encoder ---> fatfs_stream ---> sdcard
   ```

2. For the recording playback process:

   - Read the file recorded in the microSD card. The sampling rate is 16000 Hz, 16 bits, and 1 channel.
   - Use WAV decoder to decode the file data.
   - Use the resampling filter to convert the data to 48000 Hz, 16 bits, and 2 channels.
   - Write the data to the I2S peripheral.


   The pipeline to read the data from the microSD card, resample it, and play it though I2S is as follows:

   ```
   sdcard ---> fatfs_stream ---> wav_decoder ---> resample_filter ---> i2s_stream ---> codec_chip ---> speaker
   ```


## Environment Setup


#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the table below. Please remember to select the board in menuconfig as discussed in Section *Configuration* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "Incompatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Incompatible") |


## Build and Flash


### Default IDF Branch

The default IDF branch of this example is ADF's built-in branch `$ADF_PATH/esp-idf`.


### Configuration

In this example, you need to prepare an microSD card and insert the card into the development board to save the recording in WAV format.

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

- The example starts running and is waiting for you to press the [REC] key. The log is as follows:

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 20:11:50
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 40MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 2MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (68) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00100000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0f4f0 ( 62704) map
I (120) esp_image: segment 1: paddr=0x0001f518 vaddr=0x3ffb0000 size=0x00b00 (  2816) load
I (121) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x323f8 (205816) map
0x400d0020: _stext at ??:?

I (205) esp_image: segment 3: paddr=0x00052420 vaddr=0x3ffb0b00 size=0x016f0 (  5872) load
I (207) esp_image: segment 4: paddr=0x00053b18 vaddr=0x40080000 size=0x0d970 ( 55664) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (243) boot: Loaded app from partition at offset 0x10000
I (243) boot: Disabling RNG early entropy source...
I (244) cpu_start: Pro cpu up.
I (247) cpu_start: Application information:
I (252) cpu_start: Project name:     resample_app
I (258) cpu_start: App version:      v2.2-202-gc3504872-dirty
I (264) cpu_start: Compile time:     Sep 29 2021 20:11:45
I (270) cpu_start: ELF file SHA256:  4250af7904a84fee...
I (276) cpu_start: ESP-IDF:          v4.2.2
I (281) cpu_start: Starting app cpu, entry point is 0x40081970
0x40081970: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (291) heap_init: Initializing. RAM available for dynamic allocation:
I (298) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (304) heap_init: At 3FFB2B68 len 0002D498 (181 KiB): DRAM
I (310) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (317) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (323) heap_init: At 4008D970 len 00012690 (73 KiB): IRAM
I (329) cpu_start: Pro cpu start user code
I (347) spi_flash: detected chip: gd
I (348) spi_flash: flash io: dio
W (348) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (358) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (870) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (890) PERIPH_TOUCH: _touch_init
I (1410) RESAMPLE_EXAMPLE: [1.1] Initialize recorder pipeline
I (1410) RESAMPLE_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (1430) RESAMPLE_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (1430) RESAMPLE_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (1430) I2S: I2S driver already installed
I (1440) RESAMPLE_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (1460) RESAMPLE_EXAMPLE: [ 3 ] Set up  event listener
```

- Press the [REC] key on the audio board to start recording, resample the data, encode it into a WAV file, and then write it to the microSD card for storage. The log is as follows:

```c
E (25420) RESAMPLE_EXAMPLE: STOP Playback and START [Record]
W (25420) AUDIO_PIPELINE: Without stop, st:1
W (25420) AUDIO_PIPELINE: Without wait stop, st:1
I (25420) RESAMPLE_EXAMPLE: Link audio elements to make recorder pipeline ready
I (25430) RESAMPLE_EXAMPLE: Setup file path to save recorded audio
```

- When you release the [REC] key, the example will read the WAV file from the microSD card, resample it, and send it to the I2S interface to play the recorded file. The log is as follows:

```c
I (31540) RESAMPLE_EXAMPLE: STOP [Record] and START Playback
W (31540) AUDIO_ELEMENT: IN-[filter_downsample] AEL_IO_ABORT
E (31550) AUDIO_ELEMENT: [filter_downsample] Element already stopped
W (31550) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
W (31560) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
I (31570) RESAMPLE_EXAMPLE: Link audio elements to make playback pipeline ready
I (31570) RESAMPLE_EXAMPLE: Setup file path to read the wav audio to play
W (37070) FATFS_STREAM: No more data, ret:0
```


### Example Log

A complete log is as follows:

```c
cets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7152
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 20:11:50
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 40MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 2MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (68) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00100000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0f4f0 ( 62704) map
I (120) esp_image: segment 1: paddr=0x0001f518 vaddr=0x3ffb0000 size=0x00b00 (  2816) load
I (121) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x323f8 (205816) map
0x400d0020: _stext at ??:?

I (205) esp_image: segment 3: paddr=0x00052420 vaddr=0x3ffb0b00 size=0x016f0 (  5872) load
I (207) esp_image: segment 4: paddr=0x00053b18 vaddr=0x40080000 size=0x0d970 ( 55664) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (243) boot: Loaded app from partition at offset 0x10000
I (243) boot: Disabling RNG early entropy source...
I (244) cpu_start: Pro cpu up.
I (247) cpu_start: Application information:
I (252) cpu_start: Project name:     resample_app
I (258) cpu_start: App version:      v2.2-202-gc3504872-dirty
I (264) cpu_start: Compile time:     Sep 29 2021 20:11:45
I (270) cpu_start: ELF file SHA256:  4250af7904a84fee...
I (276) cpu_start: ESP-IDF:          v4.2.2
I (281) cpu_start: Starting app cpu, entry point is 0x40081970
0x40081970: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (291) heap_init: Initializing. RAM available for dynamic allocation:
I (298) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (304) heap_init: At 3FFB2B68 len 0002D498 (181 KiB): DRAM
I (310) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (317) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (323) heap_init: At 4008D970 len 00012690 (73 KiB): IRAM
I (329) cpu_start: Pro cpu start user code
I (347) spi_flash: detected chip: gd
I (348) spi_flash: flash io: dio
W (348) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (358) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (870) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (890) PERIPH_TOUCH: _touch_init
I (1410) RESAMPLE_EXAMPLE: [1.1] Initialize recorder pipeline
I (1410) RESAMPLE_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (1430) RESAMPLE_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (1430) RESAMPLE_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (1430) I2S: I2S driver already installed
I (1440) RESAMPLE_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (1460) RESAMPLE_EXAMPLE: [ 3 ] Set up  event listener
E (25420) RESAMPLE_EXAMPLE: STOP Playback and START [Record]
W (25420) AUDIO_PIPELINE: Without stop, st:1
W (25420) AUDIO_PIPELINE: Without wait stop, st:1
I (25420) RESAMPLE_EXAMPLE: Link audio elements to make recorder pipeline ready
I (25430) RESAMPLE_EXAMPLE: Setup file path to save recorded audio
I (31540) RESAMPLE_EXAMPLE: STOP [Record] and START Playback
W (31540) AUDIO_ELEMENT: IN-[filter_downsample] AEL_IO_ABORT
E (31550) AUDIO_ELEMENT: [filter_downsample] Element already stopped
W (31550) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
W (31560) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
I (31570) RESAMPLE_EXAMPLE: Link audio elements to make playback pipeline ready
I (31570) RESAMPLE_EXAMPLE: Setup file path to read the wav audio to play
W (37070) FATFS_STREAM: No more data, ret:0
```


## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
