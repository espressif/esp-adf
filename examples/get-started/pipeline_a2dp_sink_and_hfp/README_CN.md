# A2DP sink 和 HFP 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程是 A2DP sink 和 HFP 例程，使用经典蓝牙的 A2DP 协议进行音频流分发接收，使用 AVRCP 协议进行媒体信息通知控制，还可以作为 HFP Hands Free Unit 设备接收呼入的电话。

此例程完整管道如下：

```
[Bluetooth] ---> bt_stream_reader ---> i2s_stream_writer ---> codec_chip ---> speaker
```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

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

- 例程开始运行后，开发板等待用户的手机进行蓝牙连接操作，打印如下：

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 10:44:58
I (27) boot: chip revision: 3
I (31) boot.esp32: SPI Speed      : 40MHz
I (36) boot.esp32: SPI Mode       : DIO
I (40) boot.esp32: SPI Flash Size : 2MB
I (45) boot: Enabling RNG early entropy source...
I (50) boot: Partition Table:
I (54) boot: ## Label            Usage          Type ST Offset   Length
I (61) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (69) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (76) boot:  2 factory          factory app      00 00 00010000 00124f80
I (84) boot: End of partition table
I (88) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2ff20 (196384) map
I (172) esp_image: segment 1: paddr=0x0003ff48 vaddr=0x3ffbdb60 size=0x000d0 (   208) load
I (172) esp_image: segment 2: paddr=0x00040020 vaddr=0x400d0020 size=0xbbc60 (769120) map
0x400d0020: _stext at ??:?

