# Wi-Fi Power Save

- [中文版本](./README_CN.md)
- Regular Example: ![alt text](../../../docs/_static/level_regular.png "Regular Example")


## Example Brief

This example demonstrates the implementation of Wi-Fi Low-power (Auto Light-sleep mode) management under the ADF framework and the actual use of wake-up sources in different modes. If you only want to test the average current of Wi-Fi under low power consumption, you only need to set `WIFI_POWER_SAVE_TEST` to 1. Otherwise, the wake-up process in Wi-Fi Low-power mode and wake-up in Deep-sleep mode would be executed. There are many wake-up sources under the Wi-Fi Low-power mode. This example only introduces the use of GPIO, UART, and RTC Timer wake-up sources. After the wake-up source test is completed, the wake-up in Wi-Fi Low-power mode is realized by registering GPIO/EXT wake-up sources. Generally, the application needs to register the wake-up source before entering the Low-power mode, and wait for the GPIO, UART, and other wake-up events to arrive. After a successful wake-up, it prevents the system from automatically entering the Low-power mode by acquiring the power management lock, thus ensuring the normal execution of the user program.


### Prerequisites

Auto Light-sleep is a way to automatically enter low power consumption through FreeRTOS idle task based on Light-sleep mode. For details, see:  
`portTASK_FUNCTION() --> portSUPPRESS_TICKS_AND_SLEEP() --> vApplicationSleep() --> enable timer wakeup and enter sleep`

Corresponding to different working states of the system (Wi-Fi TX, Wi-Fi RX, Modem-sleep (RF off), Light-sleep, Deep-sleep), the chip mainly has five current levels, and the typical value range is shown in the table below (slightly different from different chips, Wi-Fi TX current is related to transmission power. The higher the transmission power, the higher the TX current theoretically). Through the current waveform, the state of the chip can be effectively distinguished. When in the Deep-sleep and Light-sleep modes, the current is small and can be distinguished only with the help of more precise current detection equipment. In the Wi-Fi Low-power consumption mode, the typical current waveforms are Light-sleep, Modem-sleep, Wi-Fi RX, and Wi-Fi TX in descending order. One can identify the mode by observing four current steps through the ammeter.

| Mode    | Deep-sleep | Light-sleep | Modem-sleep | Wi-Fi RX  | Wi-Fi TX   |
| ------- | ---------- | ----------- | ----------- | --------- | ---------- |
| Current | 5-20 uA    | 100-800 uA  | 15-25 mA    | 70-110 mA | 200-300 mA |

