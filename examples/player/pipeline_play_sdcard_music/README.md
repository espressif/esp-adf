
# Play multiple format music from microSD card

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example uses the fatfs element to read the music file from microSD card, the decoder element to decode it, and then the I2S element to output the music.

This example supports MP3, OPUS, OGG, FLAC, AAC, M4A, TS, MP4, AMRNB and AMRWB audio formats, MP3 format is selected by default.

The audio source referenced in the example can be obtained through [Audio Samples/Short Samples](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/audio-samples.html#short-samples) and downloaded to the microSD card.

The following table lists the music formats supported by this example:

|NO.|music format|file name|
|-|-|-|
|1|MP3|test.mp3|
|2|AMRNB|test.amr|
|3|AMRWB|test.Wamr|
|4|OPUS|test.opus|
|5|FLAC|test.flac|
|6|WAV|test.wav|
|7|AAC|test.aac|
|8|M4A|test.m4a|
|9|TS|test.ts|
|10|MP4|test.mp4|

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Example Set Up

### Default IDF Branch
This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Prepare a microSD card, and download [Audio Samples/Short Samples](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/audio-samples.html#short-samples) audio music to the microSD card. Of course it can also be user-supplied music.

> In this example, the file name to be played is fixed, starting with `test` and ending with the format name suffix, such as `test.mp3`.

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, you need to select the configuration of the development board in menuconfig, for example, select `ESP32-Lyrat-Mini V1.1`.

```c
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

This example needs to enable FATFS long file name support also.

```c
menuconfig > Component config > FAT Filesystem support > Long filename support
```

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the Getting Started Guide for full steps to configure and use  [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) to build projects.

## How to use the Example

### Example Functionality
- After the routine starts to run, it will automatically play the SmicroSD card music.

```c
I (29) SDCARD_MUSIC_EXAMPLE: [ 1 ] Mount sdcard
I (529) SDCARD_MUSIC_EXAMPLE: [ 2 ] Start codec chip
E (529) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (549) SDCARD_MUSIC_EXAMPLE: [3.0] Create audio pipeline for playback
I (549) SDCARD_MUSIC_EXAMPLE: [3.1] Create fatfs stream to read data from sdcard
I (559) SDCARD_MUSIC_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (569) SDCARD_MUSIC_EXAMPLE: [3.3] Create mp3 decoder
I (569) SDCARD_MUSIC_EXAMPLE: [3.4] Register all elements to audio pipeline
I (579) SDCARD_MUSIC_EXAMPLE: [3.5] Link it together [sdcard]-->fatfs_stream-->music_decoder-->i2s_stream-->[codec_chip]
I (589) SDCARD_MUSIC_EXAMPLE: [3.6] Set up uri: /sdcard/test.mp3
I (599) SDCARD_MUSIC_EXAMPLE: [ 4 ] Set up  event listener
I (599) SDCARD_MUSIC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (609) SDCARD_MUSIC_EXAMPLE: [4.2] Listening event from peripherals
I (619) SDCARD_MUSIC_EXAMPLE: [ 5 ] Start audio_pipeline
I (629) SDCARD_MUSIC_EXAMPLE: [ 6 ] Listen for all pipeline events
I (639) SDCARD_MUSIC_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (15869) FATFS_STREAM: No more data, ret:0
W (16579) SDCARD_MUSIC_EXAMPLE: [ * ] Stop event received
I (16579) SDCARD_MUSIC_EXAMPLE: [ 7 ] Stop audio_pipeline
E (16579) AUDIO_ELEMENT: [file] Element already stopped
E (16589) AUDIO_ELEMENT: [dec] Element already stopped
E (16599) AUDIO_ELEMENT: [i2s] Element already stopped
W (16599) AUDIO_PIPELINE: There are no listener registered
W (16609) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (16619) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (16619) AUDIO_ELEMENT: [dec] Element has not create when AUDIO_ELEMENT_TERMINATE
```

### Example Logs
A complete log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0018,len:4
load:0x3fff001c,len:6840
load:0x40078000,len:12072
load:0x40080400,len:6708
entry 0x40080778
I (73) boot: Chip Revision: 3
I (73) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) boot: ESP-IDF v3.3.2-107-g722043f73 2nd stage bootloader
I (40) boot: compile time 17:56:08
I (40) boot: Enabling RNG early entropy source...
I (46) boot: SPI Speed      : 40MHz
I (50) boot: SPI Mode       : DIO
I (54) boot: SPI Flash Size : 8MB
I (58) boot: Partition Table:
I (62) boot: ## Label            Usage          Type ST Offset   Length
I (69) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (84) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (103) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x18028 ( 98344) map
I (146) esp_image: segment 1: paddr=0x00028050 vaddr=0x3ffb0000 size=0x01f64 (  8036) load
I (150) esp_image: segment 2: paddr=0x00029fbc vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (153) esp_image: segment 3: paddr=0x0002a3c4 vaddr=0x40080400 size=0x05c4c ( 23628) load
I (172) esp_image: segment 4: paddr=0x00030018 vaddr=0x400d0018 size=0x2f65c (194140) map
0x400d0018: _flash_cache_start at ??:?

I (240) esp_image: segment 5: paddr=0x0005f67c vaddr=0x4008604c size=0x05940 ( 22848) load
0x4008604c: prvReceiveGeneric at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp_ringbuf/ringbuf.c:969

I (257) boot: Loaded app from partition at offset 0x10000
I (257) boot: Disabling RNG early entropy source...
I (257) cpu_start: Pro cpu up.
I (261) cpu_start: Application information:
I (266) cpu_start: Project name:     play_sdcard_music
I (272) cpu_start: App version:      v2.2-103-g33721b98-dirty
I (278) cpu_start: Compile time:     Apr 27 2021 19:37:04
I (284) cpu_start: ELF file SHA256:  5d6c86d684a92743...
I (290) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (296) cpu_start: Starting app cpu, entry point is 0x40081200
0x40081200: call_start_cpu1 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (307) heap_init: Initializing. RAM available for dynamic allocation:
I (314) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (320) heap_init: At 3FFB30C0 len 0002CF40 (179 KiB): DRAM
I (326) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (332) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (339) heap_init: At 4008B98C len 00014674 (81 KiB): IRAM
I (345) cpu_start: Pro cpu start user code
I (27) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (29) SDCARD_MUSIC_EXAMPLE: [ 1 ] Mount sdcard
I (529) SDCARD_MUSIC_EXAMPLE: [ 2 ] Start codec chip
E (529) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (549) SDCARD_MUSIC_EXAMPLE: [3.0] Create audio pipeline for playback
I (549) SDCARD_MUSIC_EXAMPLE: [3.1] Create fatfs stream to read data from sdcard
I (559) SDCARD_MUSIC_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (569) SDCARD_MUSIC_EXAMPLE: [3.3] Create mp3 decoder
I (569) SDCARD_MUSIC_EXAMPLE: [3.4] Register all elements to audio pipeline
I (579) SDCARD_MUSIC_EXAMPLE: [3.5] Link it together [sdcard]-->fatfs_stream-->music_decoder-->i2s_stream-->[codec_chip]
I (589) SDCARD_MUSIC_EXAMPLE: [3.6] Set up uri: /sdcard/test.mp3
I (599) SDCARD_MUSIC_EXAMPLE: [ 4 ] Set up  event listener
I (599) SDCARD_MUSIC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (609) SDCARD_MUSIC_EXAMPLE: [4.2] Listening event from peripherals
I (619) SDCARD_MUSIC_EXAMPLE: [ 5 ] Start audio_pipeline
I (629) SDCARD_MUSIC_EXAMPLE: [ 6 ] Listen for all pipeline events
I (639) SDCARD_MUSIC_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (15869) FATFS_STREAM: No more data, ret:0
W (16579) SDCARD_MUSIC_EXAMPLE: [ * ] Stop event received
I (16579) SDCARD_MUSIC_EXAMPLE: [ 7 ] Stop audio_pipeline
E (16579) AUDIO_ELEMENT: [file] Element already stopped
E (16589) AUDIO_ELEMENT: [dec] Element already stopped
E (16599) AUDIO_ELEMENT: [i2s] Element already stopped
W (16599) AUDIO_PIPELINE: There are no listener registered
W (16609) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (16619) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (16619) AUDIO_ELEMENT: [dec] Element has not create when AUDIO_ELEMENT_TERMINATE
```

## Troubleshooting
If your log has the following error message, this is because the audio file that needs to be played is not found in the microSD card, please follow the above *Configuration* section to rename the file.

```c
I (608) SDCARD_MUSIC_EXAMPLE: [4.2] Listening event from peripherals
I (618) SDCARD_MUSIC_EXAMPLE: [ 5 ] Start audio_pipeline
E (628) FATFS_STREAM: Failed to open. File name: /sdcard/gs-16b-2c-44100hz.mp3, error message: No such file or directory, line: 116
E (638) AUDIO_ELEMENT: [file] AEL_STATUS_ERROR_OPEN,-1
W (638) AUDIO_ELEMENT: [file] audio_element_on_cmd_error,7
W (648) AUDIO_ELEMENT: IN-[dec] AEL_IO_ABORT
E (648) MP3_DECODER: failed to read audio data (line 118)
W (658) AUDIO_ELEMENT: [dec] AEL_IO_ABORT, -3
W (658) AUDIO_ELEMENT: IN-[i2s] AEL_IO_ABORT
```

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
