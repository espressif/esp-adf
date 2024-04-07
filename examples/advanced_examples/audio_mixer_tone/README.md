# Multiple channel mixing example

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_regular.png "中级")


## 例程简介

This example demonstrates how to perform multi-channel mixing using the ADF.

   ```
   [sdcard] ---> file_mp3_reader ---> mp3_decoder ---+ 
                                                      |
   [http] -----> http_mp3_reader ---> mp3_decoder-----|            
                                                      |
                                                      |-------> Audio Mixer ----> i2s_stream_writer ---> [codec_chip] --->PA
   [tone] -----> tone_aac_reader ---> aac_decoder-----|                    |
                                                      |                    |-----> Sdcard
   [reorder] ----------> i2s read ------------------- |                    |
                                                      |                    |-----> Internet
   [Others]
   ```

Refer to the table below for the supported commands:
|Index|Command|Description or Usage|Related Function|
| -- | -- | -- | -- |
|01| play| Play a single audio channel using esp_sudio. Currently, `play` can only play one record, while others can be played using PMixer |cli_play|
|02| stop| Stop playing audio initiated by `play` |cli_stop|
|03| pmixer| Play a single audio channel. Multiple channels can be played simultaneously using different slots to achieve mixing effects |cli_play_mixer|
|04| smixer| Stop playback initiated by `pmixer` |cli_replay_mixer|
|05| rpmixer| Replay the audio of the `pmixer`. Ensure that the pmixer is in a stopped or completed state before replay |cli_stop_mixer|
|06| dmixer| Destroy the PMixer, which allows the released slots to be reused by other channels after destruction |cli_destory_mixer|
|07| gmixer| Set the gain value for the corresponding slot |cli_mixer_gain_set|
|08| record| Starting or stopping recording while playing or pausing can simulate a karaoke scene |recorder_fn|

## Environment Setup

#### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.

## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.


### Configuration

Prepare a microSD card and some audio files in  OGG, OPUS, MP3, WAV, AAC formats, and name them `test.ogg`, `test.opus`, `test.mp3`, `test.wav`, `test.aac`. Then, copy them to the card, and insert the card to the development board.

The default board for this example is `ESP32_S3_KORVO2_V3_BOARD`, if you need to run this example on other development boards, select the board in menuconfig, such as `ESP32_S3_KORVO2_V3_BOARD`.

```
menuconfig > Audio HAL > ESP32_S3_KORVO2_V3_BOARD
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

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.


## How to Use the Example


### Example Functionality

- The example starts running and the terminal prompts `esp32` and waits for you to input commands. The log is as follows:

```c
I (22) boot: ESP-IDF v5.1-dirty 2nd stage bootloader
I (23) boot: compile time Dec 25 2023 16:04:00
I (23) boot: Multicore bootloader
I (26) boot: chip revision: v0.1
I (30) boot.esp32s3: Boot SPI Speed : 80MHz
I (35) boot.esp32s3: SPI Mode       : DIO
I (39) boot.esp32s3: SPI Flash Size : 4MB
I (44) boot: Enabling RNG early entropy source...
I (50) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (68) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00300000
I (83) boot:  3 flash_tone       Unknown data     01 27 00310000 00096000
I (90) boot: End of partition table
I (94) esp_image: segment 0: paddr=00010020 vaddr=3c0f0020 size=4d7f4h (317428) map
I (160) esp_image: segment 1: paddr=0005d81c vaddr=3fc9cc00 size=027fch ( 10236) load
I (162) esp_image: segment 2: paddr=00060020 vaddr=42000020 size=ebdb8h (966072) map
I (339) esp_image: segment 3: paddr=0014bde0 vaddr=3fc9f3fc size=02268h (  8808) load
I (341) esp_image: segment 4: paddr=0014e050 vaddr=40374000 size=18b7ch (101244) load
I (377) boot: Loaded app from partition at offset 0x10000
I (377) boot: Disabling RNG early entropy source...
I (389) cpu_start: Multicore app
I (389) octal_psram: vendor id    : 0x0d (AP)
I (389) octal_psram: dev id       : 0x02 (generation 3)
I (392) octal_psram: density      : 0x03 (64 Mbit)
I (398) octal_psram: good-die     : 0x01 (Pass)
I (403) octal_psram: Latency      : 0x01 (Fixed)
I (408) octal_psram: VCC          : 0x01 (3V)
I (413) octal_psram: SRF          : 0x01 (Fast Refresh)
I (419) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
I (425) octal_psram: BurstLen     : 0x01 (32 Byte)
I (431) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (437) octal_psram: DriveStrength: 0x00 (1/1)
I (442) esp_psram: Found 8MB PSRAM device
I (447) esp_psram: Speed: 40MHz
I (450) cpu_start: Pro cpu up.
I (454) cpu_start: Starting app cpu, entry point is 0x40375880
0x40375880: call_start_cpu1 at /home/xutao/workspace-20/esp-adf-internal/esp-idf/components/esp_system/port/cpu_start.c:154

