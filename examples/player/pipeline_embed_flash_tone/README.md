# Play MP3 Files Embedded in Flash as Binary Data

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how the ESP-ADF uses the audio pipeline API to play MP3 files embedded in flash as binary data, which can generally be used as system prompt tone files in a project.

The pipeline for the development board to acquire the MP3 file embedded in flash as binary data and decode it for playback is as follows:

```
[flash] ---> embed_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
```

### Prerequisites

Required source file `audio_embed_tone.h` and `embed_tone.cmake` for the audio file address in flash has already been generated in the `${PROJECT_DIR}/main/` directory. `CMakeLists.txt` and `conponents.mk` are also updated.

If you need to generate your own embedding binary file, you need to put the user prompt file in the `$PROJECT_DIR/main` folder and run the `mk_embed_audio_bin.py` script with the following command. For more information regarding embedding binary, please refer to [Embedding Binary Data](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#cmake-embed-data).

The Python script command is as follows:

```
python $ADF_PATH/tools/audio_tone/mk_embed_audio_tone.py -p $(ADF_PATH)/examples/player/pipeline_embed_flash_tone/main
```


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in `menuconfig` as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch

This example supports IDF release/v5.0 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default board for this example is `ESP32-LyraT V1.1`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-LyraT-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace PORT with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/index.html) for full steps to configure and build an ESP-IDF project.


## How to Use the Example

### Example Functionality

- After the example starts running, the MP3 audio embedded in flash as binary data will be obtained for playback. The log is as follows:

```c

entry 0x40080694
I (27) boot: ESP-IDF v4.4-329-g3c9657fa4e-dirty 2nd stage bootloader
I (27) boot: compile time 21:51:10
I (27) boot: chip revision: 3
I (32) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (39) boot.esp32: SPI Speed      : 40MHz
I (44) boot.esp32: SPI Mode       : DIO
I (48) boot.esp32: SPI Flash Size : 2MB
I (53) boot: Enabling RNG early entropy source...
I (58) boot: Partition Table:
I (62) boot: ## Label            Usage          Type ST Offset   Length
I (69) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (84) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (103) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=1ee34h (126516) map
I (157) esp_image: segment 1: paddr=0002ee5c vaddr=3ffb0000 size=011bch (  4540) load
I (159) esp_image: segment 2: paddr=00030020 vaddr=400d0020 size=28008h (163848) map
I (222) esp_image: segment 3: paddr=00058030 vaddr=3ffb11bc size=01244h (  4676) load
I (224) esp_image: segment 4: paddr=0005927c vaddr=40080000 size=0de34h ( 56884) load
I (251) esp_image: segment 5: paddr=000670b8 vaddr=50000000 size=00010h (    16) load
I (258) boot: Loaded app from partition at offset 0x10000
I (258) boot: Disabling RNG early entropy source...
I (271) cpu_start: Pro cpu up.
I (272) cpu_start: Starting app cpu, entry point is 0x400811f4
0x400811f4: call_start_cpu1 at /home/xutao/workspace/esp-adf-internal/esp-idf/components/esp_system/port/cpu_start.c:160

I (0) cpu_start: App cpu up.
I (286) cpu_start: Pro cpu start user code
I (286) cpu_start: cpu freq: 160000000
I (286) cpu_start: Application information:
I (290) cpu_start: Project name:     play_embed_tone
I (296) cpu_start: App version:      v2.4-1-g22217d90-dirty
I (302) cpu_start: Compile time:     May 26 2022 21:51:03
I (308) cpu_start: ELF file SHA256:  d98530bff404d3d7...
I (314) cpu_start: ESP-IDF:          v4.4-329-g3c9657fa4e-dirty
I (321) heap_init: Initializing. RAM available for dynamic allocation:
I (328) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (334) heap_init: At 3FFB2E50 len 0002D1B0 (180 KiB): DRAM
I (340) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (347) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (353) heap_init: At 4008DE34 len 000121CC (72 KiB): IRAM
I (361) spi_flash: detected chip: gd
I (364) spi_flash: flash io: dio
W (368) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (381) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (391) PLAY_EMBED_FLASH_MP3: [ 1 ] Start audio codec chip
I (421) PLAY_EMBED_FLASH_MP3: [ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event
E (421) gpio: gpio_install_isr_service(449): GPIO isr service already installed
I (431) PLAY_EMBED_FLASH_MP3: [2.1] Create mp3 decoder to decode mp3 file and set custom read callback
I (441) PLAY_EMBED_FLASH_MP3: [2.2] Create i2s stream to write data to codec chip
I (451) PLAY_EMBED_FLASH_MP3: [2.3] Register all elements to audio pipeline
I (461) PLAY_EMBED_FLASH_MP3: [2.4] Link it together [embed flash]-->mp3_decoder-->i2s_stream-->[codec_chip]
I (471) PLAY_EMBED_FLASH_MP3: [ 3 ] Set up  event listener
I (471) PLAY_EMBED_FLASH_MP3: [3.1] Listening event from all elements of pipeline
I (481) PLAY_EMBED_FLASH_MP3: [3.2] Listening event from peripherals
I (491) PLAY_EMBED_FLASH_MP3: [ 4 ] Start audio_pipeline
I (521) PLAY_EMBED_FLASH_MP3: [ * ] Receive music info from mp3 decoder, sample_rates=16000, bits=16, ch=1
W (2481) EMBED_FLASH_STREAM: No more data,ret:0 ,info.byte_pos:22284
W (3361) PLAY_EMBED_FLASH_MP3: [ * ] Stop event received
I (3361) PLAY_EMBED_FLASH_MP3: [ 5 ] Stop audio_pipeline
E (3361) AUDIO_ELEMENT: [flash] Element already stopped
E (3361) AUDIO_ELEMENT: [mp3] Element already stopped
E (3371) AUDIO_ELEMENT: [i2s] Element already stopped
W (3371) AUDIO_PIPELINE: There are no listener registered
W (3381) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3391) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3401) AUDIO_ELEMENT: [flash] Element has not create when AUDIO_ELEMENT_TERMINATE

```


## Technical Support and Feedback
Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
