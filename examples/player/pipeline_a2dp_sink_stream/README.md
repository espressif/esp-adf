# ESP-ADF A2DP-SINK-Stream example

Example of A2DP audio sink.
This is the demo of API implementing Advanced Audio Distribution Profile to receive an audio stream.
This example involves the use of Bluetooth legacy profile A2DP for audio stream reception, AVRCP for media information notifications.
Applications such as bluetooth speakers can take advantage of this example as a reference of basic functionalities.

## 1. How to use example

#### Hardware Required

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "Compatible") |


## 2. Setup software environment

Please refer to [Get Started](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#get-started).

#### Configure the project
cmake menuconfig:
```
idf.py menuconfig
```
Or use legacy GNU make:
```
make menuconfig
```
Select compatible audio board in ``menuconfig > Audio HAL``, then compile the example.
Enable Classic Bluetooth and A2DP under Component config --> Bluetooth --> Bluedroid Enable.

#### Build and Flash
You can use `GNU make` or `cmake` to build the project.
If you are using make:
```bash
cd $ADF_PATH/examples/player/pipeline_a2dp_sink_stream
make clean
make menuconfig
make -j4 all
```

Or if you are using cmake:
```bash
cd $ADF_PATH/examples/player/pipeline_a2dp_sink_stream
idf.py fullclean
idf.py menuconfig
idf.py build
```
The firmware downloading flash address refer to follow table.

|Flash address | Bin Path|
|---|---|
|0x1000 | build/bootloader/bootloader.bin|
|0x8000 | build/partitiontable/partition-table.bin|
|0x10000 | build/bt_sink_demo.bin|

To exit the serial monitor, type ``Ctrl-]``.

## 3. Example Output

After download the follow logs should be output, here:

```
I (29) boot: ESP-IDF v4.2-47-g2532ddd-dirty 2nd stage bootloader
I (29) boot: compile time 11:56:15
I (29) boot: chip revision: 1
I (33) boot_comm: chip revision: 1, min. bootloader chip revision: 0
I (40) boot.esp32: SPI Speed      : 40MHz
I (45) boot.esp32: SPI Mode       : DIO
I (49) boot.esp32: SPI Flash Size : 2MB
I (54) boot: Enabling RNG early entropy source...
I (59) boot: Partition Table:
I (63) boot: ## Label            Usage          Type ST Offset   Length
I (70) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (78) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (85) boot:  2 factory          factory app      00 00 00010000 00124f80
I (93) boot: End of partition table
I (97) boot_comm: chip revision: 1, min. application chip revision: 0
I (104) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2cc78 (183416) map
I (183) esp_image: segment 1: paddr=0x0003cca0 vaddr=0x3ffbdb60 size=0x03378 ( 13176) load
I (188) esp_image: segment 2: paddr=0x00040020 vaddr=0x400d0020 size=0xae1d4 (713172) map
0x400d0020: _stext at ??:?

I (461) esp_image: segment 3: paddr=0x000ee1fc vaddr=0x3ffc0ed8 size=0x001a8 (   424) load
I (461) esp_image: segment 4: paddr=0x000ee3ac vaddr=0x40080000 size=0x00404 (  1028) load
0x40080000: _WindowOverflow4 at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/freertos/xtensa/xtensa_vectors.S:1730

I (468) esp_image: segment 5: paddr=0x000ee7b8 vaddr=0x40080404 size=0x16e4c ( 93772) load
I (530) boot: Loaded app from partition at offset 0x10000
I (530) boot: Disabling RNG early entropy source...
I (531) cpu_start: Pro cpu up.
I (534) cpu_start: Application information:
I (539) cpu_start: Project name:     bt_sink_demo
I (544) cpu_start: App version:      v2.2-36-gdcbdc7b-dirty
I (551) cpu_start: Compile time:     Dec 23 2020 11:56:29
I (557) cpu_start: ELF file SHA256:  ca5e58274e21d640...
I (563) cpu_start: ESP-IDF:          v4.2-47-g2532ddd-dirty
I (569) cpu_start: Starting app cpu, entry point is 0x400815e4
0x400815e4: call_start_cpu1 at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (579) heap_init: Initializing. RAM available for dynamic allocation:
I (586) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (592) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (598) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (605) heap_init: At 3FFCF100 len 00010F00 (67 KiB): DRAM
I (611) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (617) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (623) heap_init: At 40097250 len 00008DB0 (35 KiB): IRAM
I (630) cpu_start: Pro cpu start user code
I (648) spi_flash: detected chip: gd
I (649) spi_flash: flash io: dio
W (649) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (659) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (731) BLUETOOTH_EXAMPLE: [ 1 ] Init Bluetooth
I (731) BTDM_INIT: BT controller compile version [3723d5b]
I (731) system_api: Base MAC address is not set
I (741) system_api: read default base MAC address from EFUSE
I (841) phy: phy_version: 4500, 0cd6843, Sep 17 2020, 15:37:07, 0, 0
I (1431) BLUETOOTH_EXAMPLE: [ 2 ] Start codec chip
I (1431) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
I (1461) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (1461) ES8388_DRIVER: init,out:02, in:00
I (1471) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (1471) BLUETOOTH_EXAMPLE: [ 3 ] Create audio pipeline for playback
I (1471) BLUETOOTH_EXAMPLE: [4] Create i2s stream to write data to codec chip
I (1481) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1491) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1521) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1521) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1521) BLUETOOTH_EXAMPLE: [4.1] Get Bluetooth stream
W (1531) BT_BTC: A2DP Enable with AVRC
I (1551) BLUETOOTH_EXAMPLE: [4.2] Register all elements to audio pipeline
I (1551) BLUETOOTH_EXAMPLE: [4.3] Link it together [Bluetooth]-->bt_stream_reader-->i2s_stream_writer-->[codec_chip]
I (1561) AUDIO_PIPELINE: link el->rb, el:0x3ffd830c, tag:bt, rb:0x3ffd87c0
I (1561) BLUETOOTH_EXAMPLE: [ 5 ] Initialize peripherals
E (1571) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1581) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
I (1591) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3 
I (1601) BLUETOOTH_EXAMPLE: [ 5.1 ] Create and start input key service
I (1601) BLUETOOTH_EXAMPLE: [5.2] Create Bluetooth peripheral
W (1621) PERIPH_TOUCH: _touch_init
I (1621) BLUETOOTH_EXAMPLE: [5.3] Start all peripherals
I (1631) BLUETOOTH_EXAMPLE: [ 6 ] Set up  event listener
I (1631) BLUETOOTH_EXAMPLE: [6.1] Listening event from all elements of pipeline
I (1641) BLUETOOTH_EXAMPLE: [ 7 ] Start audio_pipeline
I (1651) AUDIO_ELEMENT: [bt-0x3ffd830c] Element task created
I (1661) AUDIO_ELEMENT: [i2s-0x3ffd7fec] Element task created
I (1661) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:134064 Bytes

I (1681) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (1681) I2S_STREAM: AUDIO_STREAM_WRITER
I (1681) AUDIO_PIPELINE: Pipeline started
I (1681) BLUETOOTH_EXAMPLE: [ 8 ] Listen for all pipeline events
E (23161) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (23171) A2DP_STREAM: A2DP bd address:, [e4:9a:dc:62:2d:d0]
I (23171) A2DP_STREAM: AVRC conn_state evt: state 1, [e4:9a:dc:62:2d:d0]
I (23381) A2DP_STREAM: AVRC register event notification: 13, param: 0x0
I (23381) A2DP_STREAM: A2DP audio stream configuration, codec type 0
I (23381) A2DP_STREAM: Bluetooth configured, sample rate=44100
I (23391) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=44100, bits=16, ch=2
I (23421) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_PAUSE
I (23461) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (23461) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:3
I (23471) I2S_STREAM: AUDIO_STREAM_WRITER
I (23761) A2DP_STREAM: AVRC remote features 25b, CT features 2
W (23821) BT_APPL: new conn_srvc id:19, app_id:0
I (23831) A2DP_STREAM: A2DP bd address:, [e4:9a:dc:62:2d:d0]
I (23831) A2DP_STREAM: A2DP connection state =  CONNECTED
I (23961) A2DP_STREAM: AVRC set absolute volume: 54%
I (23961) A2DP_STREAM: Volume is set by remote controller 54%

I (24361) A2DP_STREAM: AVRC remote features 25b, CT features 2
I (36401) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

```

Load and run the example:

- Connect with Bluetooth on your smartphone to the audio board identified as "ESP_SINK_STREAM_DEMO"
- Play some audio from the smartphone and it will be transmitted over Bluetooth to the audio bard.
- The audio playback may be controlled from the smartphone, as well as from the audio board. The following controls may be used:

    |   Smartphone   | Audio Board |
    |:--------------:|:-----------:|
    |   Play Music   |    [Play]   |
    |   Stop Music   |    [Set]    |
    |   Next Song    | [long Vol+] |
    | Previous Song  | [long Vol-] |
    |   Volume Up    |    [Vol+]   |
    |  Volume Down   |    [Vol-]   |

- `Volume Up` and `Volume Down` is not been supported in IDF 3.3.

## 4. Troubleshooting

- For current stage, the supported audio codec in ESP32 A2DP is SBC. SBC data stream is transmitted to A2DP sink and then decoded into PCM samples as output. The PCM data format is normally of 44.1kHz sampling rate, two-channel 16-bit sample stream. Other decoder configurations in ESP32 A2DP sink is supported but need additional modifications of protocol stack settings.

- As a usage limitation, ESP32 A2DP sink can support at most one connection with remote A2DP source devices. Also, A2DP sink cannot be used together with A2DP source at the same time, but can be used with other profiles such as SPP and HFP.