I (0) cpu_start: App cpu up.
I (1192) esp_psram: SPI SRAM memory test OK
I (1201) cpu_start: Pro cpu start user code
I (1201) cpu_start: cpu freq: 160000000 Hz
I (1201) cpu_start: Application information:
I (1204) cpu_start: Project name:     audio_mixer
I (1210) cpu_start: App version:      v2.5-127-ga2dbcacd-dirty
I (1216) cpu_start: Compile time:     Dec 26 2023 10:28:44
I (1222) cpu_start: ELF file SHA256:  5f7f9becdaaf7e4d...
I (1228) cpu_start: ESP-IDF:          v5.1-dirty
I (1234) cpu_start: Min chip rev:     v0.0
I (1238) cpu_start: Max chip rev:     v0.99
I (1243) cpu_start: Chip rev:         v0.1
I (1248) heap_init: Initializing. RAM available for dynamic allocation:
I (1256) heap_init: At 3FCA6730 len 00042FE0 (267 KiB): DRAM
I (1262) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
I (1269) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (1275) heap_init: At 600FE010 len 00001FF0 (7 KiB): RTCRAM
I (1281) esp_psram: Adding pool of 8192K of PSRAM memory to heap allocator
I (1290) spi_flash: detected chip: gd
I (1293) spi_flash: flash io: dio
W (1297) spi_flash: Detected size(16384k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
W (1311) i2s(legacy): legacy i2s driver is deprecated, please migrate to use driver/i2s_std.h, driver/i2s_pdm.h or driver/i2s_tdm.h
W (1323) ADC: legacy driver is deprecated, please migrate to `esp_adc/adc_oneshot.h`
I (1331) sleep: Configure to isolate all GPIO pins in sleep state
I (1338) sleep: Enable automatic switching of GPIO sleep configuration
I (1346) app_start: Starting scheduler on CPU0
I (1351) app_start: Starting scheduler on CPU1
I (1351) main_task: Started on CPU0
I (1361) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1371) main_task: Calling app_main()
I (1391) DRV8311: ES8311 in Slave mode
I (1411) gpio: GPIO[48]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (1411) I2C_BUS: I2C bus has been already created, [port:0]
I (1421) ES7210: ES7210 in Slave mode
I (1431) ES7210: Enable ES7210_INPUT_MIC1
I (1431) ES7210: Enable ES7210_INPUT_MIC2
I (1441) ES7210: Enable ES7210_INPUT_MIC3
W (1441) ES7210: Enable TDM mode. ES7210_SDP_INTERFACE2_REG12: 2
I (1451) ES7210: config fmt 60
I (1451) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (1461) SDCARD: Using 1-line SD mode,  base path=/sdcard
I (1461) gpio: GPIO[15]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (1461) gpio: GPIO[7]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (1471) gpio: GPIO[4]| InputEn: 0| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:0
I (1481) AUDIO_THREAD: The esp_periph task allocate stack on internal memory
I (1521) SDCARD: CID name SDABC!

