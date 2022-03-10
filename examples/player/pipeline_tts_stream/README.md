# Chinese Text-to-Speech (TTS) Service

- [中文版本](./README_CN.md)
- Regular Example: ![alt text](../../../docs/_static/level_regular.png "Regular Example")


## Example Brief

This example demonstrates how to convert Chinese text into Chinese speech. If the input text is a mixture of Chinese and English, the English words will be read letter by letter. If you need the English text-to-speech (TTS) service, you can send an email to our Business Support team for individual customization.


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

You can use `GNU make` or `cmake` for configuration.

If you are using `cmake`, run the command below:
```
idf.py menuconfig
```

Or if you are using `GNU make`, run the command below:
```
make menuconfig
```

### Build and Flash

You can use `GNU make` or `cmake` to build the project.
If you are using `GNU make`:
```bash
cd $ADF_PATH/examples/player/pipeline_tts_stream
make clean
make menuconfig
make -j4 all
```

Or if you are using `cmake`:
```bash
cd $ADF_PATH/examples/player/pipeline_tts_stream
idf.py fullclean
idf.py menuconfig
idf.py build
```
Refer to the following table for the firmware flash address.

| Flash address | Bin Path |
|---|---|
|0x1000 | build/bootloader/bootloader.bin|
|0x8000 | build/partitions.bin|
|0x10000 | build/play_tts_example.bin|
|0x100000 | components/esp-sr/esp-tts/esp_tts_chinese/esp_tts_voice_data_xiaole.dat|


Select compatible audio board in ``menuconfig > Audio HAL``, build the project and flash it to the board, then run monitor tool to view serial output.

- Flash `components/esp-tts/esp_tts_chinese/esp_tts_voice_data_xiaole.dat` to the partition table address.
- Then flash the app bin, the board will start playing automatically.

To exit the serial monitor, type ``Ctrl-]``.


## Example Log

A complete log is as follows:

```
I (10) boot: ESP-IDF v3.3.2-107-g722043f73-dirty 2nd stage bootloader
I (10) boot: compile time 15:04:38
I (11) boot: Enabling RNG early entropy source...
I (16) qio_mode: Enabling default flash chip QIO
I (21) boot: SPI Speed      : 80MHz
I (25) boot: SPI Mode       : QIO
I (29) boot: SPI Flash Size : 8MB
I (33) boot: Partition Table:
I (37) boot: ## Label            Usage          Type ST Offset   Length
I (44) boot:  0 factory          factory app      00 00 00010000 000e0000
I (52) boot:  1 voice_data       Unknown data     01 81 00100000 00300000
I (59) boot: End of partition table
I (63) boot_comm: chip revision: 1, min. application chip revision: 0
I (70) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x16a74 ( 92788) map
I (104) esp_image: segment 1: paddr=0x00026a9c vaddr=0x3ffb0000 size=0x03aac ( 15020) load
I (109) esp_image: segment 2: paddr=0x0002a550 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /repo/workspace/experiments/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (111) esp_image: segment 3: paddr=0x0002a958 vaddr=0x40080400 size=0x056b8 ( 22200) load
I (127) esp_image: segment 4: paddr=0x00030018 vaddr=0x400d0018 size=0x27238 (160312) map
0x400d0018: _flash_cache_start at ??:?

I (171) esp_image: segment 5: paddr=0x00057258 vaddr=0x40085ab8 size=0x049b8 ( 18872) load
0x40085ab8: rtc_init at /repo/workspace/experiments/esp-adf-internal/esp-idf/components/soc/esp32/rtc_init.c:40

I (184) boot: Loaded app from partition at offset 0x10000
I (184) boot: Disabling RNG early entropy source...
I (185) cpu_start: Pro cpu up.
I (188) cpu_start: Application information:
I (193) cpu_start: Project name:     play_tts_example
I (199) cpu_start: App version:      v2.1-93-g9fab9a49-dirty
I (205) cpu_start: Compile time:     Nov  6 2020 15:04:37
I (211) cpu_start: ELF file SHA256:  29860e7987740ea4...
I (217) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73-dirty
I (224) cpu_start: Starting app cpu, entry point is 0x40081044
0x40081044: call_start_cpu1 at /repo/workspace/experiments/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (234) heap_init: Initializing. RAM available for dynamic allocation:
I (241) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (247) heap_init: At 3FFB50C0 len 0002AF40 (171 KiB): DRAM
I (253) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (260) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (266) heap_init: At 4008A470 len 00015B90 (86 KiB): IRAM
I (272) cpu_start: Pro cpu start user code
I (66) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (66) PLAY_TTS_EXAMPLE: [1.0] Init Peripheral Set
I (66) PLAY_TTS_EXAMPLE: [2.0] Start codec chip
W (96) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
I (106) PLAY_TTS_EXAMPLE: [3.0] Create audio pipeline for playback
I (106) PLAY_TTS_EXAMPLE: [3.1] Create tts stream to read data from chinese strings
inti voice set:template
ESP Chinese TTS v1.0 (Jul  1 2020 16:09:41)
I (116) PLAY_TTS_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (126) PLAY_TTS_EXAMPLE: [3.4] Register all elements to audio pipeline
I (126) PLAY_TTS_EXAMPLE: [3.5] Link it together [strings]-->tts_stream-->i2s_stream-->[codec_chip]
I (136) PLAY_TTS_EXAMPLE: [3.6] Set up  uri (tts as tts_stream, and directly output is i2s)
I (146) PLAY_TTS_EXAMPLE: [3.7] 欢迎使用乐鑫语音开源框架
I (156) PLAY_TTS_EXAMPLE: [4.0] Set up  event listener
I (156) PLAY_TTS_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (166) PLAY_TTS_EXAMPLE: [4.2] Listening event from peripherals
I (176) PLAY_TTS_EXAMPLE: [5.0] Start audio_pipeline
I (286) PLAY_TTS_EXAMPLE: [6.0] Listen for all pipeline events
W (2986) TTS_STREAM: No more data,ret:0
W (3496) PLAY_TTS_EXAMPLE: [ * ] Stop event received
I (3496) PLAY_TTS_EXAMPLE: [7.0] Stop audio_pipeline
W (3496) AUDIO_PIPELINE: There are no listener registered
W (3506) AUDIO_ELEMENT: [tts] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3516) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE

```


## Troubleshooting

- If the TTS example does not play the speech, please check if the ``esp_tts_voice_data_xiaole.dat`` file has been flashed to the specified address.


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
