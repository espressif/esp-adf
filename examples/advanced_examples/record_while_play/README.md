# Record While Playing

- [中文版本](./README_CN.md)
- Regular Example: ![alt text](../../../docs/_static/level_regular.png "Regular Example")


## Example Brief

This example uses an ADF API to playback and record audio simultaneously. In the playback pipeline, you can choose to obtain MP3 audio from a microSD card or an HTTP server. In the recording pipeline, you can choose to store the recorded data in the microSD card or directly use it for speech recognition.

The playback pipeline is as follows:

  ```  
  [sdcard] ---> fatfs_mp3_reader ---> mp3_decoder ---> i2s_stream_writer ---> [codec_chip]
                                           ^
                                           | (CONFIG_OUTPUT_STREAM_HTTP)
  [http_server] ---> http_stream_reader ---
  ```

The recording pipeline is as follows:

  ```
  [mic] ---> i2s_stream_reader ---> resample_filter ---> fatfs_stream_writer ---> [sdcard]
                                                              |
                                    (CONFIG_INPUT_STREAM_ASR) |
                                                               ---> raw_reader ---> wakenet_model ---> [asr_relsult]
  ```



## Environment Setup

#### Hardware Required


This example runs on the boards that are marked with a green checkbox in the table below. Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "Incompatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |


## Build and Flash

### Default IDF Branch

The default IDF branch of this example is ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Prepare a microSD card. Go to [Audio Samples/Short Samples](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/audio-samples.html#short-samples) page to download the audio file `ff-16b-2c-44100hz.mp3`, rename it `test.mp3`, and copy it to the microSD card. Of course, you can use your own audio sources, if they are renamed according to the above rules.

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

Configure the Wi-Fi connection information first. Go to `menuconfig> Example Configuration` and fill in the `Wi-Fi SSID` and `Wi-Fi Password`.

```
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

You can select and configure the target of the recorded data, either used for automatic speech recognition (ASR) or directly written into the microSD card.

```
menuconfig > Example Configuration > Input stream selection > (X) record for ASR
                                                            |
                                                            --> ( ) record and save to sdcard
```

You can also select and configure the source of the audio, obtained from either the HTTP server or the microSD card.

```
menuconfig > Example Configuration > Output stream selection > (X) play music from http
                                                            |
                                                            --> ( ) play music from sdacrd
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

- After the example starts running, it connects to the Wi-Fi hotspot as configured by default. After a successful connection, it plays the MP3 audio file obtained from the HTTP server. The log is as follows:

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7556
load:0x40078000,len:13904
ho 0 tail 12 room 4
load:0x40080400,len:5296
0x40080400: _init at ??:?

entry 0x40080710
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 15:40:26
I (29) boot: chip revision: 3
I (33) qio_mode: Enabling default flash chip QIO
I (38) boot.esp32: SPI Speed      : 80MHz
I (43) boot.esp32: SPI Mode       : QIO
I (48) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (58) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00300000
I (91) boot: End of partition table
I (95) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x403ec (263148) map
I (181) esp_image: segment 1: paddr=0x00050414 vaddr=0x3ffb0000 size=0x034c4 ( 13508) load
I (186) esp_image: segment 2: paddr=0x000538e0 vaddr=0x40080000 size=0x0c738 ( 51000) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (206) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0xbf388 (783240) map
0x400d0020: _stext at ??:?

I (437) esp_image: segment 4: paddr=0x0011f3b0 vaddr=0x4008c738 size=0x0d9c8 ( 55752) load
0x4008c738: vTaskPlaceOnEventListRestricted at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/tasks.c:3085

I (471) boot: Loaded app from partition at offset 0x10000
I (471) boot: Disabling RNG early entropy source...
I (471) psram: This chip is ESP32-D0WD
I (476) spiram: Found 64MBit SPI RAM device
I (480) spiram: SPI RAM mode: flash 80m sram 80m
I (486) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (493) cpu_start: Pro cpu up.
I (497) cpu_start: Application information:
I (501) cpu_start: Project name:     record_while_play
I (507) cpu_start: App version:      v2.2-213-g8a9b502f-dirty
I (514) cpu_start: Compile time:     Nov  4 2021 15:40:20
I (520) cpu_start: ELF file SHA256:  3e1907f67991ccba...
I (526) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (532) cpu_start: Starting app cpu, entry point is 0x40081e08
0x40081e08: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1035) spiram: SPI SRAM memory test OK
I (1036) heap_init: Initializing. RAM available for dynamic allocation:
I (1036) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1042) heap_init: At 3FFB47C0 len 0002B840 (174 KiB): DRAM
I (1048) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1054) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1061) heap_init: At 4009A100 len 00005F00 (23 KiB): IRAM
I (1067) cpu_start: Pro cpu start user code
I (1072) spiram: Adding pool of 4082K of external SPI memory to heap allocator
I (1092) spi_flash: detected chip: gd
I (1093) spi_flash: flash io: qio
W (1093) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1104) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1119) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1157) REC_AND_PLAY_EXAMPLE: [ 1 ] Initialize the peripherals
I (1157) REC_AND_PLAY_EXAMPLE: [ 1.1 ] Initialize sd card
I (1659) REC_AND_PLAY_EXAMPLE: [ 1.2 ] Initialize keys
I (1659) REC_AND_PLAY_EXAMPLE: [ 1.3 ] Initialize wifi
W (1678) PERIPH_TOUCH: _touch_init
W (3562) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (5158) REC_AND_PLAY_EXAMPLE: [ 2 ] Start codec chip
E (5159) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (5217) REC_AND_PLAY_EXAMPLE: [ 3 ] Create pipeline for play and record
I (5217) REC_AND_PLAY_EXAMPLE: [ * ] Play [https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3] from HTTP
I (5242) REC_AND_PLAY_EXAMPLE: [ * ] Ready for speech recognization
W (5242) I2S: I2S driver already installed
I (5244) REC_AND_PLAY_EXAMPLE: [ * ] Create asr model
Quantized wakeNet5: wakeNet5_v1_hilexin_5_0.95_0.90, mode:0 (Oct 14 2020 16:26:17)
I (5255) REC_AND_PLAY_EXAMPLE: [ 4 ] Set up  event listener
I (5263) REC_AND_PLAY_EXAMPLE: [4.1] Listening event from pipelines
I (5269) REC_AND_PLAY_EXAMPLE: [4.2] Listening event from peripherals
I (5276) REC_AND_PLAY_EXAMPLE: [ 5 ] Start audio_pipeline
I (8256) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8301) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8326) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8356) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8374) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8406) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8432) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8475) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8507) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8536) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8568) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8583) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8619) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8650) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8677) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8695) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8742) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8769) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8795) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8829) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8851) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8879) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8930) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8945) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8981) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9011) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9027) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9061) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9096) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9126) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9160) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9178) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9257) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9276) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9285) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9315) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9333) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9374) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9389) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9435) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9462) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9493) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9511) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9554) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9570) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9601) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9651) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9664) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9700) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9725) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9755) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9782) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9813) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9851) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9876) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9917) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9932) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9976) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9990) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2

