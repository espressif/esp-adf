# Flexible Pipeline

- [中文版本](./README_CN.md)
- Regular Example: ![alt text](../../../docs/_static/level_regular.png "Regular Example")


## Example Brief

This example shows how to use the ADF pipeline to play different audio dynamically, involving pipeline operations such as link, breakup, and relink.

1. When playing or switching to play audio in AAC format, first pause the previous MP3 playback pipeline, break it up, and relink it to form a new AAC playback pipeline. The combined AAC audio pipeline is as follows:

   ```
   [sdcard] ---> file_aac_reader ---> aac_decoder ---> i2s_stream_writer ---> [codec_chip]
   ```

2. When playing or switching to play audio in MP3 format, first pause the previous AAC playback pipeline, break it up, and relink it to form a new MP3 playback pipeline. The combined MP3 audio pipeline is as follows:

   ```
   [sdcard] ---> file_mp3_reader ---> mp3_decoder ---> i2s_stream_writer ---> [codec_chip]
   ```

## Environment Setup

#### Hardware Required


This example runs on the boards that are marked with a green checkbox in the table below. Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Incompatible") |


## Build and Flash

### Default IDF Branch

The default IDF branch of this example is ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Prepare a microSD card. Go to [Audio Samples/Short Samples](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/audio-samples.html#short-samples) page to download the audio files `ff-16b-2c-44100hz.aac` and `ff-16b-2c-44100hz.mp3`, rename them `test.aac` and `test.mp3`, and copy them to the microSD card. Of course, you can use your own audio sources, if they are renamed according to the above rules.

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
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

- After the example starts running, it will first playback the `test.aac` file from the microSD card. The log is as follows:

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 17:38:17
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 80MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 4MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (68) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00300000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1de10 (122384) map
I (135) esp_image: segment 1: paddr=0x0002de38 vaddr=0x3ffb0000 size=0x021c4 (  8644) load
I (138) esp_image: segment 2: paddr=0x00030004 vaddr=0x40080000 size=0x00014 (    20) load
0x40080000: _WindowOverflow4 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x51f68 (335720) map
0x400d0020: _stext at ??:?

I (258) esp_image: segment 4: paddr=0x00081f90 vaddr=0x40080014 size=0x0df14 ( 57108) load
0x40080014: _WindowOverflow4 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1734

I (288) boot: Loaded app from partition at offset 0x10000
I (288) boot: Disabling RNG early entropy source...
I (288) psram: This chip is ESP32-D0WD
I (293) spiram: Found 64MBit SPI RAM device
I (297) spiram: SPI RAM mode: flash 80m sram 80m
I (303) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (310) cpu_start: Pro cpu up.
I (314) cpu_start: Application information:
I (319) cpu_start: Project name:     flexible_pipeline
I (324) cpu_start: App version:      v2.2-210-g86396c1f-dirty
I (331) cpu_start: Compile time:     Nov  2 2021 17:38:18
I (337) cpu_start: ELF file SHA256:  9018ad4a70009e78...
I (343) cpu_start: ESP-IDF:          v4.2.2
I (348) cpu_start: Starting app cpu, entry point is 0x40081d74
0x40081d74: call_start_cpu1 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (846) spiram: SPI SRAM memory test OK
I (846) heap_init: Initializing. RAM available for dynamic allocation:
I (846) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (852) heap_init: At 3FFB2C18 len 0002D3E8 (180 KiB): DRAM
I (858) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (865) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (871) heap_init: At 4008DF28 len 000120D8 (72 KiB): IRAM
I (877) cpu_start: Pro cpu start user code
I (882) spiram: Adding pool of 4092K of external SPI memory to heap allocator
I (903) spi_flash: detected chip: gd
I (903) spi_flash: flash io: dio
W (903) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (913) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (928) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (956) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1001) SDCARD: CID name SD16G!

