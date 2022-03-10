# 经典蓝牙 HFP 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了免提配置文件 (Hands Free Profile, HFP) 在 ADF 框架下接收和发送音频流的 API 的过程。

本例程的管道如下图：

```
[Bluetooth] ---> hfp_in_stream ---> i2s_stream_writer ---> [codec_chip]
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
请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程

### 功能和用法

- 例程开始运行后，会等待手机等蓝牙设备主动进行连接，HFP 设备名为 `ESP-ADF-HFP`， 打印如下：

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
I (27) boot: compile time 10:43:35
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
I (83) boot:  2 factory          factory app      00 00 00010000 00124f80
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x29164 (168292) map
I (175) esp_image: segment 1: paddr=0x0003918c vaddr=0x3ffbdb60 size=0x03454 ( 13396) load
I (181) esp_image: segment 2: paddr=0x0003c5e8 vaddr=0x40080000 size=0x03a30 ( 14896) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (188) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xa2e04 (667140) map
0x400d0020: _stext at ??:?

I (445) esp_image: segment 4: paddr=0x000e2e2c vaddr=0x40083a30 size=0x13834 ( 79924) load
0x40083a30: esp_flash_erase_region at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/spi_flash/esp_flash_api.c:313

I (492) boot: Loaded app from partition at offset 0x10000
I (493) boot: Disabling RNG early entropy source...
I (493) cpu_start: Pro cpu up.
I (497) cpu_start: Application information:
I (501) cpu_start: Project name:     pipeline_hfp_stream_example
I (508) cpu_start: App version:      v2.2-230-g511116d7
I (514) cpu_start: Compile time:     Nov 12 2021 10:43:27
I (520) cpu_start: ELF file SHA256:  2c459d66b8a1bbd8...
I (526) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (532) cpu_start: Starting app cpu, entry point is 0x400819ec
0x400819ec: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (543) heap_init: Initializing. RAM available for dynamic allocation:
I (549) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (555) heap_init: At 3FFB7468 len 00000B98 (2 KiB): DRAM
I (561) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (568) heap_init: At 3FFCE0E8 len 00011F18 (71 KiB): DRAM
I (574) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (580) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (587) heap_init: At 40097264 len 00008D9C (35 KiB): IRAM
I (593) cpu_start: Pro cpu start user code
I (611) spi_flash: detected chip: gd
I (612) spi_flash: flash io: dio
W (612) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (622) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (673) HFP_EXAMPLE: [ 1 ] init Bluetooth
I (673) BTDM_INIT: BT controller compile version [ba56601]
I (673) system_api: Base MAC address is not set
I (683) system_api: read default base MAC address from EFUSE
I (683) phy_init: phy_version 4660,0162888,Dec 23 2020
I (1413) HFP_EXAMPLE: [ 2 ] Start codec chip
I (1423) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1443) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1443) ES8388_DRIVER: init,out:02, in:00
I (1453) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (1453) HFP_EXAMPLE: [ 3 ] Create audio pipeline for playback
I (1453) HFP_EXAMPLE: [3.1] Create i2s stream to write data to codec chip and read data from codec chip
I (1463) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1473) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1503) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1503) LYRAT_V4_3: I2S0, MCLK output by GPIO0
W (1513) I2S: I2S driver already installed
I (1513) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1523) HFP_EXAMPLE: [3.2] Get hfp stream
I (1523) HFP_STREAM: outgoing stream init
I (1533) HFP_STREAM: incoming stream init
I (1533) HFP_EXAMPLE: [3.2] Register all elements to audio pipeline
I (1543) HFP_EXAMPLE: [3.3] Link it together [Bluetooth]-->hfp_in_stream-->i2s_stream_writer-->[codec_chip]
I (1553) AUDIO_PIPELINE: link el->rb, el:0x3ffd8944, tag:incoming, rb:0x3ffd8d00
I (1563) AUDIO_PIPELINE: link el->rb, el:0x3ffd7f78, tag:i2s_reader, rb:0x3ffdae3c
I (1563) HFP_EXAMPLE: [ 4 ] Initialize peripherals
E (1573) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1583) HFP_EXAMPLE: [ 5 ] Set up  event listener
I (1583) HFP_EXAMPLE: [5.1] Listening event from all elements of pipeline
I (1593) HFP_EXAMPLE: [5.2] Listening event from peripherals
I (1603) HFP_EXAMPLE: [ 6 ] Start audio_pipeline
I (1603) AUDIO_ELEMENT: [incoming-0x3ffd8944] Element task created
I (1613) AUDIO_ELEMENT: [i2s_writer-0x3ffd8304] Element task created
I (1623) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:133920 Bytes

I (1633) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (1633) I2S_STREAM: AUDIO_STREAM_WRITER
I (1643) AUDIO_PIPELINE: Pipeline started
I (1643) HFP_EXAMPLE: [ 7 ] Listen for all pipeline events

```

