# Play MP3 Files from MicroSD Card with Playlist

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how to play MP3 files stored on a microSD card using the audio pipeline interface. The example first scans the microSD card for the MP3 files and saves the results of the scan as a playlist on the microSD card.

You can start, stop, pause, resume playback, advance to the next song, and adjust the volume. When the previous music ends playing, the application will automatically advance to the next song.


The pipeline is as follows:

```
[sdcard] ---> fatfs_stream ---> mp3_decoder ---> resample ---> i2s_stream ---> [codec_chip]
```

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Prepare a microSD card with some MP3 files on the card for backup.

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

Then, enable FatFs long filename support.

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

- After the example starts running, it automatically scans the microSD card for MP3 music files and saves them as a playlist on the microSD card, waiting to be played till keys are pressed. The log is as follows:

```c
I (0) cpu_start: App cpu up.
I (398) heap_init: Initializing. RAM available for dynamic allocation:
I (405) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (411) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (417) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (424) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (430) heap_init: At 4008E128 len 00011ED8 (71 KiB): IRAM
I (436) cpu_start: Pro cpu start user code
I (454) spi_flash: detected chip: gd
I (455) spi_flash: flash io: dio
W (455) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (465) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (476) SDCARD_MP3_CONTROL_EXAMPLE: [1.0] Initialize peripherals management
I (486) SDCARD_MP3_CONTROL_EXAMPLE: [1.1] Initialize and start peripherals
W (516) PERIPH_TOUCH: _touch_init
I (996) SDCARD_MP3_CONTROL_EXAMPLE: [1.2] Set up a sdcard playlist and scan sdcard music save to it
I (1776) SDCARD_MP3_CONTROL_EXAMPLE: [ 2 ] Start codec chip
E (1776) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [ 3 ] Create and start input key service
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [4.0] Create audio pipeline for playback
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [4.1] Create i2s stream to write data to codec chip
I (1856) SDCARD_MP3_CONTROL_EXAMPLE: [4.2] Create mp3 decoder to decode mp3 file
I (1856) SDCARD_MP3_CONTROL_EXAMPLE: [4.3] Create resample filter
I (1866) SDCARD_MP3_CONTROL_EXAMPLE: [4.4] Create fatfs stream to read data from sdcard
I (1886) SDCARD_MP3_CONTROL_EXAMPLE: [4.5] Register all elements to audio pipeline
I (1886) SDCARD_MP3_CONTROL_EXAMPLE: [4.6] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->resample-->i2s_stream-->[codec_chip]
I (1896) SDCARD_MP3_CONTROL_EXAMPLE: [5.0] Set up  event listener
I (1896) SDCARD_MP3_CONTROL_EXAMPLE: [5.1] Listen for all pipeline events
W (1906) SDCARD_MP3_CONTROL_EXAMPLE: [ 6 ] Press the keys to control music player:
W (1916) SDCARD_MP3_CONTROL_EXAMPLE:       [Play] to start, pause and resume, [Set] next song.
W (1936) SDCARD_MP3_CONTROL_EXAMPLE:       [Vol-] or [Vol+] to adjust volume.

```

- Press the [Play] key to start playback, and press the [Play] key again to pause playback. Press the [Set] key to play the next song, and use [Vol-] and [Vol+] to decrease and increase the volume.

