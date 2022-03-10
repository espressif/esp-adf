# Algorithm examples

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example is to play music while performing echo cancellation of the sound recorded by the microphone, and then save it to the microSD card.

This algorithm example has two pipelines. One is to play the MP3 file in the flash, and the other is to record the audio. First, the audio is processed by the algorithm of AEC, AGC, and NS, and then is encoded into the WAV format and saved in the microSD card. At last we compare the original audio with the recorded audio.

- Playing MP3 pipeline:

  ```c
  [flash] ---> mp3_decoder ---> filter ---> i2s_stream ---> [codec_chip]
  ```

- Recording WAV pipeline:

  ```c
  [codec_chip] ---> i2s_stream ---> filter ---> algorithm ---> wav_encoder ---> fatfs_stream ---> [sdcard]
  ```


## Environment Setup

#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Example Set Up

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Prepare the audio board:

- Insert a microSD card required memory of 1 MB into board's SD card slot.

Load and run the example:

- The board will start playing automatically.
- After finish, you can open `/sdcard/aec_out.wav` to hear the recorded file.


### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the Getting Started Guide for full steps to configure and use [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) to build projects.


## How to use the Example

### Example Functionality

After download the follow log should be output:

```
I (10) boot: ESP-IDF v3.3.2-107-g722043f 2nd stage bootloader
I (10) boot: compile time 17:34:10
I (10) boot: Enabling RNG early entropy source...
I (14) boot: SPI Speed      : 80MHz
I (19) boot: SPI Mode       : DIO
I (23) boot: SPI Flash Size : 2MB
I (27) boot: Partition Table:
I (30) boot: ## Label            Usage          Type ST Offset   Length
I (37) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (45) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (52) boot:  2 factory          factory app      00 00 00010000 00100000
I (60) boot: End of partition table
I (64) boot_comm: chip revision: 1, min. application chip revision: 0
I (71) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x44d80 (281984) map
I (162) esp_image: segment 1: paddr=0x00054da8 vaddr=0x3ffb0000 size=0x01f7c (  8060) load
I (165) esp_image: segment 2: paddr=0x00056d2c vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (169) esp_image: segment 3: paddr=0x00057134 vaddr=0x40080400 size=0x08edc ( 36572) load
I (191) esp_image: segment 4: paddr=0x00060018 vaddr=0x400d0018 size=0x3be68 (245352) map
0x400d0018: _flash_cache_start at ??:?

I (263) esp_image: segment 5: paddr=0x0009be88 vaddr=0x400892dc size=0x02bbc ( 11196) load
0x400892dc: xTaskResumeAll at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/freertos/tasks.c:3507

I (274) boot: Loaded app from partition at offset 0x10000
I (274) boot: Disabling RNG early entropy source...
I (275) cpu_start: Pro cpu up.
I (278) cpu_start: Application information:
I (283) cpu_start: Project name:     esp-idf
I (288) cpu_start: App version:      v1.0-667-g3cc7193
I (294) cpu_start: Compile time:     Nov 18 2020 17:34:09
I (300) cpu_start: ELF file SHA256:  919997b2271ad260...
I (306) cpu_start: ESP-IDF:          v3.3.2-107-g722043f
I (312) cpu_start: Starting app cpu, entry point is 0x400811fc
0x400811fc: call_start_cpu1 at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (322) heap_init: Initializing. RAM available for dynamic allocation:
I (329) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (335) heap_init: At 3FFB3140 len 0002CEC0 (179 KiB): DRAM
I (341) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (348) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (354) heap_init: At 4008BE98 len 00014168 (80 KiB): IRAM
I (360) cpu_start: Pro cpu start user code
I (154) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (155) ALGORITHM_EXAMPLES: [1.0] Mount sdcard
I (661) ALGORITHM_EXAMPLES: [2.0] Start codec chip
W (682) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
I (692) ALGORITHM_EXAMPLES: [3.0] Create audio pipeline_rec for recording
I (692) ALGORITHM_EXAMPLES: [3.1] Create i2s stream to read audio data from codec chip
I (699) ALGORITHM_EXAMPLES: [3.2] Create mp3 encoder to encode mp3 format
I (705) ALGORITHM_EXAMPLES: [3.3] Create fatfs stream to write data to sdcard
I (713) ALGORITHM_EXAMPLES: [3.4] Register all elements to audio pipeline
I (720) ALGORITHM_EXAMPLES: [3.5] Link it together [codec_chip]-->i2s_stream-->mp3_encoder-->fatfs_stream-->[sdcard]
I (732) ALGORITHM_EXAMPLES: [3.6] Set up  uri (file as fatfs_stream, mp3 as mp3 encoder)
I (740) ALGORITHM_EXAMPLES: [4.0] Create audio pipeline_rec for recording
I (748) ALGORITHM_EXAMPLES: [4.1] Create fatfs stream to read data from sdcard
I (756) ALGORITHM_EXAMPLES: [4.2] Create mp3 decoder to decode mp3 file
I (763) ALGORITHM_EXAMPLES: [4.3] Create i2s stream to write data to codec chip
I (773) ALGORITHM_EXAMPLES: [4.4] Register all elements to audio pipeline
I (778) ALGORITHM_EXAMPLES: [4.5] Link it together [sdcard]-->fatfs_stream-->mp3_decoder-->i2s_stream-->[codec_chip]
I (790) ALGORITHM_EXAMPLES: [4.6] Set up  uri (file as fatfs_stream, mp3 as mp3 decoder, and default output is i2s)
I (801) ALGORITHM_EXAMPLES: [5.0] Set up event listener
I (807) ALGORITHM_EXAMPLES: [5.1] Listening event from all elements of pipeline
I (815) ALGORITHM_EXAMPLES: [5.2] Listening event from peripherals
I (821) ALGORITHM_EXAMPLES: [6.0] Start audio_pipeline
I (869) ALGORITHM_EXAMPLES: [7.0] Listen for all pipeline events
```


## Troubleshooting

- If the AEC effect is not very good, you can open the `DEBUG_ALGO_INPUT` define to get the original input data (left channel is the signal captured from the microphone, and right channel is the signal played to the speaker), and then check the delay with an audio analysis tool.

- The AEC internal buffering mechanism requires that the recording signal is delayed by around 0 - 10 ms compared to the corresponding reference (playback) signal.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
