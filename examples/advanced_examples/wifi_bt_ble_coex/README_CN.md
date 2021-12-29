# Wi-Fi、A2DP、HFP、Bluetooth LE 共存例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程创建了几个 GATT 服务并启动了 ADV，ADV 名称为 `BLUFI_DEVICE`，然后等待连接，可以将设备配置为连接 Wi-Fi 和 BluFi 服务。当 Wi-Fi 连接时，例程将演示如何创建 HTTP STREAM 模块，它可以播放带有 HTTP URI 的音乐。

本例程的经典蓝牙部分实现了 A2DP SINK 部分。程序启动后，智能手机等蓝牙设备可以发现名为 `ESP_ADF_COEX_EXAMPLE` 的设备，建立连接后，使用 AVRCP 配置文件控制蓝牙音乐状态。


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程涉及 Wi-Fi、A2DP、HFP、Bluetooth LE 共存，所以使用的是优化共存的 IDF 分支：[audio/stack_on_psram_v3.3](https://github.com/espressif/esp-idf/tree/audio/stack_on_psram_v3.3)。


### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```


### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程


### 功能和用法

- 本例同时还可以使用共存的 Bluetooth LE 服务，用户使用手机 `EspBluFi` 应用程序连接名为 `BLUFI_DEVICE` 的 Bluetooth LE 设备，配置 SSID 和密码然后连接 Wi-Fi 网络。

- 例程开始运行后，等待用户手机主动去连接设备，A2DP 设备广播名为：`ESP_ADF_COEX_EXAMPLE`，打印如下：

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 19:42:18
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 40MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 4MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (68) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00300000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x4fe38 (327224) map
I (220) esp_image: segment 1: paddr=0x0005fe60 vaddr=0x3ffbdb60 size=0x001b8 (   440) load
I (221) esp_image: segment 2: paddr=0x00060020 vaddr=0x400d0020 size=0x146e50 (1338960) map
0x400d0020: _stext at ??:?

I (737) esp_image: segment 3: paddr=0x001a6e78 vaddr=0x3ffbdd18 size=0x040f0 ( 16624) load
I (744) esp_image: segment 4: paddr=0x001aaf70 vaddr=0x40080000 size=0x1d354 (119636) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (812) boot: Loaded app from partition at offset 0x10000
I (813) boot: Disabling RNG early entropy source...
I (813) psram: This chip is ESP32-D0WD
I (818) spiram: Found 64MBit SPI RAM device
I (822) spiram: SPI RAM mode: flash 40m sram 40m
I (827) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (835) cpu_start: Pro cpu up.
I (838) cpu_start: Application information:
I (843) cpu_start: Project name:     wifi_bt_ble_coex
I (849) cpu_start: App version:      v2.2-201-g6fa02923
I (855) cpu_start: Compile time:     Oct  8 2021 19:42:12
I (861) cpu_start: ELF file SHA256:  d0e590c88daac1b0...
I (867) cpu_start: ESP-IDF:          v4.2.2
I (872) cpu_start: Starting app cpu, entry point is 0x40081d3c
0x40081d3c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1766) spiram: SPI SRAM memory test OK
I (1775) heap_init: Initializing. RAM available for dynamic allocation:
I (1775) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1777) heap_init: At 3FFB7468 len 00000B98 (2 KiB): DRAM
I (1783) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1790) heap_init: At 3FFC3A48 len 0001C5B8 (113 KiB): DRAM
I (1796) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1802) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1809) heap_init: At 4009D354 len 00002CAC (11 KiB): IRAM
I (1815) cpu_start: Pro cpu start user code
I (1820) spiram: Adding pool of 4020K of external SPI memory to heap allocator
I (1843) spi_flash: detected chip: gd
I (1843) spi_flash: flash io: dio
W (1843) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1854) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1864) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1914) COEX_EXAMPLE: [ 1 ] Create coex handle for a2dp-gatt-wifi
I (1914) COEX_EXAMPLE: [ 2 ] Initialize peripherals
I (1914) COEX_EXAMPLE: [ 3 ] create and start input key service
I (1914) COEX_EXAMPLE: [ 4 ] Start a2dp and blufi network
W (1934) PERIPH_TOUCH: _touch_init
I (1934) COEX_EXAMPLE: [4.1] Init Bluetooth
I (2454) COEX_EXAMPLE: [4.2] init gatts
I (2454) COEX_EXAMPLE: Blufi module init
E (2454) DISPATCHER: exe first list: 0x0
I (2484) COEX_EXAMPLE: [4.3] Create Bluetooth peripheral
I (2484) COEX_EXAMPLE: [4.4] Start peripherals
I (2484) COEX_EXAMPLE: [4.5] init hfp_stream
E (2534) gpio: gpio_install_isr_service(438): GPIO isr service already installed

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-9-g84df87e-037bef3-09be8fe                |
|                     Compile date: Jul 20 2021-13:51:36                     |
------------------------------------------------------------------------------
W (2744) BT_BTC: A2DP Enable with AVRC
I (2754) COEX_EXAMPLE: A2DP sink user cb
I (2754) COEX_EXAMPLE: User cb A2DP event: 4
I (2804) COEX_EXAMPLE: Func:setup_player, Line:298, MEM Total:4150532 Bytes, Inter:49696 Bytes, Dram:38296 Bytes

