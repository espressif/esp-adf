# Record WAV and AMR Files Simultaneously to MicroSD Card Using Callback Function 

- [中文版本](./README.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how to use the element API to set up a flexible audio data stream with one I2S input and two file outputs, realizing the function of recording 10-second AMR and WAV files simultaneously and saving them to the microSD card. In this example, `audio_element_set_input_ringbuf` and `audio_element_set_output_ringbuf` insert the ringbuf into the element to complete the data copy distribution and stream handling, `audio_element_set_event_callback` registers the element events, and `audio_element_run` and `audio_element_resume` control the start of data transfer.

AMR supports two types of audio encoders, AMR-NB and AMR-WB. The AMR-NB encoder is selected by default to record audios before they are saved to the microSD card.

The recording pipeline is as follows:

```
                                                ---> ringbuf01 ---> wav_encoder ---> ringbuf02 ---> fatfs_stream_writer ---> [wav_file]
                                               |
[mic] ---> codec_chip ---> i2s_stream_reader --
                                               |
                                                ---> ringbuf10 ---> amr_encoder ---> ringbuf12 ---> fatfs_stream_writer ---> [amr_file]
                                                                        ▲
                                                                ┌───────┴────────┐
                                                                │  AMRNB_ENCODER │
                                                                │  AMRWB_ENCODER │
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

In this example, the default encoding is `AMRNB`. If you want to change the encoding to `AMRWB`, select it in `menuconfig`.

```
menuconfig > Example configuration > Audio encoder file type  > amrwb
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

After the example starts running, it signals the start of recording and prints recording seconds. The log is as follows:

```
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
I (29) boot: compile time 11:13:10
I (29) boot: chip revision: 3
I (33) qio_mode: Enabling default flash chip QIO
I (38) boot.esp32: SPI Speed      : 80MHz
I (43) boot.esp32: SPI Mode       : QIO
I (48) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (58) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x407bc (264124) map
I (182) esp_image: segment 1: paddr=0x000507e4 vaddr=0x3ffb0000 size=0x02250 (  8784) load
I (185) esp_image: segment 2: paddr=0x00052a3c vaddr=0x40080000 size=0x0d5dc ( 54748) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (208) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0x39018 (233496) map
0x400d0020: _stext at ??:?

I (277) esp_image: segment 4: paddr=0x00099040 vaddr=0x4008d5dc size=0x01724 (  5924) load
0x4008d5dc: spi_flash_ll_program_page at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/spi_flash_ll.h:204
 (inlined by) spi_flash_hal_program_page at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/hal/spi_flash_hal_iram.c:47

I (287) boot: Loaded app from partition at offset 0x10000
I (287) boot: Disabling RNG early entropy source...
I (288) psram: This chip is ESP32-D0WD
I (293) spiram: Found 64MBit SPI RAM device
I (298) spiram: SPI RAM mode: flash 80m sram 80m
I (303) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (310) cpu_start: Pro cpu up.
I (314) cpu_start: Application information:
I (319) cpu_start: Project name:     element_wav_amr_sdcard
I (325) cpu_start: App version:      v2.2-242-g571753b2-dirty
I (331) cpu_start: Compile time:     Nov 22 2021 11:13:08
I (338) cpu_start: ELF file SHA256:  2c3e262decb5a017...
I (343) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (349) cpu_start: Starting app cpu, entry point is 0x40081c34
0x40081c34: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (853) spiram: SPI SRAM memory test OK
I (853) heap_init: Initializing. RAM available for dynamic allocation:
I (853) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (859) heap_init: At 3FFB2C60 len 0002D3A0 (180 KiB): DRAM
I (865) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (872) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (878) heap_init: At 4008ED00 len 00011300 (68 KiB): IRAM
I (884) cpu_start: Pro cpu start user code
I (889) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (909) spi_flash: detected chip: gd
I (909) spi_flash: flash io: qio
W (909) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (920) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (931) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (941) ELEMENT_REC_WAV_AMR_SDCARD: [1.0] Mount sdcard
I (1451) ELEMENT_REC_WAV_AMR_SDCARD: [2.0] Start codec chip
E (1451) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1951) ELEMENT_REC_WAV_AMR_SDCARD: [3.0] Create i2s stream to read audio data from codec chip
I (1951) ELEMENT_REC_WAV_AMR_SDCARD: [3.1] Create wav encoder to encode wav format
I (1951) ELEMENT_REC_WAV_AMR_SDCARD: [3.2] Create fatfs stream to write data to sdcard
I (1961) ELEMENT_REC_WAV_AMR_SDCARD: [3.3] Create a ringbuffer and insert it between i2s_stream_reader and wav_encoder
I (1971) ELEMENT_REC_WAV_AMR_SDCARD: [3.4] Create a ringbuffer and insert it between wav_encoder and wav_fatfs_stream_writer
I (1981) ELEMENT_REC_WAV_AMR_SDCARD: [3.5] Set up uri (file as fatfs_stream, wav as wav encoder)
I (1991) ELEMENT_REC_WAV_AMR_SDCARD: [4.0] Create amr encoder to encode wav format
I (2001) ELEMENT_REC_WAV_AMR_SDCARD: [4.1] Create fatfs stream to write data to sdcard
I (2011) ELEMENT_REC_WAV_AMR_SDCARD: [4.2] Create a ringbuffer and insert it between i2s_stream_reader and wav_encoder
I (2021) ELEMENT_REC_WAV_AMR_SDCARD: [4.3] Create a ringbuffer and insert it between wav_encoder and wav_fatfs_stream_writer
I (2031) ELEMENT_REC_WAV_AMR_SDCARD: [4.4] Set up uri (file as fatfs_stream, wav as wav encoder)
I (2041) ELEMENT_REC_WAV_AMR_SDCARD: [5.0] Set callback function for audio_elements
I (2051) ELEMENT_REC_WAV_AMR_SDCARD: [6.0] Set up event listener
I (2061) ELEMENT_REC_WAV_AMR_SDCARD: [7.0] Listening event from peripherals
I (2071) ELEMENT_REC_WAV_AMR_SDCARD: [8.0] Start audio elements
W (2071) AUDIO_ELEMENT: [iis-0x3f806afc] RESUME timeout
I (2071) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from iis element
I (2081) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from wav element
I (2081) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
I (2081) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from amr element
I (2091) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
I (2111) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
W (2081) AUDIO_ELEMENT: [file-0x3f808040] RESUME timeout
I (2141) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 0
I (2311) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from file element
I (2311) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
I (2481) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from file element
I (2481) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
I (3151) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 1
I (4221) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 2
I (5291) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 3
I (6301) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 4
I (7321) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 5
I (8361) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 6
I (9371) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 7
I (10381) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 8
I (11411) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 9
I (12451) ELEMENT_REC_WAV_AMR_SDCARD: Finishing recording
I (12461) ELEMENT_REC_WAV_AMR_SDCARD: [9.0] Stop elements and release resources
W (12461) AUDIO_ELEMENT: IN-[wav] AEL_IO_ABORT
I (12461) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from wav element
I (12471) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
W (12481) AUDIO_ELEMENT: IN-[amr] AEL_IO_ABORT
I (12481) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 11 from amr element
I (12491) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from amr element
I (12501) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
W (12651) AUDIO_ELEMENT: OUT-[iis] AEL_IO_ABORT
W (12651) AUDIO_ELEMENT: OUT-[iis] AEL_IO_ABORT
W (12651) AUDIO_ELEMENT: OUT-[iis] AEL_IO_ABORT
W (12651) AUDIO_ELEMENT: OUT-[iis] AEL_IO_ABORT
I (12661) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 11 from iis element
I (12671) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from iis element
I (12671) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
I (12681) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from wav element
I (12691) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
E (12691) AUDIO_ELEMENT: [wav] Element already stopped
W (12701) AUDIO_ELEMENT: IN-[file] AEL_IO_ABORT
I (12831) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 9 from file element
I (12831) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from file element
I (12831) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
I (12841) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from amr element
I (12841) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
E (12851) AUDIO_ELEMENT: [amr] Element already stopped
W (12861) AUDIO_ELEMENT: IN-[file] AEL_IO_ABORT
I (12861) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 9 from file element
I (12871) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from file element
I (12881) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED

```


### Example Log

A complete log is as follows:

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
I (29) boot: compile time 11:13:10
I (29) boot: chip revision: 3
I (33) qio_mode: Enabling default flash chip QIO
I (38) boot.esp32: SPI Speed      : 80MHz
I (43) boot.esp32: SPI Mode       : QIO
I (48) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (58) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x407bc (264124) map
I (182) esp_image: segment 1: paddr=0x000507e4 vaddr=0x3ffb0000 size=0x02250 (  8784) load
I (185) esp_image: segment 2: paddr=0x00052a3c vaddr=0x40080000 size=0x0d5dc ( 54748) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (208) esp_image: segment 3: paddr=0x00060020 vaddr=0x400d0020 size=0x39018 (233496) map
0x400d0020: _stext at ??:?

I (277) esp_image: segment 4: paddr=0x00099040 vaddr=0x4008d5dc size=0x01724 (  5924) load
0x4008d5dc: spi_flash_ll_program_page at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/esp32/include/hal/spi_flash_ll.h:204
 (inlined by) spi_flash_hal_program_page at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/soc/src/hal/spi_flash_hal_iram.c:47

I (287) boot: Loaded app from partition at offset 0x10000
I (287) boot: Disabling RNG early entropy source...
I (288) psram: This chip is ESP32-D0WD
I (293) spiram: Found 64MBit SPI RAM device
I (298) spiram: SPI RAM mode: flash 80m sram 80m
I (303) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (310) cpu_start: Pro cpu up.
I (314) cpu_start: Application information:
I (319) cpu_start: Project name:     element_wav_amr_sdcard
I (325) cpu_start: App version:      v2.2-242-g571753b2-dirty
I (331) cpu_start: Compile time:     Nov 22 2021 11:13:08
I (338) cpu_start: ELF file SHA256:  2c3e262decb5a017...
I (343) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (349) cpu_start: Starting app cpu, entry point is 0x40081c34
0x40081c34: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (853) spiram: SPI SRAM memory test OK
I (853) heap_init: Initializing. RAM available for dynamic allocation:
I (853) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (859) heap_init: At 3FFB2C60 len 0002D3A0 (180 KiB): DRAM
I (865) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (872) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (878) heap_init: At 4008ED00 len 00011300 (68 KiB): IRAM
I (884) cpu_start: Pro cpu start user code
I (889) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (909) spi_flash: detected chip: gd
I (909) spi_flash: flash io: qio
W (909) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (920) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (931) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (941) ELEMENT_REC_WAV_AMR_SDCARD: [1.0] Mount sdcard
I (1451) ELEMENT_REC_WAV_AMR_SDCARD: [2.0] Start codec chip
E (1451) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1951) ELEMENT_REC_WAV_AMR_SDCARD: [3.0] Create i2s stream to read audio data from codec chip
I (1951) ELEMENT_REC_WAV_AMR_SDCARD: [3.1] Create wav encoder to encode wav format
I (1951) ELEMENT_REC_WAV_AMR_SDCARD: [3.2] Create fatfs stream to write data to sdcard
I (1961) ELEMENT_REC_WAV_AMR_SDCARD: [3.3] Create a ringbuffer and insert it between i2s_stream_reader and wav_encoder
I (1971) ELEMENT_REC_WAV_AMR_SDCARD: [3.4] Create a ringbuffer and insert it between wav_encoder and wav_fatfs_stream_writer
I (1981) ELEMENT_REC_WAV_AMR_SDCARD: [3.5] Set up uri (file as fatfs_stream, wav as wav encoder)
I (1991) ELEMENT_REC_WAV_AMR_SDCARD: [4.0] Create amr encoder to encode wav format
I (2001) ELEMENT_REC_WAV_AMR_SDCARD: [4.1] Create fatfs stream to write data to sdcard
I (2011) ELEMENT_REC_WAV_AMR_SDCARD: [4.2] Create a ringbuffer and insert it between i2s_stream_reader and wav_encoder
I (2021) ELEMENT_REC_WAV_AMR_SDCARD: [4.3] Create a ringbuffer and insert it between wav_encoder and wav_fatfs_stream_writer
I (2031) ELEMENT_REC_WAV_AMR_SDCARD: [4.4] Set up uri (file as fatfs_stream, wav as wav encoder)
I (2041) ELEMENT_REC_WAV_AMR_SDCARD: [5.0] Set callback function for audio_elements
I (2051) ELEMENT_REC_WAV_AMR_SDCARD: [6.0] Set up event listener
I (2061) ELEMENT_REC_WAV_AMR_SDCARD: [7.0] Listening event from peripherals
I (2071) ELEMENT_REC_WAV_AMR_SDCARD: [8.0] Start audio elements
W (2071) AUDIO_ELEMENT: [iis-0x3f806afc] RESUME timeout
I (2071) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from iis element
I (2081) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from wav element
I (2081) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
I (2081) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from amr element
I (2091) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
I (2111) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
W (2081) AUDIO_ELEMENT: [file-0x3f808040] RESUME timeout
I (2141) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 0
I (2311) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from file element
I (2311) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
I (2481) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from file element
I (2481) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_RUNNING
I (3151) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 1
I (4221) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 2
I (5291) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 3
I (6301) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 4
I (7321) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 5
I (8361) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 6
I (9371) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 7
I (10381) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 8
I (11411) ELEMENT_REC_WAV_AMR_SDCARD: [ * ] Recording ... 9
I (12451) ELEMENT_REC_WAV_AMR_SDCARD: Finishing recording
I (12461) ELEMENT_REC_WAV_AMR_SDCARD: [9.0] Stop elements and release resources
W (12461) AUDIO_ELEMENT: IN-[wav] AEL_IO_ABORT
I (12461) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from wav element
I (12471) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
W (12481) AUDIO_ELEMENT: IN-[amr] AEL_IO_ABORT
I (12481) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 11 from amr element
I (12491) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from amr element
I (12501) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
W (12651) AUDIO_ELEMENT: OUT-[iis] AEL_IO_ABORT
W (12651) AUDIO_ELEMENT: OUT-[iis] AEL_IO_ABORT
W (12651) AUDIO_ELEMENT: OUT-[iis] AEL_IO_ABORT
W (12651) AUDIO_ELEMENT: OUT-[iis] AEL_IO_ABORT
I (12661) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 11 from iis element
I (12671) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from iis element
I (12671) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
I (12681) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from wav element
I (12691) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
E (12691) AUDIO_ELEMENT: [wav] Element already stopped
W (12701) AUDIO_ELEMENT: IN-[file] AEL_IO_ABORT
I (12831) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 9 from file element
I (12831) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from file element
I (12831) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
I (12841) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from amr element
I (12841) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED
E (12851) AUDIO_ELEMENT: [amr] Element already stopped
W (12861) AUDIO_ELEMENT: IN-[file] AEL_IO_ABORT
I (12861) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 9 from file element
I (12871) ELEMENT_REC_WAV_AMR_SDCARD: Audio event 8 from file element
I (12881) ELEMENT_REC_WAV_AMR_SDCARD: AEL_STATUS_STATE_STOPPED

```

## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
