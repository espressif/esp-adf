# Audio Sonic

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how to use the ADF sonic feature. This feature creates two pipelines. One is the recording pipeline, which records sound to the microSD card. The trigger is long pressing the [REC] key. The other is the playback pipeline, which reads the `rec.wav` file from the microSD card and plays it back. The trigger is releasing the long-pressed [REC] key. The sonic feature allows to you adjust audio speed and pitch with the [Mode] key.

You can adjust the audio output effect with the following parameters:

- Audio speed
- Audio pitch

The pipeline to save recordings to the microSD card is as follows:

```
[mic] ---> codec_chip ---> i2s_stream ---> wav_encoder ---> fatfs_stream ---> [sdcard]

```

As shown in the pipeline below, the program first reads the WAV file from the microSD card, processes it with the sonic, and then plays back the processed file.

```
[sdcard] ---> fatfs_stream ---> wav_decoder ---> sonic ---> i2s_stream ---> [codec_chip]

```


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch
This example supports IDF release/v5.0 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.


### Configuration

In this example, you need to prepare an microSD card and insert the card into the development board to save the recording in WAV format.

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

If you need to modify the audio file name, please enable FatFs long file name support.

```
menuconfig > Component config > FAT Filesystem support > Long filename support
```

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

### Example Functionality

- The example starts running and is waiting for you to press and hold the [REC] key. The log is as follows:

```c
ets Jul 29 2019 12:21:46

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
I (27) boot: compile time 15:08:16
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x10584 ( 66948) map
I (137) esp_image: segment 1: paddr=0x000205ac vaddr=0x3ffb0000 size=0x02218 (  8728) load
I (140) esp_image: segment 2: paddr=0x000227cc vaddr=0x40080000 size=0x0d84c ( 55372) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (168) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x2ee44 (192068) map
0x400d0020: _stext at ??:?

I (241) esp_image: segment 4: paddr=0x0005ee6c vaddr=0x4008d84c size=0x0044c (  1100) load
0x4008d84c: spi_flash_common_read_status_16b_rdsr_rdsr2 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/spi_flash/spi_flash_chip_generic.c:427

I (249) boot: Loaded app from partition at offset 0x10000
I (249) boot: Disabling RNG early entropy source...
I (251) cpu_start: Pro cpu up.
I (254) cpu_start: Application information:
I (259) cpu_start: Project name:     sonic_app
I (264) cpu_start: App version:      v2.2-216-gbb375dee-dirty
I (271) cpu_start: Compile time:     Nov  8 2021 15:08:02
I (277) cpu_start: ELF file SHA256:  1878ad41c12b7600...
I (283) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (289) cpu_start: Starting app cpu, entry point is 0x400819b8
0x400819b8: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (299) heap_init: Initializing. RAM available for dynamic allocation:
I (306) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (312) heap_init: At 3FFB2BA0 len 0002D460 (181 KiB): DRAM
I (318) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (325) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (331) heap_init: At 4008DC98 len 00012368 (72 KiB): IRAM
I (337) cpu_start: Pro cpu start user code
I (355) spi_flash: detected chip: gd
I (356) spi_flash: flash io: dio
W (356) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (366) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (878) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (898) PERIPH_TOUCH: _touch_init
I (918) SONIC_EXAMPLE: [1.1] Initialize recorder pipeline
I (918) SONIC_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (958) SONIC_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (958) SONIC_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (958) I2S: I2S driver already installed
I (968) SONIC_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (968) SONIC_EXAMPLE: [ 3 ] Set up  event listener
W (988) SONIC_EXAMPLE: Press [Rec] to start recording

```

Below is the pipeline that reads the WAV file from the microSD card, processes it with the sonic, and plays back the processed file.

```c
E (2894608) SONIC_EXAMPLE: Now recording, release [Rec] to STOP
W (2894608) AUDIO_PIPELINE: Without stop, st:1
W (2894608) AUDIO_PIPELINE: Without wait stop, st:1
W (2894608) AUDIO_ELEMENT: [file_reader] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894628) AUDIO_ELEMENT: [wav_decoder] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894628) AUDIO_ELEMENT: [sonic] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894638) AUDIO_ELEMENT: [i2s_writer] Element has not create when AUDIO_ELEMENT_TERMINATE
I (2894648) SONIC_EXAMPLE: Setup file path to save recorded audio

```

- When you release the [REC] key on the audio board, the example reads the WAV file saved in the microSD card, processes it with the sonic, and then plays back the processed file.

```c
I (2901158) SONIC_EXAMPLE: START Playback
W (2901158) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
E (2901158) AUDIO_ELEMENT: [wav_encoder] Element already stopped
W (2901168) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
I (2901198) SONIC_EXAMPLE: Setup file path to read the wav audio to play
W (2903928) FATFS_STREAM: No more data, ret:0

```

