
# Downmix Pipeline Example

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief
This example shows how to use ESP-ADF multiple input pipelines to play back multiple files with a down-mixer.

The structure of multiple pipelines is shown below:

```
mp3 base input stream ---> resample ---> down-mix ---> I2S output stream ---> codec chip
                                           ^
                                           |
mp3 new come input stream ---> resample ---
```
This example uses dual-channel mp3 with at the sampling rate of 44.1 kHz as the basic music, and single-channel mp3 at the sampling rate of 16 kHz as the mixed input audio.

First, the two audios were decoded and resampled to 48 kHz respectively, and then the PCM with the sampling rate of 48 kHz was processed with Down-mix function.

Illustration of Downmixing Process:
```
        ^
    gain|
        |
        |
        |
        |        base music                       newcome music                         base music
     0dB|—————————————————————————        —————————————————————————————        —————————————————————————————>
        |                          \    /                               \    /
        |                           \  /                                 \  /
        |                            \/                                   \/
        |                            /\                                   /\
        |                           /  \                                 /  \
        |        newcome music     /    \          base music           /    \         newcome music
   -10dB|-------------------------        —————————————————————————————        ------------------------------
        |                         |< t1 >|                             |< t2 >|
        |                       TRANSMITTIME                         TRANSMITTIME
        |                         |
        |                   [mode-key event]
        |
        |----------------------------------------------------------------------------------------------------------->
        |                                                                                                  timeline
        |
```


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Example Set Up

### Default IDF Branch
This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

In this example, prepare a microSD card, prepare a dual-channel MP3 music with a sampling rate of 44.1 kHz as the basic music to play, and prepare a short Single channel 16 kHz sampling rate prompt tone, rename them and save to the microSD card.

> In this example, the file name to be played is fixed, a base music file name `music.mp3` and a tone name `tone.mp3`.