I (1456) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (1456) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1461) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1472) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1492) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1492) ES8388_DRIVER: init,out:02, in:00
W (1497) PERIPH_TOUCH: _touch_init
I (1512) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (1522) FLEXIBLE_PIPELINE: [ 1 ] Create all audio elements for playback pipeline
I (1522) MP3_DECODER: MP3 init
I (1523) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1530) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1554) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1557) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1562) FLEXIBLE_PIPELINE: [ 2 ] Register all audio elements to playback pipeline
I (1571) FLEXIBLE_PIPELINE: [ 3 ] Set up  event listener
I (1578) FLEXIBLE_PIPELINE: [3.1] Set up  i2s clock
I (1599) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (1602) FLEXIBLE_PIPELINE: [ 4 ] Start playback pipeline
I (1609) AUDIO_PIPELINE: link el->rb, el:0x3f807c50, tag:file_aac_reader, rb:0x3f808604
I (1617) AUDIO_PIPELINE: link el->rb, el:0x3f808098, tag:aac_decoder, rb:0x3f80a644
I (1626) AUDIO_PIPELINE: link el->rb, el:0x3f808200, tag:filter_upsample, rb:0x3f80ae84
I (1635) AUDIO_ELEMENT: [file_aac_reader-0x3f807c50] Element task created
I (1642) AUDIO_ELEMENT: [aac_decoder-0x3f808098] Element task created
I (1649) AUDIO_ELEMENT: [filter_upsample-0x3f808200] Element task created
I (1656) AUDIO_ELEMENT: [i2s_writer-0x3f80838c] Element task created
I (1663) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4374264 Bytes, Inter:330132 Bytes, Dram:256224 Bytes

I (1675) AUDIO_ELEMENT: [file_aac_reader] AEL_MSG_CMD_RESUME,state:1
I (1684) FATFS_STREAM: File size: 2994446 byte, file position: 0
I (1689) AUDIO_ELEMENT: [aac_decoder] AEL_MSG_CMD_RESUME,state:1
I (1696) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_RESUME,state:1
I (1766) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 2
I (1771) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (1777) I2S_STREAM: AUDIO_STREAM_WRITER
I (1782) AUDIO_PIPELINE: Pipeline started
I (1782) CODEC_ELEMENT_HELPER: The element is 0x3f808098. The reserve data 2 is 0x0.
I (1794) AAC_DECODER: a new song playing
I (1799) AAC_DECODER: this audio is RAW AAC

```

- At this time, press the [Mode] button, then the current AAC audio is paused, the current pipeline is broken up to form a new pipeline, and the audio file named `test.mp3` starts to play. The log is as follows:

```c
I (41285) AUDIO_ELEMENT: [file_aac_reader] AEL_MSG_CMD_PAUSE
I (41302) AAC_DECODER: Closed by pause
I (41302) AUDIO_ELEMENT: [aac_decoder] AEL_MSG_CMD_PAUSE
I (41308) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_PAUSE
I (41339) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_PAUSE
E (41339) FLEXIBLE_PIPELINE: Changing music to mp3 format
W (41340) AUDIO_PIPELINE: There are no listener registered
I (41347) AUDIO_PIPELINE: create new rb,rb:0x3f80c6dc
I (41352) AUDIO_PIPELINE: create new rb,rb:0x3f80e71c
I (41358) AUDIO_ELEMENT: [file_mp3_reader-0x3f807d88] Element task created
I (41365) AUDIO_ELEMENT: [mp3_decoder-0x3f807efc] Element task created
I (41373) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4352144 Bytes, Inter:320528 Bytes, Dram:246620 Bytes

I (41385) AUDIO_ELEMENT: [file_mp3_reader] AEL_MSG_CMD_RESUME,state:1
I (41392) AUDIO_ELEMENT: [mp3_decoder] AEL_MSG_CMD_RESUME,state:1
I (41398) MP3_DECODER: MP3 opened
I (41403) FATFS_STREAM: File size: 2209488 byte, file position: 0
I (41410) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_RESUME,state:1
I (41480) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 2
I (41485) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (41491) I2S_STREAM: AUDIO_STREAM_WRITER
I (41497) AUDIO_PIPELINE: Pipeline started
E (41518) FLEXIBLE_PIPELINE: [ 4.1 ] Start playback new pipeline

```

- Press the [Mode] key again at this time, then the current MP3 playback is paused, the current pipeline is broken up to form a new pipeline, and the example switches back to play the original `test.aac`audio from exactly where it was stopped last time. The log is as follows:

```c
E (41518) FLEXIBLE_PIPELINE: [ 4.1 ] Start playback new pipeline
I (80605) AUDIO_ELEMENT: [file_mp3_reader] AEL_MSG_CMD_PAUSE
I (80628) MP3_DECODER: Closed
I (80628) AUDIO_ELEMENT: [mp3_decoder] AEL_MSG_CMD_PAUSE
I (80639) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_PAUSE
I (80664) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_PAUSE
E (80664) FLEXIBLE_PIPELINE: Changing music to aac format
I (80666) AUDIO_PIPELINE: create new rb,rb:0x3f80ef5c
I (80671) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4349764 Bytes, Inter:320260 Bytes, Dram:246352 Bytes

