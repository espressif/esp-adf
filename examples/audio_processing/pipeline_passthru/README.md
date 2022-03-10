# Audio Passthrough

- [中文版本](./README_CN.md)
- Regular Example: ![alt text](../../../docs/_static/level_regular.png "Regular Example")


## Example Brief

This example demonstrates how to pass through the audio received from the `aux_in` port to the earphone or speaker ports.

Application scenarios of audio passthrough:

- Verify the integrity of the audio pipeline from beginning to end when using the new hardware design.
- Check the consistency of the left and right channels through the audio path.
- Measure THD+N together with audio test equipment for production line testing or performance evaluation.

The passthrough pipeline of this example is as follows:

```

[codec_chip] ---> i2s_stream_reader ---> i2s_stream_writer ---> [codec_chip]

```


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Prepare an aux audio cable to connect the `AUX_IN` of the development board and the `Line-Out` of the audio output side.

The default board for this example is `ESP32-Lyrat V4.3`, which is the only supported board so far.


### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

### Example Functionality

- The example starts running and is waiting for input signal from `AUX_IN` port. This port is connected to the `Line-Out` port of the PC with an aux cable. Below is the log.

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
I (27) boot: compile time 11:27:17
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0a194 ( 41364) map
I (127) esp_image: segment 1: paddr=0x0001a1bc vaddr=0x3ffb0000 size=0x02110 (  8464) load
I (131) esp_image: segment 2: paddr=0x0001c2d4 vaddr=0x40080000 size=0x03d44 ( 15684) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (140) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x1f2ac (127660) map
0x400d0020: _stext at ??:?

I (191) esp_image: segment 4: paddr=0x0003f2d4 vaddr=0x40083d44 size=0x081a0 ( 33184) load
0x40083d44: i2c_master_cmd_begin_static at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/driver/i2c.c:1014

I (212) boot: Loaded app from partition at offset 0x10000
I (213) boot: Disabling RNG early entropy source...
I (213) cpu_start: Pro cpu up.
I (217) cpu_start: Application information:
I (221) cpu_start: Project name:     passthru
I (226) cpu_start: App version:      v2.2-216-gbb375dee-dirty
I (233) cpu_start: Compile time:     Nov  8 2021 11:27:11
I (239) cpu_start: ELF file SHA256:  26639813b619eb6a...
I (245) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (251) cpu_start: Starting app cpu, entry point is 0x40081704
0x40081704: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (261) heap_init: Initializing. RAM available for dynamic allocation:
I (268) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (274) heap_init: At 3FFB2988 len 0002D678 (181 KiB): DRAM
I (281) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (287) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (293) heap_init: At 4008BEE4 len 0001411C (80 KiB): IRAM
I (299) cpu_start: Pro cpu start user code
I (318) spi_flash: detected chip: gd
I (318) spi_flash: flash io: dio
W (318) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (328) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (339) PASSTHRU: [ 1 ] Start codec chip
I (349) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (369) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (369) ES8388_DRIVER: init,out:02, in:00
I (379) AUDIO_HAL: Codec mode is 4, Ctrl:1
I (379) PASSTHRU: [ 2 ] Create audio pipeline for playback
I (379) PASSTHRU: [3.1] Create i2s stream to write data to codec chip
I (389) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (399) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (429) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (429) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (429) PASSTHRU: [3.2] Create i2s stream to read data from codec chip
W (439) I2S: I2S driver already installed
I (449) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (449) PASSTHRU: [3.3] Register all elements to audio pipeline
I (459) PASSTHRU: [3.4] Link it together [codec_chip]-->i2s_stream_reader-->i2s_stream_writer-->[codec_chip]
I (469) AUDIO_PIPELINE: link el->rb, el:0x3ffb881c, tag:i2s_read, rb:0x3ffb8ba0
I (479) PASSTHRU: [ 4 ] Set up  event listener
I (479) PASSTHRU: [4.1] Listening event from all elements of pipeline
I (489) PASSTHRU: [ 5 ] Start audio_pipeline
I (489) AUDIO_ELEMENT: [i2s_read-0x3ffb881c] Element task created
I (499) AUDIO_ELEMENT: [i2s_write-0x3ffb8490] Element task created
I (509) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:268604 Bytes