I (1991) example.mixer: Start Wi-Fi
I (1991) AUDIO_THREAD: The audio_mixer task allocate stack on external memory
I (1991) AUDIO_MIXER: Audio mixer initialized, 0x3c1477dc
I (1991) main_task: Returned from app_main()
I (2001) pp: pp rom version: e7ae62f
I (2001) net80211: net80211 rom version: e7ae62f
I (2021) wifi:wifi driver task: 3fcbbf84, prio:23, stack:6656, core=0
I (2021) wifi:wifi firmware version: b2f1f86
I (2021) wifi:wifi certification version: v7.0
I (2021) wifi:config NVS flash: enabled
I (2021) wifi:config nano formating: disabled
I (2031) wifi:Init data frame dynamic rx buffer num: 32
I (2031) wifi:Init management frame dynamic rx buffer num: 32
I (2041) wifi:Init management short buffer num: 32
I (2041) wifi:Init static tx buffer num: 16
I (2051) wifi:Init tx cache buffer num: 32
I (2051) wifi:Init static tx FG buffer num: 2
I (2061) wifi:Init static rx buffer size: 1600
I (2061) wifi:Init static rx buffer num: 10
I (2061) wifi:Init dynamic rx buffer num: 32
I (2071) wifi_init: rx ba win: 6
I (2071) wifi_init: tcpip mbox: 32
I (2081) wifi_init: udp mbox: 6
I (2081) wifi_init: tcp mbox: 6
I (2081) wifi_init: tcp tx win: 5744
I (2091) wifi_init: tcp rx win: 5744
I (2091) wifi_init: tcp mss: 1440
I (2101) wifi_init: WiFi/LWIP prefer SPIRAM
I (2101) wifi_init: WiFi IRAM OP enabled
I (2101) wifi_init: WiFi RX IRAM OP enabled
I (2111) wifi:Set ps type: 1

I (2111) phy_init: phy_version 601,fe52df4,May 10 2023,17:26:54
I (2191) wifi:mode : sta (7c:df:a1:e7:88:ac)
I (2191) wifi:enable tsf
```

- At this point, I can use the 'join' command to connect to the Wi-Fi AP. The SSID of the wireless router is 'esp32', and the password is 'esp123456'. After executing the command, the log will show that the device has successfully obtained an IP address. The printout is as follows:
```c
esp32> join esp32 esp123456
I (101941) example.mixer: Connecting Wi-Fi, SSID:"esp32" PASSWORD:"esp123456"
esp32> I (103171) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (104571) wifi:state: init -> auth (b0)
I (104601) wifi:state: auth -> assoc (0)
I (104631) wifi:state: assoc -> run (10)
I (104661) wifi:connected with esp32, aid = 37, channel 11, BW20, bssid = ec:56:23:e9:7e:f0
I (104661) wifi:security: WPA2-PSK, phy: bgn, rssi: -39
I (104661) wifi:pm start, type: 1