I (471) esp_image: segment 3: paddr=0x000fbc88 vaddr=0x3ffbdc30 size=0x03514 ( 13588) load
I (477) esp_image: segment 4: paddr=0x000ff1a4 vaddr=0x40080000 size=0x1846c ( 99436) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (534) boot: Loaded app from partition at offset 0x10000
I (534) boot: Disabling RNG early entropy source...
I (535) psram: This chip is ESP32-D0WD
I (540) spiram: Found 64MBit SPI RAM device
I (544) spiram: SPI RAM mode: flash 40m sram 40m
I (549) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (557) cpu_start: Pro cpu up.
I (560) cpu_start: Application information:
I (565) cpu_start: Project name:     a2dp_sink_and_hfp_example
I (572) cpu_start: App version:      v2.2-252-gdd93b207-dirty
I (578) cpu_start: Compile time:     Nov 26 2021 10:44:52
I (584) cpu_start: ELF file SHA256:  e1f292df795c02c7...
I (590) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (596) cpu_start: Starting app cpu, entry point is 0x40081cd4
0x40081cd4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1490) spiram: SPI SRAM memory test OK
I (1498) heap_init: Initializing. RAM available for dynamic allocation:
I (1498) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1500) heap_init: At 3FFB7468 len 00000B98 (2 KiB): DRAM
I (1506) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1512) heap_init: At 3FFC2838 len 0001D7C8 (117 KiB): DRAM
I (1519) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1525) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1531) heap_init: At 4009846C len 00007B94 (30 KiB): IRAM
I (1538) cpu_start: Pro cpu start user code
I (1543) spiram: Adding pool of 4034K of external SPI memory to heap allocator
I (1565) spi_flash: detected chip: gd
I (1566) spi_flash: flash io: dio
W (1566) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1576) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1587) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1627) BLUETOOTH_EXAMPLE: [ 1 ] Create Bluetooth service
I (1627) BTDM_INIT: BT controller compile version [ba56601]
I (1627) system_api: Base MAC address is not set
I (1637) system_api: read default base MAC address from EFUSE
I (1647) phy_init: phy_version 4660,0162888,Dec 23 2020
W (1647) phy_init: failed to load RF calibration data (0xffffffff), falling back to full calibration
W (2357) BT_BTC: A2DP Enable with AVRC
I (2387) BLUETOOTH_EXAMPLE: [ 2 ] Start codec chip
I (2387) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (2647) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (2647) ES8388_DRIVER: init,out:02, in:00
I (2807) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (2897) BLUETOOTH_EXAMPLE: [ 3 ] Create audio pipeline for playback
I (2897) BLUETOOTH_EXAMPLE: [3.1] Create i2s stream to write data to codec chip and read data from codec chip
I (2897) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2907) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2927) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (2937) LYRAT_V4_3: I2S0, MCLK output by GPIO0
W (2937) I2S: I2S driver already installed
I (2947) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (2947) BLUETOOTH_EXAMPLE: [3.2] Create Bluetooth stream
I (2957) BLUETOOTH_EXAMPLE: [3.3] Register all elements to audio pipeline
I (2957) BLUETOOTH_EXAMPLE: [3.4] Link it together [Bluetooth]-->bt_stream_reader-->i2s_stream_writer-->[codec_chip]
I (2977) AUDIO_PIPELINE: link el->rb, el:0x3f80fde0, tag:bt, rb:0x3f80ff74
I (2977) AUDIO_PIPELINE: link el->rb, el:0x3f80fba8, tag:i2s_r, rb:0x3f811fb4
I (2987) BLUETOOTH_EXAMPLE: [ 4 ] Initialize peripherals
E (2997) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (2997) BLUETOOTH_EXAMPLE: [4.1] Initialize Touch peripheral
I (3007) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (3017) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (3027) BLUETOOTH_EXAMPLE: [4.2] Create Bluetooth peripheral
W (3047) PERIPH_TOUCH: _touch_init
I (3047) BLUETOOTH_EXAMPLE: [4.2] Start all peripherals
I (3047) BLUETOOTH_EXAMPLE: [ 5 ] Set up  event listener
I (3057) BLUETOOTH_EXAMPLE: [5.1] Listening event from all elements of pipeline
I (3067) BLUETOOTH_EXAMPLE: [5.2] Listening event from peripherals
I (3067) BLUETOOTH_EXAMPLE: [ 6 ] Start audio_pipeline
I (3077) AUDIO_ELEMENT: [bt-0x3f80fde0] Element task created
I (3087) AUDIO_ELEMENT: [i2s_w-0x3f80fa24] Element task created
I (3087) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4271372 Bytes, Inter:218832 Bytes, Dram:187232 Bytes

I (3107) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_RESUME,state:1
I (3107) I2S_STREAM: AUDIO_STREAM_WRITER
I (3107) AUDIO_PIPELINE: Pipeline started
I (3117) AUDIO_ELEMENT: [i2s_r-0x3f80fba8] Element task created
I (3117) AUDIO_ELEMENT: [raw-0x3f80fcc8] Element task created
I (3127) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4265380 Bytes, Inter:214892 Bytes, Dram:183292 Bytes

I (3137) AUDIO_ELEMENT: [i2s_r] AEL_MSG_CMD_RESUME,state:1
I (3147) I2S_STREAM: AUDIO_STREAM_READER,Rate:44100,ch:2
I (3167) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (3177) AUDIO_PIPELINE: Pipeline started
I (3177) BLUETOOTH_EXAMPLE: [ 7 ] Listen for all pipeline events