The default board for this example is `ESP32-Lyrat V4.3`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```c
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

This example needs to enable FATFS long file name support also.

```c
menuconfig > Component config > FAT Filesystem support > Long filename support
```

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the Getting Started Guide for full steps to configure and use  [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) to build projects.

## How to use the Example

### Example Functionality
- After the routine starts to run, it will automatically play the file `music.mp3` in the sdcard.

```c
I (998) AUDIO_ELEMENT: [base_file] AEL_MSG_CMD_RESUME,state:1
I (1005) AUDIO_ELEMENT: [base_mp3] AEL_MSG_CMD_RESUME,state:1
I (1011) MP3_DECODER: MP3 opened
I (1015) ADF_BIT_STREAM: The element is 0x3f8093c4. The reserve data 2 is 0x0.
I (1024) AUDIO_ELEMENT: [base_filter] AEL_MSG_CMD_RESUME,state:1
I (1126) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 1
I (1133) AUDIO_PIPELINE: Pipeline started
I (1134) FATFS_STREAM: File size: 2911638 byte, file position: 0
I (1143) AUDIO_ELEMENT: [mixer-0x3f806c74] Element task created
I (1165) AUDIO_ELEMENT: [i2s-0x3f806f4c] Element task created
I (1166) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4300152 Bytes, Inter:307160 Bytes, Dram:236708 Bytes
```

- After pressing the [mode] button, the prompt sound `tone.mp3` will be mixed in, and will gradually change the gain value within the `TRANSMIT TIME` time set by the program.

```c
I (1264) DOWNMIX_PIPELINE: [6.0] Base stream pipeline running
I (10923) AUDIO_ELEMENT: [newcome_file-0x3f809238] Element task created
I (10924) AUDIO_ELEMENT: [newcome_mp3-0x3f809534] Element task created
I (10927) AUDIO_ELEMENT: [newcome_filter-0x3f80980c] Element task created
I (10947) AUDIO_ELEMENT: [newcome_raw-0x3f809a54] Element task created
I (10948) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4277292 Bytes, Inter:293780 Bytes, Dram:223328 Bytes
I (10958) AUDIO_ELEMENT: [newcome_file] AEL_MSG_CMD_RESUME,state:1
I (10965) AUDIO_ELEMENT: [newcome_mp3] AEL_MSG_CMD_RESUME,state:1
I (10971) MP3_DECODER: MP3 opened
I (10976) ADF_BIT_STREAM: The element is 0x3f809534. The reserve data 2 is 0x0.
I (10990) FATFS_STREAM: File size: 20864 byte, file position: 0
I (10991) AUDIO_ELEMENT: [newcome_filter] AEL_MSG_CMD_RESUME,state:1
I (10998) RSP_FILTER: sample rate of source data : 16000, channel of source data : 1, sample rate of destination data : 48000, channel of destination data : 1
I (11016) AUDIO_PIPELINE: Pipeline started
I (11017) DOWNMIX_PIPELINE: New come music running...
I (11082) DOWNMIX: Reopen downmix
W (12009) FATFS_STREAM: No more data, ret:0
I (12009) AUDIO_ELEMENT: IN-[newcome_file] AEL_IO_DONE,0
I (14056) AUDIO_ELEMENT: IN-[newcome_mp3] AEL_IO_DONE,-2
I (14561) MP3_DECODER: Closed
I (14628) AUDIO_ELEMENT: IN-[newcome_filter] AEL_IO_DONE,-2
E (14638) AUDIO_ELEMENT: [newcome_file] Element already stopped
E (14638) AUDIO_ELEMENT: [newcome_mp3] Element already stopped
E (14640) AUDIO_ELEMENT: [newcome_filter] Element already stopped
```

### Example Logs
A complete log is as follows:

```c
I (62) boot: Chip Revision: 3
I (71) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (42) boot: ESP-IDF v3.3.2-107-g722043f73 2nd stage bootloader
I (42) boot: compile time 20:04:22
I (42) boot: Enabling RNG early entropy source...
I (48) boot: SPI Speed      : 80MHz
I (52) boot: SPI Mode       : DIO
I (56) boot: SPI Flash Size : 8MB
I (60) boot: Partition Table:
I (63) boot: ## Label            Usage          Type ST Offset   Length
I (71) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (78) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (86) boot:  2 factory          factory app      00 00 00010000 00300000
I (93) boot: End of partition table
I (97) boot_comm: chip revision: 3, min. application chip revision: 0
I (104) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x17424 ( 95268) map
I (140) esp_image: segment 1: paddr=0x0002744c vaddr=0x3ffb0000 size=0x020e4 (  8420) load
I (143) esp_image: segment 2: paddr=0x00029538 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (147) esp_image: segment 3: paddr=0x00029940 vaddr=0x40080400 size=0x066d0 ( 26320) load
I (164) esp_image: segment 4: paddr=0x00030018 vaddr=0x400d0018 size=0x397b8 (235448) map
0x400d0018: _flash_cache_start at ??:?

I (230) esp_image: segment 5: paddr=0x000697d8 vaddr=0x40086ad0 size=0x081d8 ( 33240) load
0x40086ad0: prvGetItemDefault at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp_ringbuf/ringbuf.c:969

