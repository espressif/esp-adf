# Output MP3 music with PWM and I2S-DAC
- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example uses the MP3 decoder callback function `mp3_music_read_cb` to read the MP3 file from the flash. After the data decoding process, the data is output through PWM.

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


**NOTE:**
 - You need an ESP32 board which has suitable GPIOs exposed to output PWM.

## Example Set Up

### Default IDF Branch
This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default development board selected for this example is `ESP32-Lyrat V4.3`.

```c
menuconfig > Example Configuration > Select play mp3 output > Enable PWM output
```

This example selects the two exposed pins `I2C_SCK` and `I2C_SDA` on the `ESP32-Lyrat V4.3` development board to output the PWM waveform.

```c
menuconfig > Example Configuration > PWM Stream Right Output GPIO NUM > 18
```

```c
menuconfig > Example Configuration > PWM Stream Left Output GPIO NUM > 23
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
**1. PWM output method**

Prepare the ESP32 board by connecting GPIO18, GPIO23 and GND pins according to the schematic below. If you use other GPIO, please modify `menuconfig`.

```c
menuconfig > Example Configuration > Select play mp3 output > Enable PWM output
```

PWM output method connections:

```
GPIO18 +--------------+
                      |  __  
                      | /  \ _
                     +-+    | |
                     | |    | |  Earphone
                     +-+    |_|
                      | \__/
             330R     |
GND +-------/\/\/\----+
                      |  __  
                      | /  \ _
                     +-+    | |
                     | |    | |  Earphone
                     +-+    |_|
                      | \__/
                      |
GPIO23 +--------------+
```                      


### Example Logs
A complete log is as follows:

```c
I (66) boot: Chip Revision: 3
I (71) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (42) boot: ESP-IDF v3.3.2-107-g722043f73 2nd stage bootloader
I (42) boot: compile time 17:07:48
I (42) boot: Enabling RNG early entropy source...
I (48) qio_mode: Enabling default flash chip QIO
I (53) boot: SPI Speed      : 80MHz
I (57) boot: SPI Mode       : QIO
I (61) boot: SPI Flash Size : 8MB
I (65) boot: Partition Table:
I (69) boot: ## Label            Usage          Type ST Offset   Length
I (76) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (83) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (91) boot:  2 factory          factory app      00 00 00010000 00100000
I (98) boot: End of partition table
I (103) boot_comm: chip revision: 3, min. application chip revision: 0
I (110) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x22174 (139636) map
I (156) esp_image: segment 1: paddr=0x0003219c vaddr=0x3ffb0000 size=0x01ee4 (  7908) load
I (159) esp_image: segment 2: paddr=0x00034088 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /workshop/audio/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (163) esp_image: segment 3: paddr=0x00034490 vaddr=0x40080400 size=0x08f5c ( 36700) load
I (184) esp_image: segment 4: paddr=0x0003d3f4 vaddr=0x00000000 size=0x02c1c ( 11292) 
I (187) esp_image: segment 5: paddr=0x00040018 vaddr=0x400d0018 size=0x1ec20 (125984) map
0x400d0018: _flash_cache_start at ??:?

I (230) boot: Loaded app from partition at offset 0x10000
I (230) boot: Disabling RNG early entropy source...
I (231) cpu_start: Pro cpu up.
I (234) cpu_start: Application information:
I (239) cpu_start: Project name:     play_mp3_pwm_or_dac
I (245) cpu_start: App version:      v2.2-151-g0db7cb28-dirty
I (252) cpu_start: Compile time:     Jun 23 2021 17:07:46
I (258) cpu_start: ELF file SHA256:  ba34710dd9b2148c...
I (264) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (270) cpu_start: Starting app cpu, entry point is 0x40080fac
0x40080fac: call_start_cpu1 at /workshop/audio/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (280) heap_init: Initializing. RAM available for dynamic allocation:
I (287) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (293) heap_init: At 3FFB2F70 len 0002D090 (180 KiB): DRAM
I (299) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (306) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (312) heap_init: At 4008935C len 00016CA4 (91 KiB): IRAM
I (318) cpu_start: Pro cpu start user code
I (335) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (336) MP3_PWM_DAC_EXAMPLE: [ 1 ] Periph init
I (336) MP3_PWM_DAC_EXAMPLE: [ 2 ] Create audio pipeline for playback
I (346) MP3_PWM_DAC_EXAMPLE: [2.1] Create output stream to write data to codec chip
I (356) MP3_PWM_DAC_EXAMPLE: [2.2] Create wav decoder to decode wav file
I (356) MP3_PWM_DAC_EXAMPLE: [2.3] Register all elements to audio pipeline
I (366) MP3_PWM_DAC_EXAMPLE: [2.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->output_stream-->[codec_chip]
I (376) MP3_PWM_DAC_EXAMPLE: [ 3 ] Set up  event listener
I (386) MP3_PWM_DAC_EXAMPLE: [3.1] Listening event from all elements of pipeline
I (396) MP3_PWM_DAC_EXAMPLE: [3.2] Listening event from peripherals
I (396) MP3_PWM_DAC_EXAMPLE: [ 4 ] Start audio_pipeline
I (406) MP3_PWM_DAC_EXAMPLE: [ 5 ] Listen for all pipeline events
I (416) MP3_PWM_DAC_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (7026) MP3_PWM_DAC_EXAMPLE: [ * ] Stop event received
I (7026) MP3_PWM_DAC_EXAMPLE: [ 6 ] Stop audio_pipeline
E (7026) AUDIO_ELEMENT: [mp3] Element already stopped
E (7026) AUDIO_ELEMENT: [output] Element already stopped
W (7036) AUDIO_PIPELINE: There are no listener registered
W (7036) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7046) AUDIO_ELEMENT: [output] Element has not create when AUDIO_ELEMENT_TERMINATE

```

## Troubleshooting


## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.


