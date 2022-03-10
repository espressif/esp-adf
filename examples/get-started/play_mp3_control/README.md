
# Play mp3 file with playback control

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example uses MP3 element and I2S element to play embedded MP3 music. The mp3 element call back function `read_cb` read the music data form flash, decode the data then use I2S element output the music.

This example also shows how to control MP3 playback by buttons.

- Playback: start, stop, pause, resume.
- Volume: volume up, volume down.
- Other button: MODE/FUNC button, which is used to switch between audios played at different sample rates.

The three audio files at the sample rate of 8000 Hz, 22050 Hz, and 44100 Hz are embedded in flash.


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Example Set Up

### Default IDF Branch
This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration
The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, you need to select the configuration of the development board in menuconfig, for example, select `ESP32-Lyrat-Mini V1.1`.

```c
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
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

- After the routine starts to run, it will automatically play the low sample rate (8000 Hz) mp3 file.

```c
I (260) PLAY_FLASH_MP3_CONTROL: [3.1] Initialize keys on board
I (260) PLAY_FLASH_MP3_CONTROL: [ 4 ] Set up  event listener
W (280) PERIPH_TOUCH: _touch_init
I (280) PLAY_FLASH_MP3_CONTROL: [4.1] Listening event from all elements of pipeline
I (290) PLAY_FLASH_MP3_CONTROL: [4.2] Listening event from peripherals
W (290) PLAY_FLASH_MP3_CONTROL: [ 5 ] Tap touch buttons to control music player:
W (300) PLAY_FLASH_MP3_CONTROL:       [Play] to start, pause and resume, [Set] to stop.
W (310) PLAY_FLASH_MP3_CONTROL:       [Vol-] or [Vol+] to adjust volume.
I (330) PLAY_FLASH_MP3_CONTROL: [ 5.1 ] Start audio_pipeline
I (340) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
```
- During playback, you can use the [Play] button to pause playback and resume playback.

```c
I (330) PLAY_FLASH_MP3_CONTROL: [ 5.1 ] Start audio_pipeline
I (340) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (7330) PLAY_FLASH_MP3_CONTROL: [ * ] [Play] touch tap event
I (7330) PLAY_FLASH_MP3_CONTROL: [ * ] Pausing audio pipeline
I (8830) PLAY_FLASH_MP3_CONTROL: [ * ] [Play] touch tap event
I (8830) PLAY_FLASH_MP3_CONTROL: [ * ] Resuming audio pipeline
I (8850) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
```

- Use volume down key [Vol-] to decrease the volume, and volume up key [Vol+] to increase the volume.

```c
I (81590) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
I (106330) PLAY_FLASH_MP3_CONTROL: [ * ] [Vol+] touch tap event
I (106340) PLAY_FLASH_MP3_CONTROL: [ * ] Volume set to 79 %
```

- Use [Mode] key to switch MP3 playback of different audio file (8000 Hz, 22050 Hz, 44100 Hz).

```c
I (330) PLAY_FLASH_MP3_CONTROL: [ 5.1 ] Start audio_pipeline
I (340) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (16230) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (16230) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (16230) MP3_DECODER: output aborted -3
I (16420) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
I (19130) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (19130) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (19130) MP3_DECODER: output aborted -3
I (19190) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (21440) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (21440) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (21440) MP3_DECODER: output aborted -3
I (21480) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
```

- Use the [Set] key to exit the routine.

```c
I (21480) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (63580) PLAY_FLASH_MP3_CONTROL: [ * ] [Set] touch tap event
I (63580) PLAY_FLASH_MP3_CONTROL: [ * ] Stopping audio pipeline
I (63580) PLAY_FLASH_MP3_CONTROL: [ 6 ] Stop audio_pipeline
E (63580) AUDIO_ELEMENT: [mp3] Element already stopped
E (63600) AUDIO_ELEMENT: [i2s] Element already stopped
W (63600) AUDIO_PIPELINE: There are no listener registered
W (63600) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (63610) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
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
I (40) boot: compile time 15:30:10
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
I (103) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x8a0d0 (565456) map
I (310) esp_image: segment 1: paddr=0x0009a0f8 vaddr=0x3ffb0000 size=0x01f40 (  8000) load
I (314) esp_image: segment 2: paddr=0x0009c040 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (317) esp_image: segment 3: paddr=0x0009c448 vaddr=0x40080400 size=0x03bc8 ( 15304) load
I (332) esp_image: segment 4: paddr=0x000a0018 vaddr=0x400d0018 size=0x26a64 (158308) map
0x400d0018: _flash_cache_start at ??:?

