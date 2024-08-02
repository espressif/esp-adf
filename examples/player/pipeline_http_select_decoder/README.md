# Play HTTP Audio Files with Different Decoders

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

In this example, you can choose to play audio files downloaded from HTTP in AAC, AMR, FLAC, MP3, OGG, OPUS, or WAV formats.

The complete pipeline to download from HTTP, decode and playback audio files is as follows:

```c
[Music_file_on_HTTP_server] ---> http_stream ---> music_decoder ---> i2s_stream ---> codec_chip ---> [Speaker]
                                                        ▲
                                                ┌───────┴────────┐
                                                │  AAC_DECODER   │
                                                │  AMR_DECODER   │
                                                │  FLAC_DECODER  │
                                                │  MP3_DECODER   │
                                                │  OGG_DECODER   │
                                                │  OPUS_DECODER  │
                                                │  WAV_DECODER   │
                                                └────────────────┘
```

http_stream downloads the audio file set by the program from the Internet, and then the data is decoded by the preset decoder, transmitted to the codec chip through I2S, and finally played back.


## Environment Setup


#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash


### Default IDF Branch

This example supports IDF release/v5.0 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.


### Configuration

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

Configure the Wi-Fi connection information first. Go to `menuconfig> Example Configuration` and fill in the `Wi-Fi SSID` and `Wi-Fi Password`.

```c
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

### Build and Flash

For `esp32-s3` chip compilation, please use the following command to select the esp32-s3 default compilation options.

````c
cp sdkconfig.defaults.esp32s3 sdkconfig
````

For `esp32` chip compilation, please use the following command to select the esp32 default compilation options.

````c
cp sdkconfig.defaults.esp32 sdkconfig
````

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/index.html) for full steps to configure and build an ESP-IDF project.


## How to Use the Example


### Example Functionality

The default decoder in this example is the AAC decoder:

```c
#define SELECT_AAC_DECODER 1
```

- After the example starts running, it will first try to connect to the Wi-Fi network. If the connection is successful, it will play the audio file downloaded from the HTTP URI preset in `play_http_select_decoder_example.c`. The log is as follows:

```c
entry 0x40080710
I (29) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (29) boot: compile time 15:45:30
I (29) boot: chip revision: 3
I (32) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (39) qio_mode: Enabling default flash chip QIO
I (44) boot.esp32: SPI Speed      : 80MHz
I (49) boot.esp32: SPI Mode       : QIO
I (53) boot.esp32: SPI Flash Size : 4MB
I (58) boot: Enabling RNG early entropy source...
I (63) boot: Partition Table:
I (67) boot: ## Label            Usage          Type ST Offset   Length
I (74) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (82) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (89) boot:  2 factory          factory app      00 00 00010000 0016e360
I (97) boot: End of partition table
I (101) boot_comm: chip revision: 3, min. application chip revision: 0
I (108) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2a4e4 (173284) map
I (168) esp_image: segment 1: paddr=0x0003a50c vaddr=0x3ffb0000 size=0x0335c ( 13148) load
I (173) esp_image: segment 2: paddr=0x0003d870 vaddr=0x40080000 size=0x027a8 ( 10152) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (178) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xabac8 (703176) map
0x400d0020: _stext at ??:?

I (391) esp_image: segment 4: paddr=0x000ebaf0 vaddr=0x400827a8 size=0x14880 ( 84096) load
0x400827a8: _xt_user_exit at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:730

I (433) boot: Loaded app from partition at offset 0x10000
I (433) boot: Disabling RNG early entropy source...
I (433) cpu_start: Pro cpu up.
I (437) cpu_start: Application information:
I (442) cpu_start: Project name:     pipeline_http_select_decoder_ex
I (449) cpu_start: App version:      v2.2-201-g6fa02923
I (455) cpu_start: Compile time:     Sep 30 2021 15:45:28
I (461) cpu_start: ELF file SHA256:  a1c69de73ff06992...
I (467) cpu_start: ESP-IDF:          v4.2.2
I (472) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (482) heap_init: Initializing. RAM available for dynamic allocation:
I (489) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (495) heap_init: At 3FFB7A98 len 00028568 (161 KiB): DRAM
I (501) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (508) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (514) heap_init: At 40097028 len 00008FD8 (35 KiB): IRAM
I (520) cpu_start: Pro cpu start user code
I (537) spi_flash: detected chip: gd
I (538) spi_flash: flash io: qio
W (538) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (549) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (579) HTTP_SELECT_AAC_EXAMPLE: [ 1 ] Start audio codec chip
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.0] Create audio pipeline for playback
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.1] Create http stream to read data
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.2] Create aac decoder to decode aac file
I (1119) HTTP_SELECT_AAC_EXAMPLE: [2.3] Create i2s stream to write data to codec chip
I (1139) HTTP_SELECT_AAC_EXAMPLE: [2.4] Register all elements to audio pipeline
I (1139) HTTP_SELECT_AAC_EXAMPLE: [2.5] Link it together http_stream-->aac_decoder-->i2s_stream-->[codec_chip]
I (1149) HTTP_SELECT_AAC_EXAMPLE: [2.6] Set up  uri (http as http_stream, aac as aac_decoder, and default output is i2s)
I (1159) HTTP_SELECT_AAC_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (1169) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (3049) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (5079) HTTP_SELECT_AAC_EXAMPLE: [ 4 ] Set up  event listener
I (5079) HTTP_SELECT_AAC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (5079) HTTP_SELECT_AAC_EXAMPLE: [4.2] Listening event from peripherals
I (5089) HTTP_SELECT_AAC_EXAMPLE: [ 5 ] Start audio_pipeline
I (6079) HTTP_SELECT_AAC_EXAMPLE: [ * ] Receive music info from aac decoder, sample_rates=44100, bits=16, ch=2
W (191799) HTTP_STREAM: No more data,errno:0, total_bytes:2994446, rlen = 0
W (193299) HTTP_SELECT_AAC_EXAMPLE: [ * ] Stop event received
I (193299) HTTP_SELECT_AAC_EXAMPLE: [ 6 ] Stop audio_pipeline and release all resources
E (193299) AUDIO_ELEMENT: [http] Element already stopped
E (193309) AUDIO_ELEMENT: [aac] Element already stopped
E (193309) AUDIO_ELEMENT: [i2s] Element already stopped
W (193319) AUDIO_PIPELINE: There are no listener registered
W (193329) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193339) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193339) AUDIO_ELEMENT: [aac] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193359) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (193359) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3