```

- 用户使用手机蓝牙扫描，搜索到名为 `ESP_ADF_AUDIO` 的蓝牙设备然后点击连接，打印如下：

```c
E (14397) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
W (14427) BT_APPL: new conn_srvc id:27, app_id:1
E (14437) BT_HF: APP HFP event: CONNECTION_STATE_EVT
W (14537) BT_APPL: new conn_srvc id:19, app_id:0
E (14537) BT_HF: --connection state connected, peer feats 0x0, chld_feats 0x0
E (14537) BT_HF: APP HFP event: NETWORK_STATE_EVT
E (14547) BT_HF: --NETWORK STATE available
E (14547) BT_HF: APP HFP event: CALL_IND_EVT
E (14557) BT_HF: --Call indicator NO call in progress
E (14557) BT_HF: APP HFP event: CALL_SETUP_IND_EVT
E (14567) BT_HF: --Call setup indicator NONE
E (14577) BT_HF: APP HFP event: BATTERY_LEVEL_IND_EVT
E (14577) BT_HF: --battery level 5
E (14587) BT_HF: APP HFP event: SIGNAL_STRENGTH_IND_EVT
E (14587) BT_HF: -- signal strength: 4
E (14597) BT_HF: APP HFP event: ROAMING_STATUS_IND_EVT
E (14597) BT_HF: --ROAMING: inactive
E (14607) BT_HF: APP HFP event: CALL_HELD_IND_EVT
E (14607) BT_HF: --Call held indicator NONE held
I (14617) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=44100, bits=16, ch=2
I (14657) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_PAUSE
I (14687) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (14687) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_RESUME,state:4
I (14697) I2S_STREAM: AUDIO_STREAM_WRITER
E (14707) BT_HF: APP HFP event: CONNECTION_STATE_EVT
E (14717) BT_HF: --connection state slc_connected, peer feats 0x3ef, chld_feats 0x3f
E (14717) BT_HF: APP HFP event: INBAND_RING_TONE_EVT
E (14717) BT_HF: --inband ring state Provided
I (29317) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

```

- 蓝牙连接成功后，手机打开播放器点击播放，开发板就输出手机蓝牙下发的音频，还可以作为 `HFP Hands Free Unit` 接听呼入的电话，打印如下：

```c
W (29327) BT_APPL: new conn_srvc id:19, app_id:1
E (164707) BT_HF: APP HFP event: CALL_IND_EVT
E (164707) BT_HF: --Call indicator NO call in progress
E (164707) BT_HF: APP HFP event: ROAMING_STATUS_IND_EVT
E (164707) BT_HF: --ROAMING: active
E (164717) BT_HF: APP HFP event: BATTERY_LEVEL_IND_EVT
E (164717) BT_HF: --battery level 1
E (165107) BT_BTM: btm_sco_connected, handle 180
E (165117) BT_HF: APP HFP event: AUDIO_STATE_EVT
E (165117) BT_HF: --audio state connected_msbc
E (165117) BT_HF: bt_app_hf_client_audio_open
I (165127) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=16000, bits=16, ch=1
I (165167) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_PAUSE
I (165237) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (165247) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_RESUME,state:4
I (165247) I2S_STREAM: AUDIO_STREAM_WRITER
E (166877) BT_HF: APP HFP event: RING_IND_EVT
E (166877) BT_HF: HF_CLIENT EVT: 20
E (166877) BT_HF: APP HFP event: CLIP_EVT
E (166887) BT_HF: --clip number 18151558537
E (167657) BT_APPL: bta_dm_pm_btm_status hci_status=32
E (169947) BT_HF: APP HFP event: RING_IND_EVT
E (169947) BT_HF: HF_CLIENT EVT: 20
E (169947) BT_HF: APP HFP event: CLIP_EVT
E (169957) BT_HF: --clip number 18151558537
E (173157) BT_HF: APP HFP event: CALL_SETUP_IND_EVT
E (173157) BT_HF: --Call setup indicator INCOMING
E (173157) BT_HF: APP HFP event: BATTERY_LEVEL_IND_EVT
E (173177) BT_HF: --battery level 0
E (173267) BT_HF: APP HFP event: AUDIO_STATE_EVT
E (173267) BT_HF: --audio state disconnected
E (173267) BT_HF: bt_app_hf_client_audio_close
I (173277) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=44100, bits=16, ch=2
I (173457) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_PAUSE
I (173487) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (173487) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_RESUME,state:4
I (173497) I2S_STREAM: AUDIO_STREAM_WRITER
E (186447) BT_HF: APP HFP event: CALL_SETUP_IND_EVT
E (186447) BT_HF: --Call setup indicator NONE
I (186717) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