I (80683) AUDIO_ELEMENT: [file_aac_reader] AEL_MSG_CMD_RESUME,state:4
I (80692) FATFS_STREAM: File size: 2994446 byte, file position: 643072
I (80697) AUDIO_ELEMENT: [aac_decoder] AEL_MSG_CMD_RESUME,state:4
I (80704) AAC_DECODER: AAC song resume
I (80709) AAC_DECODER: this audio is RAW AAC
I (80726) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_RESUME,state:1
I (80790) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 2
I (80796) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (80800) I2S_STREAM: AUDIO_STREAM_WRITER
I (80827) AUDIO_PIPELINE: Pipeline started
E (80828) FLEXIBLE_PIPELINE: [ 4.1 ] Start playback new pipeline

```

- Press the [Mode] key one more time, then the current AAC playback is paused, the current pipeline is broken up to form a new pipeline, and the example switches back to play `test.mp3`audio from exactly where it was stopped last time. The log is as follows:

```c
I (116068) AUDIO_ELEMENT: [file_aac_reader] AEL_MSG_CMD_PAUSE
I (116078) AAC_DECODER: Closed by pause
I (116078) AUDIO_ELEMENT: [aac_decoder] AEL_MSG_CMD_PAUSE
I (116090) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_PAUSE
I (116114) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_PAUSE
E (116114) FLEXIBLE_PIPELINE: Changing music to mp3 format
I (116116) AUDIO_PIPELINE: create new rb,rb:0x3f80ffa0
I (116121) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4347388 Bytes, Inter:319996 Bytes, Dram:246088 Bytes