```


### Example Log

A complete log is as follows:

```c
entry 0x40080710
I (29) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (29) boot: compile time 15:45:30
I (29) boot: chip revision: 3
I (32) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (39) qio_mode: Enabling default flash chip QIO
I (44) boot.esp32: SPI Speed      : 80MHz
I (49) boot.esp32: SPI Mode       : QIO
I (53) boot.esp32: SPI Flash Size : 4MB
I (58) boot: Enabling RNG early entropy source...
I (63) boot: Partition Table:
I (67) boot: ## Label            Usage          Type ST Offset   Length
I (74) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (82) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (89) boot:  2 factory          factory app      00 00 00010000 0016e360
I (97) boot: End of partition table
I (101) boot_comm: chip revision: 3, min. application chip revision: 0
I (108) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2a4e4 (173284) map
I (168) esp_image: segment 1: paddr=0x0003a50c vaddr=0x3ffb0000 size=0x0335c ( 13148) load
I (173) esp_image: segment 2: paddr=0x0003d870 vaddr=0x40080000 size=0x027a8 ( 10152) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (178) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xabac8 (703176) map
0x400d0020: _stext at ??:?

I (391) esp_image: segment 4: paddr=0x000ebaf0 vaddr=0x400827a8 size=0x14880 ( 84096) load
0x400827a8: _xt_user_exit at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:730

I (433) boot: Loaded app from partition at offset 0x10000
I (433) boot: Disabling RNG early entropy source...
I (433) cpu_start: Pro cpu up.
I (437) cpu_start: Application information:
I (442) cpu_start: Project name:     pipeline_http_select_decoder_ex
I (449) cpu_start: App version:      v2.2-201-g6fa02923
I (455) cpu_start: Compile time:     Sep 30 2021 15:45:28
I (461) cpu_start: ELF file SHA256:  a1c69de73ff06992...
I (467) cpu_start: ESP-IDF:          v4.2.2
I (472) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (482) heap_init: Initializing. RAM available for dynamic allocation:
I (489) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (495) heap_init: At 3FFB7A98 len 00028568 (161 KiB): DRAM
I (501) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (508) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (514) heap_init: At 40097028 len 00008FD8 (35 KiB): IRAM
I (520) cpu_start: Pro cpu start user code
I (537) spi_flash: detected chip: gd
I (538) spi_flash: flash io: qio
W (538) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (549) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (579) HTTP_SELECT_AAC_EXAMPLE: [ 1 ] Start audio codec chip
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.0] Create audio pipeline for playback
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.1] Create http stream to read data
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.2] Create aac decoder to decode aac file
I (1119) HTTP_SELECT_AAC_EXAMPLE: [2.3] Create i2s stream to write data to codec chip
I (1139) HTTP_SELECT_AAC_EXAMPLE: [2.4] Register all elements to audio pipeline
I (1139) HTTP_SELECT_AAC_EXAMPLE: [2.5] Link it together http_stream-->aac_decoder-->i2s_stream-->[codec_chip]
I (1149) HTTP_SELECT_AAC_EXAMPLE: [2.6] Set up  uri (http as http_stream, aac as aac_decoder, and default output is i2s)
I (1159) HTTP_SELECT_AAC_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (1169) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (3049) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (5079) HTTP_SELECT_AAC_EXAMPLE: [ 4 ] Set up  event listener
I (5079) HTTP_SELECT_AAC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (5079) HTTP_SELECT_AAC_EXAMPLE: [4.2] Listening event from peripherals
I (5089) HTTP_SELECT_AAC_EXAMPLE: [ 5 ] Start audio_pipeline
I (6079) HTTP_SELECT_AAC_EXAMPLE: [ * ] Receive music info from aac decoder, sample_rates=44100, bits=16, ch=2
W (191799) HTTP_STREAM: No more data,errno:0, total_bytes:2994446, rlen = 0
W (193299) HTTP_SELECT_AAC_EXAMPLE: [ * ] Stop event received
I (193299) HTTP_SELECT_AAC_EXAMPLE: [ 6 ] Stop audio_pipeline and release all resources
E (193299) AUDIO_ELEMENT: [http] Element already stopped
E (193309) AUDIO_ELEMENT: [aac] Element already stopped
E (193309) AUDIO_ELEMENT: [i2s] Element already stopped
W (193319) AUDIO_PIPELINE: There are no listener registered
W (193329) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193339) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193339) AUDIO_ELEMENT: [aac] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193359) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (193359) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3
```

## Troubleshooting

If your development board cannot play audio files from the network, please check the following configuration:
1. Whether the Wi-Fi configuration of the development board is correct.
2. Whether the development board has been connected to Wi-Fi and obtained the IP address successfully.
3. Whether the HTTP URI of the server can be accessed. 
4. If the HTTP audio stutters, please consider optimizing configuration by yourself with reference to [Wi-Fi Menuconfig](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/wifi.html#psram) and [Lwip Performance Optimization](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/lwip.html#performance-optimization).
5. It is recommended to use the default sdkconfig.default configuration.


## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
