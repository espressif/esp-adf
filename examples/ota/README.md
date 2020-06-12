# Example of OTA_SERVICE

This example shows how to configure the `ota_service` to upgrade both the `app` and `data` partitions.

## How to use example

### Compatibility

This example is will run on boards marked with green checkbox. Please remember to select the board in menuconfig as discussed is section *Usage* below.

| Board Name | Getting Started | Chip | Compatible |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraTD-MSC | [![alt text](../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-LyraT-Mini | [![alt text](../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-Korvo-DU1906 | [![alt text](../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../docs/_static/yes-button.png "Compatible") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../docs/_static/no-button.png "Compatible") |

### Configure the project

1. Run `make menuconfig` or `idf.py menuconfig`.
2. Select the board type in `Audio HAL`
3. Configure `WiFi SSID` and `WiFi Password` in `OTA App Configuration`.
4. Configure `firmware upgrade uri` and `data image upgrade uri` in `OTA App Configuration` to the images.
5. Configure `data partition label` in `OTA App Configuration` to the one need upgrade.

### Format of 'uri'

* Rule of `uri` are as follow.
  * `UF_SCHEMA` field of URI for choose input stream. e.g:"http", "https", "file".
  * `UF_PATH` field of URI for choose images. e.g:"/sdcard/flash_tone.bin".
* Example:
  * file stored on web server: `https://192.168.1.10:8000/hello_world.bin`.
  * file stored in SDCard: `file://sdcard/tone.bin`.

### Build and flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
make -j4 flash monitor
```

Or, for CMake based build system (replace PORT with serial port name):

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Note

All the upgrade processes can be override, a group of default processes are provide by `ota_service`, it's recommended to use the default one when upgrade `app` partition.

## Example output

Here is an example console output. In this case images are stored in a SDCard, WiFi also configured, after connect to the AP and mount SDCard, the upgrade process begin.

After all partitions upgrade success, device restart automatic.

```
I (344) HTTPS_OTA_EXAMPLE: [1.0] Initialize peripherals management
I (344) HTTPS_OTA_EXAMPLE: [1.1] Start and wait for Wi-Fi network
I (364) wifi: wifi driver task: 3ffc31fc, prio:23, stack:3584, core=0
I (364) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (364) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (464) wifi: wifi firmware version: 85618d5
I (464) wifi: config NVS flash: enabled
I (464) wifi: config nano formating: disabled
I (464) wifi: Init dynamic tx buffer num: 32
I (474) wifi: Init data frame dynamic rx buffer num: 32
I (474) wifi: Init management frame dynamic rx buffer num: 32
I (484) wifi: Init management short buffer num: 32
I (484) wifi: Init static rx buffer size: 1600
I (494) wifi: Init static rx buffer num: 10
I (494) wifi: Init dynamic rx buffer num: 32
I (594) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 0
I (594) wifi: mode : sta (30:ae:a4:c3:88:5c)
I (1804) wifi: new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2794) wifi: state: init -> auth (b0)
I (2794) wifi: state: auth -> assoc (0)
I (2804) wifi: state: assoc -> run (10)
I (2834) wifi: connected with ESPRESSIF, aid = 3, channel 11, BW20, bssid = 18:31:bf:4b:8b:68
I (2834) wifi: security type: 3, phy: bgn, rssi: -53
I (2834) wifi: pm start, type: 1

I (2834) wifi: AP's beacon interval = 102400 us, DTIM period = 1
I (3844) event: sta ip: 192.168.50.87, mask: 255.255.255.0, gw: 192.168.50.1
I (3844) HTTPS_OTA_EXAMPLE: [1.2] Mount SDCard
I (3854) SDCARD: Trying to mount with base path=/sdcard
I (3894) SDCARD: CID name BB1QT!

I (4344) HTTPS_OTA_EXAMPLE: [2.0] Create OTA service
I (4344) HTTPS_OTA_EXAMPLE: [2.1] Set upgrade list
I (4344) HTTPS_OTA_EXAMPLE: [2.2] Start OTA service
I (4344) OTA_SERVICE: set list  2
I (4344) HTTPS_OTA_EXAMPLE: Func:app_main, Line:149, MEM Total:189924 Bytes

I (7304) OTA_DEFAULT: data upgrade uri file://sdcard/tone.bin
I (7314) FATFS_STREAM: File size is 434756 byte,pos:0
I (7314) HTTPS_OTA_EXAMPLE: current version -1.-1, the incoming image version 1384531.0
I (7314) OTA_DEFAULT: write_offset 0, size 8
I (7324) OTA_DEFAULT: write_offset 8, r_size 2048
I (7334) OTA_DEFAULT: write_offset 2056, r_size 2048
I (7334) OTA_DEFAULT: write_offset 4104, r_size 2048
I (7344) OTA_DEFAULT: write_offset 6152, r_size 2048
I (7354) OTA_DEFAULT: write_offset 8200, r_size 2048
I (7354) OTA_DEFAULT: write_offset 10248, r_size 2048
I (7364) OTA_DEFAULT: write_offset 12296, r_size 2048

...

I (8694) OTA_DEFAULT: write_offset 432136, r_size 2048
I (8694) OTA_DEFAULT: write_offset 434184, r_size 572
W (8694) FATFS_STREAM: No more data,ret:0
I (8704) AUDIO_ELEMENT: IN-[file] AEL_IO_DONE,0
I (8704) OTA_DEFAULT: partition flash_tone upgrade successes
W (10714) AUDIO_ELEMENT: [file] Element has not create when AUDIO_ELEMENT_TERMINATE
I (10714) esp_fs_ota: Starting OTA...
I (10714) esp_fs_ota: Writing to partition subtype 17 at offset 0x110000
I (10724) OTA_DEFAULT: Running firmware version: v1.0-384-gcb91768f, the incoming firmware version v2.0-beta3-23-gd3994a7c-dirty
I (13594) OTA_DEFAULT: Image bytes read: 578
I (13624) OTA_DEFAULT: Image bytes read: 5698
I (13654) OTA_DEFAULT: Image bytes read: 10818
I (13684) OTA_DEFAULT: Image bytes read: 15938
I (13714) OTA_DEFAULT: Image bytes read: 21058
I (13744) OTA_DEFAULT: Image bytes read: 26178

...

I (18634) OTA_DEFAULT: Image bytes read: 824898
I (18674) OTA_DEFAULT: Image bytes read: 830018
I (18684) OTA_DEFAULT: Image bytes read: 832129
I (18684) boot_comm: chip revision: 1, min. application chip revision: 0
I (18684) esp_image: segment 0: paddr=0x00110020 vaddr=0x3f400020 size=0x23fb0 (147376) map
I (18804) esp_image: segment 1: paddr=0x00133fd8 vaddr=0x3ffb0000 size=0x03624 ( 13860)
I (18814) esp_image: segment 2: paddr=0x00137604 vaddr=0x40080000 size=0x00400 (  1024)
0x40080000: _WindowOverflow4 at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (18814) esp_image: segment 3: paddr=0x00137a0c vaddr=0x40080400 size=0x08604 ( 34308)
I (18844) esp_image: segment 4: paddr=0x00140018 vaddr=0x400d0018 size=0x8ee30 (585264) map
0x400d0018: _flash_cache_start at ??:?

I (19284) esp_image: segment 5: paddr=0x001cee50 vaddr=0x40088a04 size=0x0c2e4 ( 49892)
0x40088a04: panicHandler at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/panic.c:715

I (19324) boot_comm: chip revision: 1, min. application chip revision: 0
I (19324) esp_image: segment 0: paddr=0x00110020 vaddr=0x3f400020 size=0x23fb0 (147376) map
I (19444) esp_image: segment 1: paddr=0x00133fd8 vaddr=0x3ffb0000 size=0x03624 ( 13860)
I (19454) esp_image: segment 2: paddr=0x00137604 vaddr=0x40080000 size=0x00400 (  1024)
0x40080000: _WindowOverflow4 at /home/joseph/esp/esp-adf-internal/esp-idf/components/freertos/xtensa_vectors.S:1779

I (19454) esp_image: segment 3: paddr=0x00137a0c vaddr=0x40080400 size=0x08604 ( 34308)
I (19484) esp_image: segment 4: paddr=0x00140018 vaddr=0x400d0018 size=0x8ee30 (585264) map
0x400d0018: _flash_cache_start at ??:?

I (19924) esp_image: segment 5: paddr=0x001cee50 vaddr=0x40088a04 size=0x0c2e4 ( 49892)
0x40088a04: panicHandler at /home/joseph/esp/esp-adf-internal/esp-idf/components/esp32/panic.c:715

I (20024) HTTPS_OTA_EXAMPLE: [2.3] Clear OTA service
W (20024) OTA_SERVICE: OTA_END!
W (20024) OTA_SERVICE: restart!
I (20024) wifi: state: run -> init (0)
I (20024) wifi: pm stop, total sleep time: 9962583 us / 17191040 us

I (20034) wifi: new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (20044) PERIPH_WIFI: Wi-Fi disconnected from SSID ESPRESSIF, auto-reconnect enabled, reconnect after 1000 ms
I (20104) wifi: flush txq
I (20104) wifi: stop sw txq
I (20104) wifi: lmac stop hw txq
ets Jun  8 2016 00:22:57

rst:0xc (SW_CPU_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0018,len:4
load:0x3fff001c,len:6836
load:0x40078000,len:12072
load:0x40080400,len:6708
entry 0x40080778
```

## Troubleshooting

### Failure to download images

```
I (12979) esp_https_ota: Starting OTA...
I (12979) esp_https_ota: Writing to partition subtype 17 at offset 0x110000
E (14799) TRANS_SSL: esp_tls_conn_read error, errno=No more processes
W (14799) HTTP_CLIENT: esp_transport_read returned:-26880 and errno:11
```

Make sure of the accessible of the remote server.