- 此示例中，手机搜索到 `ESP-ADF-HFP` 并连接成功，日志如下：

```c
W (36303) BT_APPL: new conn_srvc id:27, app_id:1
I (36313) HFP_STREAM: APP HFP event: CONNECTION_STATE_EVT
I (36313) HFP_STREAM: --Connection state connected, peer feats 0x0, chld_feats 0x0
I (36443) HFP_STREAM: APP HFP event: NETWORK_STATE_EVT
I (36443) HFP_STREAM: --NETWORK state available
I (36443) HFP_STREAM: APP HFP event: CALL_IND_EVT
I (36453) HFP_STREAM: --Call indicator NO call in progress
I (36453) HFP_STREAM: APP HFP event: CALL_SETUP_IND_EVT
I (36463) HFP_STREAM: --Call setup indicator NONE
I (36463) HFP_STREAM: APP HFP event: BATTERY_LEVEL_IND_EVT
I (36473) HFP_STREAM: --Battery level 5
I (36483) HFP_STREAM: APP HFP event: SIGNAL_STRENGTH_IND_EVT
I (36483) HFP_STREAM: --Signal strength: 5
I (36483) HFP_STREAM: APP HFP event: ROAMING_STATUS_IND_EVT
I (36493) HFP_STREAM: --ROAMING: inactive
I (36503) HFP_STREAM: APP HFP event: CALL_HELD_IND_EVT
I (36503) HFP_STREAM: --Call held indicator NONE held
I (36513) HFP_STREAM: APP HFP event: CONNECTION_STATE_EVT
I (36513) HFP_STREAM: --Connection state slc_connected, peer feats 0x3ef, chld_feats 0x3f
I (36523) HFP_STREAM: APP HFP event: INBAND_RING_TONE_EVT
I (36533) HFP_STREAM: --Inband ring state Provided
I (94813) HFP_STREAM: APP HFP event: BATTERY_LEVEL_IND_EVT
I (94813) HFP_STREAM: --Battery level 2
I (94833) HFP_STREAM: APP HFP event: VOLUME_CONTROL_EVT
I (94843) HFP_STREAM: --Volume_target: SPEAKER, volume 7
E (94873) BT_BTM: btm_sco_connected, handle 180
I (94873) HFP_STREAM: APP HFP event: AUDIO_STATE_EVT
I (94873) HFP_STREAM: --Audio state connected_msbc
I (94873) HFP_EXAMPLE: bt_app_hf_client_audio_open type = 1
I (94903) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (95093) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_PAUSE
I (95123) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (95133) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:4
I (95143) I2S_STREAM: AUDIO_STREAM_WRITER
I (95143) AUDIO_ELEMENT: [i2s_reader-0x3ffd7f78] Element task created
I (95153) AUDIO_ELEMENT: [outgoing-0x3ffd8624] Element task created
I (95153) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:124940 Bytes

I (95163) AUDIO_ELEMENT: [i2s_reader] AEL_MSG_CMD_RESUME,state:1
I (95173) I2S_STREAM: AUDIO_STREAM_READER,Rate:16000,ch:1
I (95183) AUDIO_PIPELINE: Pipeline started
I (95233) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (95233) AUDIO_ELEMENT: [i2s_reader] AEL_MSG_CMD_RESUME,state:3
I (95803) HFP_STREAM: APP HFP event: ROAMING_STATUS_IND_EVT
I (95803) HFP_STREAM: --ROAMING: active

```

- 然后，可以使用此设备接听呼入的电话，日志如下。