- You can also use the [Mode] key to change audio speed and pitch. The log is as follows:

```c
I (3043078) SONIC_EXAMPLE: The pitch of audio file is changed
I (3044078) SONIC_EXAMPLE: The speed of audio file is changed
I (3047098) SONIC_EXAMPLE: The pitch of audio file is changed
I (3047508) SONIC_EXAMPLE: The speed of audio file is changed
I (3048278) SONIC_EXAMPLE: The pitch of audio file is changed

```



### Example Log
A complete log is as follows:

```c
ets Jul 29 2019 12:21:46

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
I (27) boot: compile time 15:08:16
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x10584 ( 66948) map
I (137) esp_image: segment 1: paddr=0x000205ac vaddr=0x3ffb0000 size=0x02218 (  8728) load
I (140) esp_image: segment 2: paddr=0x000227cc vaddr=0x40080000 size=0x0d84c ( 55372) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (168) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x2ee44 (192068) map
0x400d0020: _stext at ??:?

I (241) esp_image: segment 4: paddr=0x0005ee6c vaddr=0x4008d84c size=0x0044c (  1100) load
0x4008d84c: spi_flash_common_read_status_16b_rdsr_rdsr2 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/spi_flash/spi_flash_chip_generic.c:427

I (249) boot: Loaded app from partition at offset 0x10000
I (249) boot: Disabling RNG early entropy source...
I (251) cpu_start: Pro cpu up.
I (254) cpu_start: Application information:
I (259) cpu_start: Project name:     sonic_app
I (264) cpu_start: App version:      v2.2-216-gbb375dee-dirty
I (271) cpu_start: Compile time:     Nov  8 2021 15:08:02
I (277) cpu_start: ELF file SHA256:  1878ad41c12b7600...
I (283) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (289) cpu_start: Starting app cpu, entry point is 0x400819b8
0x400819b8: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (299) heap_init: Initializing. RAM available for dynamic allocation:
I (306) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (312) heap_init: At 3FFB2BA0 len 0002D460 (181 KiB): DRAM
I (318) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (325) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (331) heap_init: At 4008DC98 len 00012368 (72 KiB): IRAM
I (337) cpu_start: Pro cpu start user code
I (355) spi_flash: detected chip: gd
I (356) spi_flash: flash io: dio
W (356) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (366) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
E (878) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (898) PERIPH_TOUCH: _touch_init
I (918) SONIC_EXAMPLE: [1.1] Initialize recorder pipeline
I (918) SONIC_EXAMPLE: [1.2] Create audio elements for recorder pipeline
I (958) SONIC_EXAMPLE: [1.3] Register audio elements to recorder pipeline
I (958) SONIC_EXAMPLE: [2.2] Create audio elements for playback pipeline
W (958) I2S: I2S driver already installed
I (968) SONIC_EXAMPLE: [2.3] Register audio elements to playback pipeline
I (968) SONIC_EXAMPLE: [ 3 ] Set up  event listener
W (988) SONIC_EXAMPLE: Press [Rec] to start recording
E (2894608) SONIC_EXAMPLE: Now recording, release [Rec] to STOP
W (2894608) AUDIO_PIPELINE: Without stop, st:1
W (2894608) AUDIO_PIPELINE: Without wait stop, st:1
W (2894608) AUDIO_ELEMENT: [file_reader] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894628) AUDIO_ELEMENT: [wav_decoder] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894628) AUDIO_ELEMENT: [sonic] Element has not create when AUDIO_ELEMENT_TERMINATE
W (2894638) AUDIO_ELEMENT: [i2s_writer] Element has not create when AUDIO_ELEMENT_TERMINATE
I (2894648) SONIC_EXAMPLE: Setup file path to save recorded audio
I (2901158) SONIC_EXAMPLE: START Playback
W (2901158) AUDIO_ELEMENT: IN-[wav_encoder] AEL_IO_ABORT
E (2901158) AUDIO_ELEMENT: [wav_encoder] Element already stopped
W (2901168) AUDIO_ELEMENT: IN-[file_writer] AEL_IO_ABORT
I (2901198) SONIC_EXAMPLE: Setup file path to read the wav audio to play
W (2903928) FATFS_STREAM: No more data, ret:0
I (3043078) SONIC_EXAMPLE: The pitch of audio file is changed
I (3044078) SONIC_EXAMPLE: The speed of audio file is changed
I (3047098) SONIC_EXAMPLE: The pitch of audio file is changed
I (3047508) SONIC_EXAMPLE: The speed of audio file is changed
I (3048278) SONIC_EXAMPLE: The pitch of audio file is changed

```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