Sleep mode and wake-up source:
[Sleep mode](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/api-reference/system/sleep_modes.html),
[Reference manual](https://www.espressif.com.cn/sites/default/files/documentation/esp32-s3_technical_reference_manual_cn.pdf),
[Deep-sleep wakeup stub](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/api-guides/deep-sleep-stub.html)

Low power management:
[Power management](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/api-reference/system/power_management.html),
[FreeRTOS low power support](https://freertos.org/zh-cn-cmn-s/low-power-tickless-rtos.html)

Wi-Fi related knowledge:
[Wi-Fi FAQ](https://docs.espressif.com/projects/espressif-esp-faq/zh_CN/latest/software-framework/wifi.html#),
[Wi-Fi Driver](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/wifi.html)

Reference examples:
[Wi-Fi power save](https://github.com/espressif/esp-idf/tree/master/examples/wifi/power_save),
[Light-sleep](https://github.com/espressif/esp-idf/tree/master/examples/system/light_sleep),
[Deep-sleep](https://github.com/espressif/esp-idf/tree/master/examples/system/deep_sleep),
[Deep-sleep wakeup stub](https://github.com/espressif/esp-idf/tree/master/examples/system/deep_sleep_wake_stub),
[Startup time](https://github.com/espressif/esp-idf/tree/master/examples/system/startup_time)


## Environment Setup

### Hardware Required

This example runs on the boards that are marked with a green checkbox in the [table](../../README.md#compatibility-of-examples-with-espressif-audio-boards). Please remember to select the board in menuconfig as discussed in Section [Configuration](#configuration) below.


## Build and Flash

### Default IDF Branch

This example supports IDF release/v4.4 and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

### Configuration

The default development board of this example is `ESP32-S3-Korvo-2 v3`. Since the HTTP task stack is allocated in PSRAM by default, it is required to apply `idf_v4.4_freertos.patch` in the `$ADF_PATH/esp-idf` directory. If you select `ESP32-C3-Lyra-v2.0`, in order to get better sound quality, you need to apply `idf_v4.4_i2s_c3_pdm_tx.patch` in the `$ADF_PATH/esp-idf` directory. After selecting the development board, you also need to configure the network name and password for the network connection.

```
menuconfig > Audio HAL > ESP32-S3-Korvo-2 v3
menuconfig > Example Configuration > WiFi SSID
menuconfig > Example Configuration > WiFi Password
```

**Notes:**

1. When Wi-Fi enters the Low-power mode, if the MAX modem sleep mode is configured, the listen interval parameter is used as the wake-up cycle. If the MIN modem sleep mode is configured, the AP DTIM cycle is used as the wake-up cycle. In order to ensure timely and accurate packet-receiving, the external 32.768 khz crystal/oscillator needs to be reserved on the hardware. If an internal RC oscillator is used, it may lead to inaccurate packet reception and wake-up, causing multiple wake-up packet receptions and increasing power consumption. If the hardware does not have an external 32.768 khz crystal/oscillator, but the `RTC clock source` is set to `External 32kHz crystal`, the system will automatically switch to the internal RC oscillator during operation. For ESP32S3, this method will cause I2S clock output abnormality. If the configuration in menuconfig matches the actual hardware, this problem will not occur.

2. When Wi-Fi is in the Low-power consumption mode, its power consumption mainly comes from several aspects: the bottom current during Light-sleep, the Wi-Fi wake-up cycle and the size of beacon packet data, data frame transceiver frequency, and the energy consumption of other tasks.
For example, for the ESP32S3 module, the typical value of Light-sleep bottom current is 250 uA, the default Wi-Fi wake-up period (DTIM10 in MIN Modem-sleep mode or Wi-Fi listen interval is 10 in MAX Modem-sleep mode) is 1.024 s, and the packet receiving duration is about 3 ms (the size of the beacon packet is about 300 bytes). The timer wake-up cycle of `ARP_TMR_INTERVAL` and `DHCP_COARSE_TIMER_MSECS` is 1 s, which can be manually changed to 10 s.

3. When entering Deep-sleep, if the GPIO/EXT0/EXT1 wake-up source is configured, it is recommended to connect the pull-up or pull-down resistance externally. The resistance value needs to be checked, otherwise, the power consumption will be higher or the system cannot be waked up stably. In order to maintain the GPIO output level unchanged in the deep-sleep mode (such as maintaining high level), call `gpio_hold_en()` and `gpio_deep_sleep_hold_en()`.

4. ESP32S3 uses 8-line PSRAM by default. If it cannot be executed normally after downloading, consider reconfiguring the PSRAM:  
`Component config --> ESP32S3 Specific --> Support for external, SPI-connected RAM --> SPI RAM config --> Mode (QUAD/OCT) of SPI RAM chip in use`. ( QUAD is 4-line and OCT is 8-line )

5. In order to ensure that the four chips ESP32C3, ESP32, ESP32S2, and ESP32S3 can play the HTTP audio data stream normally and realize the Wi-Fi Low-power and wake-up functions, this example turns off `WiFi IRAM speed optimization` and `WiFi RX IRAM speed optimization` by default to reduce the use of IRAM, but will have a certain impact on Wi-Fi performance. For details, please refer to the specific description of this parameter in menuconfig. In the actual application, you can decide whether to turn off this option according to the needs of the project.

6. The design of some development boards' pins does not take into account the low power consumption requirements. The development boards `ESP32-S3-Korvo-2 v3` and `ESP32-S2-Kaluga-1` will automatically trigger wake-up when they use the BOOT pin to wake up in Deep-sleep, while the corresponding core board can wake up normally. For details, please refer to the [Deep-sleep Example](https://github.com/espressif/esp-idf/tree/master/examples/system/deep_sleep) and the document reference instructions.


### Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output (replace `PORT` with your board's serial port name):

```
idf.py -p PORT flash monitor
```

To exit the serial monitor, type ``Ctrl-]``.

See [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/esp32s3/index.html) for full steps to configure and build an ESP-IDF project.

## How to Use the Example

### Example Functionality

After the example starts running, the following process will be executed circularly:  
System initialization (such as Wi-Fi, codec, etc.) --> Normal operation of the system (such as obtaining audio data through HTTP and playing) --> Pause or terminate non-Wi-Fi related tasks (such as audio decoding, playback and other tasks) --> Enable Light-sleep wake-up source, enter Wi-Fi low-power mode, and wait for the wake-up source to arrive.  
Notice:
1. After entering Low-power consumption, if wake-up is not performed for a long time, the timer wake-up source will wake up 20 seconds later by default.
2. If you use the GPIO wake-up source, just press the BOOT key.
3. If you use the UART wake-up source, just send "123456789ABCED\r\n" or any longer characters through console (uart0).

After the above wake-up process reaches four times, it will automatically enter Deep-sleep and enable the wake-up source as required. By default, the EXT0 wake-up source is enabled. The user can wake up by pressing the BOOT button.
Since the ESP32C3 does not support an EXT0/EXT1 wake-up source, the GPIO wake-up source can be used instead. You can wake up by pressing the 'VOL+' button on the `ESP32-C3-Lyra-v2.0` development board instead of the BOOT button.  
The key log is as follows:

```c
I (1471) AUDIO_SLEEP_WAKEUP: Uart wakeup source is ready
I (1471) AUDIO_SLEEP_WAKEUP: Power manage init ok
E (23299) AUDIO_ELEMENT: [http] Element already stopped
E (23300) AUDIO_ELEMENT: [mp3] Element already stopped
E (23300) AUDIO_ELEMENT: [i2s] Element already stopped
I (23312) AUDIO_SLEEP_WAKEUP: Enter power manage
I (23312) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (23318) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
W (25306) AUDIO_SLEEP_WAKEUP: Send uart wakeup group event
W (25306) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: UART
I (25316) AUDIO_SLEEP_WAKEUP: Uart0 recved event:0
I (25316) AUDIO_SLEEP_WAKEUP: [UART DATA]: 7
▒ (25318) AUDIO_SLEEP_WAKEUP: [DATA EVT]: ▒▒
E (46159) AUDIO_ELEMENT: [http] Element already stopped
E (46159) AUDIO_ELEMENT: [mp3] Element already stopped
E (46160) AUDIO_ELEMENT: [i2s] Element already stopped
I (46176) AUDIO_SLEEP_WAKEUP: Enter power manage
I (46176) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (46178) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
I (49532) AUDIO_SLEEP_WAKEUP: GPIO[0], val: 0
W (49535) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: GPIO
E (70532) AUDIO_ELEMENT: [http] Element already stopped
E (70533) AUDIO_ELEMENT: [mp3] Element already stopped
E (70533) AUDIO_ELEMENT: [i2s] Element already stopped
I (70550) AUDIO_SLEEP_WAKEUP: Enter power manage
I (70550) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (70551) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
W (90669) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: TIMER
E (109624) AUDIO_ELEMENT: [http] Element already stopped
E (109625) AUDIO_ELEMENT: [mp3] Element already stopped
E (109626) AUDIO_ELEMENT: [i2s] Element already stopped
I (109721) AUDIO_SLEEP_WAKEUP: Enter power manage
I (109722) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (109722) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
I (112743) AUDIO_SLEEP_WAKEUP: GPIO[0], val: 0
W (112743) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: GPIO
I (117754) AUDIO_SLEEP_WAKEUP: Enable EXT0 wakeup in deep sleep
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x5 (DSLEEP),boot:0x2a (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x1674
load:0x403c9700,len:0xbc0
load:0x403cc700,len:0x2f3c
entry 0x403c9950

```


### Example Log

The complete log is as follows:

```c
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x2a (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x1674
load:0x403c9700,len:0xbc0
load:0x403cc700,len:0x2f3c
entry 0x403c9950
I (25) boot: ESP-IDF v4.4.3-316-ge86181704a-dirty 2nd stage bootloader
I (25) boot: compile time 19:15:31
I (25) boot: chip revision: 0
I (28) boot.esp32s3: Boot SPI Speed : 80MHz
I (33) boot.esp32s3: SPI Mode       : DIO
I (38) boot.esp32s3: SPI Flash Size : 4MB
I (42) boot: Enabling RNG early entropy source...
I (48) boot: Partition Table:
I (51) boot: ## Label            Usage          Type ST Offset   Length
I (59) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (66) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (74) boot:  2 factory          factory app      00 00 00010000 00200000
I (81) boot: End of partition table
I (85) esp_image: segment 0: paddr=00010020 vaddr=3c0b0020 size=26120h (155936) map
I (122) esp_image: segment 1: paddr=00036148 vaddr=3fc9ade0 size=048b4h ( 18612) load
I (126) esp_image: segment 2: paddr=0003aa04 vaddr=40374000 size=05614h ( 22036) load
I (132) esp_image: segment 3: paddr=00040020 vaddr=42000020 size=a1b00h (662272) map
I (255) esp_image: segment 4: paddr=000e1b28 vaddr=40379614 size=117c4h ( 71620) load
I (271) esp_image: segment 5: paddr=000f32f4 vaddr=600fe000 size=0002ch (    44) load
I (280) boot: Loaded app from partition at offset 0x10000
I (280) boot: Disabling RNG early entropy source...
I (292) opi psram: vendor id : 0x0d (AP)
I (292) opi psram: dev id    : 0x02 (generation 3)
I (292) opi psram: density   : 0x03 (64 Mbit)
I (296) opi psram: good-die  : 0x01 (Pass)
I (300) opi psram: Latency   : 0x01 (Fixed)
I (305) opi psram: VCC       : 0x01 (3V)
I (310) opi psram: SRF       : 0x01 (Fast Refresh)
I (315) opi psram: BurstType : 0x01 (Hybrid Wrap)
I (321) opi psram: BurstLen  : 0x01 (32 Byte)
I (326) opi psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (332) opi psram: DriveStrength: 0x00 (1/1)
I (337) spiram: Found 64MBit SPI RAM device
I (342) spiram: SPI RAM mode: sram 40m
I (346) spiram: PSRAM initialized, cache is in normal (1-core) mode.
I (353) cpu_start: Pro cpu up.
I (357) cpu_start: Starting app cpu, entry point is 0x40375764
0x40375764: call_start_cpu1 at /home/dailei/workspace/internal/esp-adf-internal/esp-idf/components/esp_system/port/cpu_start.c:148

I (0) cpu_start: App cpu up.
I (1094) spiram: SPI SRAM memory test OK
W (1305) clk: 32 kHz XTAL not found, switching to internal 150 kHz oscillator
I (1313) cpu_start: Pro cpu start user code
I (1313) cpu_start: cpu freq: 240000000
I (1313) cpu_start: Application information:
I (1316) cpu_start: Project name:     power_save
I (1322) cpu_start: App version:      v2.4-179-g6dd42bac-dirty
I (1328) cpu_start: Compile time:     Jan 11 2023 19:15:27
I (1334) cpu_start: ELF file SHA256:  f01e0c851ff0486d...
I (1340) cpu_start: ESP-IDF:          v4.4.3-316-ge86181704a-dirty
I (1347) heap_init: Initializing. RAM available for dynamic allocation:
I (1354) heap_init: At 3FCA30D0 len 00046640 (281 KiB): D/IRAM
I (1361) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
I (1368) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (1374) heap_init: At 600FE02C len 00001FD4 (7 KiB): RTCRAM
I (1380) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (1389) spi_flash: detected chip: gd
I (1393) spi_flash: flash io: dio
W (1397) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1421) sleep: Configure to isolate all GPIO pins in sleep state
I (1422) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1432) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
W (1441) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: Undefined 0x1
I (1471) AUDIO_SLEEP_WAKEUP: Uart wakeup source is ready
I (1471) AUDIO_SLEEP_WAKEUP: Power manage init ok
E (23299) AUDIO_ELEMENT: [http] Element already stopped
E (23300) AUDIO_ELEMENT: [mp3] Element already stopped
E (23300) AUDIO_ELEMENT: [i2s] Element already stopped
I (23312) AUDIO_SLEEP_WAKEUP: Enter power manage
I (23312) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (23318) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
W (25306) AUDIO_SLEEP_WAKEUP: Send uart wakeup group event
W (25306) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: UART
I (25316) AUDIO_SLEEP_WAKEUP: Uart0 recved event:0
I (25316) AUDIO_SLEEP_WAKEUP: [UART DATA]: 7
▒ (25318) AUDIO_SLEEP_WAKEUP: [DATA EVT]: ▒▒
E (46159) AUDIO_ELEMENT: [http] Element already stopped
E (46159) AUDIO_ELEMENT: [mp3] Element already stopped
E (46160) AUDIO_ELEMENT: [i2s] Element already stopped
I (46176) AUDIO_SLEEP_WAKEUP: Enter power manage
I (46176) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (46178) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
I (49532) AUDIO_SLEEP_WAKEUP: GPIO[0], val: 0
W (49535) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: GPIO
E (70532) AUDIO_ELEMENT: [http] Element already stopped
E (70533) AUDIO_ELEMENT: [mp3] Element already stopped
E (70533) AUDIO_ELEMENT: [i2s] Element already stopped
I (70550) AUDIO_SLEEP_WAKEUP: Enter power manage
I (70550) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (70551) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
W (90669) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: TIMER
E (109624) AUDIO_ELEMENT: [http] Element already stopped
E (109625) AUDIO_ELEMENT: [mp3] Element already stopped
E (109626) AUDIO_ELEMENT: [i2s] Element already stopped
I (109721) AUDIO_SLEEP_WAKEUP: Enter power manage
I (109722) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (109722) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready
I (112743) AUDIO_SLEEP_WAKEUP: GPIO[0], val: 0
W (112743) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: GPIO
I (117754) AUDIO_SLEEP_WAKEUP: Enable EXT0 wakeup in deep sleep
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x5 (DSLEEP),boot:0x2a (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x1674
load:0x403c9700,len:0xbc0
load:0x403cc700,len:0x2f3c
entry 0x403c9950
I (24) boot: ESP-IDF v4.4.3-316-ge86181704a-dirty 2nd stage bootloader
I (25) boot: compile time 19:15:31
I (25) boot: chip revision: 0
I (28) boot.esp32s3: Boot SPI Speed : 80MHz
I (33) boot.esp32s3: SPI Mode       : DIO
I (38) boot.esp32s3: SPI Flash Size : 4MB
I (42) boot: Enabling RNG early entropy source...
I (48) boot: Partition Table:
I (51) boot: ## Label            Usage          Type ST Offset   Length
I (59) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (66) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (74) boot:  2 factory          factory app      00 00 00010000 00200000
I (81) boot: End of partition table
I (85) esp_image: segment 0: paddr=00010020 vaddr=3c0b0020 size=26120h (155936) map
I (122) esp_image: segment 1: paddr=00036148 vaddr=3fc9ade0 size=048b4h ( 18612) load
I (126) esp_image: segment 2: paddr=0003aa04 vaddr=40374000 size=05614h ( 22036) load
I (132) esp_image: segment 3: paddr=00040020 vaddr=42000020 size=a1b00h (662272) map
I (255) esp_image: segment 4: paddr=000e1b28 vaddr=40379614 size=117c4h ( 71620) load
I (271) esp_image: segment 5: paddr=000f32f4 vaddr=600fe000 size=0002ch (    44)
I (280) boot: Loaded app from partition at offset 0x10000
I (280) boot: Disabling RNG early entropy source...
I (292) opi psram: vendor id : 0x0d (AP)
I (292) opi psram: dev id    : 0x02 (generation 3)
I (292) opi psram: density   : 0x03 (64 Mbit)
I (296) opi psram: good-die  : 0x01 (Pass)
I (300) opi psram: Latency   : 0x01 (Fixed)
I (305) opi psram: VCC       : 0x01 (3V)
I (310) opi psram: SRF       : 0x01 (Fast Refresh)
I (315) opi psram: BurstType : 0x01 (Hybrid Wrap)
I (321) opi psram: BurstLen  : 0x01 (32 Byte)
I (326) opi psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (332) opi psram: DriveStrength: 0x00 (1/1)
I (337) spiram: Found 64MBit SPI RAM device
I (342) spiram: SPI RAM mode: sram 40m
I (346) spiram: PSRAM initialized, cache is in normal (1-core) mode.
I (353) cpu_start: Pro cpu up.
I (357) cpu_start: Starting app cpu, entry point is 0x40375764
0x40375764: call_start_cpu1 at /home/dailei/workspace/internal/esp-adf-internal/esp-idf/components/esp_system/port/cpu_start.c:148

I (0) cpu_start: App cpu up.
I (1094) spiram: SPI SRAM memory test OK
W (1305) clk: 32 kHz XTAL not found, switching to internal 150 kHz oscillator
I (1313) cpu_start: Pro cpu start user code
I (1313) cpu_start: cpu freq: 240000000
I (1313) cpu_start: Application information:
I (1316) cpu_start: Project name:     power_save
I (1321) cpu_start: App version:      v2.4-179-g6dd42bac-dirty
I (1328) cpu_start: Compile time:     Jan 11 2023 19:15:27
I (1334) cpu_start: ELF file SHA256:  f01e0c851ff0486d...
I (1340) cpu_start: ESP-IDF:          v4.4.3-316-ge86181704a-dirty
I (1347) heap_init: Initializing. RAM available for dynamic allocation:
I (1354) heap_init: At 3FCA30D0 len 00046640 (281 KiB): D/IRAM
I (1361) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
I (1368) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (1374) heap_init: At 600FE02C len 00001FD4 (7 KiB): RTCRAM
I (1380) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (1389) spi_flash: detected chip: gd
I (1393) spi_flash: flash io: dio
W (1397) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1421) sleep: Configure to isolate all GPIO pins in sleep state
I (1422) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1432) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
W (1441) AUDIO_SLEEP_WAKEUP: Returned from sleep, reason: EXT0
I (1470) AUDIO_SLEEP_WAKEUP: Uart wakeup source is ready
I (1470) AUDIO_SLEEP_WAKEUP: Power manage init ok
E (23074) AUDIO_ELEMENT: [http] Element already stopped
E (23074) AUDIO_ELEMENT: [mp3] Element already stopped
E (23075) AUDIO_ELEMENT: [i2s] Element already stopped
I (23087) AUDIO_SLEEP_WAKEUP: Enter power manage
I (23087) AUDIO_SLEEP_WAKEUP: Timer wakeup source is ready
I (23093) AUDIO_SLEEP_WAKEUP: Gpio wakeup source is ready

```


## Troubleshooting

1. High power consumption (check real-time current through ammeter, etc.):
* If the Light-sleep bottom current is larger than expected, it is considered to be a hardware circuit design-related problem. The results of this test are all module currents.
* If the packet receiving duration is longer than expected (5-10 ms), it is considered to be the router packet size problem. You can check the actual packet size through network packet capturing, or change the router for testing.
* If the peak value of packet receiving current occurs repeatedly with an interval of 100 ms, the network environment problem should be considered. The `Wifi sleep optimize when beacon lost` parameter can be increased to achieve more accurate packet receiving.
* In case of periodic Modem-sleep current peak, it is considered that some tasks are being executed, for example, the lwip timer wakes up once a second. After the troubleshooting, you can decide whether to terminate the task as needed.
* If the system cannot sleep, consider whether any task delay cycle is less than the setting value of the `Minimum number of ticks to enter sleep mode for`, or whether the power management lock has not been released. You can open the `Enable profiling counters for PM locks` option in menuconfig and periodically call `esp_pm_dump_locks(stdout)` to view the information such as the current management lock occupation time. After debugging, you need to turn off this option to reduce the cost. GPIO debugging also can be realized by opening the `Enable debug tracing of PM using GPIOs` option. See for details [pm_trace.c](https://github.com/espressif/esp-idf/blob/release/v4.4/components/esp_pm/pm_trace.c).
* If the actual test is slightly different from the image folder, it may be a module batch problem, or wait for a period of time, and then test again after the network environment is good.

2. GPIO wake-up problem:
* To ensure that GPIO can wake up the system successfully, it is necessary to configure GPIO to level trigger mode, and timely prohibit GPIO interrupt source in the interrupt callback function to prevent frequent triggering of interrupts.
* `esp_pm_configure()` calls `esp_sleep_enable_gpio_switch()` to reduce power consumption, which will change the GPIO level state automatically when entering and exiting Light-sleep, causing multiple triggering interrupts. You should call `esp_pm_configure()` before `gpio_sleep_sel_dis()` to ensure that the GPIO level in Light-sleep mode is consistent with Modem-sleep mode. See `Disable all GPIO when chip at sleep` for details.
* After waking up the system successfully, in order to not affect the execution of subsequent applications, it is recommended to reset the GPIO wake-up source to its normal state and re-register the wake-up source when needs to be used.

3. UART wake-up problem:
* To ensure that UART wakes up the system in time, it needs to send a certain number of bytes. At 115200 baud rate, sending "123456789ABCDE\r\n" or longer bytes can effectively wake up the system. At higher baud rate, it needs to send more bytes to effectively wake up. After wake-up, the reliability of the received data can be guaranteed according to the specific protocol.
* Sometimes sending a wake-up string cannot wake up the system accurately. The wake-up character can be sent continuously for times until the system wakes up. Both parties can realize this process through protocol communication.
* The best way to wake up is to create a power management lock (the lock type depends on the specific business needs) and a semaphore, release the semaphore in the UART ISR, and block the semaphore in the UART task. After acquiring the semaphore, immediately acquire the power management lock to prevent the system from automatically entering the sleep state, and then conduct normal serial port data transmission. You can call `esp_pm_lock_release()` when you need to enter the Low-power consumption mode to releases the power management lock, and the system will automatically enter the sleep state at the appropriate time.

4. RTC Timer wake-up problem:  
In Auto Light-sleep mode, the idle task will automatically register the timer wake-up source by calling `esp_sleep_enable_timer_Wakeup()`. If the application also calls the API, it will be overwritten by the idle task. Users can use `esp_timer_create()` to recreate the Timer wake-up source, When parameter `skip_unhandled_events = false`, the system wakes up at a fixed time.

5. Serial port output is intermittent in Light-sleep mode:  
When using function `esp_light_sleep_start()` to enter Light-sleep mode, UART FIFO will not be flushed. In contrast, UART output will be suspended, and the remaining characters in FIFO will be sent after Light-sleep wakes up. You can add `uart_wait_tx_idle_polling()` after the location that you want to send to achieve the effect of entering sleep after sending.

6. In Auto Light-sleep mode, entering Deep-sleep will be automatically wakened by the timer:  
In Auto Light-sleep mode, the timer will be enabled in idle task to wake up. If you call `vTaskDelay()` or another API that will cause the idle task to run before `esp_deep_sleep_start()`, it will wake up within a period of time after entering deep sleep. One solution is to call `esp_sleep_enable_timer_wakeup(portMAX_DELAY)` before `esp_deep_sleep_start()` to Reconfigure the timer wake-up time.

7. The application runs abnormally in Low-power consumption mode:  
When Power Management is enabled, the system will run between `max_freq_Mhz` and `min_freq_mhz` by dynamic frequency modulation. So, for some tasks that require system frequency (CPU and Bus frequency), it is necessary to create and maintain power management locks that meet the task requirements, such as `ESP_PM_CPU_FREQ_MAX`.

8. Whether cloud data can wake up the system:  
The IEEE802.11 protocol has the protocol specification for power save, which is now supported. For IP and above levels, the system sleep and wake-up receive data are insensitive.

9. Once the TCP connection is established, the power consumption in the low power consumption mode is large for a period of time:  
When using HTTP to obtain data streams, although Wi-Fi is turned off, TCP connect is in a wait time state and will continue to wait for 2 MSL (Maximum Segment Lifetime) time. The default time is two minutes. You can configure the `Maximum segment lifetime` in menuconfig. It is generally not recommended to change it. After the two MSL waiting time is over, the TCP is properly closed, and the power consumption reaches the normal state. The default value of `TCP timer interval` is 250ms, after the TCP connection is enabled, the power consumption will increase. Increasing the interval time can reduce power consumption, but it is not recommended to change it.

10. Accelerate chip startup:  
* If there are certain requirements for chip startup time, the following configuration can be made to reduce startup time and ensure that there is no warning error and other information output during startup.  
`Bootloader config --> Bootloader log verbosity --> No output`  
`Component config --> ESP32S3-Specific --> Support for external, SPI-connected RAM --> SPI RAM config --> Run memory test on SPI RAM initialization` (Uncheck it)  
`Component config --> Log output --> Default log verbosity --> Warning`
* If you need to further shorten the startup time, you can try to use the following configuration, but it may have some impact on the system operation:  
`Bootloader config --> Skip image validation from power on reset (READ HELP FIRST)`  
`Component config --> ESP32S3-Specific --> Number of cycles for RTC_SLOW_CLK calibration --> 16`


## Technical Support and Feedback
Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