I (250) boot: Loaded app from partition at offset 0x10000
I (251) boot: Disabling RNG early entropy source...
I (251) psram: This chip is ESP32-D0WD
I (255) spiram: Found 64MBit SPI RAM device
I (260) spiram: SPI RAM mode: flash 80m sram 80m
I (265) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (273) cpu_start: Pro cpu up.
I (276) cpu_start: Application information:
I (281) cpu_start: Project name:     downmix_pipeline
I (287) cpu_start: App version:      v2.2-112-g7f633143-dirty
I (293) cpu_start: Compile time:     Apr 28 2021 20:04:20
I (299) cpu_start: ELF file SHA256:  3bd9c54c2b4cff94...
I (305) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73
I (312) cpu_start: Starting app cpu, entry point is 0x40081478
0x40081478: call_start_cpu1 at /repo/adfs/bugfix/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268
I (0) cpu_start: App cpu up.
I (799) spiram: SPI SRAM memory test OK
I (800) heap_init: Initializing. RAM available for dynamic allocation:
I (800) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (806) heap_init: At 3FFB2B08 len 0002D4F8 (181 KiB): DRAM
I (812) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (818) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (825) heap_init: At 4008ECA8 len 00011358 (68 KiB): IRAM
I (831) cpu_start: Pro cpu start user code
I (836) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (185) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (186) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (194) DOWNMIX_PIPELINE: [1.0] Start audio codec chip
I (200) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (221) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (222) ES8388_DRIVER: init,out:02, in:00
I (232) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (235) DOWNMIX_PIPELINE: [2.0] Start and wait for SDCARD to mount
E (237) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (245) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (300) SDCARD: CID name NCard!
I (753) DOWNMIX_PIPELINE: [3.0] Create pipeline_mix to mix
I (753) DOWNMIX_PIPELINE: [3.1] Create downmixer element
I (754) DOWNMIX_PIPELINE: [3.2] Create i2s stream to read audio data from codec chip
I (761) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (772) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
W (798) PERIPH_TOUCH: _touch_init
I (799) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (799) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (807) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (819) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (824) DOWNMIX_PIPELINE: [3.3] Link elements together downmixer-->i2s_writer
I (832) DOWNMIX_PIPELINE: [3.4] Link elements together downmixer-->i2s_stream-->[codec_chip]
I (842) AUDIO_PIPELINE: link el->rb, el:0x3f806c74, tag:mixer, rb:0x3f8070a0
I (848) DOWNMIX_PIPELINE: [4.0] Create Fatfs stream to read input data
I (856) DOWNMIX_PIPELINE: [4.1] Create mp3 decoder to decode mp3 file
I (863) MP3_DECODER: MP3 init
I (866) MP3_DECODER: MP3 init
I (870) DOWNMIX_PIPELINE: [4.1] Create resample element
I (876) DOWNMIX_PIPELINE: [4.2] Create raw stream of base wav to write data
I (884) DOWNMIX_PIPELINE: [5.0] Set up  event listener
I (890) AUDIO_PIPELINE: link el->rb, el:0x3f8090e8, tag:base_file, rb:0x3f809c58
I (897) AUDIO_PIPELINE: link el->rb, el:0x3f8093c4, tag:base_mp3, rb:0x3f80bc98
I (905) AUDIO_PIPELINE: link el->rb, el:0x3f80969c, tag:base_filter, rb:0x3f80c4d8
I (914) AUDIO_PIPELINE: link el->rb, el:0x3f809238, tag:newcome_file, rb:0x3f80ce14
I (922) AUDIO_PIPELINE: link el->rb, el:0x3f809534, tag:newcome_mp3, rb:0x3f80ee54
I (930) AUDIO_PIPELINE: link el->rb, el:0x3f80980c, tag:newcome_filter, rb:0x3f80f694
I (939) DOWNMIX_PIPELINE: [5.1] Listening event from peripherals
I (948) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (960) AUDIO_ELEMENT: [base_file-0x3f8090e8] Element task created
I (967) AUDIO_ELEMENT: [base_mp3-0x3f8093c4] Element task created
I (974) AUDIO_ELEMENT: [base_filter-0x3f80969c] Element task created
I (980) AUDIO_ELEMENT: [base_raw-0x3f809934] Element task created
I (986) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4364052 Bytes, Inter:332996 Bytes, Dram:262544 Bytes
I (998) AUDIO_ELEMENT: [base_file] AEL_MSG_CMD_RESUME,state:1
I (1005) AUDIO_ELEMENT: [base_mp3] AEL_MSG_CMD_RESUME,state:1
I (1011) MP3_DECODER: MP3 opened
I (1015) ADF_BIT_STREAM: The element is 0x3f8093c4. The reserve data 2 is 0x0.
I (1024) AUDIO_ELEMENT: [base_filter] AEL_MSG_CMD_RESUME,state:1
I (1126) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 48000, channel of destination data : 1
I (1133) AUDIO_PIPELINE: Pipeline started
I (1134) FATFS_STREAM: File size: 2911638 byte, file position: 0
I (1143) AUDIO_ELEMENT: [mixer-0x3f806c74] Element task created
I (1165) AUDIO_ELEMENT: [i2s-0x3f806f4c] Element task created
I (1166) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4300152 Bytes, Inter:307160 Bytes, Dram:236708 Bytes
I (1174) AUDIO_ELEMENT: [mixer] AEL_MSG_CMD_RESUME,state:1
I (1251) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (1252) I2S_STREAM: AUDIO_STREAM_WRITER
I (1263) AUDIO_PIPELINE: Pipeline started
I (1264) DOWNMIX_PIPELINE: [6.0] Base stream pipeline running
I (10923) AUDIO_ELEMENT: [newcome_file-0x3f809238] Element task created
I (10924) AUDIO_ELEMENT: [newcome_mp3-0x3f809534] Element task created
I (10927) AUDIO_ELEMENT: [newcome_filter-0x3f80980c] Element task created
I (10947) AUDIO_ELEMENT: [newcome_raw-0x3f809a54] Element task created
I (10948) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4277292 Bytes, Inter:293780 Bytes, Dram:223328 Bytes
I (10958) AUDIO_ELEMENT: [newcome_file] AEL_MSG_CMD_RESUME,state:1
I (10965) AUDIO_ELEMENT: [newcome_mp3] AEL_MSG_CMD_RESUME,state:1
I (10971) MP3_DECODER: MP3 opened
I (10976) ADF_BIT_STREAM: The element is 0x3f809534. The reserve data 2 is 0x0.
I (10990) FATFS_STREAM: File size: 20864 byte, file position: 0
I (10991) AUDIO_ELEMENT: [newcome_filter] AEL_MSG_CMD_RESUME,state:1
I (10998) RSP_FILTER: sample rate of source data : 16000, channel of source data : 1, sample rate of destination data : 48000, channel of destination data : 1
I (11016) AUDIO_PIPELINE: Pipeline started
I (11017) DOWNMIX_PIPELINE: New come music running...
I (11082) DOWNMIX: Reopen downmix
W (12009) FATFS_STREAM: No more data, ret:0
I (12009) AUDIO_ELEMENT: IN-[newcome_file] AEL_IO_DONE,0
I (14056) AUDIO_ELEMENT: IN-[newcome_mp3] AEL_IO_DONE,-2
I (14561) MP3_DECODER: Closed
I (14628) AUDIO_ELEMENT: IN-[newcome_filter] AEL_IO_DONE,-2
E (14638) AUDIO_ELEMENT: [newcome_file] Element already stopped
E (14638) AUDIO_ELEMENT: [newcome_mp3] Element already stopped
E (14640) AUDIO_ELEMENT: [newcome_filter] Element already stopped
I (14707) DOWNMIX: Reopen downmix
I (14756) DOWNMIX_PIPELINE: New come music stoped or finsihed
W (14756) AUDIO_PIPELINE: Without stop, st:1
W (14756) AUDIO_PIPELINE: Without wait stop, st:1
W (14778) AUDIO_ELEMENT: [newcome_file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (14780) AUDIO_ELEMENT: [newcome_mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (14788) AUDIO_ELEMENT: [newcome_filter] Element has not create when AUDIO_ELEMENT_TERMINATE
W (14811) AUDIO_ELEMENT: [newcome_raw] Element has not create when AUDIO_ELEMENT_TERMINATE
I (14811) DOWNMIX_PIPELINE: New come music stoped or finsihed
I (19858) AUDIO_ELEMENT: [newcome_file-0x3f809238] Element task created
I (19861) AUDIO_ELEMENT: [newcome_mp3-0x3f809534] Element task created
I (19864) AUDIO_ELEMENT: [newcome_filter-0x3f80980c] Element task created
I (19885) AUDIO_ELEMENT: [newcome_raw-0x3f809a54] Element task created
I (19885) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4277224 Bytes, Inter:294296 Bytes, Dram:223844 Bytes
I (19894) AUDIO_ELEMENT: [newcome_file] AEL_MSG_CMD_RESUME,state:1
I (19902) AUDIO_ELEMENT: [newcome_mp3] AEL_MSG_CMD_RESUME,state:1
I (19908) MP3_DECODER: MP3 opened
I (19912) ADF_BIT_STREAM: The element is 0x3f809534. The reserve data 2 is 0x0.
I (19933) FATFS_STREAM: File size: 20864 byte, file position: 0
I (19946) AUDIO_ELEMENT: [newcome_filter] AEL_MSG_CMD_RESUME,state:1
I (19949) RSP_FILTER: sample rate of source data : 16000, channel of source data : 1, sample rate of destination data : 48000, channel of destination data : 1
I (19973) AUDIO_PIPELINE: Pipeline started
I (19974) DOWNMIX_PIPELINE: New come music running...
I (20045) DOWNMIX: Reopen downmix
W (20979) FATFS_STREAM: No more data, ret:0
I (20979) AUDIO_ELEMENT: IN-[newcome_file] AEL_IO_DONE,0
I (23016) AUDIO_ELEMENT: IN-[newcome_mp3] AEL_IO_DONE,-2
I (23525) MP3_DECODER: Closed
I (23588) AUDIO_ELEMENT: IN-[newcome_filter] AEL_IO_DONE,-2
E (23598) AUDIO_ELEMENT: [newcome_file] Element already stopped
E (23600) AUDIO_ELEMENT: [newcome_mp3] Element already stopped
E (23602) AUDIO_ELEMENT: [newcome_filter] Element already stopped
I (23669) DOWNMIX: Reopen downmix
I (23732) DOWNMIX_PIPELINE: New come music stoped or finsihed
W (23732) AUDIO_PIPELINE: Without stop, st:1
W (23740) AUDIO_PIPELINE: Without wait stop, st:1
W (23740) AUDIO_ELEMENT: [newcome_file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (23749) AUDIO_ELEMENT: [newcome_mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (23758) AUDIO_ELEMENT: [newcome_filter] Element has not create when AUDIO_ELEMENT_TERMINATE
W (23789) AUDIO_ELEMENT: [newcome_raw] Element has not create when AUDIO_ELEMENT_TERMINATE
I (23795) DOWNMIX_PIPELINE: New come music stoped or finsihed
I (36208) AUDIO_ELEMENT: [newcome_file-0x3f809238] Element task created
I (36223) AUDIO_ELEMENT: [newcome_mp3-0x3f809534] Element task created
I (36226) AUDIO_ELEMENT: [newcome_filter-0x3f80980c] Element task created
I (36232) AUDIO_ELEMENT: [newcome_raw-0x3f809a54] Element task created
I (36245) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4277224 Bytes, Inter:294296 Bytes, Dram:223844 Bytes
I (36253) AUDIO_ELEMENT: [newcome_file] AEL_MSG_CMD_RESUME,state:1
I (36270) AUDIO_ELEMENT: [newcome_mp3] AEL_MSG_CMD_RESUME,state:1
I (36271) MP3_DECODER: MP3 opened
I (36273) ADF_BIT_STREAM: The element is 0x3f809534. The reserve data 2 is 0x0.
I (36282) AUDIO_ELEMENT: [newcome_filter] AEL_MSG_CMD_RESUME,state:1
I (36290) RSP_FILTER: sample rate of source data : 16000, channel of source data : 1, sample rate of destination data : 48000, channel of destination data : 1
I (36313) AUDIO_PIPELINE: Pipeline started
I (36313) DOWNMIX_PIPELINE: New come music running...
I (36315) FATFS_STREAM: File size: 20864 byte, file position: 0
I (36382) DOWNMIX: Reopen downmix
W (37317) FATFS_STREAM: No more data, ret:0
I (37317) AUDIO_ELEMENT: IN-[newcome_file] AEL_IO_DONE,0
I (39354) AUDIO_ELEMENT: IN-[newcome_mp3] AEL_IO_DONE,-2
I (39862) MP3_DECODER: Closed
I (39929) AUDIO_ELEMENT: IN-[newcome_filter] AEL_IO_DONE,-2
E (39937) AUDIO_ELEMENT: [newcome_file] Element already stopped
E (39937) AUDIO_ELEMENT: [newcome_mp3] Element already stopped
E (39939) AUDIO_ELEMENT: [newcome_filter] Element already stopped
I (40007) DOWNMIX: Reopen downmix
I (40056) DOWNMIX_PIPELINE: New come music stoped or finsihed
W (40058) AUDIO_PIPELINE: Without stop, st:1
W (40078) AUDIO_PIPELINE: Without wait stop, st:1
W (40078) AUDIO_ELEMENT: [newcome_file] Element has not create when AUDIO_ELEMENT_TERMINATE
W (40083) AUDIO_ELEMENT: [newcome_mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (40092) AUDIO_ELEMENT: [newcome_filter] Element has not create when AUDIO_ELEMENT_TERMINATE
W (40111) AUDIO_ELEMENT: [newcome_raw] Element has not create when AUDIO_ELEMENT_TERMINATE
I (40114) DOWNMIX_PIPELINE: New come music stoped or finsihed
```

## Troubleshooting
The CPU loading of the Down-mix algorithm itself is very low.

If the basic music and mixed music decoding are both audio files with high CPU loading (such as the basic music and mixed music are both 48 kHz and dual-channel mp3 audio), then the Down-mix process may have a time out error in data reading-writing, and output music is choppy.

It is recommended to select the appropriate input audio.


## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
