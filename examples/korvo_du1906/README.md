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
0x542000 | dsp_v1.4.0.C.bin
0x7c2000 | profile.bin
0x7c3000 | audio_tone.bin

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
0x542000 ./firmware/dsp_v1.4.0.C.bin \
0x7c2000 ./profiles/profile.bin \
0x7c3000 ./tone/audio_tone.bin
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

In addition, ESP32-Korvo-DU1906 have three more bins, `./firmware/dsp_v1.4.0.C.bin`,  `./profiles/profile.bin` and `./tone/audio-esp.bin`.
```bash
python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 \
--port PORT --baud 921600 \
--before default_reset \
--after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect \
0x542000 ./firmware/dsp_v1.4.0.C.bin \
0x7c2000 ./profiles/profile.bin \
0x7c3000 ./tone/audio_tone.bin
```

The firmware downloading flash address refer to above table in jumpstart part.

### Usage

Please refer to jumpstart part.

### Upgrade function

The application, flash tone and profile bins upgrade are supported. Those bins can be store on SDcard or HTTP Server, such as,`/sdcard/flash_tone.bin`,`/sdcard/profile.bin`, "https://github.com/espressif/esp-adf/raw/master/examples/korvo_du1906/firmware/app.bin". The bin files version checking after every booting, exculde `profile.bin`.

User copy the `flash_tone.bin` and `profile.bin` to SDCard root folder and inserted to ESP32-Korvo-DU1906 sdcard slot could be execute upgrade.

#### Firmware version