```c
I (108073) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (108073) HEADPHONE: Headphone jack inserted
I (123143) HFP_STREAM: APP HFP event: BATTERY_LEVEL_IND_EVT
I (123153) HFP_STREAM: --Battery level 3
I (123273) HFP_STREAM: APP HFP event: CALL_SETUP_IND_EVT
I (123273) HFP_STREAM: --Call setup indicator INCOMING
I (123273) HFP_STREAM: APP HFP event: BATTERY_LEVEL_IND_EVT
I (123273) HFP_STREAM: --Battery level 0
I (207163) HFP_STREAM: APP HFP event: AUDIO_STATE_EVT
I (207163) HFP_STREAM: --Audio state disconnected
I (207163) HFP_EXAMPLE: bt_app_hf_client_audio_close
I (207173) AUDIO_ELEMENT: [i2s_reader] AEL_MSG_CMD_PAUSE
I (207173) HFP_STREAM: APP HFP event: CALL_SETUP_IND_EVT
I (207183) HFP_STREAM: --Call setup indicator NONE
#FEAT# ATTE2 status:0

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
I (27) boot: compile time 10:43:35
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
I (83) boot:  2 factory          factory app      00 00 00010000 00124f80
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x29164 (168292) map
I (175) esp_image: segment 1: paddr=0x0003918c vaddr=0x3ffbdb60 size=0x03454 ( 13396) load
I (181) esp_image: segment 2: paddr=0x0003c5e8 vaddr=0x40080000 size=0x03a30 ( 14896) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (188) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xa2e04 (667140) map
0x400d0020: _stext at ??:?

I (445) esp_image: segment 4: paddr=0x000e2e2c vaddr=0x40083a30 size=0x13834 ( 79924) load
0x40083a30: esp_flash_erase_region at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/spi_flash/esp_flash_api.c:313

I (492) boot: Loaded app from partition at offset 0x10000
I (493) boot: Disabling RNG early entropy source...
I (493) cpu_start: Pro cpu up.
I (497) cpu_start: Application information:
I (501) cpu_start: Project name:     pipeline_hfp_stream_example
I (508) cpu_start: App version:      v2.2-230-g511116d7
I (514) cpu_start: Compile time:     Nov 12 2021 10:43:27
I (520) cpu_start: ELF file SHA256:  2c459d66b8a1bbd8...
I (526) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (532) cpu_start: Starting app cpu, entry point is 0x400819ec
0x400819ec: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (543) heap_init: Initializing. RAM available for dynamic allocation:
I (549) heap_init: At 3FFAFF10 len 000000F0 (0 KiB): DRAM
I (555) heap_init: At 3FFB7468 len 00000B98 (2 KiB): DRAM
I (561) heap_init: At 3FFB9A20 len 00004108 (16 KiB): DRAM
I (568) heap_init: At 3FFCE0E8 len 00011F18 (71 KiB): DRAM
I (574) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (580) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (587) heap_init: At 40097264 len 00008D9C (35 KiB): IRAM
I (593) cpu_start: Pro cpu start user code
I (611) spi_flash: detected chip: gd
I (612) spi_flash: flash io: dio
W (612) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (622) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (673) HFP_EXAMPLE: [ 1 ] init Bluetooth
I (673) BTDM_INIT: BT controller compile version [ba56601]
I (673) system_api: Base MAC address is not set
I (683) system_api: read default base MAC address from EFUSE
I (683) phy_init: phy_version 4660,0162888,Dec 23 2020
I (1413) HFP_EXAMPLE: [ 2 ] Start codec chip
I (1423) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1443) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1443) ES8388_DRIVER: init,out:02, in:00
I (1453) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (1453) HFP_EXAMPLE: [ 3 ] Create audio pipeline for playback
I (1453) HFP_EXAMPLE: [3.1] Create i2s stream to write data to codec chip and read data from codec chip
I (1463) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1473) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (1503) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (1503) LYRAT_V4_3: I2S0, MCLK output by GPIO0
W (1513) I2S: I2S driver already installed
I (1513) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (1523) HFP_EXAMPLE: [3.2] Get hfp stream
I (1523) HFP_STREAM: outgoing stream init
I (1533) HFP_STREAM: incoming stream init
I (1533) HFP_EXAMPLE: [3.2] Register all elements to audio pipeline
I (1543) HFP_EXAMPLE: [3.3] Link it together [Bluetooth]-->hfp_in_stream-->i2s_stream_writer-->[codec_chip]
I (1553) AUDIO_PIPELINE: link el->rb, el:0x3ffd8944, tag:incoming, rb:0x3ffd8d00
I (1563) AUDIO_PIPELINE: link el->rb, el:0x3ffd7f78, tag:i2s_reader, rb:0x3ffdae3c
I (1563) HFP_EXAMPLE: [ 4 ] Initialize peripherals
E (1573) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (1583) HFP_EXAMPLE: [ 5 ] Set up  event listener
I (1583) HFP_EXAMPLE: [5.1] Listening event from all elements of pipeline
I (1593) HFP_EXAMPLE: [5.2] Listening event from peripherals
I (1603) HFP_EXAMPLE: [ 6 ] Start audio_pipeline
I (1603) AUDIO_ELEMENT: [incoming-0x3ffd8944] Element task created
I (1613) AUDIO_ELEMENT: [i2s_writer-0x3ffd8304] Element task created
I (1623) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:133920 Bytes

I (1633) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:1
I (1633) I2S_STREAM: AUDIO_STREAM_WRITER
I (1643) AUDIO_PIPELINE: Pipeline started
I (1643) HFP_EXAMPLE: [ 7 ] Listen for all pipeline events
W (36303) BT_APPL: new conn_srvc id:27, app_id:1
I (36313) HFP_STREAM: APP HFP event: CONNECTION_STATE_EVT
I (36313) HFP_STREAM: --Connection state connected, peer feats 0x0, chld_feats 0x0
I (36443) HFP_STREAM: APP HFP event: NETWORK_STATE_EVT
I (36443) HFP_STREAM: --NETWORK state available
I (36443) HFP_STREAM: APP HFP event: CALL_IND_EVT
I (36453) HFP_STREAM: --Call indicator NO call in progress
I (36453) HFP_STREAM: APP HFP event: CALL_SETUP_IND_EVT
I (36463) HFP_STREAM: --Call setup indicator NONE
I (36463) HFP_STREAM: APP HFP event: BATTERY_LEVEL_IND_EVT
I (36473) HFP_STREAM: --Battery level 5
I (36483) HFP_STREAM: APP HFP event: SIGNAL_STRENGTH_IND_EVT
I (36483) HFP_STREAM: --Signal strength: 5
I (36483) HFP_STREAM: APP HFP event: ROAMING_STATUS_IND_EVT
I (36493) HFP_STREAM: --ROAMING: inactive
I (36503) HFP_STREAM: APP HFP event: CALL_HELD_IND_EVT
I (36503) HFP_STREAM: --Call held indicator NONE held
I (36513) HFP_STREAM: APP HFP event: CONNECTION_STATE_EVT
I (36513) HFP_STREAM: --Connection state slc_connected, peer feats 0x3ef, chld_feats 0x3f
I (36523) HFP_STREAM: APP HFP event: INBAND_RING_TONE_EVT
I (36533) HFP_STREAM: --Inband ring state Provided
I (94813) HFP_STREAM: APP HFP event: BATTERY_LEVEL_IND_EVT
I (94813) HFP_STREAM: --Battery level 2
I (94833) HFP_STREAM: APP HFP event: VOLUME_CONTROL_EVT
I (94843) HFP_STREAM: --Volume_target: SPEAKER, volume 7
E (94873) BT_BTM: btm_sco_connected, handle 180
I (94873) HFP_STREAM: APP HFP event: AUDIO_STATE_EVT
I (94873) HFP_STREAM: --Audio state connected_msbc
I (94873) HFP_EXAMPLE: bt_app_hf_client_audio_open type = 1
I (94903) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (95093) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_PAUSE
I (95123) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (95133) AUDIO_ELEMENT: [i2s_writer] AEL_MSG_CMD_RESUME,state:4
I (95143) I2S_STREAM: AUDIO_STREAM_WRITER
I (95143) AUDIO_ELEMENT: [i2s_reader-0x3ffd7f78] Element task created
I (95153) AUDIO_ELEMENT: [outgoing-0x3ffd8624] Element task created
I (95153) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:124940 Bytes

I (95163) AUDIO_ELEMENT: [i2s_reader] AEL_MSG_CMD_RESUME,state:1
I (95173) I2S_STREAM: AUDIO_STREAM_READER,Rate:16000,ch:1
I (95183) AUDIO_PIPELINE: Pipeline started
I (95233) I2S: APLL: Req RATE: 16000, real rate: 15999.986, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 4095996.500, SCLK: 511999.562500, diva: 1, divb: 0
I (95233) AUDIO_ELEMENT: [i2s_reader] AEL_MSG_CMD_RESUME,state:3
I (95803) HFP_STREAM: APP HFP event: ROAMING_STATUS_IND_EVT
I (95803) HFP_STREAM: --ROAMING: active
I (108073) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (108073) HEADPHONE: Headphone jack inserted
I (123143) HFP_STREAM: APP HFP event: BATTERY_LEVEL_IND_EVT
I (123153) HFP_STREAM: --Battery level 3
I (123273) HFP_STREAM: APP HFP event: CALL_SETUP_IND_EVT
I (123273) HFP_STREAM: --Call setup indicator INCOMING
I (123273) HFP_STREAM: APP HFP event: BATTERY_LEVEL_IND_EVT
I (123273) HFP_STREAM: --Battery level 0
I (207163) HFP_STREAM: APP HFP event: AUDIO_STATE_EVT
I (207163) HFP_STREAM: --Audio state disconnected
I (207163) HFP_EXAMPLE: bt_app_hf_client_audio_close
I (207173) AUDIO_ELEMENT: [i2s_reader] AEL_MSG_CMD_PAUSE
I (207173) HFP_STREAM: APP HFP event: CALL_SETUP_IND_EVT
I (207183) HFP_STREAM: --Call setup indicator NONE
#FEAT# ATTE2 status:0

```

## 故障排除

- 目前阶段，ESP32 HFP 设备默认支持的音频编解码器是 CVSD。CVSD 数据流传输到 HFP 设备，然后解码为 PCM 数据输出。ESP32 支持 HFP 的其他解码器配置，但需要额外修改协议栈设置。
- 由于使用限制，ESP32 HFP 设备最多只能支持一个与远程设备的连接。


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