I (104671) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
W (104681) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (104691) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (105681) esp_netif_handlers: sta ip: 192.168.3.9, mask: 255.255.255.0, gw: 192.168.3.1
I (105681) PERIPH_WIFI: Got ip:192.168.3.9
```
- Use 'pmixer' to play audio. The first parameter is the URL, and the second parameter is the selected channel. For example, to play audio from an SD card, you can use the command 'pmixer /sdcard/test.mp3'. The printout is as follows:
```c
esp32> pmixer /sdcard/test.mp3 0
I (275681) ESP_DECODER: esp_decoder_init, stack size is 10240
I (275681) MIXER_PIPE: One pipeline has been created, uri:/sdcard/test.mp3, pipe:0x3c157df0, mixer:0x3c157788
I (275691) AUDIO_THREAD: The mixer_manager task allocate stack on external memory
I (275701) MIXER_PIPE: Mixer pipeline:0x3c157788
esp32> I (275711) AUDIO_THREAD: The mixing_in task allocate stack on internal memory
I (275711) AUDIO_ELEMENT: [mixing_in-0x3c1577b8] Element task created
I (275721) AUDIO_THREAD: The mixing_dec task allocate stack on external memory
I (275721) AUDIO_ELEMENT: [mixing_dec-0x3c1579a0] Element task created
I (275741) AUDIO_THREAD: The mixing_filter task allocate stack on external memory
I (275741) AUDIO_ELEMENT: [mixing_filter-0x3c157cd0] Element task created
I (275751) AUDIO_ELEMENT: [mixing_raw-0x3c157ad0] Element task created
I (275761) AUDIO_ELEMENT: [mixing_in] AEL_MSG_CMD_RESUME,state:1
I (275761) AUDIO_ELEMENT: [mixing_dec] AEL_MSG_CMD_RESUME,state:1
I (275771) ESP_DECODER: Ready to do audio type check, pos:0, (line 104)
I (275771) AUDIO_ELEMENT: [mixing_filter] AEL_MSG_CMD_RESUME,state:1
I (275801) MIXER_PIPE: [CMD] START Mixer ch[0] mixer[0x3c157788]
I (275801) MIXER_PIPE: Mixer manager REC CMD:5,dat:0 src:0x3c157788, Mixer:0x3c157788
I (275831) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c157ad0, Mixer:0x3c157788
I (275831) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c157ad0, Mixer:0x3c157788
I (275941) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 16000, channel of destination data : 1
I (275951) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c157cd0, Mixer:0x3c157788
I (275771) FATFS_STREAM: File size: 44873 byte, file position: 0
I (275961) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c1577b8, Mixer:0x3c157788
I (275981) ESP_DECODER: Detect audio type is MP3
I (275981) MP3_DECODER: MP3 opened
I (275991) AUDIO_MIXER: audio_mixer_open
I (275991) MIXER_PIPE: [STATUS] Mixer pipeline is running, Mixer:0x3c157788
I (276011) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c1579a0, Mixer:0x3c157788
I (276011) MIXER_PIPE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (276021) MIXER_PIPE: Mixer manager REC CMD:9,dat:0 src:0x3c1579a0, Mixer:0x3c157788
I (276141) AUDIO_MIXER: Going into mixing
I (276141) AUDIO_MIXER: audio_mixer_open
I (276281) AUDIO_MIXER: Reopen the audip mixer
```

- If you need to insert a new audio, you can use 'pmixer' and switch to a different channel. For example, to play a network audio, you can use the command 'pmixer https/example.mp3 1'.
```c
pmixer https://test.mp3 1
I (79441) ESP_DECODER: esp_decoder_init, stack size is 10240
I (79441) MIXER_PIPE: One pipeline has been created, uri:https://test.mp3, pipe:0x3c1634c0, mixer:0x3c162c28
I (79451) AUDIO_THREAD: The mixer_manager task allocate stack on external memory
I (79461) MIXER_PIPE: Mixer pipeline:0x3c162c28
esp32> I (79471) AUDIO_THREAD: The mixing_in task allocate stack on external memory
I (79471) AUDIO_ELEMENT: [mixing_in-0x3c162edc] Element task created
I (79481) AUDIO_THREAD: The mixing_dec task allocate stack on external memory
I (79481) AUDIO_ELEMENT: [mixing_dec-0x3c1630fc] Element task created
I (79491) AUDIO_THREAD: The mixing_filter task allocate stack on external memory
I (79511) AUDIO_ELEMENT: [mixing_filter-0x3c1633a0] Element task created
I (79511) AUDIO_ELEMENT: [mixing_raw-0x3c16322c] Element task created
I (79521) AUDIO_ELEMENT: [mixing_in] AEL_MSG_CMD_RESUME,state:1
I (79521) AUDIO_ELEMENT: [mixing_dec] AEL_MSG_CMD_RESUME,state:1
I (79531) ESP_DECODER: Ready to do audio type check, pos:0, (line 104)
I (79541) AUDIO_ELEMENT: [mixing_filter] AEL_MSG_CMD_RESUME,state:1
I (79561) MIXER_PIPE: [CMD] START Mixer ch[1] mixer[0x3c162c28]
I (79571) MIXER_PIPE: Mixer manager REC CMD:5,dat:0 src:0x3c162c28, Mixer:0x3c162c28
I (79571) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c16322c, Mixer:0x3c162c28
I (79591) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c16322c, Mixer:0x3c162c28
I (79711) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 16000, channel of destination data : 1
I (79731) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c1633a0, Mixer:0x3c162c28
I (80431) wifi:<ba-add>idx:0 (ifx:0, ec:56:23:e9:7e:f0), tid:0, ssn:10, winSize:64
I (80471) HTTP_STREAM: total_bytes=11437
I (80471) MIXER_PIPE: Mixer manager REC CMD:10,dat:0 src:0x3c162edc, Mixer:0x3c162c28
W (80471) HTTP_STREAM: No more data,errno:0, total_bytes:11437, rlen = 0
I (80471) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c162edc, Mixer:0x3c162c28
I (80481) AUDIO_ELEMENT: IN-[mixing_in] AEL_IO_DONE,0
I (80471) ESP_DECODER: Detect audio type is MP3
I (80511) MP3_DECODER: MP3 opened
I (80501) MIXER_PIPE: Mixer manager REC CMD:11,dat:1008123716 src:0x3c162edc, Mixer:0x3c162c28
I (80521) MIXER_PIPE: Mixer manager REC CMD:8,dat:15 src:0x3c162edc, Mixer:0x3c162c28
I (80521) MIXER_PIPE: [STATUS] Mixer pipeline is running, Mixer:0x3c162c28
I (80521) AUDIO_MIXER: Going into mixing
I (80541) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c1630fc, Mixer:0x3c162c28
I (80551) MIXER_PIPE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (80581) MIXER_PIPE: Mixer manager REC CMD:9,dat:0 src:0x3c1630fc, Mixer:0x3c162c28
I (81771) AUDIO_ELEMENT: IN-[mixing_dec] AEL_IO_DONE,-2
I (82831) MP3_DECODER: Closed
I (82831) MIXER_PIPE: Mixer manager REC CMD:11,dat:1008090768 src:0x3c1630fc, Mixer:0x3c162c28
I (82831) MIXER_PIPE: [STATUS] Mixer pipeline is stopped or finished, el:15, Mixer:0x3c162c28
W (82841) AUDIO_ELEMENT: [mixing_in] Element already stopped
W (82851) AUDIO_ELEMENT: [mixing_dec] Element already stopped
W (82861) AUDIO_ELEMENT: OUT-[mixing_filter] AEL_IO_ABORT
I (82871) MIXER_PIPE: Mixer manager REC CMD:8,dat:15 src:0x3c1630fc, Mixer:0x3c162c28
I (82871) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c162edc, Mixer:0x3c162c28
I (82881) MIXER_PIPE: [STATUS] Mixer pipeline is stopped or finished, el:14, Mixer:0x3c162c28
W (82891) AUDIO_PIPELINE: Without stop, st:1
W (82901) AUDIO_PIPELINE: Without wait stop, st:1
I (82901) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c1630fc, Mixer:0x3c162c28
I (82901) AUDIO_MIXER: No input data, will be blocked
I (82911) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c16322c, Mixer:0x3c162c28
I (82921) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c1633a0, Mixer:0x3c162c28
```

- If you want to continue playing on the previous channel, you can use 'rmixer' to resume playback. The first parameter is the previous channel, for example, 'rmixer 0'.
```c
esp32> rpmixer 0
esp32> I (151571) AUDIO_ELEMENT: [mixing_in] AEL_MSG_CMD_RESUME,state:1
I (151581) AUDIO_ELEMENT: [mixing_dec] AEL_MSG_CMD_RESUME,state:1
I (151581) ESP_DECODER: Ready to do audio type check, pos:0, (line 104)
I (151581) AUDIO_ELEMENT: [mixing_filter] AEL_MSG_CMD_RESUME,state:1
I (151601) MIXER_PIPE: [CMD] START Mixer ch[0] mixer[0x3c157788]
I (151601) MIXER_PIPE: Mixer manager REC CMD:5,dat:0 src:0x3c157788, Mixer:0x3c157788
I (151621) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c157ad0, Mixer:0x3c157788
I (151741) RSP_FILTER: sample rate of source data : 44100, channel of source data : 2, sample rate of destination data : 16000, channel of destination data : 1
I (151751) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c157cd0, Mixer:0x3c157788
I (151581) FATFS_STREAM: File size: 9660 byte, file position: 0
I (151771) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c1577b8, Mixer:0x3c157788
I (151781) ESP_DECODER: Detect audio type is MP3
I (151781) MP3_DECODER: MP3 opened
I (151791) AUDIO_MIXER: Going into mixing
I (151791) MIXER_PIPE: [STATUS] Mixer pipeline is running, Mixer:0x3c157788
I (151801) MIXER_PIPE: Mixer manager REC CMD:8,dat:12 src:0x3c1579a0, Mixer:0x3c157788
I (151811) MIXER_PIPE: [ * ] Received music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
I (151831) MIXER_PIPE: Mixer manager REC CMD:9,dat:0 src:0x3c1579a0, Mixer:0x3c157788
W (151861) FATFS_STREAM: No more data, ret:0
I (151861) AUDIO_ELEMENT: IN-[mixing_in] AEL_IO_DONE,0
I (151861) MIXER_PIPE: Mixer manager REC CMD:9,dat:0 src:0x3c1577b8, Mixer:0x3c157788
I (151881) MIXER_PIPE: Mixer manager REC CMD:8,dat:15 src:0x3c1577b8, Mixer:0x3c157788
I (152631) AUDIO_ELEMENT: IN-[mixing_dec] AEL_IO_DONE,-2
I (153621) MP3_DECODER: Closed
I (153621) MIXER_PIPE: Mixer manager REC CMD:11,dat:1008091788 src:0x3c1579a0, Mixer:0x3c157788
I (153621) MIXER_PIPE: [STATUS] Mixer pipeline is stopped or finished, el:15, Mixer:0x3c157788
W (153641) AUDIO_ELEMENT: [mixing_in] Element already stopped
W (153651) AUDIO_ELEMENT: [mixing_dec] Element already stopped
I (153651) MIXER_PIPE: Mixer manager REC CMD:8,dat:15 src:0x3c1579a0, Mixer:0x3c157788
I (153661) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c1577b8, Mixer:0x3c157788
I (153671) MIXER_PIPE: [STATUS] Mixer pipeline is stopped or finished, el:14, Mixer:0x3c157788
W (153681) AUDIO_PIPELINE: Without stop, st:1
W (153681) AUDIO_PIPELINE: Without wait stop, st:1
I (153691) AUDIO_MIXER: No input data, will be blocked
I (153691) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c1579a0, Mixer:0x3c157788
I (153701) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c157ad0, Mixer:0x3c157788
I (153721) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c157cd0, Mixer:0x3c157788
```
- If you want to pause the audio on a specific channel, you can use 'smixer' to pause. The first parameter is the channel you want to pause, for example, 'smixer 0'.
```c
esp32> smixer 0
I (210571) MIXER_PIPE: [CMD] STOP Mixer ch[2], Mixer:0x3c16bfe4
W (210571) AUDIO_ELEMENT: OUT-[mixing_filter] AEL_IO_ABORT
I (210571) MP3_DECODER: Closed
W (210571) AUDIO_ELEMENT: OUT-[mixing_in] AEL_IO_ABORT
I (210581) MIXER_PIPE: Mixer manager REC CMD:3,dat:0 src:0x3c16bfe4, Mixer:0x3c16bfe4
I (210591) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c16bb14, Mixer:0x3c16bfe4
I (210601) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c16bbf0, Mixer:0x3c16bfe4
I (210601) AUDIO_MIXER: No input data, will be blocked
I (210611) MIXER_PIPE: Mixer manager REC CMD:11,dat:1008226396 src:0x3c163f28, Mixer:0x3c16bfe4
I (210621) MIXER_PIPE: [STATUS] Mixer pipeline is stopped or finished, el:14, Mixer:0x3c16bfe4
W (210641) AUDIO_PIPELINE: Without stop, st:1
W (210641) AUDIO_PIPELINE: Without wait stop, st:1
esp32> I (210651) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c163f28, Mixer:0x3c16bfe4
I (210651) MIXER_PIPE: Mixer manager REC CMD:9,dat:0 src:0x3c16c014, Mixer:0x3c16bfe4
I (210661) MIXER_PIPE: Mixer manager REC CMD:8,dat:14 src:0x3c16c014, Mixer:0x3c16bfe4
```
- If you want to destroy the audio on a specific channel, you can use 'dmixer' to do so. The first parameter is the channel you want to destroy, for example, 'dmixer 0'.
```c
esp32> dmixer 0
I (230031) MIXER_PIPE: Waiting the task destroied...
I (230031) MIXER_PIPE: mixer pipeline destory
W (230041) AUDIO_ELEMENT: [mixing_in] Element has not create when AUDIO_ELEMENT_TERMINATE
W (230051) AUDIO_ELEMENT: [mixing_dec] Element has not create when AUDIO_ELEMENT_TERMINATE
I (230061) CODEC_ELEMENT_HELPER: The element is 0x3c1579a0. The reserve data 2 is 0x0.
W (230071) AUDIO_ELEMENT: [mixing_filter] Element has not create when AUDIO_ELEMENT_TERMINATE
W (230091) AUDIO_ELEMENT: [mixing_raw] Element has not create when AUDIO_ELEMENT_TERMINATE
```

## Troubleshooting

- To run the `stat` command, please go to `menuconfig > Component Config > FreeRTOS > Enable FreeRTOS to collect run time stats` and enable `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`.
- To run the `tasklist` command, please go to `menuconfig > Component Config > FreeRTOS > Enable FreeRTOS trace facility and Enable FreeRTOS stats formatting functions` and enable `CONFIG_FREERTOS_USE_TRACE_FACILITY`.
- To run AAC decoders, please set `CONFIG_FREERTOS_HZ` to 1000 Hz.
- If run `pmixer` command fails, the corresponding `slot` will also be occupied.
- ESP32-S2-kaluga-1 does not support microSD card.
- When mixing, it is recommended to use audio formats with the same sample rate (such as the current default configuration of 44.1KHZ) to avoid the phenomenon of audio stuttering caused by excessive sampling and increasing CPU load


## Technical Support and Feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.