```

- If you configured to use ASR wake-up, say “Hi Lexin”. Once the speech is recognized and the device wakes up, `###spot keyword###` is printed. The log is as follows:

```c
I (10808) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10833) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10867) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10905) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10937) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10961) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10995) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (11014) REC_AND_PLAY_EXAMPLE: ###spot keyword###
I (11015) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11046) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11095) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11099) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11145) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11168) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11201) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2

```


### Example Log
A complete log is as follows:

```c
--- idf_monitor on /dev/ttyUSB0 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7556
load:0x40078000,len:13904
ho 0 tail 12 room 4
load:0x40080400,len:5296
0x40080400: _init at ??:?

entry 0x40080710
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 15:40:26
I (29) boot: chip revision: 3
I (33) qio_mode: Enabling default flash chip QIO
I (38) boot.esp32: SPI Speed      : 80MHz
I (43) boot.esp32: SPI Mode       : QIO
I (48) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (58) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00300000
I (91) boot: End of partition table
I (95) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x403ec (263148) map
I (181) esp_image: segment 1: paddr=0x00050414 vaddr=0x3ffb0000 size=0x034c4 ( 13508) load
I (186) esp_image: segment 2: paddr=0x000538e0 vaddr=0x40080000 size=0x0c738 ( 51000) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (206) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0xbf388 (783240) map
0x400d0020: _stext at ??:?

I (437) esp_image: segment 4: paddr=0x0011f3b0 vaddr=0x4008c738 size=0x0d9c8 ( 55752) load
0x4008c738: vTaskPlaceOnEventListRestricted at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/tasks.c:3085

I (471) boot: Loaded app from partition at offset 0x10000
I (471) boot: Disabling RNG early entropy source...
I (471) psram: This chip is ESP32-D0WD
I (476) spiram: Found 64MBit SPI RAM device
I (480) spiram: SPI RAM mode: flash 80m sram 80m
I (486) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (493) cpu_start: Pro cpu up.
I (497) cpu_start: Application information:
I (501) cpu_start: Project name:     record_while_play
I (507) cpu_start: App version:      v2.2-213-g8a9b502f-dirty
I (514) cpu_start: Compile time:     Nov  4 2021 15:40:20
I (520) cpu_start: ELF file SHA256:  3e1907f67991ccba...
I (526) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (532) cpu_start: Starting app cpu, entry point is 0x40081e08
0x40081e08: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1035) spiram: SPI SRAM memory test OK
I (1036) heap_init: Initializing. RAM available for dynamic allocation:
I (1036) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1042) heap_init: At 3FFB47C0 len 0002B840 (174 KiB): DRAM
I (1048) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1054) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1061) heap_init: At 4009A100 len 00005F00 (23 KiB): IRAM
I (1067) cpu_start: Pro cpu start user code
I (1072) spiram: Adding pool of 4082K of external SPI memory to heap allocator
I (1092) spi_flash: detected chip: gd
I (1093) spi_flash: flash io: qio
W (1093) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1104) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1119) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1157) REC_AND_PLAY_EXAMPLE: [ 1 ] Initialize the peripherals
I (1157) REC_AND_PLAY_EXAMPLE: [ 1.1 ] Initialize sd card
I (1659) REC_AND_PLAY_EXAMPLE: [ 1.2 ] Initialize keys
I (1659) REC_AND_PLAY_EXAMPLE: [ 1.3 ] Initialize wifi
W (1678) PERIPH_TOUCH: _touch_init
W (3562) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (5158) REC_AND_PLAY_EXAMPLE: [ 2 ] Start codec chip
E (5159) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (5217) REC_AND_PLAY_EXAMPLE: [ 3 ] Create pipeline for play and record
I (5217) REC_AND_PLAY_EXAMPLE: [ * ] Play [https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3] from HTTP
I (5242) REC_AND_PLAY_EXAMPLE: [ * ] Ready for speech recognization
W (5242) I2S: I2S driver already installed
I (5244) REC_AND_PLAY_EXAMPLE: [ * ] Create asr model
Quantized wakeNet5: wakeNet5_v1_hilexin_5_0.95_0.90, mode:0 (Oct 14 2020 16:26:17)
I (5255) REC_AND_PLAY_EXAMPLE: [ 4 ] Set up  event listener
I (5263) REC_AND_PLAY_EXAMPLE: [4.1] Listening event from pipelines
I (5269) REC_AND_PLAY_EXAMPLE: [4.2] Listening event from peripherals
I (5276) REC_AND_PLAY_EXAMPLE: [ 5 ] Start audio_pipeline
I (8256) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8301) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8326) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8356) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8374) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8406) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8432) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8475) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8507) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8536) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8568) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8583) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8619) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8650) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8677) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8695) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8742) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8769) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8795) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8829) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8851) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8879) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8930) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8945) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (8981) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9011) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9027) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9061) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9096) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9126) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9160) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9178) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9257) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9276) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9285) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9315) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9333) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9374) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9389) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9435) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9462) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9493) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9511) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9554) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9570) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9601) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9651) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9664) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9700) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9725) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9755) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9782) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9813) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9851) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9876) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9917) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9932) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9976) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (9990) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10030) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10058) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10087) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10119) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10140) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10187) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10201) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10244) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10265) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10289) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10321) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10352) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10382) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10416) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10493) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10505) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10511) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10555) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10564) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10599) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10631) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10655) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10688) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10726) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10751) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10775) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10808) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10833) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10867) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10905) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10937) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10961) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (10995) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (11014) REC_AND_PLAY_EXAMPLE: ###spot keyword###
I (11015) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11046) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11095) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11099) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11145) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11168) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11201) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11226) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11264) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11276) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11323) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11407) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11412) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11429) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11437) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11462) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11506) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11537) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11562) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11583) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11617) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11639) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11685) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11733) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11739) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11763) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11795) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11822) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11858) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11901) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11908) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11950) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (11987) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (12008) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (12050) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (12064) REC_AND_PLAY_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2

```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