I (519) AUDIO_ELEMENT: [i2s_read] AEL_MSG_CMD_RESUME,state:1
I (519) I2S_STREAM: AUDIO_STREAM_READER,Rate:44100,ch:2
I (549) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (549) AUDIO_ELEMENT: [i2s_write] AEL_MSG_CMD_RESUME,state:1
I (559) I2S_STREAM: AUDIO_STREAM_WRITER
I (559) AUDIO_PIPELINE: Pipeline started
I (569) PASSTHRU: [ 6 ] Listen for all pipeline events
I (9959) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (9959) HEADPHONE: Headphone jack removed

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
I (27) boot: compile time 11:27:17
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0a194 ( 41364) map
I (127) esp_image: segment 1: paddr=0x0001a1bc vaddr=0x3ffb0000 size=0x02110 (  8464) load
I (131) esp_image: segment 2: paddr=0x0001c2d4 vaddr=0x40080000 size=0x03d44 ( 15684) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (140) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x1f2ac (127660) map
0x400d0020: _stext at ??:?

I (191) esp_image: segment 4: paddr=0x0003f2d4 vaddr=0x40083d44 size=0x081a0 ( 33184) load
0x40083d44: i2c_master_cmd_begin_static at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/driver/i2c.c:1014

I (212) boot: Loaded app from partition at offset 0x10000
I (213) boot: Disabling RNG early entropy source...
I (213) cpu_start: Pro cpu up.
I (217) cpu_start: Application information:
I (221) cpu_start: Project name:     passthru
I (226) cpu_start: App version:      v2.2-216-gbb375dee-dirty
I (233) cpu_start: Compile time:     Nov  8 2021 11:27:11
I (239) cpu_start: ELF file SHA256:  26639813b619eb6a...
I (245) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (251) cpu_start: Starting app cpu, entry point is 0x40081704
0x40081704: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (261) heap_init: Initializing. RAM available for dynamic allocation:
I (268) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (274) heap_init: At 3FFB2988 len 0002D678 (181 KiB): DRAM
I (281) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (287) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (293) heap_init: At 4008BEE4 len 0001411C (80 KiB): IRAM
I (299) cpu_start: Pro cpu start user code
I (318) spi_flash: detected chip: gd
I (318) spi_flash: flash io: dio
W (318) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (328) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (339) PASSTHRU: [ 1 ] Start codec chip
I (349) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (369) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (369) ES8388_DRIVER: init,out:02, in:00
I (379) AUDIO_HAL: Codec mode is 4, Ctrl:1
I (379) PASSTHRU: [ 2 ] Create audio pipeline for playback
I (379) PASSTHRU: [3.1] Create i2s stream to write data to codec chip
I (389) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (399) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (429) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (429) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (429) PASSTHRU: [3.2] Create i2s stream to read data from codec chip
W (439) I2S: I2S driver already installed
I (449) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (449) PASSTHRU: [3.3] Register all elements to audio pipeline
I (459) PASSTHRU: [3.4] Link it together [codec_chip]-->i2s_stream_reader-->i2s_stream_writer-->[codec_chip]
I (469) AUDIO_PIPELINE: link el->rb, el:0x3ffb881c, tag:i2s_read, rb:0x3ffb8ba0
I (479) PASSTHRU: [ 4 ] Set up  event listener
I (479) PASSTHRU: [4.1] Listening event from all elements of pipeline
I (489) PASSTHRU: [ 5 ] Start audio_pipeline
I (489) AUDIO_ELEMENT: [i2s_read-0x3ffb881c] Element task created
I (499) AUDIO_ELEMENT: [i2s_write-0x3ffb8490] Element task created
I (509) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:268604 Bytes

I (519) AUDIO_ELEMENT: [i2s_read] AEL_MSG_CMD_RESUME,state:1
I (519) I2S_STREAM: AUDIO_STREAM_READER,Rate:44100,ch:2
I (549) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (549) AUDIO_ELEMENT: [i2s_write] AEL_MSG_CMD_RESUME,state:1
I (559) I2S_STREAM: AUDIO_STREAM_WRITER
I (559) AUDIO_PIPELINE: Pipeline started
I (569) PASSTHRU: [ 6 ] Listen for all pipeline events
I (9959) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (9959) HEADPHONE: Headphone jack removed

```

## Troubleshooting

If there is no sound from the development board, or the sound is very weak, this is because by default the volume of the sound input from`AUX_IN` is low. So please increase the volume on the `Line-Out` end so that you can hear the audio output from the board.


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
