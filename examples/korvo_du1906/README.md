# Example of ESP32-Korvo-DU1906 board

This example shows how to use ESP32-Korvo-DU1906 board working with DuHome AIOT Voice Platform (度家-AIOT语音平台).The board supports features such as:
- ASR, TTS and NLP
- Bluetooth music
- BLE Wi-Fi provisioning
- OTA
- Mesh and infrared controller

The board together with the platform provide easy way to develop a smart speaker or AIOT device.

# How to use example

## Hardware Required

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/no-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/no-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/no-button.png "Compatible") |
| ESP32-Du1906 | ![alt text](../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DUL1906") | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | ![alt text](../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit") | <img src="../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../docs/_static/no-button.png "Compatible") |

## Setup software environment

Please refer to [Get Started](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/index.html#get-started).

## Authentication code

Please refer to [度家 AIOT 快速入门](https://cloud.baidu.com/doc/SHC/s/wk7bl9g8i) and apply for factory code (fc), product Key (pk), access key (ak) and secret key (sk) that should be then saved in `profiles/profile.bin`

## Jumpstart the example
No need to compile the project, just use the firmware in this example.

The firmware downloading flash address refer to follow table.

Flash address | Bin Path
---|---
0x1000 | bootloader.bin
0x8000 | partitions.bin
0x10000 | app.bin
0x510000 | DU1906_slave_v1.4.8.E.bin
0x790000 | profile.bin
0x791000 | audio_tone.bin

### Download firmware

#### Linux operating system

Run the command below:
```bash
python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 \
--port PORT --baud 921600 \
--before default_reset \
--after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect \
0x1000   ./firmware/bootloader.bin \
0x8000   ./firmware/partitions.bin \
0x10000  ./firmware/app.bin \
0x510000 ./firmware/DU1906_slave_v1.4.8.E.bin \
0x790000 ./profiles/profile.bin \
0x791000 ./tone/audio_tone.bin
```
#### Windows operating system

- **step 1:** [Download the firmware download tool](https://www.espressif.com/sites/default/files/tools/flash_download_tool_v3.8.5_0.zip) and unzip the compressed package, then run the executable file with ".exe" suffix.
- **step 2:** Choose download mode (Developer Mode)

    <img src="./docs/pictures/tool_choose_mode.jpg" height="320" width="480" alt="Tool choose mode">

- **step 3:** Choose chip (ESP32)

    <img src="./docs/pictures/tool_choose_chip.jpg" height="640" width="480" alt="Tool choose chip">

- **step 4:** open firmware directory (`$ADF_PATH/example/korvo_du1906/firmware`) and fill in the address according to the above flash address table.

    <img src="./docs/pictures/tool_download.png" height="640" width="480" alt="Tool download">

**Note: The tone bin is in `$ADF_PATH/example/korvo_du1906/tone/audio_tone.bin` and profile.bin is in `$ADF_PATH/example/korvo_du1906/profile/profile.bin`**

- **step 5:** Click `START` button on the graphical interface to download the firmware

After download firmware, press `[RST]` button, and then there will be some logs print on the serial port.

### Network configuration

- **step 1:** Download and install Blufi app on cell phone, [App for Andriod](https://github.com/EspressifApp/EspBlufiForAndroid/releases), [App for IOS](https://github.com/EspressifApp/EspBlufiForiOS/releases)
- **step 2:** Open bluetooth and open blufi app on mobilephone, scan the device.
- **step 3:** Press `[FUNC]` button on device for 4s, the device will enter wifi-setting mode, and play a tone music "请点击确认开始配网".
- **step 4:** Fresh the scan list, there will be a device named "BLUFI_DEVICE", click it and choose `[连接]` on phone.

    <img src="./docs/pictures/blufi_connect.png" height="640" width="480" alt="Blufi connect">

- **step 5:** After connect to device, click `[配网]`, input wifi ssid and password that to connect.
- **step 6:** Click `[确认]`, the device will acquire the wifi information and connect to network. If conenct to wifi successfully, the app will receive a string "hello world" and the device will play a tone music "网络连接成功".

    <img src="./docs/pictures/blufi_config.png" height="640" width="480" alt="Blufi configuration">

**Note: If configurate fails, check the above process and try again. Be careful and patient!**

### Features experience

**Note that, please make sure that there is a speaker inserts to the board at least.**

#### Voice interaction

After configurate wifi information and connect to network, you can start a conversation with a voice wake-up word "xiaodu xiaodu", such as below supported command:
- "小度小度" "在呢" "讲个笑话"
- "小度小度" "在呢" "上海天气怎么样？"
- "小度小度" "在呢" "播放一首歌"
- "小度小度" "在呢" "百度百科乐鑫信息科技"

If you need more instructions, you can define them in the background of Baidu.

#### bluetooth music

Press `[MUTE]` button for 3-5s enter BT mode, open bluetooth on your phone and connect to device named "ESP_BT_COEX_DEV", and then you can play bt music on the device.

#### Buttons usage
Name of Button | Short press | Long press
:-:|:-:|:-:
VOL + | Volume up | NA
VOL - | Volume down| NA
MUTE | Enter mute mode |Enter/Exit BT mode
FUNC | NA |Setting Wi-Fi

## Build and flash

After the experience, it's time to build the example now! you can also add some features by yourself and then build it.

### Apply patch

For now, we need to apply an IDF patch.
```bash
cd $IDF_PATH
git apply $ADF_PATH/idf_patches/idf_v3.3_freertos.patch
```

###  Menuconfig

Select the default sdkconfig for build

```bash
cp sdkconfig.defaults sdkconfig
```

### Build

You can use `GNU make` or `cmake` to build the project.
If you are using make:
```bash
cd $ADF_PATH/examples/korvo_du1906
make clean
make menuconfig
make -j4 all
```

Or if you are using cmake:
```bash
cd $ADF_PATH/examples/korvo_du1906
idf.py fullclean
idf.py menuconfig
idf.py build
```

### Downloading

Download application with make
```bash
make flash ESPPORT=PORT
```
Or if you are using cmake
```bash
idf.py flash -p PORT
```

**Note:** Replace `PORT` with USB port name where ESP32-Korvo-DU1906 board is connected to.

In addition, ESP32-Korvo-DU1906 have three more bins, `./firmware/DU1906_slave_v1.4.8.E.bin`,  `./profiles/profile.bin` and `./tone/audio-esp.bin`.
```bash
python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 \
--port PORT --baud 921600 \
--before default_reset \
--after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect \
0x510000 ./firmware/DU1906_slave_v1.4.8.E.bin \
0x790000 ./profiles/profile.bin \
0x791000 ./tone/audio_tone.bin
```

The firmware downloading flash address refer to above table in jumpstart part.

### Usage

Please refer to jumpstart part.

### Upgrade function
	
The application, flash tone, profile bins, dsp firmware and app firmware upgrade are supported. Those bins can be store on SDcard or HTTP Server, such as, "/sdcard/profile.bin", "https://github.com/espressif/esp-adf/raw/master/examples/korvo_du1906/firmware/app.bin".	

#### Upgrade strategy

We have two kind of firmware ota strategy in this example like below.
- Each partition to be upgraded has a separate URL. **(strategy 1)**
- Combine different binary bins to a whole firmware and add a description header to the firmware and just need one URL to access it. **(strategy 2)**

#### strategy 1 (default)

Before the upgration, usually compare the firmware version first, and then judge whether the firmware need to be upgraded. (Sdcard ota in this example doesn't compare firmware version).
To edit the version of firmware like below:
- App bin:  Change "version.txt" in the project directory and recompile.
- Tone bin: Use this script to assign version ``` python  $ADF_PATH/tools/audio_tone/mk_audio_tone.py -r tone/ -f components/audio_flash_tone -v v1.1.1 ```

The bin files version checking after every booting, exculde profile.bin.
User copy the profile.bin to SDCard root folder and inserted to ESP32-Korvo-DU1906 sdcard slot could be execute profile bin upgrade.

The app will wait 15s for wifi connection. If wifi connect, the ota process will start, if not, skip it.

#### strategy 2

To use ota strategy 2, some patchs should be applied first.
```bash
cd $ADF_PATH; git apply $ADF_PATH/examples/korvo_du1906/patches/adf_ota_patch.patch
cd $IDF_PATH; git apply $ADF_PATH/examples/korvo_du1906/patches/idf_ota_patch.patch
```
Use script under tools directory to assign version
```bash
cd $ADF_PATH/examples/korvo_du1906/tools/iot_ota_pkg_generator
python mk_ota_bin.py -c tda -st x.x.x -sd x.x.x -sa x.x.x -v xxx -p ../../
```
Use ``` python mk_ota_bin.py -h ``` to get more information about the script.

After execute the script, there will be a combine firmware named "combine_ota_default.bin" generated under the directory. Put the firmware on your website and update the upgrade URL.

Press button `[VOL +]` to excute ota process.

## Example Output

After download the follow logs should be output.
```c
boot: ESP-IDF v3.3.2-107-g722043f73-dirty 2nd stage bootloader
I (35) boot: compile time 11:07:53
I (35) boot: Enabling RNG early entropy source...
I (35) qio_mode: Enabling default flash chip QIO
I (36) boot: SPI Speed      : 80MHz
I (37) boot: SPI Mode       : QIO
I (37) boot: SPI Flash Size : 8MB
I (38) boot: Partition Table:
I (38) boot: ## Label            Usage          Type ST Offset   Length
I (39) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (40) boot:  1 otadata          OTA data         01 00 0000d000 00002000
I (41) boot:  2 phy_init         RF data          01 01 0000f000 00001000
I (42) boot:  3 ota_0            OTA app          00 10 00010000 00280000
I (43) boot:  4 ota_1            OTA app          00 11 00290000 00280000
I (44) boot:  5 dsp_bin          Unknown data     01 24 00510000 00280000
I (45) boot:  6 profile          Unknown data     01 29 00790000 00001000
I (46) boot:  7 flash_tone       Unknown data     01 27 00791000 00060000
I (47) boot: End of partition table
I (47) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0xa9bf8 (695288) map
I (223) esp_image: segment 1: paddr=0x000b9c20 vaddr=0x3ffbdb60 size=0x04d64 ( 19812) load
I (229) esp_image: segment 2: paddr=0x000be98c vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /home/donglianghao/esp/esp-adf-dlh/esp-idf/components/freertos/xtensa_vectors.S:1779

I (230) esp_image: segment 3: paddr=0x000bed94 vaddr=0x40080400 size=0x0127c (  4732) load
I (232) esp_image: segment 4: paddr=0x000c0018 vaddr=0x400d0018 size=0x1a29c4 (1714628) map
0x400d0018: _stext at ??:?

I (663) esp_image: segment 5: paddr=0x002629e4 vaddr=0x4008167c size=0x19720 (104224) load
0x4008167c: call_start_cpu0 at /home/donglianghao/esp/esp-adf-dlh/esp-idf/components/esp32/cpu_start.c:217

I (712) boot: Loaded app from partition at offset 0x10000
I (712) boot: Disabling RNG early entropy source...
I (712) psram: This chip is ESP32-D0WD
I (713) spiram: Found 64MBit SPI RAM device
I (713) spiram: SPI RAM mode: flash 80m sram 80m
I (714) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (715) cpu_start: Pro cpu up.
I (715) cpu_start: Application information:
I (716) cpu_start: Project name:     korvo_du1906
I (717) cpu_start: App version:      v1.1.1
I (717) cpu_start: ELF file SHA256:  d438b2d0efdff2ef...
I (718) cpu_start: ESP-IDF:          v3.3.2-107-g722043f73-dirty
I (719) cpu_start: Starting app cpu, entry point is 0x400814b4
0x400814b4: call_start_cpu1 at /home/donglianghao/esp/esp-adf-dlh/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (1199) spiram: SPI SRAM memory test OK
I (1201) heap_init: Initializing. RAM available for dynamic allocation:
I (1201) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1201) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (1202) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1203) heap_init: At 3FFBDB5C len 00000004 (0 KiB): DRAM
I (1204) heap_init: At 3FFC5F38 len 0001A0C8 (104 KiB): DRAM
I (1205) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1205) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1206) heap_init: At 4009AD9C len 00005264 (20 KiB): IRAM
I (1207) cpu_start: Pro cpu start user code
I (1208) spiram: Adding pool of 4056K of external SPI memory to heap allocator
I (99) esp_core_dump_common: Init core dump to UART
E (104) esp_core_dump_common: No core dump partition found!
I (105) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (105) spiram: Reserving pool of 18K of internal memory for DMA/internal allocations
I (127) AUDIO_THREAD: The esp_periph task allocate stack on external memory
E (128) PERIPH_SDCARD: no sdcard detect
E (2628) AUDIO_BOARD: Sdcard mount failed
I (2635) wifi:wifi driver task: 3ffcfcd8, prio:23, stack:3584, core=0
I (2635) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (2635) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (2652) wifi:wifi firmware version: 5f8804c
I (2652) wifi:config NVS flash: enabled
I (2652) wifi:config nano formating: disabled
I (2652) wifi:Init dynamic tx buffer num: 32
I (2652) wifi:Init data frame dynamic rx buffer num: 512
I (2653) wifi:Init management frame dynamic rx buffer num: 512
I (2654) wifi:Init management short buffer num: 32
I (2654) wifi:Init static tx buffer num: 16
I (2655) wifi:Init static rx buffer size: 1600
I (2655) wifi:Init static rx buffer num: 16
I (2656) wifi:Init dynamic rx buffer num: 512
I (2759) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 0
I (2760) wifi:mode : sta (fc:f5:c4:37:bf:0c)
I (3634) DU1910_APP: SMARTCONFIG wifi setting module has been selected
I (3638) WIFI_SERV: Connect to wifi ssid: ESP-Audio, pwd: esp123456
I (3818) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1
I (3818) wifi:state: init -> auth (b0)
I (3820) wifi:state: auth -> assoc (0)
I (3824) wifi:state: assoc -> run (10)
I (3841) wifi:connected with ESP-Audio, aid = 2, channel 6, BW20, bssid = ac:22:0b:d2:ee:58
I (3841) wifi:security type: 4, phy: bgn, rssi: -58
I (3846) wifi:pm start, type: 1

I (3856) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4628) event: sta ip: 192.168.1.61, mask: 255.255.255.0, gw: 192.168.1.1
I (4628) WIFI_SERV: Got ip:192.168.1.61
W (4628) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (4642) DU1910_APP: PERIPH_WIFI_CONNECTED [64]
I (4642) SNTP_INIT: ------------Initializing SNTP
I (4644) APP_OTA_UPGRADE: Create OTA service
I (4645) APP_OTA_UPGRADE: Start OTA service
I (4645) OTA_DEFAULT: data upgrade uri https://github.com/espressif/esp-adf/raw/master/examples/korvo_du1906/tone/audio_tone.bin
I (7278) HTTP_STREAM: total_bytes=166
I (9093) HTTP_STREAM: total_bytes=107950
I (9093) flashPartition: 146: label:flash_tone
I (9093) TONE_STREAM: header tag 2053, format 1
I (9094) TONE_STREAM: audio tone's tail is DFAC
E (9094) APP_OTA_UPGRADE: not audio tone bin
E (9095) OTA_SERVICE: No need to upgrade
E (9095) APP_OTA_UPGRADE: List id: 0, OTA failed, result: -1
I (9096) OTA_DEFAULT: data upgrade uri file://sdcard/profile.bin
I (9097) FATFS_STREAM: File size is 0 byte,pos:0
E (9098) FATFS_STREAM: Failed to open file /sdcard/profile.bin
E (9098) AUDIO_ELEMENT: [file] AEL_STATUS_ERROR_OPEN,-1
E (9099) OTA_DEFAULT: reader stream init failed
E (9100) OTA_SERVICE: OTA prepared fail
E (9100) APP_OTA_UPGRADE: List id: 1, OTA failed, result: 589835
I (13701) esp_https_ota: Starting OTA...
I (13701) esp_https_ota: Writing to partition subtype 17 at offset 0x290000
I (13703) OTA_DEFAULT: Running firmware version: v1.1.1, the incoming firmware version 
E (13705) OTA_DEFAULT: Got invalid version: , the version should be (V0.0.0 - V255.255.255)
E (13706) OTA_DEFAULT: Error version incoming
E (13707) OTA_SERVICE: No need to upgrade
E (13707) APP_OTA_UPGRADE: List id: 2, OTA failed, result: 589827
W (13708) OTA_SERVICE: OTA_END!
W (13709) APP_OTA_UPGRADE: upgrade finished, Please check the result of OTA
I (13710) TAS5805M: Power ON CODEC with GPIO 12
I (13710) gpio: GPIO[12]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 1| Intr:0 
I (13719) AUDIO_THREAD: The button_task task allocate stack on external memory
I (14654) TAS5805M: tas5805m_transmit_registers:  write 1677 reg done
W (14655) TAS5805M: volume = 0x44
W (14655) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
W (14658) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
I (14661) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (14661) APP_BT_INIT: Init Bluetooth module
I (14662) BTDM_INIT: BT controller compile version [3cd70f2]
I (14663) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (15020) BLE_GATTS: ble gatts module init
I (15020) APP_BT_INIT: Start Bluetooth peripherals
I (15021) AUDIO_THREAD: The media_task task allocate stack on external memory

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                         ESP_AUDIO-v1.6.2-8-g9d685b8                        |
|                     Compile date: Jul  2 2020-10:23:28                     |
------------------------------------------------------------------------------
I (15026) BT_HELPER: ap_helper_a2dp_handle_set, 42, 0x0x3f8712e4, 0x3ffc42ec
I (15026) BLE_GATTS: create attribute table successful, the number handle = 8
I (15027) MP3_DECODER: MP3 init
I (15032) BLE_GATTS: SERVICE_START_EVT, status 0, service_handle 40
I (15036) ESP_DECODER: esp_decoder_init, stack size is 5120
W (15038) TAS5805M: volume = 0x4a
I (15038) APP_PLAYER_INIT: Func:setup_app_esp_audio_instance, Line:151, MEM Total:3803080 Bytes, Inter:108960 Bytes, Dram:87904 Bytes

I (15039) APP_PLAYER_INIT: esp_audio instance is:0x3f86f760
I (15040) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (15043) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (15045) ESP32_Korvo_DU1906: I2S0, MCLK output by GPIO0
I (15045) AUDIO_THREAD: The audio_manager_task task allocate stack on external memory
I (15046) SD_HELPER: default_sdcard_player_init
I (15047) SD_HELPER: default_sdcard_player_init  playlist_create
I (15048) RAW_HELPER: default_raw_player_init :106, que:0x3ffebfdc
W (15049) TAS5805M: volume = 0x58
I (15049) BDSC_ENGINE: APP version is 4c50d0cfc76cf43e12c0b600575a5a217d66aaab
I (15050) AUDIO_PLAYER: tone play, type:203, cur media type:203
I (15051) AP_HELPER: audio_player_helper_default_play, type:203,url:flash://tone/1_boot.mp3,pos:0,block:0,auto:0,mixed:0,inter:1
W (15052) AUDIO_MANAGER: ap_manager_backup_audio_info, not found the current operations
W (15053) AUDIO_MANAGER: AP_MANAGER_PLAY, Unblock playing, type:203, auto:0, block:0
I (15058) AUDIO_THREAD: The DEC_auto task allocate stack on external memory
I (15059) ESP_DECODER: Ready to do audio type check, pos:0, (line 104)
I (15059) TONE_STREAM: Wanted read flash tone index is 1
I (15059) TONE_STREAM: Tone offset:00001a38, Tone length:39019, total num:13  pos:1

I (15070) ESP_DECODER: Detect audio type is MP3
I (15071) MP3_DECODER: MP3 opened
I (15080) RSP_FILTER: reset sample rate of source data : 44100, reset channel of source data : 1
I (15081) AUDIO_THREAD: The resample task allocate stack on external memory
I (15082) AUDIO_THREAD: The OUT_iis task allocate stack on external memory
I (15084) I2S_STREAM: AUDIO_STREAM_WRITER
I (15257) RSP_FILTER: sample rate of source data : 44100, channel of source data : 1, sample rate of destination data : 48000, channel of destination data : 2
W (15258) DU1910_APP: AUDIO_PLAYER_CALLBACK send OK, status:1, err_msg:0, media_src:203, ctx:0x0
W (15259) AUDIO_MANAGER: AP_MANAGER_PLAY, Unblock playing timeout occurred
W (15267) TAS5805M: volume = 0x58
I (15268) SNTP_INIT: ------------Initializing SNTP
I (15270) PROFILE: 55: type[0x1]
I (15270) PROFILE: 56: subtype[0x29]
I (15271) PROFILE: 57: address:0x790000
I (15271) PROFILE: 58: size:0x1000
I (15271) PROFILE: 59: label:profile
I (15286) PROFILE: fc            = your_fc
I (15286) PROFILE: pk            = your_pk
I (15287) PROFILE: ak            = your_ak
I (15287) PROFILE: sk            = your_sk
I (15288) PROFILE: mqtt_broker   =
I (15289) PROFILE: mqtt_username =
I (15290) PROFILE: mqtt_password =
I (15291) PROFILE: mqtt_cid      = 
I (15291) PROFILE: cur_version_num = 100
I (15292) PROFILE: tone_sub_ver    = 1.1.1
I (15293) PROFILE: dsp_sub_ver     = 1.1.1
I (15293) PROFILE: app_sub_ver     = 1.1.1
I (15294) BLUFI_CONFIG: Set blufi customized data: your_fc#your_pk#your_ak#your_sk, length: 56
I (15296) gpio: GPIO[27]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0 
I (15296) ESP32_GPIO_DRIVER: Initializing GPIO reset is successful.
I (15297) dsp_download_imp: esp32_spi_init...
I (15298) ESP32_SPI_DRIVER: Initializing SPI is successful,clock speed is 16MHz,mode 0.
I (15299) gpio: GPIO[38]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:1 
E (15300) gpio: gpio_install_isr_service(412): GPIO isr service already installed
esp32_intr_gpio_init 38.
I (15301) wakeup_hal: esp32_intr_gpio_init, ret:0
I (15302) BDSC_ENGINE: cuid: your_fcyour_pkyour_ak
 [1595215022.681 I (bdsc-client_adapter:bdsc_adapter_send:363)] send key=103, length=141, content=0x3fff2004
 [1595215022.682 I (bdsc-client_adapter:bdsc_adapter_start:273)] bdsc_adapter_start
 [1595215022.682 I (bdsc-client_adapter:bdsc_adapter_start:277)] dsp uploading ...
GPIO_DSP_RST is high.
I (15525) BDDSP_SPI_DRIVER: bddsp_load_bin_slave: entering...
I (15526) BDDSP_SPI_DRIVER: type: 55, load dst:5ff60000 src:60000000 storage src:00000240 len:131072.
I (15526) BDDSP_SPI_DRIVER: index :0 total:8.
md5:92B8C60C4122D986D8C166BEBF1E5F22.
I (15791) BDDSP_SPI_DRIVER: md5 wait time 78.
I (15861) BDDSP_SPI_DRIVER: type: 55, load dst:5ff80000 src:60000000 storage src:00020240 len:507904.
I (15862) BDDSP_SPI_DRIVER: index :1 total:8.
md5:806C15442352D6F7E3968195E5D16E84.
I (16898) BDDSP_SPI_DRIVER: md5 wait time 304.
I (17137) BDDSP_SPI_DRIVER: type: 55, load dst:60050000 src:60050000 storage src:0009c240 len:536576.
I (17138) BDDSP_SPI_DRIVER: index :2 total:8.
md5:E37F61A507D025162AC3AC01888491A9.
I (18231) BDDSP_SPI_DRIVER: md5 wait time 321.
I (18481) BDDSP_SPI_DRIVER: type: aa, load dst:5ff60000 src:50000000 storage src:0011f240 len:16.
I (18482) BDDSP_SPI_DRIVER: index :3 total:8.
I (18482) BDDSP_SPI_DRIVER: core boot addr: 0x5ff60000 0x50000000

I (18484) BDDSP_SPI_DRIVER: type: 55, load dst:5ff60000 src:a0000000 storage src:0011f250 len:131072.
I (18484) BDDSP_SPI_DRIVER: index :4 total:8.
md5:9F5D31CF954149A4D77539BAFEAC820A.
I (18751) BDDSP_SPI_DRIVER: md5 wait time 78.
I (18822) BDDSP_SPI_DRIVER: type: 55, load dst:5ff80000 src:a0000000 storage src:0013f250 len:507904.
I (18822) BDDSP_SPI_DRIVER: index :5 total:8.
md5:1D19794309F5AE4C5B0BE39CC85B1482.
I (19849) BDDSP_SPI_DRIVER: md5 wait time 304.
W (20028) TONE_STREAM: No more data,ret:0 ,info.byte_pos:39019
I (20082) BDDSP_SPI_DRIVER: type: 55, load dst:a0000000 src:a0000000 storage src:001bb250 len:487424.
I (20083) BDDSP_SPI_DRIVER: index :6 total:8.
md5:80CA6C2A586F552A7BA1203E0BC45281.
I (21068) BDDSP_SPI_DRIVER: md5 wait time 292.
I (21298) BDDSP_SPI_DRIVER: type: 55, load dst:60010000 src:60010000 storage src:00232250 len:195616.
I (21299) BDDSP_SPI_DRIVER: index :7 total:8.
md5:6B7424978E55D90F986D83B2F683F5BF.
I (21697) BDDSP_SPI_DRIVER: md5 wait time 117.
I (21797) BDDSP_SPI_DRIVER: type: aa, load dst:5ff60000 src:50000000 storage src:00261e70 len:16.
I (21798) BDDSP_SPI_DRIVER: index :8 total:8.
I (21798) BDDSP_SPI_DRIVER: core1 boot addr: 0x5ff60000 0x50000000

I (21800) BDDSP_SPI_DRIVER: Loading DSP bins is successful.
 [1595215029.174 I (bdsc-upload_slave:exec_mode_slave:35)] dsp load success!
I (21801) dsp_download_imp: check dsp status before
 [1595215029.176 D (bdsc-dsp_reg_info:init_dsp_reg_info:24)] ++
 [1595215029.182 D (bdsc-dsp_reg_info:init_dsp_reg_info:31)] struct magic_number :0x0
 [1595215029.182 D (bdsc-dsp_reg_info:init_dsp_reg_info:33)] struct reg 1 group:0x0, 0x0
 [1595215029.183 D (bdsc-dsp_reg_info:init_dsp_reg_info:37)] struct reg 2 group:0x0, 0x0, 0x0, 0x0
 [1595215029.184 D (bdsc-dsp_reg_info:init_dsp_reg_info:41)] struct reg 3 group:0x0, 0x0, 0x0, 0x0
 [1595215029.185 D (bdsc-dsp_reg_info:init_dsp_reg_info:45)] struct reg 4 group:0x0, 0x0, 0x0, 0x0
 [1595215029.186 D (bdsc-dsp_reg_info:init_dsp_reg_info:46)] dci_base_addr:0x0
 [1595215029.187 D (bdsc-dsp_reg_info:init_dsp_reg_info:47)] ver_base_addr:0x0
 [1595215029.188 D (bdsc-dsp_reg_info:init_dsp_reg_info:48)] ver_info_len:0
 [1595215029.189 D (bdsc-dsp_reg_info:init_dsp_reg_info:49)] dci_info_len:0
 [1595215029.190 D (bdsc-dsp_reg_info:init_dsp_reg_info:50)] asr_mode_base_addr:0
 [1595215029.191 D (bdsc-dsp_reg_info:init_dsp_reg_info:51)] asr backoff, opus 0, feat 0
 [1595215029.192 D (bdsc-dsp_reg_info:init_dsp_reg_info:53)] get_asr_mode_cmd_reg, 0x0
 [1595215029.193 D (bdsc-dsp_reg_info:init_dsp_reg_info:54)] --
 [1595215029.293 D (bdsc-dsp_reg_info:init_dsp_reg_info:24)] ++
 [1595215029.299 D (bdsc-dsp_reg_info:init_dsp_reg_info:31)] struct magic_number :0x55aabeef
 [1595215029.299 D (bdsc-dsp_reg_info:init_dsp_reg_info:33)] struct reg 1 group:0x31343845, 0x80003814
 [1595215029.300 D (bdsc-dsp_reg_info:init_dsp_reg_info:37)] struct reg 2 group:0x60000000, 0x80003830, 0x8, 0x800
 [1595215029.301 D (bdsc-dsp_reg_info:init_dsp_reg_info:41)] struct reg 3 group:0xa0077000, 0x80003818, 0x80, 0x40
 [1595215029.302 D (bdsc-dsp_reg_info:init_dsp_reg_info:45)] struct reg 4 group:0x60040000, 0x80003824, 0xc0, 0x100
 [1595215029.304 D (bdsc-dsp_reg_info:init_dsp_reg_info:46)] dci_base_addr:0x60056900
 [1595215029.305 D (bdsc-dsp_reg_info:init_dsp_reg_info:47)] ver_base_addr:0x60056800
 [1595215029.306 D (bdsc-dsp_reg_info:init_dsp_reg_info:48)] ver_info_len:128
 [1595215029.309 D (bdsc-dsp_reg_info:init_dsp_reg_info:49)] dci_info_len:1200
 [1595215029.310 D (bdsc-dsp_reg_info:init_dsp_reg_info:50)] asr_mode_base_addr:2147498024
 [1595215029.311 D (bdsc-dsp_reg_info:init_dsp_reg_info:51)] asr backoff, opus 150, feat 150
 [1595215029.312 D (bdsc-dsp_reg_info:init_dsp_reg_info:53)] get_asr_mode_cmd_reg, 0x8000381c
 [1595215029.313 D (bdsc-dsp_reg_info:init_dsp_reg_info:54)] --
I (21945) dsp_download_imp: check dsp status end : 0
I (21945) BDDSP_SPI_DRIVER: <bddsp_get_dsp_version>The version of DSP is 1.4.8E
 [1595215029.319 I (bdsc-client_adapter:bdsc_adapter_start:279)] dsp upload ret:0
 [1595215029.321 D (bdsc-client_adapter:callback_task:100)] + event pop
 [1595215029.321 D (bdsc-client_adapter:command_task:221)] + bdsc_condition_queue_pop
 [1595215029.322 D (bdsc-client_adapter:command_task:223)] - bdsc_condition_queue_pop key=103
 [1595215029.323 I (bdsc-asr_engine:bds_asr_config:550)] config_params={"host":"111.202.114.98","head_host":"vse.baidu.com","port":443,"engine_uri":"wss://111.202.114.98:443/ws/long_asr?sn=1","qnet_log_level":1}
 [1595215029.324 D (bdsc-basic_adapter:callback_event:44)] callback key=7000, length=0, content=0x0
 [1595215029.326 D (bdsc-client_adapter:command_task:221)] + bdsc_condition_queue_pop
 [1595215029.328 D (bdsc-client_adapter:callback_task:102)] - event pop key=7000
 [1595215029.329 D (bdsc-client_adapter:callback_task:100)] + event pop
I (21956) EVENT_IN: Handle sdk event start.
I (21956) ==========: ---> EVENT_SDK_START_COMPLETED
I (21957) EVENT_OUT: Handle sdk event end.
 [1595215029.331 I (bdsc-client_adapter:bdsc_adapter_send:363)] send key=700, length=15, content=0x3fff2190
W (21957) ==========: Stack: 2352
 [1595215029.333 D (bdsc-client_adapter:command_task:223)] - bdsc_condition_queue_pop key=700
 [1595215029.334 D (bdsc-basic_adapter:dynamic_config:121)] config_str={"nqe_mode":1}, length=15
 [1595215029.336 I (bdsc-client_adapter:bdsc_adapter_send:363)] send key=500, length=0, content=0x0
 [1595215029.336 I (bdsc-nqe_result:update_dsp_current_audio_mode:65)] dsp mode equal! up mode:dsp mode=3:3
 [1595215029.338 D (bdsc-client_adapter:command_task:221)] + bdsc_condition_queue_pop
 [1595215029.339 D (bdsc-client_adapter:command_task:223)] - bdsc_condition_queue_pop key=500
 [1595215029.340 D (bdsc-client_adapter:command_task:221)] + bdsc_condition_queue_pop
 [1595215029.341 I (bdsc-client_adapter:bdsc_adapter_send:363)] send key=200, length=0, content=0x0
 [1595215029.342 D (bdsc-client_adapter:command_task:223)] - bdsc_condition_queue_pop key=200
 [1595215029.343 D (bdsc-wakeup_adapter:wakeup_start:55)] command=0x3f890cc0
 [1595215029.344 I (bdsc-client_wakeup:bdsc_wakeup_start:47)] wakeup=0x3f86fc90, machine=0x3f88b71c, listen=0x4022d58c
0x4022d58c: start_listen at /home/qiyanpeng/light-asr-sdk/light-bdspeech-sdk/main_build/wp_asr/components/bds_light_sdk/wakeup/bdsc_wakeup_machine.c:32

 [1595215029.345 I (bdsc-wakeup_uninitial_state:start_listen:17)] start_listen
 [1595215029.347 I (bdsc-wakeup_stop_state:start_listen:17)] start_listen
I (21973) MQTT_TASK: mqtt_app_start ==> 
 [1595215029.348 I (bdsc-wakeup_start_state:start_listen:92)] start_listen
I (21977) MQTT_TASK: Other event id:7
esp32_intr_gpio_add, ret 0.
E (21978) wakeup_hal: wakeup cmd 0x57414b45+++
E (21980) wakeup_hal: wakeup cmd---
 [1595215029.354 D (bdsc-wakeup_start_state:start_listen:106)] create wakeup task
 [1595215029.355 D (bdsc-client_adapter:command_task:221)] + bdsc_condition_queue_pop
I (22197) MQTT_TASK: MQTT_EVENT_CONNECTED
I (22198) MQTT_TASK: sent subscribe successful, msg_id=43866
I (22242) MQTT_TASK: MQTT_EVENT_SUBSCRIBED, msg_id=43866
I (22894) MP3_DECODER: Closed
W (23075) ESP_AUDIO_TASK: Destroy the old pipeline
W (23076) ESP_AUDIO_TASK: The old pipeline destroyed
W (23076) DU1910_APP: AUDIO_PLAYER_CALLBACK send OK, status:4, err_msg:0, media_src:203, ctx:0x0
W (26977) MQTT_TASK: Stack: 2292
W (31977) MQTT_TASK: Stack: 2292
```

## Troubleshooting

### Incorrect `fc`, `pk`, `ak` and `sk`.

```c
I (4499) RAW_HELPER: default_raw_player_init :106, que:0x3ffe8804
W (4500) TAS5805M: volume = 0x58
I (4500) BDSC_ENGINE: APP version is 750c6318a6d33aa382452446f7d9b792974acbb5
I (4501) SNTP_INIT: ------------Initializing SNTP
I (4506) PROFILE: 56: type[0x1]
I (4506) PROFILE: 57: subtype[0x29]
I (4507) PROFILE: 58: address:0x7c2000
I (4507) PROFILE: 59: size:0x1000
I (4507) PROFILE: 60: label:profile
I (4517) PROFILE: fc            = xx
I (4517) PROFILE: pk            = xx
I (4517) PROFILE: ak            = xx
I (4517) PROFILE: sk            = xx
I (4518) PROFILE: cuid          = xx
I (4518) PROFILE: mqtt_broker   =
I (4519) PROFILE: mqtt_username =
I (4520) PROFILE: mqtt_password =
I (4520) PROFILE: mqtt_cid      =
==> generate_auth_pam
current ts: 1591943265
==> generate_auth_sig_needfree
sig: 043cfeb0d0b752258024131142e4ac0b
pam_string: {"fc":"xx","pk":"xx","ak":"xx","time_stamp":26532387,"signature":"043cfeb0d0b752258024131142e4ac0b"}
I (4523) AUTH_TASK: test request: POST /v1/manage/mqtt HTTP/1.0
Host: smarthome.baidubce.com
User-Agent: esp32
Content-Type: application/json
Content-Length: 100

{"fc":"xx","pk":"xx","ak":"xx","time_stamp":26532387,"signature":"043cfeb0d0b752258024131142e4ac0b"}
I (5508) HTTP_TASK: Connection established...
I (5509) HTTP_TASK: 236 bytes written
I (5509) HTTP_TASK: Reading HTTP response...
I (6109) HTTP_TASK: 202 bytes read
HTTP/1.0 200 OK
Cache-Control: no-cache
Content-Length: 57
Content-Type: application/json
Date: Fri, 12 Jun 2020 06:27:45 GMT
Server: BWS
X-Bce-Request-Id: 5367c504-d620-4e93-8966-74047f0b9175

I (6112) HTTP_TASK: 57 bytes read
{"error_code":1,"err_msg":"fc:xx, pk:xx not registered."}I (6113) HTTP_TASK: connection closed
I (6117) AUTH_TASK: recv body: {"error_code":1,"err_msg":"fc:xx, pk:xx not registered."}
E (6118) AUTH_TASK: auth failed, retry!
==> generate_auth_pam
```