Before the upgration, usually compare the firmware version first, and then judge whether the firmware need to be upgraded. (Sdcard ota in this example doesn't compare firmware version).
To edit the version of firmware like below:
- App bin:  Change "version.txt" in the project directory.
- Tone bin: Use this script to assign version ``` python  $ADF_PATH/tools/audio_tone/mk_audio_tone.py -r tone/ -f components/audio_flash_tone -v v1.1.1 ```

## Example Output

After download the follow logs should be output.
```c
I (35) boot: compile time 15:56:22
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
I (44) boot:  5 dsp_bin          Unknown data     01 24 00542000 00280000
I (45) boot:  6 profile          Unknown data     01 29 007c2000 00001000
I (46) boot:  7 flash_tone       Unknown data     01 27 007c3000 00020000
I (47) boot: End of partition table
I (47) boot: No factory image, trying OTA 0
I (48) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x9a5f8 (632312) map
I (208) esp_image: segment 1: paddr=0x000aa620 vaddr=0x3ffbdb60 size=0x04bc8 ( 19400) load
I (214) esp_image: segment 2: paddr=0x000af1f0 vaddr=0x40080000 size=0x00400 (  1024) load
0x40080000: _WindowOverflow4 at /Users/maojianxin/duros/esp-adf-internal-dev/esp-idf/components/freertos/xtensa_vectors.S:1779

I (214) esp_image: segment 3: paddr=0x000af5f8 vaddr=0x40080400 size=0x00a18 (  2584) load
I (216) esp_image: segment 4: paddr=0x000b0018 vaddr=0x400d0018 size=0x171888 (1513608) map
0x400d0018: _stext at ??:?

I (597) esp_image: segment 5: paddr=0x002218a8 vaddr=0x40080e18 size=0x1a27c (107132) load
I (646) boot: Loaded app from partition at offset 0x10000
I (662) boot: Set actual ota_seq=1 in otadata[0]
I (662) boot: Disabling RNG early entropy source...
I (662) psram: This chip is ESP32-D0WD
I (663) spiram: Found 64MBit SPI RAM device
I (663) spiram: SPI RAM mode: flash 80m sram 80m
I (664) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (665) cpu_start: Pro cpu up.
I (665) cpu_start: Application information:
I (666) cpu_start: Compile time:     Jun 18 2020 15:56:19
I (666) cpu_start: ELF file SHA256:  3a84593410f44598...
I (667) cpu_start: ESP-IDF:          v3.3.2-108-gbd1b1ff20-dirty
I (668) cpu_start: Starting app cpu, entry point is 0x400814d0
0x400814d0: call_start_cpu1 at /Users/maojianxin/duros/esp-adf-internal-dev/esp-idf/components/esp32/cpu_start.c:268

I (0) cpu_start: App cpu up.
I (1148) spiram: SPI SRAM memory test OK
I (1150) heap_init: Initializing. RAM available for dynamic allocation:
I (1151) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1151) heap_init: At 3FFB6388 len 00001C78 (7 KiB): DRAM
I (1151) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1152) heap_init: At 3FFBDB5C len 00000004 (0 KiB): DRAM
I (1153) heap_init: At 3FFC5268 len 0001AD98 (107 KiB): DRAM
I (1154) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1155) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1155) heap_init: At 4009B094 len 00004F6C (19 KiB): IRAM
I (1156) cpu_start: Pro cpu start user code
I (1157) spiram: Adding pool of 4057K of external SPI memory to heap allocator
I (48) esp_core_dump_common: Init core dump to UART
E (53) esp_core_dump_common: No core dump partition found!
I (53) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (54) spiram: Reserving pool of 18K of internal memory for DMA/internal allocations
I (77) AUDIO_THREAD: The esp_periph task allocate stack on external memory
I (77) SDCARD: Trying to mount with base path=/sdcard
I (148) SDCARD: CID name SC16G!

I (588) wifi:wifi driver task: 3ffcf7b4, prio:23, stack:3584, core=0
I (588) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (588) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (604) wifi:wifi firmware version: 5f8804c
I (605) wifi:config NVS flash: enabled
I (605) wifi:config nano formating: disabled
I (605) wifi:Init dynamic tx buffer num: 32
I (605) wifi:Init data frame dynamic rx buffer num: 512
I (606) wifi:Init management frame dynamic rx buffer num: 512
I (606) wifi:Init management short buffer num: 32
I (607) wifi:Init static tx buffer num: 8
I (608) wifi:Init static rx buffer size: 1600
I (608) wifi:Init static rx buffer num: 16
I (608) wifi:Init dynamic rx buffer num: 512
I (784) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 0
I (785) wifi:mode : sta (fc:f5:c4:37:c1:10)
I (1587) DU1910_APP: ESP_BLUFI wifi setting module has been selected
I (1587) BLUFI_CONFIG: Set blufi customized data: hello world
, length: 12
I (1591) WIFI_SERV: Connect to wifi ssid: steven, pwd: esp123456
I (1729) wifi:new:<6,2>, old:<1,0>, ap:<255,255>, sta:<6,2>, prof:1
I (1730) wifi:state: init -> auth (b0)
I (1738) wifi:state: auth -> assoc (0)
I (1748) wifi:state: assoc -> run (10)
I (1775) wifi:connected with steven, aid = 2, channel 6, 40D, bssid = b0:95:8e:17:94:a5
I (1775) wifi:security type: 4, phy: bgn, rssi: -32
I (1839) wifi:pm start, type: 1

I (1839) wifi:AP's beacon interval = 102400 us, DTIM period = 1
I (2583) event: sta ip: 192.168.1.108, mask: 255.255.255.0, gw: 192.168.1.1
I (2584) WIFI_SERV: Got ip:192.168.1.108
W (2584) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (2604) DU1910_APP: PERIPH_WIFI_CONNECTED [64]
I (2605) SNTP_INIT: ------------Initializing SNTP
I (2606) APP_OTA_UPGRADE: Create OTA service
I (2606) APP_OTA_UPGRADE: Start OTA service
I (2606) OTA_DEFAULT: data upgrade uri file://sdcard/flash_tone.bin
I (4381) FATFS_STREAM: File size is 0 byte,pos:0
E (4381) FATFS_STREAM: Failed to open file /sdcard/flash_tone.bin
E (4382) AUDIO_ELEMENT: [file] AEL_STATUS_ERROR_OPEN,-1
E (4382) OTA_DEFAULT: reader stream init failed
E (4383) OTA_SERVICE: OTA service process failed
E (4384) APP_OTA_UPGRADE: List id: 0, OTA failed
I (4384) OTA_DEFAULT: data upgrade uri file://sdcard/profile.bin
I (4386) FATFS_STREAM: File size is 112 byte,pos:0
I (4387) APP_OTA_UPGRADE: Found ota file in sdcard, uri: /sdcard/profile.bin
I (4435) OTA_DEFAULT: write_offset 0, r_size 112
W (4436) FATFS_STREAM: No more data,ret:0
I (4436) OTA_DEFAULT: partition profile upgrade successes
W (4436) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
I (4438) APP_OTA_UPGRADE: List id: 1, OTA sucessed
E (4438) esp_https_ota: Server certificate not found in esp_http_client config
E (4439) OTA_DEFAULT: ESP HTTPS OTA Begin failed
E (4440) OTA_SERVICE: OTA service process failed
E (4440) APP_OTA_UPGRADE: List id: 2, OTA failed
W (4441) OTA_SERVICE: OTA_END!
W (4442) APP_OTA_UPGRADE: upgrade finished, Please check the result of OTA
I (4443) AUDIO_THREAD: The input_key_service task allocate stack on external memory
I (4444) TAS5805M: Power ON CODEC with GPIO 12
I (4444) gpio: GPIO[12]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 1| Pulldown: 1| Intr:0
I (4451) AUDIO_THREAD: The button_task task allocate stack on external memory
I (5410) TAS5805M: tas5805m_transmit_registers:  write 1677 reg done
W (5411) TAS5805M: volume = 0x44
W (5411) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
W (5415) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
I (5418) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (5418) APP_BT_INIT: Init Bluetooth module
I (5418) BTDM_INIT: BT controller compile version [3cd70f2]
I (5419) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (5758) BLE_GATTS: ble gatts module init
I (5758) APP_BT_INIT: Start Bluetooth peripherals
I (5759) AUDIO_THREAD: The media_task task allocate stack on external memory
I (5759) ESP_AUDIO_TASK: media_ctrl_task running...,0x3f81d098

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                               ESP_AUDIO-v1.6.2                             |
|                     Compile date: Jun 16 2020-21:20:16                     |
------------------------------------------------------------------------------
I (5764) BLE_GATTS: create attribute table successful, the number handle = 8
I (5765) BT_HELPER: ap_helper_a2dp_handle_set, 42, 0x0x3f81f114, 0x3ffc4940
I (5767) MP3_DECODER: MP3 init
I (5768) ESP_DECODER: esp_decoder_init, stack size is 5120
I (5770) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (5770) BLE_GATTS: SERVICE_START_EVT, status 0, service_handle 40
I (5772) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (5773) ESP32_Korvo_DU1906: I2S0, MCLK output by GPIO0
W (5775) TAS5805M: volume = 0x4a
I (5775) APP_PLAYER_INIT: Func:setup_app_esp_audio_instance, Line:157, MEM Total:4163856 Bytes, Inter:131768 Bytes, Dram:111472 Bytes

I (5776) APP_PLAYER_INIT: esp_audio instance is:0x3f81d098
I (5777) AUDIO_THREAD: The audio_manager_task task allocate stack on external memory
I (5778) SD_HELPER: default_sdcard_player_init
I (5778) SD_HELPER: default_sdcard_player_init  playlist_create
I (5779) RAW_HELPER: default_raw_player_init :106, que:0x3ffe50d0
W (5781) TAS5805M: volume = 0x58
I (5781) BDSC_ENGINE: APP version is fdc453bfdba43e38fa5b7728e72442aa2e5e2103
I (5782) SNTP_INIT: ------------Initializing SNTP
I (5787) PROFILE: 56: type[0x1]
I (5788) PROFILE: 57: subtype[0x29]
I (5788) PROFILE: 58: address:0x7c2000
I (5788) PROFILE: 59: size:0x1000
I (5788) PROFILE: 60: label:profile
I (5799) PROFILE: fc            = your_fc
I (5799) PROFILE: pk            = your_pk
I (5799) PROFILE: ak            = your_ak
I (5799) PROFILE: sk            = your_sk
I (5800) PROFILE: cuid          = your_cuid
I (5801) PROFILE: mqtt_broker   =
I (5801) PROFILE: mqtt_username =
I (5802) PROFILE: mqtt_password =
I (5802) PROFILE: mqtt_cid      =
==> generate_auth_pam
current ts: 1592467274
==> generate_auth_sig_needfree
sig: 9c8288d0e2cf59cde94ce384a211e99a
pam_string: {"fc":"your_fc","pk":"your_pk","ak":"your_ak","time_stamp":26541121,"signature":"9c8288d0e2cf59cde94ce384a211e99a"}
I (5806) AUTH_TASK: test request: POST /v1/manage/mqtt HTTP/1.0
Host: smarthome.baidubce.com
User-Agent: esp32
Content-Type: application/json
Content-Length: 130

{"fc":"your_fc","pk":"your_pk","ak":"your_ak","time_stamp":26541121,"signature":"9c8288d0e2cf59cde94ce384a211e99a"}
I (6695) HTTP_TASK: Connection established...
I (6696) HTTP_TASK: 266 bytes written
I (6697) HTTP_TASK: Reading HTTP response...
I (6948) HTTP_TASK: 196 bytes read
HTTP/1.0 200 OK
Cache-Control: no-cache
Content-Type: application/json;charset=UTF-8
Date: Thu, 18 Jun 2020 08:01:15 GMT
Server: BWS
X-Bce-Request-Id: 50cbd47c-6f13-44dc-a2cb-a83ceb4bbb2a

I (6950) HTTP_TASK: 193 bytes read
{"broker":"azsgqzj.iot.gz.baidubce.com","user":"thingidp@azsgqzj|cabcaad9358dd1623427701be534f578|0|MD5","pass":"d57ce60bed6a3243fc12c0cd7dc30906","clientID":"cabcaad9358dd1623427701be534f578"}I (6952) HTTP_TASK: connection closed
I (6957) AUTH_TASK: recv body: {"broker":"azsgqzj.iot.gz.baidubce.com","user":"thingidp@azsgqzj|cabcaad9358dd1623427701be534f578|0|MD5","pass":"d57ce60bed6a3243fc12c0cd7dc30906","clientID":"cabcaad9358dd1623427701be534f578"}
I (6958) BDSC_ENGINE: auth restul: broker: azsgqzj.iot.gz.baidubce.com, clientID: cabcaad9358dd1623427701be534f578, username: thingidp@azsgqzj|cabcaad9358dd1623427701be534f578|0|MD5, pwd: d57ce60bed6a3243fc12c0cd7dc30906

I (7006) BDSC_ENGINE: profile save OK
I (7007) gpio: GPIO[27]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (7007) ESP32_GPIO_DRIVER: Initializing GPIO reset is successful.
I (7008) dsp_download_imp: esp32_spi_init...
I (7009) ESP32_SPI_DRIVER: Initializing SPI is successful,clock speed is 16MHz,mode 0.
I (7010) bds_connection:[bds_connection_create]: enter
I (7012) gpio: GPIO[38]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:1
E (7012) gpio: gpio_install_isr_service(412): GPIO isr service already installed
esp32_intr_gpio_init is here.
I (7013) wakeup_hal: esp32_intr_gpio_init, ret:0
I (7014) GEN_PAM: ==> generate_dcs_pam
I (7015) connect_strategy:[set_params]: conifg connect params
I (7016) client_adapter:[bdsc_adapter_start]: bdsc_adapter_start
I (7016) client_adapter:[bdsc_adapter_start]: dsp uploading ...
GPIO_DSP_RST is high.
I (7227) BDDSP_SPI_DRIVER: bddsp_load_bin_slave: entering...
I (7227) BDDSP_SPI_DRIVER: type: 55, load dst:5ff60000 src:60000000 storage src:00000240 len:131072.
I (7227) BDDSP_SPI_DRIVER: index :0 total:8.
md5:2675DC9799DD5E211B3EC04E78AB10A3.
I (7920) BDDSP_SPI_DRIVER: md5 wait time 78.
I (7990) BDDSP_SPI_DRIVER: type: 55, load dst:5ff80000 src:60000000 storage src:00020240 len:507904.
I (7990) BDDSP_SPI_DRIVER: index :1 total:8.
md5:78B4858292EAB00224440192FF79569E.
I (10416) BDDSP_SPI_DRIVER: md5 wait time 304.
I (10656) BDDSP_SPI_DRIVER: type: 55, load dst:60050000 src:60050000 storage src:0009c240 len:536576.
I (10656) BDDSP_SPI_DRIVER: index :2 total:8.
md5:4DB293568E4BE8AA442EAF0F0F529F08.
I (13246) BDDSP_SPI_DRIVER: md5 wait time 321.
I (13496) BDDSP_SPI_DRIVER: type: aa, load dst:5ff60000 src:50000000 storage src:0011f240 len:16.
I (13496) BDDSP_SPI_DRIVER: index :3 total:8.
I (13497) BDDSP_SPI_DRIVER: core boot addr: 0x5ff60000 0x50000000

I (13498) BDDSP_SPI_DRIVER: type: 55, load dst:5ff60000 src:a0000000 storage src:0011f250 len:131072.
I (13499) BDDSP_SPI_DRIVER: index :4 total:8.
md5:4EC0706C8D6A17D6A5F5589C2CF51121.
I (14141) BDDSP_SPI_DRIVER: md5 wait time 78.
I (14141) BDDSP_SPI_DRIVER: type: 55, load dst:5ff80000 src:a0000000 storage src:0013f250 len:507904.
I (14142) BDDSP_SPI_DRIVER: index :5 total:8.
md5:CA47B800EE4C95967A87328DAB12C364.
I (16616) BDDSP_SPI_DRIVER: md5 wait time 304.
I (16616) BDDSP_SPI_DRIVER: type: 55, load dst:a0000000 src:a0000000 storage src:001bb250 len:487424.
I (16617) BDDSP_SPI_DRIVER: index :6 total:8.
md5:80CA6C2A586F552A7BA1203E0BC45281.
I (19030) BDDSP_SPI_DRIVER: md5 wait time 292.
I (19030) BDDSP_SPI_DRIVER: type: 55, load dst:60010000 src:60010000 storage src:00232250 len:189264.
I (19031) BDDSP_SPI_DRIVER: index :7 total:8.
md5:FB5B58F142826F181F8C8BB62CA8F322.
I (19947) BDDSP_SPI_DRIVER: md5 wait time 113.
I (19947) BDDSP_SPI_DRIVER: type: aa, load dst:5ff60000 src:50000000 storage src:002605a0 len:16.
I (19948) BDDSP_SPI_DRIVER: index :8 total:8.
I (19948) BDDSP_SPI_DRIVER: core1 boot addr: 0x5ff60000 0x50000000

I (19949) BDDSP_SPI_DRIVER: Loading DSP bins is successful.
I (19950) dsp_download_imp: check dsp status before
I (19958) dsp_download_imp: check dsp status end : 0
I (19958) BDDSP_SPI_DRIVER: <bddsp_get_dsp_version>The version of DSP is 1.4.0C
I (19958) client_adapter:[bdsc_adapter_start]: dsp upload ret:0
E (19959) BDSC_ENGINE: !!! unknow event 7000!!!
I (19960) client_adapter:[bdsc_adapter_send]: send key=700, length=15, content=0x3ffe7e04
I (19961) client_adapter:[bdsc_adapter_send]: send key=500, length=0, content=0x0
I (19961) BDDSP_SPI_DRIVER: write_asr_mode_feature
I (19962) client_adapter:[bdsc_adapter_send]: send key=200, length=0, content=0x0
I (19963) nqe_result:[update_dsp_current_audio_mode]: write asr mode ret=0
I (19964) MQTT_TASK: mqtt_app_start ==>
 clientid: cabcaad9358dd1623427701be534f578, sub: $iot/cabcaad9358dd1623427701be534f578/user/your_fc/your_pk/your_ak/down, pub: $iot/cabcaad9358dd1623427701be534f578/user/your_fc/your_pk/your_ak/up
I (19965) bds_base_online:[bds_start_net_env]: start connect
I (19968) net_machine:[machine_connect]: +++
I (19968) MQTT_TASK: Other event id:7
I (19969) disconnected_state:[state_connect]: +++
I (19970) strategy_mbedtls:[tls_init]: Seeding the random number generator
I (19971) net_machine:[switch_state]: +++ 2
I (19972) net_machine:[switch_state]: ----
I (19972) disconnected_state:[state_connect]: ----
I (19973) net_machine:[machine_connect]: ---
I (19974) client_wakeup:[bdsc_wakeup_start]: wakeup=0x3f8222d0, machine=0x3f823df4, listen=0x4022b1c8
0x4022b1c8: start_listen at /home/meng/work_bench/1.2.4/light-asr-sdk/light-bdspeech-sdk/main_build/wp_asr/components/bds_light_sdk/wakeup/bdsc_wakeup_machine.c:32

I (19974) strategy_mbedtls:[tls_init]: Setting up the SSL/TLS structure...
I (19975) wakeup_uninitial_state:[start_listen]: start_listen
I (19976) wakeup_stop_state:[start_listen]: start_listen
I (19977) wakeup_start_state:[start_listen]: start_listen
esp32_intr_gpio_add, ret 0.
I (19978) strategy_mbedtls:[tcp_connect]: host = leetest.baidu.com, port=443
E (19978) wakeup_hal: wakeup cmd 0x57414b45+++
E (19982) wakeup_hal: wakeup cmd---
I (20066) strategy_mbedtls:[tcp_connect]: create socket success
I (20100) strategy_mbedtls:[tcp_connect]: connect sucess
I (20101) strategy_mbedtls:[tls_connect]: Performing the SSL/TLS handshake...
I (20110) MQTT_TASK: MQTT_EVENT_CONNECTED
I (20111) MQTT_TASK: sent subscribe successful, msg_id=43775
I (20177) MQTT_TASK: MQTT_EVENT_SUBSCRIBED, msg_id=43775
I (20725) strategy_mbedtls:[net_connect]: tls connect success
I (20725) disconnected_state:[do_connect_exec]: open I/O stream
I (20726) io_watch_dog:[bds_io_watchdog_start]: start watch dog
I (20726) bds_socket_stream:[open_stream]: start read task
I (20728) bds_socket_stream:[open_stream]: start write task
I (20782) net_machine:[switch_state]: +++ 1
I (20782) EVENT_IN: Handle sdk event start.
I (20782) AUDIO_PLAYER: tone play, type:203, cur media type:203
I (20783) net_machine:[switch_state]: ----
I (20783) disconnected_state:[do_connect_exec]: connect exec finish
I (20784) bds_socket_stream:[send_msg_impl]: write len = 447, id: 1392302861, type = 0x2, idx = -1
I (20784) AP_HELPER: audio_player_helper_default_play, type:203,url:flash://tone/0_linked.mp3,pos:0,block:0,auto:0,mixed:0,inter:1
I (20787) AUDIO_MANAGER: ap_manager_play, 620, inter:1, type:203, auto:0, block:0, UNKNOWN
W (20788) AUDIO_MANAGER: ap_manager_backup_audio_info, not found the current operations
W (20789) AUDIO_MANAGER: ap_manager_play, 669, type:203,auto:0,block:0
I (20790) ESP_AUDIO_TASK: It's a decoder
I (20790) ESP_AUDIO_TASK: 1.CUR IN:[IN_flash],CODEC:[DEC_auto],RESAMPLE:[48000],OUT:[OUT_iis],rate:0, ch:0
I (20792) ESP_AUDIO_TASK: 2.Handles,IN:0x3f81ef5c,CODEC:0x3f8224e8,FILTER:0x3f836674,OUT:0x3f822664
I (20793) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (20794) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (20796) ESP_AUDIO_TASK: 3. Previous starting...
I (20797) AUDIO_THREAD: The DEC_auto task allocate stack on external memory
I (20798) ESP_DECODER: Ready to do audio type check, pos:0, (line 104)
I (20798) flashPartition: 146: label:flash_tone
I (20798) TONE_STREAM: header tag 2053, format 1
I (20799) TONE_STREAM: audio tone's tail is DFAC
I (20800) TONE_STREAM: Wanted read flash tone index is 0
I (20800) TONE_STREAM: Tone offset:00000248, Tone length:3960, total num:5  pos:0

W (20811) TONE_STREAM: No more data,ret:0 ,info.byte_pos:3960
I (20812) ESP_DECODER: Detect audio type is MP3
I (20812) MP3_DECODER: MP3 opened
I (20818) RSP_FILTER: reset sample rate of source data : 16000, reset channel of source data : 1
I (20818) ESP_AUDIO_TASK: Received muisc info then send MEDIA_CTRL_EVT_PLAY
I (20819) ESP_AUDIO_TASK: MEDIA_CTRL_EVT_PLAY, status:UNKNOWN, 0
I (20820) AUDIO_THREAD: The resample task allocate stack on external memory
I (20822) AUDIO_THREAD: The OUT_iis task allocate stack on external memory
I (20823) I2S_STREAM: AUDIO_STREAM_WRITER
I (20826) RSP_FILTER: sample rate of source data : 16000, channel of source data : 1, sample rate of destination data : 48000, channel of destination data : 2
I (20827) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING
I (20828) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:801, MEM Total:3935184 Bytes, Inter:99372 Bytes, Dram:79076 Bytes

I (20830) ==========: ---> ---> EVENT_LINK_CONNECTED buffer_length=24, buffer={"????flash://tone/0_linked.mp3
I (20831) MAIN: ==> Got BDSC_EVENT_ON_LINK_CONNECTED
I (20832) EVENT_OUT: Handle sdk event end.
W (20832) ==========: Stack: 1992
I (20833) AUDIO_MANAGER: AUDIO MANAGER RECV STATUS:RUNNING, err:0, media_src:203
W (20834) DU1910_APP: AUDIO_PLAYER_CALLBACK send OK, status:1, err_msg:0, media_src:203, ctx:0x0
I (21160) bds_handle_data:[do_logic_connected]: verify value : {"status":0,"msg":"OK","data":[]}
I (22479) MP3_DECODER: Closed
I (22479) ESP_AUDIO_TASK: Received last pos: 3960 bytes
I (22835) ESP_AUDIO_TASK: Received last time: 1984 ms
I (22835) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED
I (22836) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:801, MEM Total:3976456 Bytes, Inter:100108 Bytes, Dram:79812 Bytes

W (22837) ESP_AUDIO_TASK: Destroy the old pipeline
W (22838) ESP_AUDIO_TASK: The old pipeline destroyed
I (22838) AUDIO_MANAGER: AUDIO MANAGER RECV STATUS:FINISHED, err:0, media_src:203
W (22839) DU1910_APP: AUDIO_PLAYER_CALLBACK send OK, status:4, err_msg:0, media_src:203, ctx:0x0
I (22841) AUDIO_MANAGER: AUDIO_STATUS_FINISHED, resume:0, is_backup:0
I (22842) AUDIO_MANAGER: AUDIO_PLAYER_MODE_ONE_SONG
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