# DuerOS 3.0 语音交互例程

- [English Version](./README.md)
- 例程难度：![alt text](../../docs/_static/level_complex.png "高级")


## 例程简介

本例程主要功能是连接百度 DuerOS 3.0 云端并进行语音交互，可以适用于智能音箱产品、智能玩具、语音控制设备等。此示例是一个综合性较强的例程，使用了 ADF 提供的高封装简易实用接口。建议用户构建项目时，优先使用 ADF 提供的高封装接口，可快速简便地构建项目。

其中，[esp audio](../../components/esp-adf-libs/esp_audio/include/esp_audio.h) 处理音频播放，[wifi service](../../components/wifi_service) 接口管理配网和连接 Wi-Fi，[recorder engine](../../components/esp-adf-libs/recorder_engine/include/recorder_engine.h) 负责唤醒和语音数据管理，[display service](../../components/display_service/display_service.c) 管理系统指示灯，[Dueros service](../../components/dueros_service) 连接 DuerOS，[esp_periph_set_register_callback](../../components/esp_peripherals/include/esp_peripherals.h) 管理按键事件，也可以使用 [Key service](../../components/input_key_service/input_key_service.c) 按键服务来管理按键。

此外，本例程需要预先在 [百度 DuerOS 开放平台](https://dueros.baidu.com/didp/doc/overall/console-guide_markdown) 申请 DuerOS 的 profile，并替换 `ADF_PATH/components/dueros_service/duer_profile` 中的空文件。

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程需要准备一张 microSD 卡，将例程文件夹 `tone/dingding.wav` 下的提示音文件拷贝至 microSD 卡中，然后插入开发板备用。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

如果你需要修改录音文件名，本例程建议同时打开 FatFs 长文件名支持。

```
menuconfig > Component config > FAT Filesystem support > Long filename support
```

本例需要连接 Wi-Fi 网络，通过运行 `menuconfig` 来配置 Wi-Fi 信息。

```
menuconfig > Example Configuration > `WiFi SSID` and `WiFi Password`
```

关于如何添加 DuerOS 数据点，请参考 [issue #145](https://github.com/espressif/esp-adf/issues/145#issuecomment-483531246)。

此外，本例程还需 DuerOS 的 profile，用户可自行前往 [百度 DuerOS 开放平台](https://dueros.baidu.com/didp/doc/overall/console-guide_markdown) 页面进行申请，并且将申请到的 profile 替换 `ADF_PATH/components/dueros_service/duer_profile` 文件夹下的原始空文件。


### 编译和下载
请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。

## 如何使用例程

### 功能和用法

- 例程开始运行后，开发板会先去尝试连接 Wi-Fi 网络，成功获取 IP 地址后，连接 DuerOS 服务器进行 profile 鉴权，打印如下：

```c
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7204
load:0x40078000,len:13684
load:0x40080400,len:4632
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 10:07:21
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 80MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00300000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x75a70 (481904) map
I (283) esp_image: segment 1: paddr=0x00085a98 vaddr=0x3ffb0000 size=0x03ee0 ( 16096) load
I (290) esp_image: segment 2: paddr=0x00089980 vaddr=0x40080000 size=0x06698 ( 26264) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (301) esp_image: segment 3: paddr=0x00090020 vaddr=0x400d0020 size=0x139ca8 (1285288) map
0x400d0020: _stext at ??:?

I (761) esp_image: segment 4: paddr=0x001c9cd0 vaddr=0x40086698 size=0x17428 ( 95272) load
0x40086698: spi_device_acquire_bus at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/driver/spi_master.c:858

I (818) boot: Loaded app from partition at offset 0x10000
I (818) boot: Disabling RNG early entropy source...
I (818) psram: This chip is ESP32-D0WD
I (823) spiram: Found 64MBit SPI RAM device
I (827) spiram: SPI RAM mode: flash 80m sram 80m
I (832) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (840) cpu_start: Pro cpu up.
I (843) cpu_start: Application information:
I (848) cpu_start: Project name:     esp_dueros
I (853) cpu_start: App version:      v2.2-217-g6b75ed40-dirty
I (860) cpu_start: Compile time:     Nov  9 2021 10:07:12
I (866) cpu_start: ELF file SHA256:  1c413ae0228ece43...
I (872) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (878) cpu_start: Starting app cpu, entry point is 0x400820d4
0x400820d4: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1387) spiram: SPI SRAM memory test OK
I (1388) heap_init: Initializing. RAM available for dynamic allocation:
I (1388) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1394) heap_init: At 3FFB69E0 len 00029620 (165 KiB): DRAM
I (1400) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1407) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1413) heap_init: At 4009DAC0 len 00002540 (9 KiB): IRAM
I (1419) cpu_start: Pro cpu start user code
I (1424) spiram: Adding pool of 4082K of external SPI memory to heap allocator
I (1445) spi_flash: detected chip: gd
I (1445) spi_flash: flash io: dio
W (1446) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1456) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1470) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1500) DUEROS: ADF version is v2.2-217-g6b75ed40-dirty
I (1501) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1505) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
W (1539) PERIPH_TOUCH: _touch_init
I (1539) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1584) SDCARD: CID name SA16G!

I (1848) DUEROS: AUDIO_USER_KEY_VOL_DOWN [0]
E (2026) DISPATCHER: exe first list: 0x0
I (2026) DISPATCHER: dispatcher_event_task is running...
I (2026) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2029) AUDIO_THREAD: The wifi_serv task allocate stack on external memory
I (2033) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2043) wifi:wifi driver task: 3ffcca30, prio:23, stack:6656, core=0
I (2054) system_api: Base MAC address is not set
I (2059) system_api: read default base MAC address from EFUSE
I (2063) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (2069) wifi:wifi firmware version: bb6888c
I (2079) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (2083) wifi:wifi certification version: v7.0
I (2089) AUDIO_PIPELINE: link el->rb, el:0x3f80bc54, tag:i2s, rb:0x3f80c04c
I (2092) wifi:config NVS flash: enabledI (2100) AUDIO_PIPELINE: link el->rb, el:0x3f80bdbc, tag:filter, rb:0x3f80e08c

I (2111) wifi:config nano formating: disabled
I (2113) AUDIO_ELEMENT: [i2s-0x3f80bc54] Element task created
I (2116) wifi:I (2122) AUDIO_THREAD: The filter task allocate stack on external memory
Init data frame dynamic rx buffer num: 32
I (2134) wifi:Init management frame dynamic rx buffer num: 32
I (2140) wifi:Init management short buffer num: 32
I (2145) wifi:Init static tx buffer num: 16
I (2149) wifi:Init tx cache buffer num: 32
I (2152) wifi:Init static rx buffer size: 1600
I (2156) wifi:Init static rx buffer num: 16
I (2160) wifi:Init dynamic rx buffer num: 32
I (2165) wifi_init: rx ba win: 16
I (2168) wifi_init: tcpip mbox: 32
I (2172) wifi_init: udp mbox: 6
I (2176) wifi_init: tcp mbox: 6
I (2180) wifi_init: tcp tx win: 5744
I (2184) wifi_init: tcp rx win: 5744
I (2188) wifi_init: tcp mss: 1440
I (2192) wifi_init: WiFi/LWIP prefer SPIRAM
I (2197) wifi_init: WiFi IRAM OP enabled
I (2202) wifi_init: WiFi RX IRAM OP enabled
I (2208) phy_init: phy_version 4660,0162888,Dec 23 2020
I (2303) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2305) AUDIO_ELEMENT: [filter-0x3f80bdbc] Element task created
I (2305) AUDIO_ELEMENT: [raw-0x3f80bee4] Element task created
I (2310) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4219640 Bytes, Inter:181704 Bytes, Dram:172204 Bytes

I (2322) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (2328) I2S_STREAM: AUDIO_STREAM_READER,Rate:48000,ch:2
I (2349) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (2353) AUDIO_ELEMENT: [filter] AEL_MSG_CMD_RESUME,state:1
I (2359) AUDIO_PIPELINE: Pipeline started
I (2363) DUEROS: Recorder has been created
I (2362) RSP_FILTER: sample rate of source data : 48000, channel of source data : 2, sample rate of destination data : 16000, channel of destination data : 1
Quantized wakeNet5: wakeNet5_v1_hilexin_5_0.95_0.90, mode:0 (Oct 14 2020 16:26:17)
I (2390) REC_ENG: ESP SR Engine, chunksize is 480， FRAME_SIZE:960, frequency:16000
E (2399) REC_ENG: Recorder Engine Running ..., vad_window=9, wakeup=10000 ms, vad_off=800 ms, threshold=90 ms, sensitivity=0
I (2410) REC_ENG: state idle
I (1575,tid:3ffddebc) lightduer_session.c(  44):random = 75933
I (3052) WIFI_SERV: Connect to wifi ssid: esp32, pwd: esp123456
I (3055) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (3057) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (3105) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (3105) ES8388_DRIVER: init,out:02, in:00
I (3137) AUDIO_THREAD: The media_task task allocate stack on external memory
I (3138) ESP_AUDIO_TASK: media_ctrl_task running...,0x3f820ce4

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-31-g5b8f999-3072767-09be8fe               |
|                     Compile date: Oct 14 2021-11:00:34                     |
------------------------------------------------------------------------------
I (3180) ESP_AUDIO_CTRL: Func:media_ctrl_create, Line:350, MEM Total:4178584 Bytes, Inter:141444 Bytes, Dram:131944 Bytes

I (3192) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (3208) MP3_DECODER: MP3 init
W (3224) I2S: I2S driver already installed
I (3224) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (3230) AUDIO_WRAPPER: Func:setup_player, Line:161, MEM Total:4169964 Bytes, Inter:133664 Bytes, Dram:124164 Bytes

I (3234) AUDIO_WRAPPER: esp_audio instance is:0x3f820ce4
I (4271) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (4881) wifi:state: init -> auth (b0)
I (4890) wifi:state: auth -> assoc (0)
I (4901) wifi:state: assoc -> run (10)
I (4912) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (4913) wifi:security: WPA2-PSK, phy: bgn, rssi: -40
I (4914) wifi:pm start, type: 1

W (4920) WIFI_SERV: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4967) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (7002) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (7003) WIFI_SERV: Got ip:192.168.5.187
W (7005) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (7024) DUEROS: PERIPH_WIFI_CONNECTED [218]
E (7024) DUEROS: Recv Que DUER_CMD_LOGIN
I (7025) DUEROS: duer_start, len:1469
{"configures":"{}","bindToken":"***** replace the profile ******\n-----END CERTIFICATE-----\n","macId":"","version":11581}
W (7175) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:2, data_len:0
I (5709,tid:3ffddebc) lightduer_engine.c( 242):duer_engine_start, g_handler:3F82005C, length:1469, profile:3F823170
I (5727,tid:3ffddebc) lightduer_ca_conf.c(  38):    duer_conf_get_string: uuid = 18c10000000006
I (5735,tid:3ffddebc) lightduer_ca_conf.c(  38):    duer_conf_get_string: serverAddr = device.iot.baidu.com
I (5773,tid:3ffddebc) baidu_ca_socket_adp.c( 139):DNS lookup succeeded. IP=14.215.177.163
I (5807,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
W (5807,tid:3ffddebc) lightduer_events.c(  81):[lightduer_ca] <== event end = 40143198, timespent = 99
I (8241) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4134052 Bytes, Inter:118052 Bytes, Dram:108552 Bytes

I (6842,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
W (6843,tid:3ffddebc) lightduer_events.c(  81):[lightduer_ca] <== event end = 40143358, timespent = 988
I (6851,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6858,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6868,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6877,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6884,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6892,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6912,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6913,tid:3ffddebc) lightduer_engine.c( 242):duer_engine_start, g_handler:3F82005C, length:0, profile:00000000
I (6920,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6933,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6937,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6946,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6954,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6964,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6972,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (7232,tid:3ffddebc) lightduer_voice.c( 915):Mutex initializing
I (7233,tid:3ffddebc) lightduer_connagent.c( 189):connect started!
I (7234,tid:3ffddebc) lightduer_ds_log_cache.c(  85):no cache report
E (8705) DUEROS: event: 0
I (8708) DUEROS: duer_dcs_init
I (7247,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.private.protocol
I (7255,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.screen_extended_card
I (7264,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.screen
I (7273,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.system
I (7281,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.voice_input
I (7291,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.voice_output
I (7298,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.speaker_controller
I (7308,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.audio_player
I (8783) AUDIO_WRAPPER: duer_dcs_get_speaker_state
I (8797) DUEROS: Dueros DUER_CMD_CONNECTED, duer_state:2
W (8798) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:3, data_len:0
E (8799) DUEROS: event: DUER_EVENT_STARTED
W (7341,tid:3ffddebc) lightduer_events.c(  81):[lightduer_ca] <== event end = 40143358, timespent = 115
I (7350,tid:3ffddebc) lightduer_connagent.c( 222):add resource successfully!!
I (7358,tid:3ffddebc) lightduer_connagent.c( 222):add resource successfully!!
W (7364,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (7375,tid:3ffddebc) lightduer_system_info.c( 306):Undefined memory type, 0
E (7381,tid:3ffddebc) lightduer_system_info.c( 389):Sys Info: Get disk info failed
W (7398,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (7403,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (7413,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
W (7425,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
I (13242) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4137200 Bytes, Inter:121964 Bytes, Dram:112464 Bytes

I (18244) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4137200 Bytes, Inter:121964 Bytes, Dram:112464 Bytes

```

- 此时，可以对开发板说 “Hi Lexin”，唤醒触发 DuerOS 语音交互。绿色 LED 亮起表示成功唤醒，如果 `dingding.wav` 已经拷贝到 microSD 中，则会听到 “叮叮” 唤醒提示音。

```c
W (20644) REC_ENG: ### spot keyword ###
I (20644) DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_START
I (20645) DUEROS: rec_engine_cb - Play tone
I (20649) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (20655) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (20661) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffe1974
I (20668) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4137168 Bytes, Inter:121964 Bytes, Dram:112464 Bytes

I (20669) ESP_AUDIO_TASK: It's a decoder
I (20684) ESP_AUDIO_TASK: 1.CUR IN:[IN_file],CODEC:[DEC_wav],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (20696) ESP_AUDIO_TASK: 2.Handles,IN:0x3f821f0c,CODEC:0x3f8227e4,FILTER:0x3f829300,OUT:0x3f822e70
I (20705) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (20710) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (20716) AUDIO_PIPELINE: link el->rb, el:0x3f821f0c, tag:IN_file, rb:0x3f828eec
I (20724) AUDIO_PIPELINE: link el->rb, el:0x3f8227e4, tag:DEC_wav, rb:0x3f828f28
I (20732) AUDIO_PIPELINE: link el->rb, el:0x3f829300, tag:Audio_forge, rb:0x3f828f64
I (20741) ESP_AUDIO_TASK: 3. Previous starting...
I (20768) AUDIO_ELEMENT: [IN_file-0x3f821f0c] Element task created
I (20769) AUDIO_ELEMENT: [IN_file] AEL_MSG_CMD_RESUME,state:1
I (20771) AUDIO_THREAD: The DEC_wav task allocate stack on external memory
I (20779) AUDIO_ELEMENT: [DEC_wav-0x3f8227e4] Element task created
I (20785) AUDIO_ELEMENT: [DEC_wav] AEL_MSG_CMD_RESUME,state:1
I (20791) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (20839) FATFS_STREAM: File size: 17536 byte, file position: 0
I (20840) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f821f0c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (20866) CODEC_ELEMENT_HELPER: The element is 0x3f8227e4. The reserve data 2 is 0x0.
I (20867) WAV_DECODER: a new song playing
I (20870) ESP_AUDIO_TASK: Received music info then on play
I (20875) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (20881) AUDIO_THREAD: The Audio_forge task allocate stack on external memory
I (20889) AUDIO_ELEMENT: [Audio_forge-0x3f829300] Element task created
I (20896) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (20903) AUDIO_FORGE: audio_forge opened
I (20903) AUDIO_ELEMENT: [OUT_iis-0x3f822e70] Element task created
I (20909) AUDIO_FORGE: audio_forge reopen
I (20915) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (20926) I2S_STREAM: AUDIO_STREAM_WRITER
I (20932) ESP_AUDIO_CTRL: Exit play procedure, ret:0
I (20932) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f8227e4] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (20936) ESP_AUDIO_CTRL: Sync play waiting ...
I (20947) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f829300] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (20964) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (20975) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 1, src:0, is_stopping:0
I (20985) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4062980 Bytes, Inter:111692 Bytes, Dram:102192 Bytes

W (21055) FATFS_STREAM: No more data, ret:0
I (21056) AUDIO_ELEMENT: IN-[IN_file] AEL_IO_DONE,0
I (21058) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f821f0c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (21136) AUDIO_ELEMENT: IN-[DEC_wav] AEL_IO_DONE,-2
I (21136) DEC_WAV: Closed
I (21136) ESP_AUDIO_TASK: Received last pos: 17418 bytes
I (21141) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f8227e4] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (21321) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (21321) AUDIO_FORGE: audio forge closed
I (21322) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f829300] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (21377) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (21414) ESP_AUDIO_TASK: Received last time: 453 ms
I (21415) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (21420) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 1, src:0, is_stopping:0
I (21430) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4088772 Bytes, Inter:112296 Bytes, Dram:102796 Bytes

W (21442) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (21466) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (21466) ESP_AUDIO_CTRL: Sync play done

```

- 接着，可以说出一些语音交互的命令，如“苏州明天天气怎么样？”，则 DuerOS 将会返回苏州明天的天气情况，打印如下：

```c
W (22153) REC_ENG: State VAD silence_time >= vad_off_window
I (22422) DUEROS: rec_engine_cb - REC_EVENT_VAD_START
I (22423) DUEROS: Recv Que DUER_CMD_START
I (22425) AUDIO_WRAPPER: duer_dcs_get_speaker_state
I (20963,tid:3ffc7c04) lightduer_dcs_local.c( 197):Current dialog id: 18c1000000000640b18ccf000051e200000003
W (22438) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:4, data_len:0
I (23547) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4093768 Bytes, Inter:120340 Bytes, Dram:110840 Bytes

W (24403) REC_ENG: State VAD silence_time >= vad_off_window
I (24404) DUEROS: rec_engine_cb - REC_EVENT_VAD_STOP, state:4
I (24405) DUEROS: Dueros DUER_CMD_STOP
W (24412) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:5, data_len:0
W (25203) REC_ENG: State VAD silence_time >= vad_off_window
I (23797,tid:3ffddebc) lightduer_dcs_router.c( 460):Directive name: RenderVoiceInputText
I (23803,tid:3ffddebc) lightduer_dcs_router.c( 460):Directive name: StopListen
I (25270) AUDIO_WRAPPER: stop_listen, close mic
I (25272) REC_ENG: Recorder trigger stop
W (23829,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:3, qcache_len:1
W (23832,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
E (25304) REC_ENG: State FETCH_DATA, wakeup_time_out:1, vad_speech_on:0, vad_time_mode:0
W (25313) REC_ENG: State WAKEUP_END
I (25318) DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_END
I (25323) REC_ENG: state idle
I (23861,tid:3ffdd8e8) lightduer_dcs_router.c( 460):Directive name: Speak
W (23865,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:3
I (25336) AUDIO_WRAPPER: Playing speak: http://res.iot.baidu.com:80/api/v1/tts/gcgyVNsTjSlogPFdXMAtiyIvjQy8NMAZrej-Al3yBIkoWHpkqqbHNQZc3Bor_XPblUCDuIhd1UWUkqLxUCPsrOLBywqvybvXMMzoJGDiTYI.mp3
W (23885,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
I (25361) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (25377) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
W (23916,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
I (25383) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffe1974
I (25400) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4073440 Bytes, Inter:120312 Bytes, Dram:110812 Bytes

I (25400) ESP_AUDIO_TASK: It's a decoder
I (25416) ESP_AUDIO_TASK: 1.CUR IN:[IN_http],CODEC:[DEC_mp3],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (25427) ESP_AUDIO_TASK: 2.Handles,IN:0x3f8224cc,CODEC:0x3f82296c,FILTER:0x3f829300,OUT:0x3f822e70
I (25437) AUDIO_PIPELINE: audio_pipeline_unlinked
W (25442) AUDIO_PIPELINE: There are no listener registered
I (25448) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (25453) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (25459) AUDIO_PIPELINE: link el->rb, el:0x3f8224cc, tag:IN_http, rb:0x3f8235e4
I (25467) AUDIO_PIPELINE: link el->rb, el:0x3f82296c, tag:DEC_mp3, rb:0x3f8290c4
I (25475) AUDIO_PIPELINE: link el->rb, el:0x3f829300, tag:Audio_forge, rb:0x3f8234cc
I (25484) ESP_AUDIO_TASK: 3. Previous starting...
I (25489) AUDIO_THREAD: The IN_http task allocate stack on external memory
W (23942,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:4, qcache_len:1
W (24041,tid:3ffddebc) lightduer_events.c(  81):[lightduer_ca] <== event end = 401437E0, timespent = 99
W (24060,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (24063,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:1
I (25556) AUDIO_ELEMENT: [IN_http-0x3f8224cc] Element task created
I (25557) AUDIO_ELEMENT: [IN_http] AEL_MSG_CMD_RESUME,state:1
I (25559) AUDIO_THREAD: The DEC_mp3 task allocate stack on external memory
I (25566) AUDIO_ELEMENT: [DEC_mp3-0x3f82296c] Element task created
I (25573) AUDIO_ELEMENT: [DEC_mp3] AEL_MSG_CMD_RESUME,state:1
I (25582) MP3_DECODER: MP3 opened
I (25583) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (25719) HTTP_STREAM: total_bytes=0
I (25720) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f8224cc] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (25757) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f82296c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (25762) ESP_AUDIO_TASK: Received music info then on play
I (25764) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (25771) AUDIO_THREAD: The Audio_forge task allocate stack on external memory
I (25779) AUDIO_ELEMENT: [Audio_forge-0x3f829300] Element task created
I (25786) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (25794) AUDIO_FORGE: audio_forge opened
I (25795) AUDIO_ELEMENT: [OUT_iis-0x3f822e70] Element task created
I (25804) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (25810) I2S_STREAM: AUDIO_STREAM_WRITER
I (25816) ESP_AUDIO_CTRL: Exit play procedure, ret:0
I (25817) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f829300] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
W (24356,tid:3ffdd8e8) lightduer_events.c(  81):[lightduer_handler] <== event end = 40144FAC, timespent = 495
I (25833) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (25853) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0
I (25863) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:3980636 Bytes, Inter:111160 Bytes, Dram:101660 Bytes

I (25875) AUDIO_WRAPPER: esp_auido status:1,err:0,state:1
I (24951,tid:3ffdd8e8) lightduer_dcs_router.c( 460):Directive name: RenderWeather
E (24952,tid:3ffdd8e8) lightduer_dcs_router.c( 486):unrecognized directive: RenderWeather, namespace: ai.dueros.device_interface.screen_extended_card

I (26456) AUDIO_WRAPPER: duer_dcs_get_speaker_state
W (25027,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:1
W (25038,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (25059,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (25079,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:5
W (25109,tid:3ffdd8e8) lightduer_events.c(  81):[lightduer_handler] <== event end = 40144FAC, timespent = 141
W (25118,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:9
W (25135,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:8
W (25151,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:7
W (25173,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:6
W (25221,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:5
W (25236,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (25246,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (25291,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
W (25326,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
I (28557) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:3986484 Bytes, Inter:111260 Bytes, Dram:101760 Bytes

W (29779) HTTP_STREAM: No more data,errno:0, total_bytes:22610, rlen = 0
I (29780) AUDIO_ELEMENT: IN-[IN_http] AEL_IO_DONE,0
I (29788) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f8224cc] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (33560) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:3997164 Bytes, Inter:115016 Bytes, Dram:105516 Bytes

I (34881) AUDIO_ELEMENT: IN-[DEC_mp3] AEL_IO_DONE,-2
I (35964) MP3_DECODER: Closed
I (35965) ESP_AUDIO_TASK: Received last pos: 20952 bytes
I (35966) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f82296c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (36221) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (36222) AUDIO_FORGE: audio forge closed
I (36226) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f829300] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (36271) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (36308) ESP_AUDIO_TASK: Received last time: 10469 ms
I (36309) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (36315) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 0, src:0, is_stopping:0
I (36324) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4057072 Bytes, Inter:115108 Bytes, Dram:105608 Bytes

W (36336) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (36363) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (36363) AUDIO_WRAPPER: esp_auido status:4,err:0,state:1
I (34899,tid:3ffdd8e8) lightduer_dcs_router.c( 460):Directive name: DialogueFinished
E (36373) AUDIO_WRAPPER: duer_dcs_speech_on_finished,0, wakeup:0
I (38565) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4080476 Bytes, Inter:120192 Bytes, Dram:110692 Bytes

I (43566) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4080476 Bytes, Inter:120192 Bytes, Dram:110692 Bytes

```

- 本例程也支持长按开发板的 [REC] 按键时直接说语音命令，无需先唤醒设备，设备会进行录音并同时上传语音至 DuerOS 服务器，然后 DuerOS 下发语音回复。如长按 [REC] 按键时说：“现在是几点了”，然后释放按键，打印如下：

```c
I (53048) DUEROS: PERIPH_NOTIFY_KEY_REC
I (53048) REC_ENG: Recorder trigger start
I (53069) REC_ENG: WAKEUP timer started
I (53069) DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_START
I (53070) DUEROS: rec_engine_cb - Play tone
I (53073) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (53079) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (53086) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffdf240
I (53093) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4137052 Bytes, Inter:121836 Bytes, Dram:112336 Bytes

I (53093) ESP_AUDIO_TASK: It's a decoder
I (53109) ESP_AUDIO_TASK: 1.CUR IN:[IN_file],CODEC:[DEC_wav],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (53120) ESP_AUDIO_TASK: 2.Handles,IN:0x3f821f0c,CODEC:0x3f8227e4,FILTER:0x3f828e2c,OUT:0x3f822e70
I (53129) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (53135) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (53141) AUDIO_PIPELINE: link el->rb, el:0x3f821f0c, tag:IN_file, rb:0x3f8290b0
I (53149) AUDIO_PIPELINE: link el->rb, el:0x3f8227e4, tag:DEC_wav, rb:0x3f8290ec
I (53157) AUDIO_PIPELINE: link el->rb, el:0x3f828e2c, tag:Audio_forge, rb:0x3f829128
I (53165) ESP_AUDIO_TASK: 3. Previous starting...
I (53182) AUDIO_ELEMENT: [IN_file-0x3f821f0c] Element task created
I (53183) AUDIO_ELEMENT: [IN_file] AEL_MSG_CMD_RESUME,state:1
I (53186) AUDIO_THREAD: The DEC_wav task allocate stack on external memory
I (53194) AUDIO_ELEMENT: [DEC_wav-0x3f8227e4] Element task created
I (53201) AUDIO_ELEMENT: [DEC_wav] AEL_MSG_CMD_RESUME,state:1
I (53207) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (53254) FATFS_STREAM: File size: 17536 byte, file position: 0
I (53255) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f821f0c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (53261) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4098536 Bytes, Inter:116460 Bytes, Dram:106960 Bytes

I (53283) CODEC_ELEMENT_HELPER: The element is 0x3f8227e4. The reserve data 2 is 0x0.
I (53283) WAV_DECODER: a new song playing
I (53287) ESP_AUDIO_TASK: Received music info then on play
I (53292) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (53299) AUDIO_THREAD: The Audio_forge task allocate stack on external memory
I (53307) AUDIO_ELEMENT: [Audio_forge-0x3f828e2c] Element task created
I (53314) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (53320) AUDIO_FORGE: audio_forge opened
I (53321) AUDIO_ELEMENT: [OUT_iis-0x3f822e70] Element task created
I (53327) AUDIO_FORGE: audio_forge reopen
I (53332) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (53343) I2S_STREAM: AUDIO_STREAM_WRITER
I (53348) ESP_AUDIO_CTRL: Exit play procedure, ret:0
I (53348) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f8227e4] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (53353) ESP_AUDIO_CTRL: Sync play waiting ...
I (53365) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f828e2c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (53381) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (53393) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 1, src:0, is_stopping:0
I (53402) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4062860 Bytes, Inter:111564 Bytes, Dram:102064 Bytes

W (53469) FATFS_STREAM: No more data, ret:0
I (53469) AUDIO_ELEMENT: IN-[IN_file] AEL_IO_DONE,0
I (53470) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f821f0c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (53550) AUDIO_ELEMENT: IN-[DEC_wav] AEL_IO_DONE,-2
I (53550) DEC_WAV: Closed
I (53550) ESP_AUDIO_TASK: Received last pos: 17418 bytes
I (53557) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f8227e4] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (53736) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (53736) AUDIO_FORGE: audio forge closed
I (53737) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f828e2c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (53792) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (53829) ESP_AUDIO_TASK: Received last time: 453 ms
I (53830) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (53836) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 1, src:0, is_stopping:0
I (53845) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4088656 Bytes, Inter:112168 Bytes, Dram:102668 Bytes

W (53857) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (53873) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (53874) ESP_AUDIO_CTRL: Sync play done
I (53876) DUEROS: rec_engine_cb - REC_EVENT_VAD_START
I (53881) DUEROS: Recv Que DUER_CMD_START
I (53887) AUDIO_WRAPPER: duer_dcs_get_speaker_state
I (52425,tid:3ffc7c88) lightduer_dcs_local.c( 197):Current dialog id: 18c1000000000640b18ccf0000ccc900000003
W (53901) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:4, data_len:0
W (57220) REC_ENG: State VAD silence_time >= vad_off_window
I (57221) DUEROS: rec_engine_cb - REC_EVENT_VAD_STOP, state:4
I (57222) DUEROS: Dueros DUER_CMD_STOP
W (57229) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:5, data_len:0
I (56506,tid:3ffddf40) lightduer_dcs_router.c( 460):Directive name: StopListen
I (57975) AUDIO_WRAPPER: stop_listen, close mic
I (57975) REC_ENG: Recorder trigger stop
I (56518,tid:3ffddf40) lightduer_dcs_router.c( 460):Directive name: RenderVoiceInputText
I (56531,tid:3ffdd9c4) lightduer_dcs_router.c( 460):Directive name: RenderCard
E (58005) REC_ENG: State FETCH_DATA, wakeup_time_out:1, vad_speech_on:0, vad_time_mode:0
W (58005) REC_ENG: State WAKEUP_END
W (56539,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:2
I (58009) DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_END
I (58025) REC_ENG: state idle
I (56563,tid:3ffdd9c4) lightduer_dcs_router.c( 460):Directive name: Speak
I (58041) AUDIO_WRAPPER: Playing speak: http://res.iot.baidu.com:80/api/v1/tts/NW771vmjXGpIzW1Fpgpdjr47H2M5lNO250Bkrn40jRKXHn6LMfzbgEy5mNWS4hhLWW2sWqTZOz7m7oekCswmRSGqepB1fO8P7_OSchTi7QY.mp3
I (58053) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (58058) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (58066) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffdf240
I (58072) ESP_AUDIO_TASK: It's a decoder
I (58072) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4078976 Bytes, Inter:120184 Bytes, Dram:110684 Bytes

I (58077) ESP_AUDIO_TASK: 1.CUR IN:[IN_http],CODEC:[DEC_mp3],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (58099) ESP_AUDIO_TASK: 2.Handles,IN:0x3f8224cc,CODEC:0x3f82296c,FILTER:0x3f828e2c,OUT:0x3f822e70
I (58109) AUDIO_PIPELINE: audio_pipeline_unlinked
W (58114) AUDIO_PIPELINE: There are no listener registered
I (58120) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (58125) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (58131) AUDIO_PIPELINE: link el->rb, el:0x3f8224cc, tag:IN_http, rb:0x3f8302a4
I (58139) AUDIO_PIPELINE: link el->rb, el:0x3f82296c, tag:DEC_mp3, rb:0x3f823544
I (58147) AUDIO_PIPELINE: link el->rb, el:0x3f828e2c, tag:Audio_forge, rb:0x3f823580
I (58156) ESP_AUDIO_TASK: 3. Previous starting...
I (58161) AUDIO_THREAD: The IN_http task allocate stack on external memory
I (58194) AUDIO_ELEMENT: [IN_http-0x3f8224cc] Element task created
I (58194) AUDIO_ELEMENT: [IN_http] AEL_MSG_CMD_RESUME,state:1
I (58197) AUDIO_THREAD: The DEC_mp3 task allocate stack on external memory
I (58204) AUDIO_ELEMENT: [DEC_mp3-0x3f82296c] Element task created
I (58211) AUDIO_ELEMENT: [DEC_mp3] AEL_MSG_CMD_RESUME,state:1
I (58218) MP3_DECODER: MP3 opened
I (58219) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (58273) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4059664 Bytes, Inter:115440 Bytes, Dram:105940 Bytes

I (58354) HTTP_STREAM: total_bytes=0
I (58355) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f8224cc] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (58389) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f82296c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (58395) ESP_AUDIO_TASK: Received music info then on play
I (58396) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (58403) AUDIO_THREAD: The Audio_forge task allocate stack on external memory
I (58411) AUDIO_ELEMENT: [Audio_forge-0x3f828e2c] Element task created
I (58418) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (58428) AUDIO_FORGE: audio_forge opened
I (58429) AUDIO_ELEMENT: [OUT_iis-0x3f822e70] Element task created
I (58436) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (58442) I2S_STREAM: AUDIO_STREAM_WRITER
I (58449) ESP_AUDIO_CTRL: Exit play procedure, ret:0
I (58450) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f828e2c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
W (56986,tid:3ffdd9c4) lightduer_events.c(  81):[lightduer_handler] <== event end = 40144FAC, timespent = 424
I (58464) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (58485) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0
I (58495) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:3990064 Bytes, Inter:111124 Bytes, Dram:101624 Bytes

I (58506) AUDIO_WRAPPER: esp_auido status:1,err:0,state:1
W (57041,tid:3ffddf40) lightduer_events.c(  81):[lightduer_ca] <== event end = 40143358, timespent = 478
W (57056,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (57077,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (57088,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
W (57092,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (58580) HTTP_STREAM: No more data,errno:0, total_bytes:5906, rlen = 0
I (58581) AUDIO_ELEMENT: IN-[IN_http] AEL_IO_DONE,0
I (58604) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f8224cc] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (59170) AUDIO_ELEMENT: IN-[DEC_mp3] AEL_IO_DONE,-2
I (60256) MP3_DECODER: Closed
I (60257) ESP_AUDIO_TASK: Received last pos: 4248 bytes
I (60258) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f82296c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (60511) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (60512) AUDIO_FORGE: audio forge closed
I (60512) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f828e2c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (60567) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (60604) ESP_AUDIO_TASK: Received last time: 2128 ms
I (60605) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (60611) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 0, src:0, is_stopping:0
I (60620) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4056968 Bytes, Inter:114976 Bytes, Dram:105476 Bytes

W (60632) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (60658) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (60659) AUDIO_WRAPPER: esp_auido status:4,err:0,state:1
I (59194,tid:3ffdd9c4) lightduer_dcs_router.c( 460):Directive name: DialogueFinished
E (60669) AUDIO_WRAPPER: duer_dcs_speech_on_finished,0, wakeup:0
I (63274) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4080364 Bytes, Inter:120060 Bytes, Dram:110560 Bytes

I (68275) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4080364 Bytes, Inter:120060 Bytes, Dram:110560 Bytes
```

开发板绿色 LED 指示 Wi-Fi 状态：
- 连接 Wi-Fi 时绿色 LED 亮起；
- 如果 Wi-Fi 断开连接，绿色 LED 指示灯会正常闪烁；
- 如果 Wi-Fi 处于设置状态，绿色 LED 快速闪烁。

开发板按键功能：
- 通过 [Vol-] 或 [Vol+] 调节音量；
- 通过 [Set] 按钮配置 Wi-Fi。


### 日志输出
以下是本例程的完整日志。

```c
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7204
load:0x40078000,len:13684
load:0x40080400,len:4632
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 10:07:21
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 80MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00300000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x75a70 (481904) map
I (283) esp_image: segment 1: paddr=0x00085a98 vaddr=0x3ffb0000 size=0x03ee0 ( 16096) load
I (290) esp_image: segment 2: paddr=0x00089980 vaddr=0x40080000 size=0x06698 ( 26264) load
0x40080000: _WindowOverflow4 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (301) esp_image: segment 3: paddr=0x00090020 vaddr=0x400d0020 size=0x139ca8 (1285288) map
0x400d0020: _stext at ??:?

I (761) esp_image: segment 4: paddr=0x001c9cd0 vaddr=0x40086698 size=0x17428 ( 95272) load
0x40086698: spi_device_acquire_bus at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/driver/spi_master.c:858

I (818) boot: Loaded app from partition at offset 0x10000
I (818) boot: Disabling RNG early entropy source...
I (818) psram: This chip is ESP32-D0WD
I (823) spiram: Found 64MBit SPI RAM device
I (827) spiram: SPI RAM mode: flash 80m sram 80m
I (832) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (840) cpu_start: Pro cpu up.
I (843) cpu_start: Application information:
I (848) cpu_start: Project name:     esp_dueros
I (853) cpu_start: App version:      v2.2-217-g6b75ed40-dirty
I (860) cpu_start: Compile time:     Nov  9 2021 10:07:12
I (866) cpu_start: ELF file SHA256:  1c413ae0228ece43...
I (872) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (878) cpu_start: Starting app cpu, entry point is 0x400820d4
0x400820d4: call_start_cpu1 at /workshop/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1387) spiram: SPI SRAM memory test OK
I (1388) heap_init: Initializing. RAM available for dynamic allocation:
I (1388) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1394) heap_init: At 3FFB69E0 len 00029620 (165 KiB): DRAM
I (1400) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1407) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1413) heap_init: At 4009DAC0 len 00002540 (9 KiB): IRAM
I (1419) cpu_start: Pro cpu start user code
I (1424) spiram: Adding pool of 4082K of external SPI memory to heap allocator
I (1445) spi_flash: detected chip: gd
I (1445) spi_flash: flash io: dio
W (1446) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1456) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1470) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1500) DUEROS: ADF version is v2.2-217-g6b75ed40-dirty
I (1501) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (1505) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
W (1539) PERIPH_TOUCH: _touch_init
I (1539) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1584) SDCARD: CID name SA16G!

I (1848) DUEROS: AUDIO_USER_KEY_VOL_DOWN [0]
E (2026) DISPATCHER: exe first list: 0x0
I (2026) DISPATCHER: dispatcher_event_task is running...
I (2026) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2029) AUDIO_THREAD: The wifi_serv task allocate stack on external memory
I (2033) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2043) wifi:wifi driver task: 3ffcca30, prio:23, stack:6656, core=0
I (2054) system_api: Base MAC address is not set
I (2059) system_api: read default base MAC address from EFUSE
I (2063) I2S: APLL: Req RATE: 44100, real rate: 44099.988, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 11289597.000, SCLK: 1411199.625000, diva: 1, divb: 0
I (2069) wifi:wifi firmware version: bb6888c
I (2079) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (2083) wifi:wifi certification version: v7.0
I (2089) AUDIO_PIPELINE: link el->rb, el:0x3f80bc54, tag:i2s, rb:0x3f80c04c
I (2092) wifi:config NVS flash: enabledI (2100) AUDIO_PIPELINE: link el->rb, el:0x3f80bdbc, tag:filter, rb:0x3f80e08c

I (2111) wifi:config nano formating: disabled
I (2113) AUDIO_ELEMENT: [i2s-0x3f80bc54] Element task created
I (2116) wifi:I (2122) AUDIO_THREAD: The filter task allocate stack on external memory
Init data frame dynamic rx buffer num: 32
I (2134) wifi:Init management frame dynamic rx buffer num: 32
I (2140) wifi:Init management short buffer num: 32
I (2145) wifi:Init static tx buffer num: 16
I (2149) wifi:Init tx cache buffer num: 32
I (2152) wifi:Init static rx buffer size: 1600
I (2156) wifi:Init static rx buffer num: 16
I (2160) wifi:Init dynamic rx buffer num: 32
I (2165) wifi_init: rx ba win: 16
I (2168) wifi_init: tcpip mbox: 32
I (2172) wifi_init: udp mbox: 6
I (2176) wifi_init: tcp mbox: 6
I (2180) wifi_init: tcp tx win: 5744
I (2184) wifi_init: tcp rx win: 5744
I (2188) wifi_init: tcp mss: 1440
I (2192) wifi_init: WiFi/LWIP prefer SPIRAM
I (2197) wifi_init: WiFi IRAM OP enabled
I (2202) wifi_init: WiFi RX IRAM OP enabled
I (2208) phy_init: phy_version 4660,0162888,Dec 23 2020
I (2303) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2305) AUDIO_ELEMENT: [filter-0x3f80bdbc] Element task created
I (2305) AUDIO_ELEMENT: [raw-0x3f80bee4] Element task created
I (2310) AUDIO_PIPELINE: Func:audio_pipeline_run, Line:359, MEM Total:4219640 Bytes, Inter:181704 Bytes, Dram:172204 Bytes

I (2322) AUDIO_ELEMENT: [i2s] AEL_MSG_CMD_RESUME,state:1
I (2328) I2S_STREAM: AUDIO_STREAM_READER,Rate:48000,ch:2
I (2349) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (2353) AUDIO_ELEMENT: [filter] AEL_MSG_CMD_RESUME,state:1
I (2359) AUDIO_PIPELINE: Pipeline started
I (2363) DUEROS: Recorder has been created
I (2362) RSP_FILTER: sample rate of source data : 48000, channel of source data : 2, sample rate of destination data : 16000, channel of destination data : 1
Quantized wakeNet5: wakeNet5_v1_hilexin_5_0.95_0.90, mode:0 (Oct 14 2020 16:26:17)
I (2390) REC_ENG: ESP SR Engine, chunksize is 480， FRAME_SIZE:960, frequency:16000
E (2399) REC_ENG: Recorder Engine Running ..., vad_window=9, wakeup=10000 ms, vad_off=800 ms, threshold=90 ms, sensitivity=0
I (2410) REC_ENG: state idle
I (1575,tid:3ffddebc) lightduer_session.c(  44):random = 75933
I (3052) WIFI_SERV: Connect to wifi ssid: esp32, pwd: esp123456
I (3055) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (3057) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (3105) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (3105) ES8388_DRIVER: init,out:02, in:00
I (3137) AUDIO_THREAD: The media_task task allocate stack on external memory
I (3138) ESP_AUDIO_TASK: media_ctrl_task running...,0x3f820ce4

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-31-g5b8f999-3072767-09be8fe               |
|                     Compile date: Oct 14 2021-11:00:34                     |
------------------------------------------------------------------------------
I (3180) ESP_AUDIO_CTRL: Func:media_ctrl_create, Line:350, MEM Total:4178584 Bytes, Inter:141444 Bytes, Dram:131944 Bytes

I (3192) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (3208) MP3_DECODER: MP3 init
W (3224) I2S: I2S driver already installed
I (3224) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (3230) AUDIO_WRAPPER: Func:setup_player, Line:161, MEM Total:4169964 Bytes, Inter:133664 Bytes, Dram:124164 Bytes

I (3234) AUDIO_WRAPPER: esp_audio instance is:0x3f820ce4
I (4271) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (4881) wifi:state: init -> auth (b0)
I (4890) wifi:state: auth -> assoc (0)
I (4901) wifi:state: assoc -> run (10)
I (4912) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (4913) wifi:security: WPA2-PSK, phy: bgn, rssi: -40
I (4914) wifi:pm start, type: 1

W (4920) WIFI_SERV: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4967) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (7002) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (7003) WIFI_SERV: Got ip:192.168.5.187
W (7005) WIFI_SERV: STATE type:2, pdata:0x0, len:0
I (7024) DUEROS: PERIPH_WIFI_CONNECTED [218]
E (7024) DUEROS: Recv Que DUER_CMD_LOGIN
I (7025) DUEROS: duer_start, len:1469
{"configures":"{}","bindToken":"***** replace the profile ******\n-----END CERTIFICATE-----\n","macId":"","version":11581}
W (7175) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:2, data_len:0
I (5709,tid:3ffddebc) lightduer_engine.c( 242):duer_engine_start, g_handler:3F82005C, length:1469, profile:3F823170
I (5727,tid:3ffddebc) lightduer_ca_conf.c(  38):    duer_conf_get_string: uuid = 18c10000000006
I (5735,tid:3ffddebc) lightduer_ca_conf.c(  38):    duer_conf_get_string: serverAddr = device.iot.baidu.com
I (5773,tid:3ffddebc) baidu_ca_socket_adp.c( 139):DNS lookup succeeded. IP=14.215.177.163
I (5807,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
W (5807,tid:3ffddebc) lightduer_events.c(  81):[lightduer_ca] <== event end = 40143198, timespent = 99
I (8241) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4134052 Bytes, Inter:118052 Bytes, Dram:108552 Bytes

I (6842,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
W (6843,tid:3ffddebc) lightduer_events.c(  81):[lightduer_ca] <== event end = 40143358, timespent = 988
I (6851,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6858,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6868,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6877,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6884,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6892,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6912,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6913,tid:3ffddebc) lightduer_engine.c( 242):duer_engine_start, g_handler:3F82005C, length:0, profile:00000000
I (6920,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6933,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6937,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6946,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6954,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6964,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (6972,tid:3ffddebc) lightduer_connagent.c( 208):will start latter(DUER_ERR_TRANS_WOULD_BLOCK)
I (7232,tid:3ffddebc) lightduer_voice.c( 915):Mutex initializing
I (7233,tid:3ffddebc) lightduer_connagent.c( 189):connect started!
I (7234,tid:3ffddebc) lightduer_ds_log_cache.c(  85):no cache report
E (8705) DUEROS: event: 0
I (8708) DUEROS: duer_dcs_init
I (7247,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.private.protocol
I (7255,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.screen_extended_card
I (7264,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.screen
I (7273,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.system
I (7281,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.voice_input
I (7291,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.voice_output
I (7298,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.speaker_controller
I (7308,tid:3ffddebc) lightduer_dcs_router.c( 306):namespace: ai.dueros.device_interface.audio_player
I (8783) AUDIO_WRAPPER: duer_dcs_get_speaker_state
I (8797) DUEROS: Dueros DUER_CMD_CONNECTED, duer_state:2
W (8798) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:3, data_len:0
E (8799) DUEROS: event: DUER_EVENT_STARTED
W (7341,tid:3ffddebc) lightduer_events.c(  81):[lightduer_ca] <== event end = 40143358, timespent = 115
I (7350,tid:3ffddebc) lightduer_connagent.c( 222):add resource successfully!!
I (7358,tid:3ffddebc) lightduer_connagent.c( 222):add resource successfully!!
W (7364,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (7375,tid:3ffddebc) lightduer_system_info.c( 306):Undefined memory type, 0
E (7381,tid:3ffddebc) lightduer_system_info.c( 389):Sys Info: Get disk info failed
W (7398,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (7403,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (7413,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
W (7425,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
I (13242) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4137200 Bytes, Inter:121964 Bytes, Dram:112464 Bytes

I (18244) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4137200 Bytes, Inter:121964 Bytes, Dram:112464 Bytes
W (20644) REC_ENG: ### spot keyword ###
I (20644) DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_START
I (20645) DUEROS: rec_engine_cb - Play tone
I (20649) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (20655) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (20661) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffe1974
I (20668) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4137168 Bytes, Inter:121964 Bytes, Dram:112464 Bytes

I (20669) ESP_AUDIO_TASK: It's a decoder
I (20684) ESP_AUDIO_TASK: 1.CUR IN:[IN_file],CODEC:[DEC_wav],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (20696) ESP_AUDIO_TASK: 2.Handles,IN:0x3f821f0c,CODEC:0x3f8227e4,FILTER:0x3f829300,OUT:0x3f822e70
I (20705) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (20710) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (20716) AUDIO_PIPELINE: link el->rb, el:0x3f821f0c, tag:IN_file, rb:0x3f828eec
I (20724) AUDIO_PIPELINE: link el->rb, el:0x3f8227e4, tag:DEC_wav, rb:0x3f828f28
I (20732) AUDIO_PIPELINE: link el->rb, el:0x3f829300, tag:Audio_forge, rb:0x3f828f64
I (20741) ESP_AUDIO_TASK: 3. Previous starting...
I (20768) AUDIO_ELEMENT: [IN_file-0x3f821f0c] Element task created
I (20769) AUDIO_ELEMENT: [IN_file] AEL_MSG_CMD_RESUME,state:1
I (20771) AUDIO_THREAD: The DEC_wav task allocate stack on external memory
I (20779) AUDIO_ELEMENT: [DEC_wav-0x3f8227e4] Element task created
I (20785) AUDIO_ELEMENT: [DEC_wav] AEL_MSG_CMD_RESUME,state:1
I (20791) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (20839) FATFS_STREAM: File size: 17536 byte, file position: 0
I (20840) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f821f0c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (20866) CODEC_ELEMENT_HELPER: The element is 0x3f8227e4. The reserve data 2 is 0x0.
I (20867) WAV_DECODER: a new song playing
I (20870) ESP_AUDIO_TASK: Received music info then on play
I (20875) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (20881) AUDIO_THREAD: The Audio_forge task allocate stack on external memory
I (20889) AUDIO_ELEMENT: [Audio_forge-0x3f829300] Element task created
I (20896) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (20903) AUDIO_FORGE: audio_forge opened
I (20903) AUDIO_ELEMENT: [OUT_iis-0x3f822e70] Element task created
I (20909) AUDIO_FORGE: audio_forge reopen
I (20915) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (20926) I2S_STREAM: AUDIO_STREAM_WRITER
I (20932) ESP_AUDIO_CTRL: Exit play procedure, ret:0
I (20932) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f8227e4] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (20936) ESP_AUDIO_CTRL: Sync play waiting ...
I (20947) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f829300] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (20964) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (20975) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 1, src:0, is_stopping:0
I (20985) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4062980 Bytes, Inter:111692 Bytes, Dram:102192 Bytes

W (21055) FATFS_STREAM: No more data, ret:0
I (21056) AUDIO_ELEMENT: IN-[IN_file] AEL_IO_DONE,0
I (21058) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f821f0c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (21136) AUDIO_ELEMENT: IN-[DEC_wav] AEL_IO_DONE,-2
I (21136) DEC_WAV: Closed
I (21136) ESP_AUDIO_TASK: Received last pos: 17418 bytes
I (21141) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f8227e4] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (21321) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (21321) AUDIO_FORGE: audio forge closed
I (21322) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f829300] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (21377) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (21414) ESP_AUDIO_TASK: Received last time: 453 ms
I (21415) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (21420) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 1, src:0, is_stopping:0
I (21430) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4088772 Bytes, Inter:112296 Bytes, Dram:102796 Bytes

W (21442) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (21466) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (21466) ESP_AUDIO_CTRL: Sync play done

W (22153) REC_ENG: State VAD silence_time >= vad_off_window
I (22422) DUEROS: rec_engine_cb - REC_EVENT_VAD_START
I (22423) DUEROS: Recv Que DUER_CMD_START
I (22425) AUDIO_WRAPPER: duer_dcs_get_speaker_state
I (20963,tid:3ffc7c04) lightduer_dcs_local.c( 197):Current dialog id: 18c1000000000640b18ccf000051e200000003
W (22438) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:4, data_len:0
I (23547) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4093768 Bytes, Inter:120340 Bytes, Dram:110840 Bytes

W (24403) REC_ENG: State VAD silence_time >= vad_off_window
I (24404) DUEROS: rec_engine_cb - REC_EVENT_VAD_STOP, state:4
I (24405) DUEROS: Dueros DUER_CMD_STOP
W (24412) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:5, data_len:0
W (25203) REC_ENG: State VAD silence_time >= vad_off_window
I (23797,tid:3ffddebc) lightduer_dcs_router.c( 460):Directive name: RenderVoiceInputText
I (23803,tid:3ffddebc) lightduer_dcs_router.c( 460):Directive name: StopListen
I (25270) AUDIO_WRAPPER: stop_listen, close mic
I (25272) REC_ENG: Recorder trigger stop
W (23829,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:3, qcache_len:1
W (23832,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
E (25304) REC_ENG: State FETCH_DATA, wakeup_time_out:1, vad_speech_on:0, vad_time_mode:0
W (25313) REC_ENG: State WAKEUP_END
I (25318) DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_END
I (25323) REC_ENG: state idle
I (23861,tid:3ffdd8e8) lightduer_dcs_router.c( 460):Directive name: Speak
W (23865,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:3
I (25336) AUDIO_WRAPPER: Playing speak: http://res.iot.baidu.com:80/api/v1/tts/gcgyVNsTjSlogPFdXMAtiyIvjQy8NMAZrej-Al3yBIkoWHpkqqbHNQZc3Bor_XPblUCDuIhd1UWUkqLxUCPsrOLBywqvybvXMMzoJGDiTYI.mp3
W (23885,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
I (25361) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (25377) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
W (23916,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
I (25383) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffe1974
I (25400) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4073440 Bytes, Inter:120312 Bytes, Dram:110812 Bytes

I (25400) ESP_AUDIO_TASK: It's a decoder
I (25416) ESP_AUDIO_TASK: 1.CUR IN:[IN_http],CODEC:[DEC_mp3],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (25427) ESP_AUDIO_TASK: 2.Handles,IN:0x3f8224cc,CODEC:0x3f82296c,FILTER:0x3f829300,OUT:0x3f822e70
I (25437) AUDIO_PIPELINE: audio_pipeline_unlinked
W (25442) AUDIO_PIPELINE: There are no listener registered
I (25448) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (25453) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (25459) AUDIO_PIPELINE: link el->rb, el:0x3f8224cc, tag:IN_http, rb:0x3f8235e4
I (25467) AUDIO_PIPELINE: link el->rb, el:0x3f82296c, tag:DEC_mp3, rb:0x3f8290c4
I (25475) AUDIO_PIPELINE: link el->rb, el:0x3f829300, tag:Audio_forge, rb:0x3f8234cc
I (25484) ESP_AUDIO_TASK: 3. Previous starting...
I (25489) AUDIO_THREAD: The IN_http task allocate stack on external memory
W (23942,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:4, qcache_len:1
W (24041,tid:3ffddebc) lightduer_events.c(  81):[lightduer_ca] <== event end = 401437E0, timespent = 99
W (24060,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (24063,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:1
I (25556) AUDIO_ELEMENT: [IN_http-0x3f8224cc] Element task created
I (25557) AUDIO_ELEMENT: [IN_http] AEL_MSG_CMD_RESUME,state:1
I (25559) AUDIO_THREAD: The DEC_mp3 task allocate stack on external memory
I (25566) AUDIO_ELEMENT: [DEC_mp3-0x3f82296c] Element task created
I (25573) AUDIO_ELEMENT: [DEC_mp3] AEL_MSG_CMD_RESUME,state:1
I (25582) MP3_DECODER: MP3 opened
I (25583) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (25719) HTTP_STREAM: total_bytes=0
I (25720) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f8224cc] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (25757) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f82296c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (25762) ESP_AUDIO_TASK: Received music info then on play
I (25764) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (25771) AUDIO_THREAD: The Audio_forge task allocate stack on external memory
I (25779) AUDIO_ELEMENT: [Audio_forge-0x3f829300] Element task created
I (25786) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (25794) AUDIO_FORGE: audio_forge opened
I (25795) AUDIO_ELEMENT: [OUT_iis-0x3f822e70] Element task created
I (25804) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (25810) I2S_STREAM: AUDIO_STREAM_WRITER
I (25816) ESP_AUDIO_CTRL: Exit play procedure, ret:0
I (25817) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f829300] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
W (24356,tid:3ffdd8e8) lightduer_events.c(  81):[lightduer_handler] <== event end = 40144FAC, timespent = 495
I (25833) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (25853) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0
I (25863) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:3980636 Bytes, Inter:111160 Bytes, Dram:101660 Bytes

I (25875) AUDIO_WRAPPER: esp_auido status:1,err:0,state:1
I (24951,tid:3ffdd8e8) lightduer_dcs_router.c( 460):Directive name: RenderWeather
E (24952,tid:3ffdd8e8) lightduer_dcs_router.c( 486):unrecognized directive: RenderWeather, namespace: ai.dueros.device_interface.screen_extended_card

I (26456) AUDIO_WRAPPER: duer_dcs_get_speaker_state
W (25027,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:1
W (25038,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (25059,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (25079,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:5
W (25109,tid:3ffdd8e8) lightduer_events.c(  81):[lightduer_handler] <== event end = 40144FAC, timespent = 141
W (25118,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:9
W (25135,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:8
W (25151,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:7
W (25173,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:6
W (25221,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:5
W (25236,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (25246,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (25291,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
W (25326,tid:3ffddebc) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
I (28557) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:3986484 Bytes, Inter:111260 Bytes, Dram:101760 Bytes

W (29779) HTTP_STREAM: No more data,errno:0, total_bytes:22610, rlen = 0
I (29780) AUDIO_ELEMENT: IN-[IN_http] AEL_IO_DONE,0
I (29788) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f8224cc] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (33560) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:3997164 Bytes, Inter:115016 Bytes, Dram:105516 Bytes

I (34881) AUDIO_ELEMENT: IN-[DEC_mp3] AEL_IO_DONE,-2
I (35964) MP3_DECODER: Closed
I (35965) ESP_AUDIO_TASK: Received last pos: 20952 bytes
I (35966) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f82296c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (36221) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (36222) AUDIO_FORGE: audio forge closed
I (36226) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f829300] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (36271) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (36308) ESP_AUDIO_TASK: Received last time: 10469 ms
I (36309) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (36315) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 0, src:0, is_stopping:0
I (36324) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4057072 Bytes, Inter:115108 Bytes, Dram:105608 Bytes

W (36336) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (36363) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (36363) AUDIO_WRAPPER: esp_auido status:4,err:0,state:1
I (34899,tid:3ffdd8e8) lightduer_dcs_router.c( 460):Directive name: DialogueFinished
E (36373) AUDIO_WRAPPER: duer_dcs_speech_on_finished,0, wakeup:0
I (38565) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4080476 Bytes, Inter:120192 Bytes, Dram:110692 Bytes

I (43566) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4080476 Bytes, Inter:120192 Bytes, Dram:110692 Bytes

I (53048) DUEROS: PERIPH_NOTIFY_KEY_REC
I (53048) REC_ENG: Recorder trigger start
I (53069) REC_ENG: WAKEUP timer started
I (53069) DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_START
I (53070) DUEROS: rec_engine_cb - Play tone
I (53073) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (53079) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (53086) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffdf240
I (53093) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4137052 Bytes, Inter:121836 Bytes, Dram:112336 Bytes

I (53093) ESP_AUDIO_TASK: It's a decoder
I (53109) ESP_AUDIO_TASK: 1.CUR IN:[IN_file],CODEC:[DEC_wav],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (53120) ESP_AUDIO_TASK: 2.Handles,IN:0x3f821f0c,CODEC:0x3f8227e4,FILTER:0x3f828e2c,OUT:0x3f822e70
I (53129) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (53135) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (53141) AUDIO_PIPELINE: link el->rb, el:0x3f821f0c, tag:IN_file, rb:0x3f8290b0
I (53149) AUDIO_PIPELINE: link el->rb, el:0x3f8227e4, tag:DEC_wav, rb:0x3f8290ec
I (53157) AUDIO_PIPELINE: link el->rb, el:0x3f828e2c, tag:Audio_forge, rb:0x3f829128
I (53165) ESP_AUDIO_TASK: 3. Previous starting...
I (53182) AUDIO_ELEMENT: [IN_file-0x3f821f0c] Element task created
I (53183) AUDIO_ELEMENT: [IN_file] AEL_MSG_CMD_RESUME,state:1
I (53186) AUDIO_THREAD: The DEC_wav task allocate stack on external memory
I (53194) AUDIO_ELEMENT: [DEC_wav-0x3f8227e4] Element task created
I (53201) AUDIO_ELEMENT: [DEC_wav] AEL_MSG_CMD_RESUME,state:1
I (53207) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (53254) FATFS_STREAM: File size: 17536 byte, file position: 0
I (53255) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f821f0c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (53261) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4098536 Bytes, Inter:116460 Bytes, Dram:106960 Bytes

I (53283) CODEC_ELEMENT_HELPER: The element is 0x3f8227e4. The reserve data 2 is 0x0.
I (53283) WAV_DECODER: a new song playing
I (53287) ESP_AUDIO_TASK: Received music info then on play
I (53292) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (53299) AUDIO_THREAD: The Audio_forge task allocate stack on external memory
I (53307) AUDIO_ELEMENT: [Audio_forge-0x3f828e2c] Element task created
I (53314) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (53320) AUDIO_FORGE: audio_forge opened
I (53321) AUDIO_ELEMENT: [OUT_iis-0x3f822e70] Element task created
I (53327) AUDIO_FORGE: audio_forge reopen
I (53332) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (53343) I2S_STREAM: AUDIO_STREAM_WRITER
I (53348) ESP_AUDIO_CTRL: Exit play procedure, ret:0
I (53348) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f8227e4] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (53353) ESP_AUDIO_CTRL: Sync play waiting ...
I (53365) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f828e2c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (53381) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (53393) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 1, src:0, is_stopping:0
I (53402) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4062860 Bytes, Inter:111564 Bytes, Dram:102064 Bytes

W (53469) FATFS_STREAM: No more data, ret:0
I (53469) AUDIO_ELEMENT: IN-[IN_file] AEL_IO_DONE,0
I (53470) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f821f0c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (53550) AUDIO_ELEMENT: IN-[DEC_wav] AEL_IO_DONE,-2
I (53550) DEC_WAV: Closed
I (53550) ESP_AUDIO_TASK: Received last pos: 17418 bytes
I (53557) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f8227e4] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (53736) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (53736) AUDIO_FORGE: audio forge closed
I (53737) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f828e2c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (53792) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (53829) ESP_AUDIO_TASK: Received last time: 453 ms
I (53830) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (53836) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 1, src:0, is_stopping:0
I (53845) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4088656 Bytes, Inter:112168 Bytes, Dram:102668 Bytes

W (53857) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (53873) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (53874) ESP_AUDIO_CTRL: Sync play done
I (53876) DUEROS: rec_engine_cb - REC_EVENT_VAD_START
I (53881) DUEROS: Recv Que DUER_CMD_START
I (53887) AUDIO_WRAPPER: duer_dcs_get_speaker_state
I (52425,tid:3ffc7c88) lightduer_dcs_local.c( 197):Current dialog id: 18c1000000000640b18ccf0000ccc900000003
W (53901) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:4, data_len:0
W (57220) REC_ENG: State VAD silence_time >= vad_off_window
I (57221) DUEROS: rec_engine_cb - REC_EVENT_VAD_STOP, state:4
I (57222) DUEROS: Dueros DUER_CMD_STOP
W (57229) DUEROS: duer_callback: type:0, source:0x3f80a6a4 data:5, data_len:0
I (56506,tid:3ffddf40) lightduer_dcs_router.c( 460):Directive name: StopListen
I (57975) AUDIO_WRAPPER: stop_listen, close mic
I (57975) REC_ENG: Recorder trigger stop
I (56518,tid:3ffddf40) lightduer_dcs_router.c( 460):Directive name: RenderVoiceInputText
I (56531,tid:3ffdd9c4) lightduer_dcs_router.c( 460):Directive name: RenderCard
E (58005) REC_ENG: State FETCH_DATA, wakeup_time_out:1, vad_speech_on:0, vad_time_mode:0
W (58005) REC_ENG: State WAKEUP_END
W (56539,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:1, qcache_len:2
I (58009) DUEROS: rec_engine_cb - REC_EVENT_WAKEUP_END
I (58025) REC_ENG: state idle
I (56563,tid:3ffdd9c4) lightduer_dcs_router.c( 460):Directive name: Speak
I (58041) AUDIO_WRAPPER: Playing speak: http://res.iot.baidu.com:80/api/v1/tts/NW771vmjXGpIzW1Fpgpdjr47H2M5lNO250Bkrn40jRKXHn6LMfzbgEy5mNWS4hhLWW2sWqTZOz7m7oekCswmRSGqepB1fO8P7_OSchTi7QY.mp3
I (58053) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (58058) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (58066) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffdf240
I (58072) ESP_AUDIO_TASK: It's a decoder
I (58072) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4078976 Bytes, Inter:120184 Bytes, Dram:110684 Bytes

I (58077) ESP_AUDIO_TASK: 1.CUR IN:[IN_http],CODEC:[DEC_mp3],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (58099) ESP_AUDIO_TASK: 2.Handles,IN:0x3f8224cc,CODEC:0x3f82296c,FILTER:0x3f828e2c,OUT:0x3f822e70
I (58109) AUDIO_PIPELINE: audio_pipeline_unlinked
W (58114) AUDIO_PIPELINE: There are no listener registered
I (58120) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (58125) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (58131) AUDIO_PIPELINE: link el->rb, el:0x3f8224cc, tag:IN_http, rb:0x3f8302a4
I (58139) AUDIO_PIPELINE: link el->rb, el:0x3f82296c, tag:DEC_mp3, rb:0x3f823544
I (58147) AUDIO_PIPELINE: link el->rb, el:0x3f828e2c, tag:Audio_forge, rb:0x3f823580
I (58156) ESP_AUDIO_TASK: 3. Previous starting...
I (58161) AUDIO_THREAD: The IN_http task allocate stack on external memory
I (58194) AUDIO_ELEMENT: [IN_http-0x3f8224cc] Element task created
I (58194) AUDIO_ELEMENT: [IN_http] AEL_MSG_CMD_RESUME,state:1
I (58197) AUDIO_THREAD: The DEC_mp3 task allocate stack on external memory
I (58204) AUDIO_ELEMENT: [DEC_mp3-0x3f82296c] Element task created
I (58211) AUDIO_ELEMENT: [DEC_mp3] AEL_MSG_CMD_RESUME,state:1
I (58218) MP3_DECODER: MP3 opened
I (58219) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (58273) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4059664 Bytes, Inter:115440 Bytes, Dram:105940 Bytes

I (58354) HTTP_STREAM: total_bytes=0
I (58355) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f8224cc] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (58389) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f82296c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (58395) ESP_AUDIO_TASK: Received music info then on play
I (58396) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (58403) AUDIO_THREAD: The Audio_forge task allocate stack on external memory
I (58411) AUDIO_ELEMENT: [Audio_forge-0x3f828e2c] Element task created
I (58418) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (58428) AUDIO_FORGE: audio_forge opened
I (58429) AUDIO_ELEMENT: [OUT_iis-0x3f822e70] Element task created
I (58436) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (58442) I2S_STREAM: AUDIO_STREAM_WRITER
I (58449) ESP_AUDIO_CTRL: Exit play procedure, ret:0
I (58450) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f828e2c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
W (56986,tid:3ffdd9c4) lightduer_events.c(  81):[lightduer_handler] <== event end = 40144FAC, timespent = 424
I (58464) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (58485) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0
I (58495) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:3990064 Bytes, Inter:111124 Bytes, Dram:101624 Bytes

I (58506) AUDIO_WRAPPER: esp_auido status:1,err:0,state:1
W (57041,tid:3ffddf40) lightduer_events.c(  81):[lightduer_ca] <== event end = 40143358, timespent = 478
W (57056,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:4
W (57077,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:3
W (57088,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:2
W (57092,tid:3ffddf40) lightduer_engine.c( 697):data cache has not sent, pending..., dcache_len:2, qcache_len:1
W (58580) HTTP_STREAM: No more data,errno:0, total_bytes:5906, rlen = 0
I (58581) AUDIO_ELEMENT: IN-[IN_http] AEL_IO_DONE,0
I (58604) ESP_AUDIO_TASK: Recv Element[IN_http-0x3f8224cc] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (59170) AUDIO_ELEMENT: IN-[DEC_mp3] AEL_IO_DONE,-2
I (60256) MP3_DECODER: Closed
I (60257) ESP_AUDIO_TASK: Received last pos: 4248 bytes
I (60258) ESP_AUDIO_TASK: Recv Element[DEC_mp3-0x3f82296c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (60511) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (60512) AUDIO_FORGE: audio forge closed
I (60512) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f828e2c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (60567) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (60604) ESP_AUDIO_TASK: Received last time: 2128 ms
I (60605) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f822e70] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (60611) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 0, src:0, is_stopping:0
I (60620) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:984, MEM Total:4056968 Bytes, Inter:114976 Bytes, Dram:105476 Bytes

W (60632) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (60658) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (60659) AUDIO_WRAPPER: esp_auido status:4,err:0,state:1
I (59194,tid:3ffdd9c4) lightduer_dcs_router.c( 460):Directive name: DialogueFinished
E (60669) AUDIO_WRAPPER: duer_dcs_speech_on_finished,0, wakeup:0
I (63274) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4080364 Bytes, Inter:120060 Bytes, Dram:110560 Bytes

I (68275) DUEROS: Func:sys_monitor_task, Line:379, MEM Total:4080364 Bytes, Inter:120060 Bytes, Dram:110560 Bytes

```


## 故障排除

当前触摸驱动的限制是占用大量 CPU，可能会造成系统不能被语音唤醒或者系统播放音乐时断断续续，解决方法是不要在 `duer_service_create` 中使用以下代码。

```c
    periph_touch_cfg_t touch_cfg = {
        .touch_mask = TOUCH_PAD_SEL4 | TOUCH_PAD_SEL7 | TOUCH_PAD_SEL8 | TOUCH_PAD_SEL9,
        .tap_threshold_percent = 70,
    };
    esp_periph_handle_t touch_periph = periph_touch_init(&touch_cfg);
    esp_periph_start(touch_periph);
```

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
