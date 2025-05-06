# AEC examples

- [中文版本](./README_CN.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")

## Example Brief

This example is to play music while performing echo cancellation of the sound recorded by the microphone, and then save it to the microSD card.

This AEC example consists of two pipelines: one for playing an MP3 file from flash and another for recording audio. First, the audio is processed using the AEC algorithm, then encoded into WAV format and saved to a microSD card. Finally, the original audio is compared with the recorded audio.
> In comparison to the complete `algorithm`, the AEC mode does not allow the activation of AGC and NS features, however, it supports audio input at an 8 kHz sampling rate.

- Playing MP3 pipeline:

  ```c
  [flash] ---> mp3_decoder ---> filter ---> i2s_stream ---> [codec_chip]
  ```

- Recording WAV pipeline:

  ```c
  [codec_chip] ---> i2s_stream ---> filter ---> AEC ---> wav_encoder ---> fatfs_stream ---> [sdcard]
  ```

## Environment Setup

#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Example Set Up

### Default IDF Branch

This example supports IDF release/v5.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

Prepare the audio board:

- Insert a microSD card required memory of 1 MB into board's SD card slot.

Load and run the example:

- The board will start playing automatically.
- After finish, you can open `/sdcard/aec_out.wav` to hear the recorded file.

NSNET has better noise suppression. If using NSNET, please enable 'ESP Speech Recognition ->USE_SNET' in menuconfig.

### Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```c
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See the Getting Started Guide for full steps to configure and use [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/index.html) to build projects.


## How to use the Example

### Example Functionality

After download the follow log should be output:

```
I (27) boot: ESP-IDF v5.3.1-dirty 2nd stage bootloader
I (27) boot: compile time Mar  3 2025 17:26:29
I (27) boot: Multicore bootloader
I (28) boot: chip revision: v0.2
I (30) qio_mode: Enabling default flash chip QIO
I (35) boot.esp32s3: Boot SPI Speed : 80MHz
I (39) boot.esp32s3: SPI Mode       : QIO
I (42) boot.esp32s3: SPI Flash Size : 16MB
I (46) boot: Enabling RNG early entropy source...
I (51) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (66) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (73) boot:  2 factory          factory app      00 00 00010000 00200000
I (79) boot:  3 model            Unknown data     01 82 00210000 0050c000
I (86) boot: End of partition table
I (89) esp_image: segment 0: paddr=00010020 vaddr=3c060020 size=f0e34h (986676) map
I (246) esp_image: segment 1: paddr=00100e5c vaddr=3fc98b00 size=03ed0h ( 16080) load
I (249) esp_image: segment 2: paddr=00104d34 vaddr=40374000 size=0b2e4h ( 45796) load
I (258) esp_image: segment 3: paddr=00110020 vaddr=42000020 size=5c1a8h (377256) map
I (316) esp_image: segment 4: paddr=0016c1d0 vaddr=4037f2e4 size=097b0h ( 38832) load
I (324) esp_image: segment 5: paddr=00175988 vaddr=600fe000 size=0001ch (    28) load
I (332) boot: Loaded app from partition at offset 0x10000
I (333) boot: Disabling RNG early entropy source...
I (343) octal_psram: vendor id    : 0x0d (AP)
I (343) octal_psram: dev id       : 0x02 (generation 3)
I (343) octal_psram: density      : 0x03 (64 Mbit)
I (345) octal_psram: good-die     : 0x01 (Pass)
I (349) octal_psram: Latency      : 0x01 (Fixed)
I (354) octal_psram: VCC          : 0x01 (3V)
I (358) octal_psram: SRF          : 0x01 (Fast Refresh)
I (363) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
I (367) octal_psram: BurstLen     : 0x01 (32 Byte)
I (372) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (377) octal_psram: DriveStrength: 0x00 (1/1)
I (382) MSPI Timing: PSRAM timing tuning index: 5
I (386) esp_psram: Found 8MB PSRAM device
I (389) esp_psram: Speed: 80MHz
I (392) cpu_start: Multicore app
I (828) esp_psram: SPI SRAM memory test OK
I (837) cpu_start: Pro cpu start user code
I (837) cpu_start: cpu freq: 240000000 Hz
I (837) app_init: Application information:
I (838) app_init: Project name:     aec_examples
I (842) app_init: App version:      v2.7-54-ga7363584-dirty
I (847) app_init: Compile time:     Mar  5 2025 17:11:07
I (852) app_init: ELF file SHA256:  3f33133f4...
I (856) app_init: ESP-IDF:          v5.5-dev-1748-g4fd2ed4f534
I (862) efuse_init: Min chip rev:     v0.0
I (866) efuse_init: Max chip rev:     v0.99
I (870) efuse_init: Chip rev:         v0.2
I (874) heap_init: Initializing. RAM available for dynamic allocation:
I (880) heap_init: At 3FC9D7E8 len 0004BF28 (303 KiB): RAM
I (885) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
I (890) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (895) heap_init: At 600FE01C len 00001FCC (7 KiB): RTCRAM
I (901) esp_psram: Adding pool of 8192K of PSRAM memory to heap allocator
I (908) spi_flash: detected chip: gd
I (911) spi_flash: flash io: qio
W (914) ADC: legacy driver is deprecated, please migrate to `esp_adc/adc_oneshot.h`
I (921) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (927) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (934) main_task: Started on CPU0
I (951) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (951) main_task: Calling app_main()
I (952) AEC_EXAMPLES: [1.0] Mount sdcard
I (955) SDCARD: Using 1-line SD mode,  base path=/sdcard
I (960) gpio: GPIO[15]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (968) gpio: GPIO[7]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (977) gpio: GPIO[4]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (985) AUDIO_THREAD: The esp_periph task allocate stack on internal memory
I (1025) SDCARD: CID name SD   !

I (1491) AEC_EXAMPLES: [2.0] Start codec chip
W (1492) i2c_bus_v2: I2C master handle is NULL, will create new one
I (1497) DRV8311: ES8311 in Slave mode
I (1508) gpio: GPIO[48]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1515) ES7210: ES7210 in Slave mode
I (1524) ES7210: Enable ES7210_INPUT_MIC1
I (1527) ES7210: Enable ES7210_INPUT_MIC2
I (1529) ES7210: Enable ES7210_INPUT_MIC3
W (1533) ES7210: Enable TDM mode. ES7210_SDP_INTERFACE2_REG12: 2
I (1537) ES7210: config fmt 60
I (1540) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (1548) AEC_EXAMPLES: [3.0] Create audio pipeline_rec for recording
I (1548) AEC_EXAMPLES: [3.1] Create algorithm stream for aec
I (1549) AEC_EXAMPLES: [3.2] Create wav encoder to encode wav format
I (1555) AEC_EXAMPLES: [3.3] Create fatfs stream to write data to sdcard
I (1561) AEC_EXAMPLES: [3.4] Register all elements to audio pipeline_rec
I (1567) AEC_EXAMPLES: [3.5] Link it together [codec_chip]-->aec-->wav_encoder-->fatfs_stream-->[sdcard]
I (1577) AUDIO_PIPELINE: link el->rb, el:0x3c168f10, tag:aec, rb:0x3c169300
I (1584) AUDIO_PIPELINE: link el->rb, el:0x3c169040, tag:wav_encoder, rb:0x3c16b348
I (1591) AEC_EXAMPLES: [3.6] Set up  uri (file as fatfs_stream, wav as wav encoder)
I (1598) AEC_EXAMPLES: [4.0] Create audio pipeline_play for playing
I (1604) AEC_EXAMPLES: [4.2] Create mp3 decoder to decode mp3 file
I (1610) MP3_DECODER: MP3 init
I (1614) AEC_EXAMPLES: [4.4] Register all elements to audio pipeline_play
I (1620) AEC_EXAMPLES: [4.5] Link it together [flash]-->mp3_decoder-->filter-->[codec_chip]
I (1628) AUDIO_PIPELINE: link el->rb, el:0x3c16d42c, tag:mp3_decoder, rb:0x3c16d6ec
I (1636) AEC_EXAMPLES: [5.0] Set up event listener
I (1640) AEC_EXAMPLES: [5.1] Listening event from all elements of pipeline
I (1647) AEC_EXAMPLES: [5.2] Listening event from peripherals
I (1652) AEC_EXAMPLES: [6.0] Start audio_pipeline
I (1657) AUDIO_THREAD: The mp3_decoder task allocate stack on external memory
I (1664) AUDIO_ELEMENT: [mp3_decoder-0x3c16d42c] Element task created
I (1670) AUDIO_THREAD: The filter_w task allocate stack on external memory
I (1676) AUDIO_ELEMENT: [filter_w-0x3c16d590] Element task created
I (1682) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:8624784 Bytes, Inter:334659 Bytes, Dram:334659 Bytes, Dram largest free:258048Bytes

I (1696) AUDIO_ELEMENT: [mp3_decoder] AEL_MSG_CMD_RESUME,state:1
I (1702) MP3_DECODER: MP3 opened
I (1706) CODEC_ELEMENT_HELPER: The element is 0x3c16d42c. The reserve data 2 is 0x0.
Set Mp3 Codec: mp3_decoder_open | 362
I (1721) AUDIO_ELEMENT: [filter_w] AEL_MSG_CMD_RESUME,state:1
I (1723) RSP_FILTER: sample rate of source data : 16000, channel of source data : 1, sample rate of destination data : 16000, channel of destination data : 1
I (1739) AUDIO_PIPELINE: Pipeline started
I (1740) AUDIO_THREAD: The aec task allocate stack on external memory
I (1747) AUDIO_ELEMENT: [aec-0x3c168f10] Element task created
I (1752) AUDIO_THREAD: The wav_encoder task allocate stack on external memory
I (1759) AUDIO_ELEMENT: [wav_encoder-0x3c169040] Element task created
I (1765) AUDIO_THREAD: The fatfs_stream task allocate stack on internal memory
I (1772) AUDIO_ELEMENT: [fatfs_stream-0x3c16917c] Element task created
I (1778) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:8562728 Bytes, Inter:329491 Bytes, Dram:329491 Bytes, Dram largest free:253952Bytes

I (1792) AUDIO_ELEMENT: [aec] AEL_MSG_CMD_RESUME,state:1
W (1838) AEC_STREAM: aec handle 0x3fcee0f8, mic: 1, total_ch_num: 2, sample_rate: 16000, chunk size: 512
I (1861) AUDIO_ELEMENT: [wav_encoder] AEL_MSG_CMD_RESUME,state:1
I (1871) AUDIO_ELEMENT: [fatfs_stream] AEL_MSG_CMD_RESUME,state:1
I (1879) AUDIO_PIPELINE: Pipeline started
I (1885) AEC_EXAMPLES: [ * ] Receive music info from mp3 decoder, sample_rates=16000, bits=16, ch=1
```

## Troubleshooting

- If the AEC effect is not very good, you can open the `DEBUG_ALGO_INPUT` define to get the original input data (left channel is the signal captured from the microphone, and right channel is the signal played to the speaker), and then check the delay with an audio analysis tool.

- The AEC internal buffering mechanism requires that the recording signal is delayed by around 0 - 10 ms compared to the corresponding reference (playback) signal.

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
