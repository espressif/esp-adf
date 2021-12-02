# Process WAV Playback with Equalizer

- [中文版本](./README_CN.md)
- Baisc Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates the process of using ADF's equalizer to process playback of WAV audio files. The data stream in the pipeline is as follows:

```
sdcard ---> fatfs_stream ---> wav_decoder ---> equalizer ---> i2s_stream ---> codec_chip
```

First, fatfs_stream reads the audio file named `test.wav` from the microSD card (prepared by yourself), then the data of this file is decoded by the wav_decoder decoder, processed by the equalizer, and finally sent to the codec chip through I2S for playback.


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

If you need to change the audio file name to something more than 8 characters’ long, it is recommended to enable FatFs long file name support.

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

After the example start running, the device reads the `test.wav` file from the microSD card, automatically uses the equalizer to process it, and then plays the processed audio. For the related log, please refer to [Example Log](#example-log).

To change the parameters of the equalizer, please edit the `set_gain[]` table in `equalizer_example.c`. The center frequencies of the equalizer are 31 Hz, 62 Hz, 125 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz, 4 kHz, 8 kHz, and 16 kHz.

- The `test.wav` used by this example is an 16-bit single-channel audio file at a sampling rate of 44100 Hz.
- Below is the original spectrum image of `test.wav`. It is located in `document/spectrum_before.png`.
  <div  align="center"><img src="document/spectrum_before.png" width="700" alt ="spectrum_before" align=center /></div>
- Below is the spectrum image of `test.wav` that has been processed by the equalizer. The equalizer gain is -13 dB. It is located in `document/spectrum_after.png`.
  <div align="center"><img src="document/spectrum_after.png" width="700" alt ="spectrum_after" align=center /></div>
- Below is the frequency response image when the equalizer gain is 0 dB. It is located in the `document/amplitude_frequency.png`.
  <div align="center"><img src="document/amplitude_frequency.png" width="700" alt ="amplitude_frequency" align=center /></div>


### Example Log

A complete log is as follows:

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 10:23:02
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
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0fdf0 ( 65008) map
I (135) esp_image: segment 1: paddr=0x0001fe18 vaddr=0x3ffb0000 size=0x00200 (   512) load
I (135) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x2e0a0 (188576) map
0x400d0020: _stext at ??:?

I (213) esp_image: segment 3: paddr=0x0004e0c8 vaddr=0x3ffb0200 size=0x02050 (  8272) load
I (217) esp_image: segment 4: paddr=0x00050120 vaddr=0x40080000 size=0x0daf0 ( 56048) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (252) boot: Loaded app from partition at offset 0x10000
I (252) boot: Disabling RNG early entropy source...
I (252) cpu_start: Pro cpu up.
I (256) cpu_start: Application information:
I (261) cpu_start: Project name:     equalizer_app
I (266) cpu_start: App version:      v2.2-202-g020b5c49-dirty
I (273) cpu_start: Compile time:     Sep 30 2021 10:22:56
I (279) cpu_start: ELF file SHA256:  2f6692ee6bcc59a3...
I (285) cpu_start: ESP-IDF:          v4.2.2
I (290) cpu_start: Starting app cpu, entry point is 0x40081988
0x40081988: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (300) heap_init: Initializing. RAM available for dynamic allocation:
I (307) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (313) heap_init: At 3FFB2B98 len 0002D468 (181 KiB): DRAM
I (319) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (326) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (332) heap_init: At 4008DAF0 len 00012510 (73 KiB): IRAM
I (338) cpu_start: Pro cpu start user code
I (356) spi_flash: detected chip: gd
I (357) spi_flash: flash io: dio
W (357) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (367) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (378) EQUALIZER_EXAMPLE: [1.0] Mount sdcard
I (888) EQUALIZER_EXAMPLE: [2.0] Start codec chip
E (888) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (908) EQUALIZER_EXAMPLE: [3.0] Create audio pipeline
I (938) EQUALIZER_EXAMPLE: [3.1] Register all elements to audio pipeline
I (938) EQUALIZER_EXAMPLE: [3.2] Link it together [sdcard]-->fatfs_stream-->wav_decoder-->equalizer-->i2s_stream
I (938) EQUALIZER_EXAMPLE: [3.3] Set up  uri (file as fatfs_stream, wav as wav decoder, and default output is i2s)
I (948) EQUALIZER_EXAMPLE: [4.0] Set up  event listener
I (958) EQUALIZER_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (968) EQUALIZER_EXAMPLE: [4.2] Listening event from peripherals
I (968) EQUALIZER_EXAMPLE: [5.0] Start audio_pipeline
I (988) EQUALIZER_EXAMPLE: [6.0] Listen for all pipeline events
I (998) EQUALIZER_EXAMPLE: [ * ] Receive music info from wav decoder, sample_rates=44100, bits=16, ch=1
W (91708) FATFS_STREAM: No more data, ret:0
W (91708) EQUALIZER_EXAMPLE: [ * ] Stop event received
I (91708) EQUALIZER_EXAMPLE: [7.0] Stop audio_pipeline and release all resources
E (91718) AUDIO_ELEMENT: [file_read] Element already stopped
W (91728) AUDIO_ELEMENT: OUT-[wavdec] AEL_IO_ABORT
W (91728) AUDIO_ELEMENT: OUT-[equalizer] AEL_IO_ABORT
W (91758) AUDIO_PIPELINE: There are no listener registered
W (91758) AUDIO_ELEMENT: [file_read] Element has not create when AUDIO_ELEMENT_TERMINATE
W (91768) AUDIO_ELEMENT: [wavdec] Element has not create when AUDIO_ELEMENT_TERMINATE
W (91778) AUDIO_ELEMENT: [equalizer] Element has not create when AUDIO_ELEMENT_TERMINATE
W (91788) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE

```


## Troubleshooting

To run this playback example, the following conditions must be met:

* The audio file must meet the requirements below to be supported by the equalizer:

    * Sampling rate: 11025 Hz, 22050 Hz, 44100 Hz, 48000 Hz.
    * Number of channels: 1 or 2.
    * Audio file format: WAV.


## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