I (2804) COEX_EXAMPLE: esp_audio instance is:0x3f813480

I (2814) COEX_EXAMPLE: [ 5 ] Create audio pipeline for playback
I (2814) COEX_EXAMPLE: [5.1] Create i2s stream to read data from codec chip
W (2824) I2S: I2S driver already installed
I (2834) COEX_EXAMPLE: [5.2] Create hfp stream
I (2834) COEX_EXAMPLE: [5.3] Register i2s reader and hfp outgoing to audio pipeline
I (2854) COEX_EXAMPLE: [5.4] Link it together [codec_chip]-->i2s_stream_reader-->filter-->hfp_out_stream-->[Bluetooth]
I (2854) COEX_EXAMPLE: [5.5] Start audio_pipeline out
W (6104) WIFI_SERV: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
W (7904) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (7984) COEX_EXAMPLE: WIFI_CONNECTED
```

- 当手机连接上 A2DP 设备后，打印如下：

```c
W (76644) BT_APPL: new conn_srvc id:27, app_id:1
W (77114) BT_APPL: bta_hf_client_send_at: No service, skipping 11 command
E (77344) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (77344) COEX_EXAMPLE: A2DP sink user cb
I (77404) COEX_EXAMPLE: A2DP sink user cb
I (77404) COEX_EXAMPLE: User cb A2DP event: 2
W (77574) BT_APPL: new conn_srvc id:19, app_id:0
I (77574) COEX_EXAMPLE: A2DP sink user cb
I (77574) COEX_EXAMPLE: A2DP connected
W (101574) BT_APPL: new conn_srvc id:19, app_id:1
I (101574) COEX_EXAMPLE: A2DP sink user cb
I (101574) COEX_EXAMPLE: User cb A2DP event: 1
I (112794) COEX_EXAMPLE: A2DP sink user cb
I (112794) COEX_EXAMPLE: User cb A2DP event: 1

```

- 本例中，可以按键 [MODE] 分别进入经典蓝牙模式或 Wi-Fi 模式，音频开发板可以播放 HTTP STREAM 网络音乐和 A2DP 的蓝牙音乐。

```c
I (16106) COEX_EXAMPLE: [ * ] input key id is 4, key type is 1
I (16236) COEX_EXAMPLE: [ * ] input key id is 4, key type is 2
I (16236) COEX_EXAMPLE: [ * ] [mode]
I (16236) COEX_EXAMPLE: [ * ] Enter BT mode
W (16496) BT_APPL: new conn_srvc id:19, app_id:1
I (16496) COEX_EXAMPLE: A2DP sink user cb
I (16506) COEX_EXAMPLE: User cb A2DP event: 1
I (25346) COEX_EXAMPLE: [ * ] input key id is 4, key type is 1
I (25496) COEX_EXAMPLE: [ * ] input key id is 4, key type is 2
I (25496) COEX_EXAMPLE: [ * ] [mode]
I (25496) COEX_EXAMPLE: [ * ] Enter WIFI mode
W (25806) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_ABORT
W (25806) AUDIO_ELEMENT: IN-[DEC_pcm] AEL_IO_ABORT
W (25826) ESP_AUDIO_TASK: Destroy the old pipeline, STOPPED
W (25836) ESP_AUDIO_TASK: The old pipeline destroyed, STOPPED
W (25836) AUDIO_PIPELINE: Without stop, st:1
W (25836) AUDIO_PIPELINE: Without wait stop, st:1
W (25846) AUDIO_PIPELINE: There are no listener registered
I (26066) COEX_EXAMPLE: A2DP sink user cb
I (26066) COEX_EXAMPLE: User cb A2DP event: 1