I (116133) AUDIO_ELEMENT: [file_mp3_reader] AEL_MSG_CMD_RESUME,state:4
I (116142) FATFS_STREAM: File size: 2209488 byte, file position: 870400
I (116148) AUDIO_ELEMENT: [mp3_decoder] AEL_MSG_CMD_RESUME,state:4
I (116155) MP3_DECODER: MP3 opened
I (116167) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_RESUME,state:1
I (116231) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 2
I (116237) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (116242) I2S_STREAM: AUDIO_STREAM_WRITER
I (116257) AUDIO_PIPELINE: Pipeline started
E (116260) FLEXIBLE_PIPELINE: [ 4.1 ] Start playback new pipeline
```


### Example Log

A complete log is as follows:

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 17:38:17
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 80MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 4MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (68) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00300000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1de10 (122384) map
I (135) esp_image: segment 1: paddr=0x0002de38 vaddr=0x3ffb0000 size=0x021c4 (  8644) load
I (138) esp_image: segment 2: paddr=0x00030004 vaddr=0x40080000 size=0x00014 (    20) load
0x40080000: _WindowOverflow4 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x51f68 (335720) map
0x400d0020: _stext at ??:?

I (258) esp_image: segment 4: paddr=0x00081f90 vaddr=0x40080014 size=0x0df14 ( 57108) load
0x40080014: _WindowOverflow4 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1734

I (288) boot: Loaded app from partition at offset 0x10000
I (288) boot: Disabling RNG early entropy source...
I (288) psram: This chip is ESP32-D0WD
I (293) spiram: Found 64MBit SPI RAM device
I (297) spiram: SPI RAM mode: flash 80m sram 80m
I (303) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (310) cpu_start: Pro cpu up.
I (314) cpu_start: Application information:
I (319) cpu_start: Project name:     flexible_pipeline
I (324) cpu_start: App version:      v2.2-210-g86396c1f-dirty
I (331) cpu_start: Compile time:     Nov  2 2021 17:38:18
I (337) cpu_start: ELF file SHA256:  9018ad4a70009e78...
I (343) cpu_start: ESP-IDF:          v4.2.2
I (348) cpu_start: Starting app cpu, entry point is 0x40081d74
0x40081d74: call_start_cpu1 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (846) spiram: SPI SRAM memory test OK
I (846) heap_init: Initializing. RAM available for dynamic allocation:
I (846) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (852) heap_init: At 3FFB2C18 len 0002D3E8 (180 KiB): DRAM
I (858) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (865) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (871) heap_init: At 4008DF28 len 000120D8 (72 KiB): IRAM
I (877) cpu_start: Pro cpu start user code
I (882) spiram: Adding pool of 4092K of external SPI memory to heap allocator
I (903) spi_flash: detected chip: gd
I (903) spi_flash: flash io: dio
W (903) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (913) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (928) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (956) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1001) SDCARD: CID name SD16G!

I (1456) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (1456) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1461) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1472) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1492) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1492) ES8388_DRIVER: init,out:02, in:00
W (1497) PERIPH_TOUCH: _touch_init
I (1512) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (1522) FLEXIBLE_PIPELINE: [ 1 ] Create all audio elements for playback pipeline
I (1522) MP3_DECODER: MP3 init
I (1523) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1530) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1554) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1557) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1562) FLEXIBLE_PIPELINE: [ 2 ] Register all audio elements to playback pipeline
I (1571) FLEXIBLE_PIPELINE: [ 3 ] Set up  event listener
I (1578) FLEXIBLE_PIPELINE: [3.1] Set up  i2s clock
I (1599) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (1602) FLEXIBLE_PIPELINE: [ 4 ] Start playback pipeline
I (1609) AUDIO_PIPELINE: link el->rb, el:0x3f807c50, tag:file_aac_reader, rb:0x3f808604
I (1617) AUDIO_PIPELINE: link el->rb, el:0x3f808098, tag:aac_decoder, rb:0x3f80a644
I (1626) AUDIO_PIPELINE: link el->rb, el:0x3f808200, tag:filter_upsample, rb:0x3f80ae84
I (1635) AUDIO_ELEMENT: [file_aac_reader-0x3f807c50] Element task created
I (1642) AUDIO_ELEMENT: [aac_decoder-0x3f808098] Element task created
I (1649) AUDIO_ELEMENT: [filter_upsample-0x3f808200] Element task created
I (1656) AUDIO_ELEMENT: [i2s_writer-0x3f80838c] Element task created
I (1663) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4374264 Bytes, Inter:330132 Bytes, Dram:256224 Bytes

I (1675) AUDIO_ELEMENT: [file_aac_reader] AEL_MSG_CMD_RESUME,state:1
I (1684) FATFS_STREAM: File size: 2994446 byte, file position: 0
I (1689) AUDIO_ELEMENT: [aac_decoder] AEL_MSG_CMD_RESUME,state:1
I (1696) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_RESUME,state:1
I (1766) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 2
I (1771) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (1777) I2S_STREAM: AUDIO_STREAM_WRITER
I (1782) AUDIO_PIPELINE: Pipeline started
I (1782) CODEC_ELEMENT_HELPER: The element is 0x3f808098. The reserve data 2 is 0x0.
I (1794) AAC_DECODER: a new song playing
I (1799) AAC_DECODER: this audio is RAW AAC
I (41285) AUDIO_ELEMENT: [file_aac_reader] AEL_MSG_CMD_PAUSE
I (41302) AAC_DECODER: Closed by pause
I (41302) AUDIO_ELEMENT: [aac_decoder] AEL_MSG_CMD_PAUSE
I (41308) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_PAUSE
I (41339) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_PAUSE
E (41339) FLEXIBLE_PIPELINE: Changing music to mp3 format
W (41340) AUDIO_PIPELINE: There are no listener registered
I (41347) AUDIO_PIPELINE: create new rb,rb:0x3f80c6dc
I (41352) AUDIO_PIPELINE: create new rb,rb:0x3f80e71c
I (41358) AUDIO_ELEMENT: [file_mp3_reader-0x3f807d88] Element task created
I (41365) AUDIO_ELEMENT: [mp3_decoder-0x3f807efc] Element task created
I (41373) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4352144 Bytes, Inter:320528 Bytes, Dram:246620 Bytes

I (41385) AUDIO_ELEMENT: [file_mp3_reader] AEL_MSG_CMD_RESUME,state:1
I (41392) AUDIO_ELEMENT: [mp3_decoder] AEL_MSG_CMD_RESUME,state:1
I (41398) MP3_DECODER: MP3 opened
I (41403) FATFS_STREAM: File size: 2209488 byte, file position: 0
I (41410) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_RESUME,state:1
I (41480) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 2
I (41485) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (41491) I2S_STREAM: AUDIO_STREAM_WRITER
I (41497) AUDIO_PIPELINE: Pipeline started
E (41518) FLEXIBLE_PIPELINE: [ 4.1 ] Start playback new pipeline
E (41518) FLEXIBLE_PIPELINE: [ 4.1 ] Start playback new pipeline
I (80605) AUDIO_ELEMENT: [file_mp3_reader] AEL_MSG_CMD_PAUSE
I (80628) MP3_DECODER: Closed
I (80628) AUDIO_ELEMENT: [mp3_decoder] AEL_MSG_CMD_PAUSE
I (80639) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_PAUSE
I (80664) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_PAUSE
E (80664) FLEXIBLE_PIPELINE: Changing music to aac format
I (80666) AUDIO_PIPELINE: create new rb,rb:0x3f80ef5c
I (80671) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4349764 Bytes, Inter:320260 Bytes, Dram:246352 Bytes

I (80683) AUDIO_ELEMENT: [file_aac_reader] AEL_MSG_CMD_RESUME,state:4
I (80692) FATFS_STREAM: File size: 2994446 byte, file position: 643072
I (80697) AUDIO_ELEMENT: [aac_decoder] AEL_MSG_CMD_RESUME,state:4
I (80704) AAC_DECODER: AAC song resume
I (80709) AAC_DECODER: this audio is RAW AAC
I (80726) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_RESUME,state:1
I (80790) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 2
I (80796) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (80800) I2S_STREAM: AUDIO_STREAM_WRITER
I (80827) AUDIO_PIPELINE: Pipeline started
E (80828) FLEXIBLE_PIPELINE: [ 4.1 ] Start playback new pipeline
I (116068) AUDIO_ELEMENT: [file_aac_reader] AEL_MSG_CMD_PAUSE
I (116078) AAC_DECODER: Closed by pause
I (116078) AUDIO_ELEMENT: [aac_decoder] AEL_MSG_CMD_PAUSE
I (116090) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_PAUSE
I (116114) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_PAUSE
E (116114) FLEXIBLE_PIPELINE: Changing music to mp3 format
I (116116) AUDIO_PIPELINE: create new rb,rb:0x3f80ffa0
I (116121) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4347388 Bytes, Inter:319996 Bytes, Dram:246088 Bytes

I (116133) AUDIO_ELEMENT: [file_mp3_reader] AEL_MSG_CMD_RESUME,state:4
I (116142) FATFS_STREAM: File size: 2209488 byte, file position: 870400
I (116148) AUDIO_ELEMENT: [mp3_decoder] AEL_MSG_CMD_RESUME,state:4
I (116155) MP3_DECODER: MP3 opened
I (116167) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_RESUME,state:1
I (116231) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 2
I (116237) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (116242) I2S_STREAM: AUDIO_STREAM_WRITER
I (116257) AUDIO_PIPELINE: Pipeline started
E (116260) FLEXIBLE_PIPELINE: [ 4.1 ] Start playback new pipeline

```

