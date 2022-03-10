# Connect to WPA2 Enterprise Encryption Routers

- [中文版本](./README.md)
- Basic Example: ![alt text](../../../docs/_static/level_basic.png "Basic Example")


## Example Brief

This example demonstrates how to configure ESP32 to connect to a WPA2 enterprise encryption router in ADF.


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch

This example supports IDF release/v3.3 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default board for this example is `ESP32-Lyrat V4.3`. If you need to run this example on other development boards, select the board in menuconfig, such as `ESP32-Lyrat-Mini V1.1`.

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

Configure the Wi-Fi connection information. Go to `menuconfig > Example Configuration` and fill in the `Wi-Fi SSID`, which is `Xiaomi_7909` in this example.

```
menuconfig > Example Configuration > (Xiaomi_7909) WiFi SSID
```

Configure the working mode of `EAP`, which is `REAP` in this example. Set `0` to `TLS`, `1` to `PEAP`, and `2` to `TTLS`.

```
menuconfig > Example Configuration > (1) EAP METHOD
```

Configure the working mode of `EAP ID` and set `EAP ID` to `example@espressif.com`.

```
menuconfig > Example Configuration > (example@espressif.com) EAP ID
```

Configure `EAP USERNAME` to `espressif`.

```
menuconfig > Example Configuration > (espressif) EAP USERNAME
```

Configure `EAP PASSWORD` to `test11`.

```
menuconfig > Example Configuration > (test11) EAP PASSWORD
```

**Note:**

This example also requires a router that supports enterprise encryption and another computer running the `hostapd` service on the `RADIUS` server. Please also configure `RADIUS` option on the router configuration page.

