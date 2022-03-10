# Recoding WAV and AMR File to MicroSD Card Simultaneously

- [中文版本](./README.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This routine uses the pipeline API to create an audio data stream with one I2S input and two file outputs. The function is to record 10 seconds of AMR and WAV audio files simultaneously and then save them to a microSD card. Please refer to [element_wav_amr_sdcard](../element_wav_amr_sdcard/README.md) for the example using the element API.

AMR supports two types of audio encoders, AMR-NB and AMR-WB. The AMR-NB encoder is selected by default to record audio before it is saved to the microSD card.

The recording pipeline is as follows:

```
[mic] ---> codec_chip ---> i2s_stream ---> wav_encoder ---> fatfs_stream ---> [sdcard]
                                      |
                                       ---> raw_stream ---> amr_encoder ---> fatfs_stream ---> [sdcard]
                                                                ▲
                                                        ┌───────┴────────┐
                                                        │ AMR-NB_ENCODER │
                                                        │ AMR-WB_EMCODER │
                                                        └────────────────┘
```

## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

In this example, you need to prepare a microSD card and insert the card into the development board in advance.

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

In this example, the default encoding is `AMR-NB`. If you want to change the encoding to `AMR-WB`, select it in `menuconfig`.

```
menuconfig > Example configuration > Audio encoder file type  > amrwb
```

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

For full steps to configure and build an ESP-IDF project, please go to [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) and select the chip and version in the upper left corner of the page.

## How to Use the Example

### Example Functionality

After the example starts running, it prompts the start of recording and prints recording seconds. The log is as follows:

```
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
I (27) boot: compile time 14:44:25
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x41b5c (269148) map
I (214) esp_image: segment 1: paddr=0x00051b84 vaddr=0x3ffb0000 size=0x02218 (  8728) load
I (217) esp_image: segment 2: paddr=0x00053da4 vaddr=0x40080000 size=0x0c274 ( 49780) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (242) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0x38c68 (232552) map
0x400d0020: _stext at ??:?

I (331) esp_image: segment 4: paddr=0x00098c90 vaddr=0x4008c274 size=0x018d4 (  6356) load
0x4008c274: spi_flash_ll_get_buffer_data at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/spi_flash_ll.h:146
 (inlined by) spi_flash_hal_read at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/hal/spi_flash_hal_common.inc:122

I (341) boot: Loaded app from partition at offset 0x10000
I (342) boot: Disabling RNG early entropy source...
I (343) cpu_start: Pro cpu up.
I (346) cpu_start: Application information:
I (351) cpu_start: Project name:     pipeline_wav_amr_sdcard
I (357) cpu_start: App version:      v2.2-242-g571753b2-dirty
I (364) cpu_start: Compile time:     Nov 22 2021 14:44:18
I (370) cpu_start: ELF file SHA256:  22f1f8f86cf0d8bd...
I (376) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (382) cpu_start: Starting app cpu, entry point is 0x4008198c
0x4008198c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (392) heap_init: Initializing. RAM available for dynamic allocation:
I (399) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (405) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (412) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (418) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (424) heap_init: At 4008DB48 len 000124B8 (73 KiB): IRAM
I (431) cpu_start: Pro cpu start user code
I (448) spi_flash: detected chip: gd
I (449) spi_flash: flash io: dio
W (449) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (459) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (475) PIPELINR_REC_WAV_AMR_SDCARD: [1.0] Mount sdcard
I (983) PIPELINR_REC_WAV_AMR_SDCARD: [2.0] Start codec chip
E (983) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1037) PIPELINR_REC_WAV_AMR_SDCARD: [3.0] Create audio pipeline_wav for recording
I (1037) PIPELINR_REC_WAV_AMR_SDCARD: [3.1] Create i2s stream to read audio data from codec chip
I (1045) PIPELINR_REC_WAV_AMR_SDCARD: [3.2] Create wav encoder to encode wav format
I (1052) PIPELINR_REC_WAV_AMR_SDCARD: [3.3] Create fatfs stream to write data to sdcard
I (1060) PIPELINR_REC_WAV_AMR_SDCARD: [3.4] Register all elements to audio pipeline
I (1069) PIPELINR_REC_WAV_AMR_SDCARD: [3.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]
I (1081) PIPELINR_REC_WAV_AMR_SDCARD: [3.6] Set up  uri (file as fatfs_stream, wav as wav encoder)
I (1090) PIPELINR_REC_WAV_AMR_SDCARD: [4.0] Create audio amr_pipeline for recording
I (1099) PIPELINR_REC_WAV_AMR_SDCARD: [4.1] Create raw stream to write data
I (1106) PIPELINR_REC_WAV_AMR_SDCARD: [4.2] Create amr encoder to encode wav format
I (1115) PIPELINR_REC_WAV_AMR_SDCARD: [4.3] Create fatfs stream to write data to sdcard
I (1123) PIPELINR_REC_WAV_AMR_SDCARD: [4.4] Register all elements to audio amr_pipeline
I (1132) PIPELINR_REC_WAV_AMR_SDCARD: [4.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]
I (1145) PIPELINR_REC_WAV_AMR_SDCARD: [4.6] Create ringbuf to link  i2s
I (1151) PIPELINR_REC_WAV_AMR_SDCARD: [4.7] Set up  uri (file as fatfs_stream, wav as wav encoder)
I (1161) PIPELINR_REC_WAV_AMR_SDCARD: [5.0] Set up  event listener
I (1168) PIPELINR_REC_WAV_AMR_SDCARD: [5.1] Listening event from peripherals
I (1176) PIPELINR_REC_WAV_AMR_SDCARD: [6.0] start audio_pipeline
I (1186) PIPELINR_REC_WAV_AMR_SDCARD: [7.0] Listen for all pipeline events, record for 10 seconds
I (2224) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 1
I (3240) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 2
I (4240) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 3
I (5290) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 4
I (6311) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 5
I (7324) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 6
I (8365) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 7
I (9378) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 8
I (10390) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 9
I (11449) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 10
I (11450) PIPELINR_REC_WAV_AMR_SDCARD: Finishing recording
W (11451) AUDIO_ELEMENT: IN-[wav] AEL_IO_ABORT
W (11456) AUDIO_ELEMENT: IN-[amr] AEL_IO_ABORT
E (11462) AUDIO_ELEMENT: [wav] Element already stopped
I (11498) PIPELINR_REC_WAV_AMR_SDCARD: [8.0] Stop audio_pipeline
W (11498) AUDIO_PIPELINE: There are no listener registered
W (11500) AUDIO_PIPELINE: There are no listener registered
W (11506) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11514) AUDIO_ELEMENT: [wav] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11522) AUDIO_ELEMENT: [wav_file] Element has not create when AUDIO_ELEMENT_TERMINATE
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
I (27) boot: compile time 14:44:25
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x41b5c (269148) map
I (214) esp_image: segment 1: paddr=0x00051b84 vaddr=0x3ffb0000 size=0x02218 (  8728) load
I (217) esp_image: segment 2: paddr=0x00053da4 vaddr=0x40080000 size=0x0c274 ( 49780) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (242) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0x38c68 (232552) map
0x400d0020: _stext at ??:?

I (331) esp_image: segment 4: paddr=0x00098c90 vaddr=0x4008c274 size=0x018d4 (  6356) load
0x4008c274: spi_flash_ll_get_buffer_data at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/spi_flash_ll.h:146
 (inlined by) spi_flash_hal_read at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/hal/spi_flash_hal_common.inc:122

I (341) boot: Loaded app from partition at offset 0x10000
I (342) boot: Disabling RNG early entropy source...
I (343) cpu_start: Pro cpu up.
I (346) cpu_start: Application information:
I (351) cpu_start: Project name:     pipeline_wav_amr_sdcard
I (357) cpu_start: App version:      v2.2-242-g571753b2-dirty
I (364) cpu_start: Compile time:     Nov 22 2021 14:44:18
I (370) cpu_start: ELF file SHA256:  22f1f8f86cf0d8bd...
I (376) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (382) cpu_start: Starting app cpu, entry point is 0x4008198c
0x4008198c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (392) heap_init: Initializing. RAM available for dynamic allocation:
I (399) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (405) heap_init: At 3FFB2BB0 len 0002D450 (181 KiB): DRAM
I (412) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (418) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (424) heap_init: At 4008DB48 len 000124B8 (73 KiB): IRAM
I (431) cpu_start: Pro cpu start user code
I (448) spi_flash: detected chip: gd
I (449) spi_flash: flash io: dio
W (449) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (459) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (475) PIPELINR_REC_WAV_AMR_SDCARD: [1.0] Mount sdcard
I (983) PIPELINR_REC_WAV_AMR_SDCARD: [2.0] Start codec chip
E (983) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1037) PIPELINR_REC_WAV_AMR_SDCARD: [3.0] Create audio pipeline_wav for recording
I (1037) PIPELINR_REC_WAV_AMR_SDCARD: [3.1] Create i2s stream to read audio data from codec chip
I (1045) PIPELINR_REC_WAV_AMR_SDCARD: [3.2] Create wav encoder to encode wav format
I (1052) PIPELINR_REC_WAV_AMR_SDCARD: [3.3] Create fatfs stream to write data to sdcard
I (1060) PIPELINR_REC_WAV_AMR_SDCARD: [3.4] Register all elements to audio pipeline
I (1069) PIPELINR_REC_WAV_AMR_SDCARD: [3.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]
I (1081) PIPELINR_REC_WAV_AMR_SDCARD: [3.6] Set up  uri (file as fatfs_stream, wav as wav encoder)
I (1090) PIPELINR_REC_WAV_AMR_SDCARD: [4.0] Create audio amr_pipeline for recording
I (1099) PIPELINR_REC_WAV_AMR_SDCARD: [4.1] Create raw stream to write data
I (1106) PIPELINR_REC_WAV_AMR_SDCARD: [4.2] Create amr encoder to encode wav format
I (1115) PIPELINR_REC_WAV_AMR_SDCARD: [4.3] Create fatfs stream to write data to sdcard
I (1123) PIPELINR_REC_WAV_AMR_SDCARD: [4.4] Register all elements to audio amr_pipeline
I (1132) PIPELINR_REC_WAV_AMR_SDCARD: [4.5] Link it together [codec_chip]-->i2s_stream-->wav_encoder-->fatfs_stream-->[sdcard]
I (1145) PIPELINR_REC_WAV_AMR_SDCARD: [4.6] Create ringbuf to link  i2s
I (1151) PIPELINR_REC_WAV_AMR_SDCARD: [4.7] Set up  uri (file as fatfs_stream, wav as wav encoder)
I (1161) PIPELINR_REC_WAV_AMR_SDCARD: [5.0] Set up  event listener
I (1168) PIPELINR_REC_WAV_AMR_SDCARD: [5.1] Listening event from peripherals
I (1176) PIPELINR_REC_WAV_AMR_SDCARD: [6.0] start audio_pipeline
I (1186) PIPELINR_REC_WAV_AMR_SDCARD: [7.0] Listen for all pipeline events, record for 10 seconds
I (2224) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 1
I (3240) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 2
I (4240) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 3
I (5290) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 4
I (6311) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 5
I (7324) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 6
I (8365) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 7
I (9378) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 8
I (10390) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 9
I (11449) PIPELINR_REC_WAV_AMR_SDCARD: [ * ] Recording ... 10
I (11450) PIPELINR_REC_WAV_AMR_SDCARD: Finishing recording
W (11451) AUDIO_ELEMENT: IN-[wav] AEL_IO_ABORT
W (11456) AUDIO_ELEMENT: IN-[amr] AEL_IO_ABORT
E (11462) AUDIO_ELEMENT: [wav] Element already stopped
I (11498) PIPELINR_REC_WAV_AMR_SDCARD: [8.0] Stop audio_pipeline
W (11498) AUDIO_PIPELINE: There are no listener registered
W (11500) AUDIO_PIPELINE: There are no listener registered
W (11506) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11514) AUDIO_ELEMENT: [wav] Element has not create when AUDIO_ELEMENT_TERMINATE
W (11522) AUDIO_ELEMENT: [wav_file] Element has not create when AUDIO_ELEMENT_TERMINATE
```

## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