I (390) esp_image: segment 5: paddr=0x000c6a84 vaddr=0x40083fc8 size=0x06994 ( 27028) load
0x40083fc8: i2c_master_cmd_begin_static at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/driver/i2c.c:1134

I (408) boot: Loaded app from partition at offset 0x10000
I (408) boot: Disabling RNG early entropy source...
I (409) cpu_start: Pro cpu up.
I (412) cpu_start: Application information:
I (417) cpu_start: Project name:     play_mp3_control
I (423) cpu_start: App version:      v2.2-85-gf9fa5c94-dirty
I (429) cpu_start: Compile time:     Apr 27 2021 17:11:15
I (435) cpu_start: ELF file SHA256:  87bfd7bdac8a9d70...
I (441) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (447) cpu_start: Starting app cpu, entry point is 0x40081078
0x40081078: call_start_cpu1 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (458) heap_init: Initializing. RAM available for dynamic allocation:
I (465) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (471) heap_init: At 3FFB2FE0 len 0002D020 (180 KiB): DRAM
I (477) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (483) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (490) heap_init: At 4008A95C len 000156A4 (85 KiB): IRAM
I (496) cpu_start: Pro cpu start user code
I (179) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (180) PLAY_FLASH_MP3_CONTROL: [ 1 ] Start audio codec chip
I (200) PLAY_FLASH_MP3_CONTROL: [ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event
I (200) PLAY_FLASH_MP3_CONTROL: [2.1] Create mp3 decoder to decode mp3 file and set custom read callback
I (220) PLAY_FLASH_MP3_CONTROL: [2.2] Create i2s stream to write data to codec chip
I (230) PLAY_FLASH_MP3_CONTROL: [2.3] Register all elements to audio pipeline
I (230) PLAY_FLASH_MP3_CONTROL: [2.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]
I (240) PLAY_FLASH_MP3_CONTROL: [ 3 ] Initialize peripherals
E (250) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (260) PLAY_FLASH_MP3_CONTROL: [3.1] Initialize keys on board
I (260) PLAY_FLASH_MP3_CONTROL: [ 4 ] Set up  event listener
W (280) PERIPH_TOUCH: _touch_init
I (280) PLAY_FLASH_MP3_CONTROL: [4.1] Listening event from all elements of pipeline
I (290) PLAY_FLASH_MP3_CONTROL: [4.2] Listening event from peripherals
W (290) PLAY_FLASH_MP3_CONTROL: [ 5 ] Tap touch buttons to control music player:
W (300) PLAY_FLASH_MP3_CONTROL:       [Play] to start, pause and resume, [Set] to stop.
W (310) PLAY_FLASH_MP3_CONTROL:       [Vol-] or [Vol+] to adjust volume.
I (330) PLAY_FLASH_MP3_CONTROL: [ 5.1 ] Start audio_pipeline
I (340) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (7330) PLAY_FLASH_MP3_CONTROL: [ * ] [Play] touch tap event
I (7330) PLAY_FLASH_MP3_CONTROL: [ * ] Pausing audio pipeline
I (8830) PLAY_FLASH_MP3_CONTROL: [ * ] [Play] touch tap event
I (8830) PLAY_FLASH_MP3_CONTROL: [ * ] Resuming audio pipeline
I (8850) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (86830) PLAY_FLASH_MP3_CONTROL: [ * ] [Vol+] touch tap event
I (86830) PLAY_FLASH_MP3_CONTROL: [ * ] Volume set to 79 %
I (90590) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
E (90590) AUDIO_ELEMENT: [mp3] Element already stopped
E (90590) AUDIO_ELEMENT: [i2s] Element already stopped
I (90610) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
I (92470) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (92470) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (92470) MP3_DECODER: output aborted -3
I (92540) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (94940) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (94940) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (94940) MP3_DECODER: output aborted -3
W (94940) AUDIO_ELEMENT: IN-[i2s] AEL_IO_ABORT
I (95000) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=8000, bits=16, ch=2
I (96670) PLAY_FLASH_MP3_CONTROL: [ * ] [mode] tap event
W (96670) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (96670) MP3_DECODER: output aborted -3
I (96810) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=22050, bits=16, ch=2
I (101680) PLAY_FLASH_MP3_CONTROL: [ * ] [Set] touch tap event
I (101680) PLAY_FLASH_MP3_CONTROL: [ * ] Stopping audio pipeline
I (101680) PLAY_FLASH_MP3_CONTROL: [ 6 ] Stop audio_pipeline
W (101690) AUDIO_ELEMENT: OUT-[mp3] AEL_IO_ABORT
W (101690) MP3_DECODER: output aborted -3
W (101760) AUDIO_PIPELINE: There are no listener registered
W (101760) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (101760) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
```

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