Please refer to [Hostapd Configuration of RADIUS Server](https://blog.csdn.net/espressif/article/details/80933222) for more information about configuration.

This example was configured and ran successfully, please see the logs in the Section [Example Log](#example-log).

### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v4.2/esp32/index.html) for full steps to configure and build an ESP-IDF project.


## How to Use the Example

### Example Functionality

- After the example starts running, it first connects to Wi-Fi. The log of successful connection is as follows:

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
I (27) boot: compile time 10:41:35
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1c8e8 (116968) map
I (156) esp_image: segment 1: paddr=0x0002c910 vaddr=0x3ffb0000 size=0x032fc ( 13052) load
I (161) esp_image: segment 2: paddr=0x0002fc14 vaddr=0x40080000 size=0x00404 (  1028) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (163) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x875d0 (554448) map
0x400d0020: _stext at ??:?

I (382) esp_image: segment 4: paddr=0x000b75f8 vaddr=0x40080404 size=0x15400 ( 87040) load
I (432) boot: Loaded app from partition at offset 0x10000
I (432) boot: Disabling RNG early entropy source...
I (433) cpu_start: Pro cpu up.
I (436) cpu_start: Application information:
I (441) cpu_start: Project name:     wpa2_enterprise
I (447) cpu_start: App version:      v2.2-252-g62edbd26-dirty
I (453) cpu_start: Compile time:     Nov 25 2021 20:39:37
I (459) cpu_start: ELF file SHA256:  ab0e1b23954cb0ea...
I (465) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (471) cpu_start: Starting app cpu, entry point is 0x400818b4
0x400818b4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (482) heap_init: Initializing. RAM available for dynamic allocation:
I (489) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (495) heap_init: At 3FFB79F8 len 00028608 (161 KiB): DRAM
I (501) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (507) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (514) heap_init: At 40095804 len 0000A7FC (41 KiB): IRAM
I (520) cpu_start: Pro cpu start user code
I (538) spi_flash: detected chip: gd
I (539) spi_flash: flash io: dio
W (539) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (549) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (621) wpa2_enterprise: [ 0 ] Initialize peripherals
I (621) wpa2_enterprise: [ 1 ] Set Wi-Fi config
I (621) wpa2_enterprise: [ 2 ] Start bt peripheral
I (631) wpa2_enterprise: [ 3 ] Get ip info
I (2631) wpa2_enterprise: IP:0.0.0.0
I (2631) wpa2_enterprise: MASK:0.0.0.0
I (2631) wpa2_enterprise: GW:0.0.0.0
W (3981) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4631) wpa2_enterprise: IP:192.168.51.185
I (4631) wpa2_enterprise: MASK:255.255.255.0
I (4631) wpa2_enterprise: GW:192.168.51.1
I (6631) wpa2_enterprise: IP:192.168.51.185
I (6631) wpa2_enterprise: MASK:255.255.255.0
I (6631) wpa2_enterprise: GW:192.168.51.1
I (8631) wpa2_enterprise: IP:192.168.51.185
I (8631) wpa2_enterprise: MASK:255.255.255.0
I (8631) wpa2_enterprise: GW:192.168.51.1
I (10631) wpa2_enterprise: IP:192.168.51.185
I (10631) wpa2_enterprise: MASK:255.255.255.0
I (10631) wpa2_enterprise: GW:192.168.51.1
I (12631) wpa2_enterprise: IP:192.168.51.185
I (12631) wpa2_enterprise: MASK:255.255.255.0
I (12631) wpa2_enterprise: GW:192.168.51.1
I (14631) wpa2_enterprise: IP:192.168.51.185
I (14631) wpa2_enterprise: MASK:255.255.255.0
I (14631) wpa2_enterprise: GW:192.168.51.1
I (16631) wpa2_enterprise: IP:192.168.51.185
I (16631) wpa2_enterprise: MASK:255.255.255.0
I (16631) wpa2_enterprise: GW:192.168.51.1
I (18631) wpa2_enterprise: IP:192.168.51.185
I (18631) wpa2_enterprise: MASK:255.255.255.0
I (18631) wpa2_enterprise: GW:192.168.51.1
I (20631) wpa2_enterprise: IP:192.168.51.185
I (20631) wpa2_enterprise: MASK:255.255.255.0
I (20631) wpa2_enterprise: GW:192.168.51.1
I (22631) wpa2_enterprise: IP:192.168.51.185
I (22631) wpa2_enterprise: MASK:255.255.255.0
I (22631) wpa2_enterprise: GW:192.168.51.1
I (24631) wpa2_enterprise: IP:192.168.51.185
I (24631) wpa2_enterprise: MASK:255.255.255.0
I (24631) wpa2_enterprise: GW:192.168.51.1
I (26631) wpa2_enterprise: IP:192.168.51.185
I (26631) wpa2_enterprise: MASK:255.255.255.0
I (26631) wpa2_enterprise: GW:192.168.51.1
I (28631) wpa2_enterprise: IP:192.168.51.185
I (28631) wpa2_enterprise: MASK:255.255.255.0
I (28631) wpa2_enterprise: GW:192.168.51.1
I (30631) wpa2_enterprise: IP:192.168.51.185
I (30631) wpa2_enterprise: MASK:255.255.255.0
I (30631) wpa2_enterprise: GW:192.168.51.1
I (32631) wpa2_enterprise: IP:192.168.51.185
I (32631) wpa2_enterprise: MASK:255.255.255.0
I (32631) wpa2_enterprise: GW:192.168.51.1
I (34631) wpa2_enterprise: IP:192.168.51.185
I (34631) wpa2_enterprise: MASK:255.255.255.0
I (34631) wpa2_enterprise: GW:192.168.51.1

Done
```


### Example Log

- The complete log of the development board side from startup to initialization is as follows:

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
I (27) boot: compile time 10:41:35
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x1c8e8 (116968) map
I (156) esp_image: segment 1: paddr=0x0002c910 vaddr=0x3ffb0000 size=0x032fc ( 13052) load
I (161) esp_image: segment 2: paddr=0x0002fc14 vaddr=0x40080000 size=0x00404 (  1028) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (163) esp_image: segment 3: paddr=0x00030020 vaddr=0x400d0020 size=0x875d0 (554448) map
0x400d0020: _stext at ??:?

I (382) esp_image: segment 4: paddr=0x000b75f8 vaddr=0x40080404 size=0x15400 ( 87040) load
I (432) boot: Loaded app from partition at offset 0x10000
I (432) boot: Disabling RNG early entropy source...
I (433) cpu_start: Pro cpu up.
I (436) cpu_start: Application information:
I (441) cpu_start: Project name:     wpa2_enterprise
I (447) cpu_start: App version:      v2.2-252-g62edbd26-dirty
I (453) cpu_start: Compile time:     Nov 25 2021 20:39:37
I (459) cpu_start: ELF file SHA256:  ab0e1b23954cb0ea...
I (465) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (471) cpu_start: Starting app cpu, entry point is 0x400818b4
0x400818b4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (482) heap_init: Initializing. RAM available for dynamic allocation:
I (489) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (495) heap_init: At 3FFB79F8 len 00028608 (161 KiB): DRAM
I (501) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (507) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (514) heap_init: At 40095804 len 0000A7FC (41 KiB): IRAM
I (520) cpu_start: Pro cpu start user code
I (538) spi_flash: detected chip: gd
I (539) spi_flash: flash io: dio
W (539) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (549) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (621) wpa2_enterprise: [ 0 ] Initialize peripherals
I (621) wpa2_enterprise: [ 1 ] Set Wi-Fi config
I (621) wpa2_enterprise: [ 2 ] Start bt peripheral
I (631) wpa2_enterprise: [ 3 ] Get ip info
I (2631) wpa2_enterprise: IP:0.0.0.0
I (2631) wpa2_enterprise: MASK:0.0.0.0
I (2631) wpa2_enterprise: GW:0.0.0.0
W (3981) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4631) wpa2_enterprise: IP:192.168.51.185
I (4631) wpa2_enterprise: MASK:255.255.255.0
I (4631) wpa2_enterprise: GW:192.168.51.1
I (6631) wpa2_enterprise: IP:192.168.51.185
I (6631) wpa2_enterprise: MASK:255.255.255.0
I (6631) wpa2_enterprise: GW:192.168.51.1
I (8631) wpa2_enterprise: IP:192.168.51.185
I (8631) wpa2_enterprise: MASK:255.255.255.0
I (8631) wpa2_enterprise: GW:192.168.51.1
I (10631) wpa2_enterprise: IP:192.168.51.185
I (10631) wpa2_enterprise: MASK:255.255.255.0
I (10631) wpa2_enterprise: GW:192.168.51.1
I (12631) wpa2_enterprise: IP:192.168.51.185
I (12631) wpa2_enterprise: MASK:255.255.255.0
I (12631) wpa2_enterprise: GW:192.168.51.1
I (14631) wpa2_enterprise: IP:192.168.51.185
I (14631) wpa2_enterprise: MASK:255.255.255.0
I (14631) wpa2_enterprise: GW:192.168.51.1
I (16631) wpa2_enterprise: IP:192.168.51.185
I (16631) wpa2_enterprise: MASK:255.255.255.0
I (16631) wpa2_enterprise: GW:192.168.51.1
I (18631) wpa2_enterprise: IP:192.168.51.185
I (18631) wpa2_enterprise: MASK:255.255.255.0
I (18631) wpa2_enterprise: GW:192.168.51.1
I (20631) wpa2_enterprise: IP:192.168.51.185
I (20631) wpa2_enterprise: MASK:255.255.255.0
I (20631) wpa2_enterprise: GW:192.168.51.1
I (22631) wpa2_enterprise: IP:192.168.51.185
I (22631) wpa2_enterprise: MASK:255.255.255.0
I (22631) wpa2_enterprise: GW:192.168.51.1
I (24631) wpa2_enterprise: IP:192.168.51.185
I (24631) wpa2_enterprise: MASK:255.255.255.0
I (24631) wpa2_enterprise: GW:192.168.51.1
I (26631) wpa2_enterprise: IP:192.168.51.185
I (26631) wpa2_enterprise: MASK:255.255.255.0
I (26631) wpa2_enterprise: GW:192.168.51.1
I (28631) wpa2_enterprise: IP:192.168.51.185
I (28631) wpa2_enterprise: MASK:255.255.255.0
I (28631) wpa2_enterprise: GW:192.168.51.1
I (30631) wpa2_enterprise: IP:192.168.51.185
I (30631) wpa2_enterprise: MASK:255.255.255.0
I (30631) wpa2_enterprise: GW:192.168.51.1
I (32631) wpa2_enterprise: IP:192.168.51.185
I (32631) wpa2_enterprise: MASK:255.255.255.0
I (32631) wpa2_enterprise: GW:192.168.51.1
I (34631) wpa2_enterprise: IP:192.168.51.185
I (34631) wpa2_enterprise: MASK:255.255.255.0
I (34631) wpa2_enterprise: GW:192.168.51.1

Done
```

- The partial log of the enterprise encryption server side is as follows:

```c
hengyongchao@rocket2mfg:/etc/hostapd$ sudo hostapd -dddt /etc/hostapd/hostapd.conf
1637845172.146848: random: Trying to read entropy from /dev/random
1637845172.146869: Configuration file: /etc/hostapd/hostapd.conf
1637845172.146908: Line 6: DEPRECATED: 'dump_file' configuration variable is not used anymore
1637845172.146960: Completing interface initialization
1637845172.146966: hostapd_setup_bss(hapd=0x559416fb13f0 (), first=1)
1637845172.148585: OpenSSL: tls_use_private_key_file (PEM) --> loaded
1637845172.148761: : interface state UNINITIALIZED->ENABLED
1637845172.148769: : AP-ENABLED
1637845172.148774: : Setup of interface done.
1637845172.148780: ctrl_iface not configured!
1637845172.148803: random: Got 20/20 bytes from /dev/random
1637845189.979728: RADIUS SRV: Received 155 bytes from 192.168.51.1:44318
1637845189.979766: RADIUS SRV: Received data - hexdump(len=155): 01 00 00 9b c1 d9 34 c9 6c 0d 56 a4 28 08 35 5a 35 f1 6a a2 01 17 65 78 61 6d 70 6c 65 40 65 73 70 72 65 73 73 69 66 2e 63 6f 6d 04 06 c0 a8 33 01 1e 0e 61 63 39 65 31 37 62 30 30 66 65 38 1f 0e 39 34 62 39 37 65 36 35 63 32 34 34 20 0e 61 63 39 65 31 37 62 30 30 66 65 38 05 06 00 00 00 23 0c 06 00 00 05 78 3d 06 00 00 00 13 4f 1c 02 00 00 1a 01 65 78 61 6d 70 6c 65 40 65 73 70 72 65 73 73 69 66 2e 63 6f 6d 50 12 f7 17 68 28 3e 50 61 bf 66 20 9b 2c 93 18 b6 95
1637845189.979868: RADIUS message: code=1 (Access-Request) identifier=0 length=155
1637845189.979882:    Attribute 1 (User-Name) length=23
1637845189.979897:       Value: 'example@espressif.com'
1637845189.979908:    Attribute 4 (NAS-IP-Address) length=6
1637845189.979925:       Value: 192.168.51.1
1637845189.979935:    Attribute 30 (Called-Station-Id) length=14
1637845189.979948:       Value: 'ac9e17b00fe8'
1637845189.979964:    Attribute 31 (Calling-Station-Id) length=14
1637845189.979972:       Value: '94b97e65c244'
1637845189.979988:    Attribute 32 (NAS-Identifier) length=14
1637845189.980027:       Value: 'ac9e17b00fe8'
1637845189.980032:    Attribute 5 (NAS-Port) length=6
1637845189.980041:       Value: 35
1637845189.980051:    Attribute 12 (Framed-MTU) length=6
1637845189.980062:       Value: 1400
1637845189.980073:    Attribute 61 (NAS-Port-Type) length=6
1637845189.980084:       Value: 19
1637845189.980100:    Attribute 79 (EAP-Message) length=28
1637845189.980115:       Value: 0200001a016578616d706c65406573707265737369662e636f6d
1637845189.980128:    Attribute 80 (Message-Authenticator) length=18
1637845189.980139:       Value: f71768283e5061bf66209b2c9318b695
1637845189.980169: RADIUS SRV: Creating a new session
1637845189.980179: RADIUS SRV: User-Name - hexdump_ascii(len=21):
     65 78 61 6d 70 6c 65 40 65 73 70 72 65 73 73 69   example@espressi
     66 2e 63 6f 6d                                    f.com
1637845189.980211: RADIUS SRV: Matching user entry found
1637845189.980226: RADIUS SRV: [0x0 192.168.51.1] New session created
1637845189.980240: EAP: Server state machine created
1637845189.980251: RADIUS SRV: New session 0x0 initialized
1637845189.980262: RADIUS SRV: Received EAP data - hexdump(len=26): 02 00 00 1a 01 65 78 61 6d 70 6c 65 40 65 73 70 72 65 73 73 69 66 2e 63 6f 6d
1637845189.980283: EAP: EAP entering state INITIALIZE
1637845189.980293: EAP: parseEapResp: rxResp=1 rxInitiate=0 respId=0 respMethod=1 respVendor=0 respVendorMethod=0
1637845189.980315: : CTRL-EVENT-EAP-STARTED 00:00:00:00:00:00
1637845189.980324: EAP: EAP entering state PICK_UP_METHOD
1637845189.980339: : CTRL-EVENT-EAP-PROPOSED-METHOD method=1
1637845189.980350: EAP: EAP entering state METHOD_RESPONSE
1637845189.980356: EAP-Identity: Peer identity - hexdump_ascii(len=21):
     65 78 61 6d 70 6c 65 40 65 73 70 72 65 73 73 69   example@espressi
     66 2e 63 6f 6d                                    f.com
1637845189.980392: RADIUS SRV: [0x0 192.168.51.1] EAP: EAP-Response/Identity 'example@espressif.com'
1637845189.980404: EAP: EAP entering state SELECT_ACTION
1637845189.980410: EAP: getDecision: another method available -> CONTINUE
1637845189.980418: EAP: EAP entering state PROPOSE_METHOD
1637845189.980423: EAP: getNextMethod: vendor 0 type 25
1637845189.980501: : CTRL-EVENT-EAP-PROPOSED-METHOD vendor=0 method=25
1637845189.980514: RADIUS SRV: [0x0 192.168.51.1] EAP: Propose EAP method vendor=0 method=25
1637845189.980523: EAP: EAP entering state METHOD_REQUEST
1637845189.980533: EAP: building EAP-Request: Identifier 1
1637845189.980545: EAP-PEAP: START -> PHASE1
1637845189.980557: EAP: EAP entering state SEND_REQUEST
1637845189.980570: EAP: EAP entering state IDLE
1637845189.980579: EAP: retransmit timeout 3 seconds (from dynamic back off; retransCount=0)
1637845189.980590: RADIUS SRV: EAP data from the state machine - hexdump(len=6): 01 01 00 06 19 21
1637845189.980646: RADIUS SRV: Reply to 192.168.51.1:44318
1637845189.980656: RADIUS message: code=11 (Access-Challenge) identifier=0 length=52
1637845189.980665:    Attribute 24 (State) length=6
1637845189.980680:       Value: 00000000
1637845189.980690:    Attribute 79 (EAP-Message) length=8
1637845189.980705:       Value: 010100061921
1637845189.980715:    Attribute 80 (Message-Authenticator) length=18
1637845189.980736:       Value: 4a3f5a94358b72f872e37e21a9ff559f
1637845190.009072: RADIUS SRV: Received 361 bytes from 192.168.51.1:44318

...

```


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