```c
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 3
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Play] input key event
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Starting audio pipeline
I (15206) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 2
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Set] input key event
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Stopped, advancing to the next song
W (27376) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (27376) AUDIO_ELEMENT: OUT-[filter] AEL_IO_ABORT
W (27386) MP3_DECODER: output aborted -3
W (27376) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (27396) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/DU1906_蓝牙_测试音频.mp3
I (27526) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=48000, bits=16, ch=2
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 2
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Set] input key event
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Stopped, advancing to the next song
W (37126) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (37126) AUDIO_ELEMENT: OUT-[filter] AEL_IO_ABORT
W (37136) MP3_DECODER: output aborted -3
W (37136) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (37146) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/test_output.mp3
I (37186) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 6
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Vol+] input key event
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Volume set to 79 %
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 6
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Vol+] input key event
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Volume set to 88 %
I (199216) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 4
I (200776) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 4
W (222766) FATFS_STREAM: No more data, ret:0
I (223486) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (223486) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/start_service.mp3
I (223616) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=16000, bits=16, ch=1
W (223626) FATFS_STREAM: No more data, ret:0
I (225556) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (225556) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/dingdong.mp3
I (225616) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=22050, bits=16, ch=1
W (225616) FATFS_STREAM: No more data, ret:0
I (226236) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (226236) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/haode.mp3
I (226466) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=48000, bits=16, ch=1
W (226476) FATFS_STREAM: No more data, ret:0
I (227036) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (227036) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/test.mp3
I (227076) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2

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
I (27) boot: compile time 17:17:27
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x3f734 (259892) map
I (210) esp_image: segment 1: paddr=0x0004f75c vaddr=0x3ffb0000 size=0x008bc (  2236) load
I (211) esp_image: segment 2: paddr=0x00050020 vaddr=0x400d0020 size=0x3b0a8 (241832) map
0x400d0020: _stext at ??:?

I (309) esp_image: segment 3: paddr=0x0008b0d0 vaddr=0x3ffb08bc size=0x0195c (  6492) load
I (312) esp_image: segment 4: paddr=0x0008ca34 vaddr=0x40080000 size=0x0e128 ( 57640) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (348) boot: Loaded app from partition at offset 0x10000
I (348) boot: Disabling RNG early entropy source...
I (349) cpu_start: Pro cpu up.
I (353) cpu_start: Application information:
I (357) cpu_start: Project name:     play_sdcard_mp3_control
I (364) cpu_start: App version:      v2.2-260-gec0ea830
I (370) cpu_start: Compile time:     Nov 30 2021 15:26:59
I (376) cpu_start: ELF file SHA256:  91b340aeb4ef5838...
I (382) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (388) cpu_start: Starting app cpu, entry point is 0x400819bc
0x400819bc: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (398) heap_init: Initializing. RAM available for dynamic allocation:
I (405) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (411) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (417) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (424) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (430) heap_init: At 4008E128 len 00011ED8 (71 KiB): IRAM
I (436) cpu_start: Pro cpu start user code
I (454) spi_flash: detected chip: gd
I (455) spi_flash: flash io: dio
W (455) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (465) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (476) SDCARD_MP3_CONTROL_EXAMPLE: [1.0] Initialize peripherals management
I (486) SDCARD_MP3_CONTROL_EXAMPLE: [1.1] Initialize and start peripherals
W (516) PERIPH_TOUCH: _touch_init
I (996) SDCARD_MP3_CONTROL_EXAMPLE: [1.2] Set up a sdcard playlist and scan sdcard music save to it
I (1776) SDCARD_MP3_CONTROL_EXAMPLE: [ 2 ] Start codec chip
E (1776) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [ 3 ] Create and start input key service
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [4.0] Create audio pipeline for playback
I (1816) SDCARD_MP3_CONTROL_EXAMPLE: [4.1] Create i2s stream to write data to codec chip
I (1856) SDCARD_MP3_CONTROL_EXAMPLE: [4.2] Create mp3 decoder to decode mp3 file
I (1856) SDCARD_MP3_CONTROL_EXAMPLE: [4.3] Create resample filter
I (1866) SDCARD_MP3_CONTROL_EXAMPLE: [4.4] Create fatfs stream to read data from sdcard
I (1886) SDCARD_MP3_CONTROL_EXAMPLE: [4.5] Register all elements to audio pipeline
I (1886) SDCARD_MP3_CONTROL_EXAMPLE: [4.6] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->resample-->i2s_stream-->[codec_chip]
I (1896) SDCARD_MP3_CONTROL_EXAMPLE: [5.0] Set up  event listener
I (1896) SDCARD_MP3_CONTROL_EXAMPLE: [5.1] Listen for all pipeline events
W (1906) SDCARD_MP3_CONTROL_EXAMPLE: [ 6 ] Press the keys to control music player:
W (1916) SDCARD_MP3_CONTROL_EXAMPLE:       [Play] to start, pause and resume, [Set] next song.
W (1936) SDCARD_MP3_CONTROL_EXAMPLE:       [Vol-] or [Vol+] to adjust volume.
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 3
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Play] input key event
I (15066) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Starting audio pipeline
I (15206) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 2
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Set] input key event
I (27366) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Stopped, advancing to the next song
W (27376) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (27376) AUDIO_ELEMENT: OUT-[filter] AEL_IO_ABORT
W (27386) MP3_DECODER: output aborted -3
W (27376) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (27396) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/DU1906_蓝牙_测试音频.mp3
I (27526) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=48000, bits=16, ch=2
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 2
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Set] input key event
I (37116) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Stopped, advancing to the next song
W (37126) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (37126) AUDIO_ELEMENT: OUT-[filter] AEL_IO_ABORT
W (37136) MP3_DECODER: output aborted -3
W (37136) AUDIO_ELEMENT: OUT-[file] AEL_IO_ABORT
W (37146) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/test_output.mp3
I (37186) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 6
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Vol+] input key event
I (46266) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Volume set to 79 %
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 6
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] [Vol+] input key event
I (49566) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Volume set to 88 %
I (199216) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 4
I (200776) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] input key id is 4
W (222766) FATFS_STREAM: No more data, ret:0
I (223486) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (223486) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/start_service.mp3
I (223616) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=16000, bits=16, ch=1
W (223626) FATFS_STREAM: No more data, ret:0
I (225556) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (225556) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/dingdong.mp3
I (225616) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=22050, bits=16, ch=1
W (225616) FATFS_STREAM: No more data, ret:0
I (226236) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (226236) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/haode.mp3
I (226466) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=48000, bits=16, ch=1
W (226476) FATFS_STREAM: No more data, ret:0
I (227036) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Finished, advancing to the next song
W (227036) SDCARD_MP3_CONTROL_EXAMPLE: URL: file://sdcard/test.mp3
I (227076) SDCARD_MP3_CONTROL_EXAMPLE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2

Done
```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
