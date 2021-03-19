# ESP-ADF WiFi, A2DP, HFP and BLE Coexistence Demo

The example demonstrates the coexistence of Wi-Fi, A2DP, HFP and BLE.

This demo creates several gatt services and starts ADV. The ADV name is BLUFI_DEVICE, then waits to be connected. The device can be configured to connect the wifi with the blufi service. When wifi is connected, the demo will create http stream module. It can play music with http url.

The classic bluetooth A2DP part of the demo implements Advanced Audio Distribution Profile to receive an audio stream. After the program is started, ohther bluetooth devices such as smart phones can discover a device named `ESP_ADF_COEX_EXAMPLE`. Once a connection is estalished, use AVRCP profile to control bluetooth music state.

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

#### Build and Flash
You can use `GNU make` or `cmake` to build the project.
If you are using make:
```bash
cd $ADF_PATH/examples/advanced_examples/wifi_bt_ble_coex
make clean
make menuconfig
make -j4 all
```

Or if you are using cmake:
```bash
cd $ADF_PATH/examples/advanced_examples/wifi_bt_ble_coex
idf.py fullclean
idf.py menuconfig
idf.py build
```
The firmware downloading flash address refer to follow table.

|Flash address | Bin Path|
|---|---|
|0x1000 | build/bootloader/bootloader.bin|
|0x8000 | build/partitiontable/partition-table.bin|
|0x10000 | build/wifi_bt_ble_coex.bin|

To exit the serial monitor, type ``Ctrl-]``.

## 3. Example Output

After download the follow logs should be output, here:

```
I (73) boot: Chip Revision: 3
I (33) boot: ESP-IDF v3.3.2-107-g722043f 2nd stage bootloader
I (33) boot: compile time 10:59:20
I (33) boot: Enabling RNG early entropy source...
I (39) boot: SPI Speed      : 40MHz
I (43) boot: SPI Mode       : DIO
I (47) boot: SPI Flash Size : 4MB
I (51) boot: Partition Table:
I (54) boot: ## Label            Usage          Type ST Offset   Length
I (62) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (69) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (77) boot:  2 factory          factory app      00 00 00010000 00300000
I (84) boot: End of partition table
I (88) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x50210 (328208) map
I (212) esp_image: segment 1: paddr=0x00060238 vaddr=0x3ffbdb60 size=0x04048 ( 16456) load
I (219) esp_image: segment 2: paddr=0x00064288 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (220) esp_image: segment 3: paddr=0x00064690 vaddr=0x40080400 size=0x0b980 ( 47488) load
I (248) esp_image: segment 4: paddr=0x00070018 vaddr=0x400d0018 size=0x13650c (1271052) map
0x400d0018: _flash_cache_start at ??:?

I (694) esp_image: segment 5: paddr=0x001a652c vaddr=0x4008bd80 size=0x0a964 ( 43364) load
0x4008bd80: r_lld_evt_restart at ??:?

I (726) boot: Loaded app from partition at offset 0x10000
I (726) boot: Disabling RNG early entropy source...
I (726) psram: This chip is ESP32-D0WD
I (731) spiram: Found 64MBit SPI RAM device
I (735) spiram: SPI RAM mode: flash 40m sram 40m
I (741) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (748) cpu_start: Pro cpu up.
I (752) cpu_start: Application information:
I (757) cpu_start: Project name:     esp-idf
I (761) cpu_start: App version:      v1.0-755-gaae9a8d
I (767) cpu_start: Compile time:     Mar 16 2021 10:59:16
I (773) cpu_start: ELF file SHA256:  aa7edeaa508334ff...
I (779) cpu_start: ESP-IDF:          v3.3.2-107-g722043f
I (785) cpu_start: Starting app cpu, entry point is 0x40081390
0x40081390: call_start_cpu1 at /home/zhanghu/esp/esp-adf-internal/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (1679) spiram: SPI SRAM memory test OK
I (1681) heap_init: Initializing. RAM available for dynamic allocation:
I (1682) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1686) heap_init: At 3FFB7468 len 00000B98 (2 KiB): DRAM
I (1692) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1698) heap_init: At 3FFBDB5C len 00000004 (0 KiB): DRAM
I (1704) heap_init: At 3FFC4020 len 0001BFE0 (111 KiB): DRAM
I (1711) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1717) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1723) heap_init: At 400966E4 len 0000991C (38 KiB): IRAM
I (1730) cpu_start: Pro cpu start user code
I (1735) spiram: Adding pool of 4068K of external SPI memory to heap allocator
I (79) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (81) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (131) COEX_EXAMPLE: [ 1 ] Create coex handle for a2dp-gatt-wifi
I (131) COEX_EXAMPLE: [ 2 ] Initialize peripherals
I (131) COEX_EXAMPLE: [ 3 ] create and start input key service
I (141) COEX_EXAMPLE: [ 4 ] Start a2dp and blufi network
W (161) PERIPH_TOUCH: _touch_init
I (161) COEX_EXAMPLE: [4.1] Init Bluetooth
I (311) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 0
E (821) BT_BTM: btm_sco_process_num_bufs, 4
I (841) COEX_EXAMPLE: [4.2] init gatts
I (841) COEX_EXAMPLE: Blufi module init
E (841) DISPATCHER: exe first list: 0x0
I (861) COEX_EXAMPLE: [4.3] Create Bluetooth peripheral
I (861) COEX_EXAMPLE: [4.4] Start peripherals
I (861) COEX_EXAMPLE: [4.5] init hfp_stream
I (871) wifi:wifi driver task: 3ffdc42c, prio:23, stack:3584, core=0
I (901) wifi:wifi firmware version: 5f8804c
I (901) wifi:config NVS flash: enabled
I (901) wifi:config nano formating: disabled
I (901) wifi:Init dynamic tx buffer num: 32
I (901) wifi:Init data frame dynamic rx buffer num: 64
I (911) wifi:Init management frame dynamic rx buffer num: 64
I (911) wifi:Init management short buffer num: 32
I (921) wifi:Init static tx buffer num: 32
I (921) wifi:Init static rx buffer size: 2212
I (931) wifi:Init static rx buffer num: 16
I (931) wifi:Init dynamic rx buffer num: 64
I (951) wifi:mode : sta (fc:f5:c4:37:71:10)
E (971) gpio: gpio_install_isr_service(412): GPIO isr service already installed

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.6.6-10-gf944a6b-aca0d7f-2d83f7a               |
|                     Compile date: Mar 15 2021-11:14:21                     |
------------------------------------------------------------------------------
I (1041) COEX_EXAMPLE: Func:setup_player, Line:299, MEM Total:4154600 Bytes, Inter:104896 Bytes, Dram:65736 Bytes

I (1041) COEX_EXAMPLE: esp_audio instance is:0x3f815988

I (1061) COEX_EXAMPLE: [ 5 ] Create audio pipeline for playback
I (1061) COEX_EXAMPLE: [5.1] Create i2s stream to read data from codec chip
W (1061) I2S: I2S driver already installed
I (1071) COEX_EXAMPLE: [5.2] Create hfp stream
I (1071) COEX_EXAMPLE: [5.3] Register i2s reader and hfp outgoing to audio pipeline
I (1081) COEX_EXAMPLE: [5.4] Link it together [codec_chip]-->i2s_stream_reader-->filter-->hfp_out_stream-->[Bluetooth]
I (1091) COEX_EXAMPLE: [5.5] Start audio_pipeline out
I (2781) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1
I (3761) wifi:state: init -> auth (b0)
I (3771) wifi:state: auth -> assoc (0)
I (3781) wifi:state: assoc -> run (10)
I (3811) wifi:connected with ESP-Audio, aid = 8, channel 6, BW20, bssid = ac:22:0b:d2:ee:58
I (3811) wifi:security type: 4, phy: bgn, rssi: -57
I (3811) wifi:pm start, type: 1

I (3841) wifi:AP's beacon interval = 102400 us, DTIM period = 3
W (4631) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (4731) COEX_EXAMPLE: WIFI_CONNECTED
E (235321) BT_APPL: rcb[0].lidx=3, lcb.conn_msk=x1
E (235371) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (235371) COEX_EXAMPLE: A2DP sink user cb
W (235461) BT_APPL: new conn_srvc id:27, app_id:1
I (235721) COEX_EXAMPLE: A2DP sink user cb
I (235721) COEX_EXAMPLE: User cb A2DP event: 2
W (236041) BT_APPL: new conn_srvc id:19, app_id:0
I (236051) COEX_EXAMPLE: A2DP sink user cb
I (236051) COEX_EXAMPLE: A2DP connected

```

Load and run the example:

- Connect with Bluetooth on your smartphone to the audio board identified as `ESP_ADF_COEX_EXAMPLE`.
- Conect with `ESPBlufi` APP on your smartphone to the ble device named "BLUFI_DEVICE", then configure the ssid and password to connect the Wi-Fi.
- Press button `mode` to enter BT mode or WIFI mode, the audio board can play the music from http and a2dp stream.

## 4. Troubleshooting

- Enable bluetooth dual mode for a2dp and gatts.

- Enable ble auto latency, used to enhance classic bt performance while br/edr and ble are enabled at the same time.

- Enable PSRAM for coex-demo.

The `sdkconfig.defaults` is recommended.