## Troubleshooting

If the following log appears, it may be caused by the ACC audio file prepared by yourself. The file may be encapsulated in an M4A container, but M4A initialization does not support seeking to a specific playback position, so you need to check whether the file with the .aac suffix is really in the AAC format.

```c
I (9601) AUDIO_ELEMENT: [file_aac_reader] AEL_MSG_CMD_RESUME,state:4
I (9610) FATFS_STREAM: File size: 20044370 byte, file position: 229376
I (9616) AUDIO_ELEMENT: [aac_decoder] AEL_MSG_CMD_RESUME,state:4
I (9622) AAC_DECODER: M4A song resume
E (9630) AAC_DECODER: M4A decoder encountered error 1 -1
E (9632) AUDIO_ELEMENT: [aac_decoder] ERROR_PROCESS, AEL_IO_FAIL
W (9639) AUDIO_ELEMENT: [aac_decoder] audio_element_on_cmd_error,3
I (9646) AAC_DECODER: Closed by [3]
I (9651) AUDIO_ELEMENT: [filter_upsample] AEL_MSG_CMD_RESUME,state:1
I (9721) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 2
I (9727) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (9731) I2S_STREAM: AUDIO_STREAM_WRITER
I (9737) AUDIO_PIPELINE: Pipeline started
E (9740) AUDIO_ELEMENT: [aac_decoder] RESUME: Element error, state:7
E (9747) FLEXIBLE_PIPELINE: [ 4.1 ] Start playback new pipeline
```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