E (211647) BT_HF: APP HFP event: CALL_IND_EVT
E (211647) BT_HF: --Call indicator call in progress

Done

```

### 日志输出

以下为本例程的完整日志。

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
I (27) boot: compile time 10:44:58
I (27) boot: chip revision: 3
I (31) boot.esp32: SPI Speed      : 40MHz
I (36) boot.esp32: SPI Mode       : DIO
I (40) boot.esp32: SPI Flash Size : 2MB
I (45) boot: Enabling RNG early entropy source...
I (50) boot: Partition Table:
I (54) boot: ## Label            Usage          Type ST Offset   Length
I (61) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (69) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (76) boot:  2 factory          factory app      00 00 00010000 00124f80
I (84) boot: End of partition table
I (88) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2ff20 (196384) map
I (172) esp_image: segment 1: paddr=0x0003ff48 vaddr=0x3ffbdb60 size=0x000d0 (   208) load
I (172) esp_image: segment 2: paddr=0x00040020 vaddr=0x400d0020 size=0xbbc60 (769120) map
0x400d0020: _stext at ??:?

I (471) esp_image: segment 3: paddr=0x000fbc88 vaddr=0x3ffbdc30 size=0x03514 ( 13588) load
I (477) esp_image: segment 4: paddr=0x000ff1a4 vaddr=0x40080000 size=0x1846c ( 99436) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (534) boot: Loaded app from partition at offset 0x10000
I (534) boot: Disabling RNG early entropy source...
I (535) psram: This chip is ESP32-D0WD
I (540) spiram: Found 64MBit SPI RAM device
I (544) spiram: SPI RAM mode: flash 40m sram 40m
I (549) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (557) cpu_start: Pro cpu up.
I (560) cpu_start: Application information:
I (565) cpu_start: Project name:     a2dp_sink_and_hfp_example
I (572) cpu_start: App version:      v2.2-252-gdd93b207-dirty
I (578) cpu_start: Compile time:     Nov 26 2021 10:44:52
I (584) cpu_start: ELF file SHA256:  e1f292df795c02c7...
I (590) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (596) cpu_start: Starting app cpu, entry point is 0x40081cd4
0x40081cd4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1490) spiram: SPI SRAM memory test OK
I (1498) heap_init: Initializing. RAM available for dynamic allocation:
I (1498) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (1500) heap_init: At 3FFB7468 len 00000B98 (2 KiB): DRAM
I (1506) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (1512) heap_init: At 3FFC2838 len 0001D7C8 (117 KiB): DRAM
I (1519) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1525) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1531) heap_init: At 4009846C len 00007B94 (30 KiB): IRAM
I (1538) cpu_start: Pro cpu start user code
I (1543) spiram: Adding pool of 4034K of external SPI memory to heap allocator
I (1565) spi_flash: detected chip: gd
I (1566) spi_flash: flash io: dio
W (1566) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1576) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1587) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1627) BLUETOOTH_EXAMPLE: [ 1 ] Create Bluetooth service
I (1627) BTDM_INIT: BT controller compile version [ba56601]
I (1627) system_api: Base MAC address is not set
I (1637) system_api: read default base MAC address from EFUSE
I (1647) phy_init: phy_version 4660,0162888,Dec 23 2020
W (1647) phy_init: failed to load RF calibration data (0xffffffff), falling back to full calibration
W (2357) BT_BTC: A2DP Enable with AVRC
I (2387) BLUETOOTH_EXAMPLE: [ 2 ] Start codec chip
I (2387) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (2647) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (2647) ES8388_DRIVER: init,out:02, in:00
I (2807) AUDIO_HAL: Codec mode is 2, Ctrl:1
I (2897) BLUETOOTH_EXAMPLE: [ 3 ] Create audio pipeline for playback
I (2897) BLUETOOTH_EXAMPLE: [3.1] Create i2s stream to write data to codec chip and read data from codec chip
I (2897) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2907) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2927) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (2937) LYRAT_V4_3: I2S0, MCLK output by GPIO0
W (2937) I2S: I2S driver already installed
I (2947) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (2947) BLUETOOTH_EXAMPLE: [3.2] Create Bluetooth stream
I (2957) BLUETOOTH_EXAMPLE: [3.3] Register all elements to audio pipeline
I (2957) BLUETOOTH_EXAMPLE: [3.4] Link it together [Bluetooth]-->bt_stream_reader-->i2s_stream_writer-->[codec_chip]
I (2977) AUDIO_PIPELINE: link el->rb, el:0x3f80fde0, tag:bt, rb:0x3f80ff74
I (2977) AUDIO_PIPELINE: link el->rb, el:0x3f80fba8, tag:i2s_r, rb:0x3f811fb4
I (2987) BLUETOOTH_EXAMPLE: [ 4 ] Initialize peripherals
E (2997) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (2997) BLUETOOTH_EXAMPLE: [4.1] Initialize Touch peripheral
I (3007) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (3017) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (3027) BLUETOOTH_EXAMPLE: [4.2] Create Bluetooth peripheral
W (3047) PERIPH_TOUCH: _touch_init
I (3047) BLUETOOTH_EXAMPLE: [4.2] Start all peripherals
I (3047) BLUETOOTH_EXAMPLE: [ 5 ] Set up  event listener
I (3057) BLUETOOTH_EXAMPLE: [5.1] Listening event from all elements of pipeline
I (3067) BLUETOOTH_EXAMPLE: [5.2] Listening event from peripherals
I (3067) BLUETOOTH_EXAMPLE: [ 6 ] Start audio_pipeline
I (3077) AUDIO_ELEMENT: [bt-0x3f80fde0] Element task created
I (3087) AUDIO_ELEMENT: [i2s_w-0x3f80fa24] Element task created
I (3087) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4271372 Bytes, Inter:218832 Bytes, Dram:187232 Bytes

I (3107) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_RESUME,state:1
I (3107) I2S_STREAM: AUDIO_STREAM_WRITER
I (3107) AUDIO_PIPELINE: Pipeline started
I (3117) AUDIO_ELEMENT: [i2s_r-0x3f80fba8] Element task created
I (3117) AUDIO_ELEMENT: [raw-0x3f80fcc8] Element task created
I (3127) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4265380 Bytes, Inter:214892 Bytes, Dram:183292 Bytes

I (3137) AUDIO_ELEMENT: [i2s_r] AEL_MSG_CMD_RESUME,state:1
I (3147) I2S_STREAM: AUDIO_STREAM_READER,Rate:44100,ch:2
I (3167) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (3177) AUDIO_PIPELINE: Pipeline started
I (3177) BLUETOOTH_EXAMPLE: [ 7 ] Listen for all pipeline events
E (14397) BT_APPL: bta_av_rc_create ACP handle exist for shdl:0
W (14427) BT_APPL: new conn_srvc id:27, app_id:1
E (14437) BT_HF: APP HFP event: CONNECTION_STATE_EVT
W (14537) BT_APPL: new conn_srvc id:19, app_id:0
E (14537) BT_HF: --connection state connected, peer feats 0x0, chld_feats 0x0
E (14537) BT_HF: APP HFP event: NETWORK_STATE_EVT
E (14547) BT_HF: --NETWORK STATE available
E (14547) BT_HF: APP HFP event: CALL_IND_EVT
E (14557) BT_HF: --Call indicator NO call in progress
E (14557) BT_HF: APP HFP event: CALL_SETUP_IND_EVT
E (14567) BT_HF: --Call setup indicator NONE
E (14577) BT_HF: APP HFP event: BATTERY_LEVEL_IND_EVT
E (14577) BT_HF: --battery level 5
E (14587) BT_HF: APP HFP event: SIGNAL_STRENGTH_IND_EVT
E (14587) BT_HF: -- signal strength: 4
E (14597) BT_HF: APP HFP event: ROAMING_STATUS_IND_EVT
E (14597) BT_HF: --ROAMING: inactive
E (14607) BT_HF: APP HFP event: CALL_HELD_IND_EVT
E (14607) BT_HF: --Call held indicator NONE held
I (14617) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=44100, bits=16, ch=2
I (14657) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_PAUSE
I (14687) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (14687) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_RESUME,state:4
I (14697) I2S_STREAM: AUDIO_STREAM_WRITER
E (14707) BT_HF: APP HFP event: CONNECTION_STATE_EVT
E (14717) BT_HF: --connection state slc_connected, peer feats 0x3ef, chld_feats 0x3f
E (14717) BT_HF: APP HFP event: INBAND_RING_TONE_EVT
E (14717) BT_HF: --inband ring state Provided
I (29317) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

W (29327) BT_APPL: new conn_srvc id:19, app_id:1
E (164707) BT_HF: APP HFP event: CALL_IND_EVT
E (164707) BT_HF: --Call indicator NO call in progress
E (164707) BT_HF: APP HFP event: ROAMING_STATUS_IND_EVT
E (164707) BT_HF: --ROAMING: active
E (164717) BT_HF: APP HFP event: BATTERY_LEVEL_IND_EVT
E (164717) BT_HF: --battery level 1
E (165107) BT_BTM: btm_sco_connected, handle 180
E (165117) BT_HF: APP HFP event: AUDIO_STATE_EVT
E (165117) BT_HF: --audio state connected_msbc
E (165117) BT_HF: bt_app_hf_client_audio_open
I (165127) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=16000, bits=16, ch=1
I (165167) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_PAUSE
I (165237) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (165247) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_RESUME,state:4
I (165247) I2S_STREAM: AUDIO_STREAM_WRITER
E (166877) BT_HF: APP HFP event: RING_IND_EVT
E (166877) BT_HF: HF_CLIENT EVT: 20
E (166877) BT_HF: APP HFP event: CLIP_EVT
E (166887) BT_HF: --clip number 18151558537
E (167657) BT_APPL: bta_dm_pm_btm_status hci_status=32
E (169947) BT_HF: APP HFP event: RING_IND_EVT
E (169947) BT_HF: HF_CLIENT EVT: 20
E (169947) BT_HF: APP HFP event: CLIP_EVT
E (169957) BT_HF: --clip number 18151558537
E (173157) BT_HF: APP HFP event: CALL_SETUP_IND_EVT
E (173157) BT_HF: --Call setup indicator INCOMING
E (173157) BT_HF: APP HFP event: BATTERY_LEVEL_IND_EVT
E (173177) BT_HF: --battery level 0
E (173267) BT_HF: APP HFP event: AUDIO_STATE_EVT
E (173267) BT_HF: --audio state disconnected
E (173267) BT_HF: bt_app_hf_client_audio_close
I (173277) BLUETOOTH_EXAMPLE: [ * ] Receive music info from Bluetooth, sample_rates=44100, bits=16, ch=2
I (173457) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_PAUSE
I (173487) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (173487) AUDIO_ELEMENT: [i2s_w] AEL_MSG_CMD_RESUME,state:4
I (173497) I2S_STREAM: AUDIO_STREAM_WRITER
E (186447) BT_HF: APP HFP event: CALL_SETUP_IND_EVT
E (186447) BT_HF: --Call setup indicator NONE
I (186717) BT_LOG: bta_av_link_role_ok hndl:x41 role:1 conn_audio:x1 bits:1 features:x824b

E (211647) BT_HF: APP HFP event: CALL_IND_EVT
E (211647) BT_HF: --Call indicator call in progress

Done
```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