```

### 日志输出
以下为本例程的完整日志。

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7152
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 19:42:18
I (27) boot: chip revision: 3
I (30) boot.esp32: SPI Speed      : 40MHz
I (35) boot.esp32: SPI Mode       : DIO
I (39) boot.esp32: SPI Flash Size : 4MB
I (44) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (53) boot: ## Label            Usage          Type ST Offset   Length
I (60) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (68) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (75) boot:  2 factory          factory app      00 00 00010000 00300000
I (83) boot: End of partition table
I (87) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x4fe38 (327224) map
I (220) esp_image: segment 1: paddr=0x0005fe60 vaddr=0x3ffbdb60 size=0x001b8 (   440) load
I (221) esp_image: segment 2: paddr=0x00060020 vaddr=0x400d0020 size=0x146e50 (1338960) map
0x400d0020: _stext at ??:?

I (737) esp_image: segment 3: paddr=0x001a6e78 vaddr=0x3ffbdd18 size=0x040f0 ( 16624) load
I (744) esp_image: segment 4: paddr=0x001aaf70 vaddr=0x40080000 size=0x1d354 (119636) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (812) boot: Loaded app from partition at offset 0x10000
I (813) boot: Disabling RNG early entropy source...
I (813) psram: This chip is ESP32-D0WD
I (818) spiram: Found 64MBit SPI RAM device
I (822) spiram: SPI RAM mode: flash 40m sram 40m
I (827) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (835) cpu_start: Pro cpu up.
I (838) cpu_start: Application information:
I (843) cpu_start: Project name:     wifi_bt_ble_coex
I (849) cpu_start: App version:      v2.2-201-g6fa02923
I (855) cpu_start: Compile time:     Oct  8 2021 19:42:12
I (861) cpu_start: ELF file SHA256:  d0e590c88daac1b0...
I (867) cpu_start: ESP-IDF:          v4.2.2
I (872) cpu_start: Starting app cpu, entry point is 0x40081d3c
0x40081d3c: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1766) spiram: SPI SRAM memory test OK
I (1775) heap_init: Initializing. RAM available for dynamic allocation:
I (1775) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1777) heap_init: At 3FFB7468 len 00000B98 (2 KiB): DRAM
I (1783) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1790) heap_init: At 3FFC3A48 len 0001C5B8 (113 KiB): DRAM
I (1796) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1802) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1809) heap_init: At 4009D354 len 00002CAC (11 KiB): IRAM
I (1815) cpu_start: Pro cpu start user code
I (1820) spiram: Adding pool of 4020K of external SPI memory to heap allocator
I (1843) spi_flash: detected chip: gd
I (1843) spi_flash: flash io: dio
W (1843) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1854) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1864) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1914) COEX_EXAMPLE: [ 1 ] Create coex handle for a2dp-gatt-wifi
I (1914) COEX_EXAMPLE: [ 2 ] Initialize peripherals
I (1914) COEX_EXAMPLE: [ 3 ] create and start input key service
I (1914) COEX_EXAMPLE: [ 4 ] Start a2dp and blufi network
W (1934) PERIPH_TOUCH: _touch_init
I (1934) COEX_EXAMPLE: [4.1] Init Bluetooth
I (2454) COEX_EXAMPLE: [4.2] init gatts
I (2454) COEX_EXAMPLE: Blufi module init
E (2454) DISPATCHER: exe first list: 0x0
I (2484) COEX_EXAMPLE: [4.3] Create Bluetooth peripheral
I (2484) COEX_EXAMPLE: [4.4] Start peripherals
I (2484) COEX_EXAMPLE: [4.5] init hfp_stream
E (2534) gpio: gpio_install_isr_service(438): GPIO isr service already installed

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-9-g84df87e-037bef3-09be8fe                |
|                     Compile date: Jul 20 2021-13:51:36                     |
------------------------------------------------------------------------------
W (2744) BT_BTC: A2DP Enable with AVRC
I (2754) COEX_EXAMPLE: A2DP sink user cb
I (2754) COEX_EXAMPLE: User cb A2DP event: 4
I (2804) COEX_EXAMPLE: Func:setup_player, Line:298, MEM Total:4150532 Bytes, Inter:49696 Bytes, Dram:38296 Bytes

I (2804) COEX_EXAMPLE: esp_audio instance is:0x3f813480

I (2814) COEX_EXAMPLE: [ 5 ] Create audio pipeline for playback
I (2814) COEX_EXAMPLE: [5.1] Create i2s stream to read data from codec chip
W (2824) I2S: I2S driver already installed
I (2834) COEX_EXAMPLE: [5.2] Create hfp stream
I (2834) COEX_EXAMPLE: [5.3] Register i2s reader and hfp outgoing to audio pipeline
I (2854) COEX_EXAMPLE: [5.4] Link it together [codec_chip]-->i2s_stream_reader-->filter-->hfp_out_stream-->[Bluetooth]
I (2854) COEX_EXAMPLE: [5.5] Start audio_pipeline out
W (6104) WIFI_SERV: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
W (7904) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (7984) COEX_EXAMPLE: WIFI_CONNECTED
W (76644) BT_APPL: new conn_srvc id:27, app_id:1
W (77114) BT_APPL: bta_hf_client_send_at: No service, skipping 11 command
E (77344) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
I (77344) COEX_EXAMPLE: A2DP sink user cb
I (77404) COEX_EXAMPLE: A2DP sink user cb
I (77404) COEX_EXAMPLE: User cb A2DP event: 2
W (77574) BT_APPL: new conn_srvc id:19, app_id:0
I (77574) COEX_EXAMPLE: A2DP sink user cb
I (77574) COEX_EXAMPLE: A2DP connected
W (101574) BT_APPL: new conn_srvc id:19, app_id:1
I (101574) COEX_EXAMPLE: A2DP sink user cb
I (101574) COEX_EXAMPLE: User cb A2DP event: 1
I (112794) COEX_EXAMPLE: A2DP sink user cb
I (112794) COEX_EXAMPLE: User cb A2DP event: 1

```

## 故障排除

当你的应用出现共存一类的错误时候，可以从以下方面的查找错误原因：

- 是否使用默认的 `sdkconfig.defaults` 配置，强烈建议使用默认的 sdkconfig 配置。
- 是否使能了蓝牙的双模配置。
- 是否打开了 PSRAM 的配置，或者开发板模组是否配置了 PSRAM 功能。


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
