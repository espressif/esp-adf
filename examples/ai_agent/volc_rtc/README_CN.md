# VOLC RTC 语音交互例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_complex.png "高级")


## 例程简介

本例程主要功能是连接豆包 volcano rtc 云端并进行语音交互，可以适用于智能音箱产品、智能玩具、语音控制设备等。此示例是一个综合性较强的例程，使用了 ADF 提供的高封装简易实用接口。建议用户构建项目时，优先使用 ADF 提供的高封装接口，可快速简便地构建项目。

目前 demo 中支持两种模式， 一个是唤醒对话模式，一个是普通模式 
- 唤醒对话模式是用户需要通过唤醒词去唤醒设备，唤醒后设备进入语音交互模式，用户可以与设备进行语音交互。默认的唤醒词是 `Hi 乐鑫`, 可以在 `menuconfig -> ESP Speech Recognition → use wakenet → Select wake words` 中去更换唤醒词
- 普通模式是用户无需唤醒词，直接与设备进行语音交互。

## 环境配置

### 硬件要求

目前 volcano rtc 仅仅支持 `esp32s3` 相关的开发板。

## 前期准备

1. 关于豆包简介

请参考[火山·引擎 开放平台](https://www.volcengine.com/docs/6348/1315561) 开通火山引擎服务， 需要获取 `appid`, `userid`, `roomid` 和 `临时token`, 将四个参数填到 `config.h` 对应的参数中， 然后在 [api-explorer](https://api.volcengine.com/api-explorer/?action=AssumeRole&groupName=%E8%A7%92%E8%89%B2%E6%89%AE%E6%BC%94&serviceCode=sts&version=2018-01-01) 中启动智能体，之后设备就可以连接到火山引擎进行语音交互了。

2. 在 `config.h` 中配置 wifi 的 SSID 和 PASSWORD。

## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v5.3.1 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.3/esp32/index.html)。

## 如何使用例程

### 功能和用法

- 例程运行后，会先尝试连接 Wi-Fi 网络，待成功获取 IP 地址后会进入一个房间。 进入房间成功后，log 中会有 `volc_rtc: join room success` 的打印，设备端听到服务器下发的"你好呀，很高兴认识你，哈哈哈"，便可以与智能体进行对话。
```c
I (25) boot: ESP-IDF v5.5-dev-972-gfa41fafd27-dirty 2nd stage bootloader
I (25) boot: compile time Jan 10 2025 10:39:14
I (25) boot: Multicore bootloader
I (27) boot: chip revision: v0.2
I (30) boot: efuse block revision: v1.3
I (34) qio_mode: Enabling default flash chip QIO
I (38) boot.esp32s3: Boot SPI Speed : 80MHz
I (42) boot.esp32s3: SPI Mode       : QIO
I (46) boot.esp32s3: SPI Flash Size : 16MB
I (49) boot: Enabling RNG early entropy source...
I (54) boot: Partition Table:
I (56) boot: ## Label            Usage          Type ST Offset   Length
I (63) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (69) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (76) boot:  2 factory          factory app      00 00 00010000 00300000
I (82) boot:  3 model            Unknown data     01 82 00310000 0040e000
I (89) boot:  4 spiffs_data      Unknown data     01 82 0071e000 00010000
I (95) boot: End of partition table
I (99) esp_image: segment 0: paddr=00010020 vaddr=3c180020 size=5db38h (383800) map
I (163) esp_image: segment 1: paddr=0006db60 vaddr=3fca2600 size=024b8h (  9400) load
I (165) esp_image: segment 2: paddr=00070020 vaddr=42000020 size=178fc4h (1544132) map
I (396) esp_image: segment 3: paddr=001e8fec vaddr=3fca4ab8 size=05db8h ( 23992) load
I (401) esp_image: segment 4: paddr=001eedac vaddr=40378000 size=1a520h (107808) load
I (421) esp_image: segment 5: paddr=002092d4 vaddr=600fe000 size=0001ch (    28) load
I (433) boot: Loaded app from partition at offset 0x10000
I (433) boot: Disabling RNG early entropy source...
W (443) flash HPM: HPM mode is optional feature that depends on flash model. Read Docs First!
W (443) flash HPM: HPM mode with DC adjustment is disabled. Some flash models may not be supported. Read Docs First!
W (452) flash HPM: High performance mode of this flash model hasn't been supported.
I (460) MSPI Timing: Flash timing tuning index: 2
I (466) octal_psram: vendor id    : 0x0d (AP)
I (471) octal_psram: dev id       : 0x02 (generation 3)
I (476) octal_psram: density      : 0x03 (64 Mbit)
I (482) octal_psram: good-die     : 0x01 (Pass)
I (487) octal_psram: Latency      : 0x01 (Fixed)
I (492) octal_psram: VCC          : 0x01 (3V)
I (497) octal_psram: SRF          : 0x01 (Fast Refresh)
I (503) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
I (509) octal_psram: BurstLen     : 0x01 (32 Byte)
I (515) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (521) octal_psram: DriveStrength: 0x00 (1/1)
I (531) MSPI Timing: PSRAM timing tuning index: 2
I (532) esp_psram: Found 8MB PSRAM device
I (536) esp_psram: Speed: 120MHz
I (557) mmu_psram: Read only data copied and mapped to SPIRAM
I (641) mmu_psram: Instructions copied and mapped to SPIRAM
I (641) cpu_start: Multicore app
I (821) esp_psram: SPI SRAM memory test OK
I (830) cpu_start: Pro cpu start user code
I (830) cpu_start: cpu freq: 240000000 Hz
I (830) app_init: Application information:
I (833) app_init: Project name:     volc_rtc
I (838) app_init: App version:      v2.7-48-g01596882-dirty
I (844) app_init: Compile time:     Jan  9 2025 22:09:38
I (850) app_init: ELF file SHA256:  75b7595ca...
I (856) app_init: ESP-IDF:          v5.5-dev-972-gfa41fafd27-dirty
I (863) efuse_init: Min chip rev:     v0.0
I (867) efuse_init: Max chip rev:     v0.99
I (872) efuse_init: Chip rev:         v0.2
I (877) heap_init: Initializing. RAM available for dynamic allocation:
I (884) heap_init: At 3FCAEF20 len 0003A7F0 (233 KiB): RAM
I (890) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
I (897) heap_init: At 600FE01C len 00001FCC (7 KiB): RTCRAM
I (903) esp_psram: Adding pool of 6258K of PSRAM memory to heap allocator
I (911) spi_flash: detected chip: gd
I (914) spi_flash: flash io: qio
W (919) ADC: legacy driver is deprecated, please migrate to `esp_adc/adc_oneshot.h`
I (927) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (934) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (942) main_task: Started on CPU0
I (954) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (955) main_task: Calling app_main()
I (961) main: Initialize board peripherals
I (965) PERIPH_SPIFFS: Partition size: total: 52961, used: 0
I (970) AUDIO_THREAD: The esp_periph task allocate stack on internal memory
W (978) i2c_bus_v2: I2C master handle is NULL, will create new one
I (989) DRV8311: ES8311 in Slave mode
I (999) gpio: GPIO[48]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1006) ES7210: ES7210 in Slave mode
I (1015) ES7210: Enable ES7210_INPUT_MIC1
I (1018) ES7210: Enable ES7210_INPUT_MIC2
I (1020) ES7210: Enable ES7210_INPUT_MIC3
W (1024) ES7210: Enable TDM mode. ES7210_SDP_INTERFACE2_REG12: 2
I (1028) ES7210: config fmt 60
I (1030) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (1040) pp: pp rom version: e7ae62f
I (1040) net80211: net80211 rom version: e7ae62f
I (1044) wifi:wifi driver task: 3fcbee58, prio:23, stack:6656, core=0
I (1050) wifi:wifi firmware version: 34d97ea27
I (1053) wifi:wifi certification version: v7.0
I (1057) wifi:config NVS flash: enabled
I (1061) wifi:config nano formatting: disabled
I (1065) wifi:Init data frame dynamic rx buffer num: 32
I (1070) wifi:Init static rx mgmt buffer num: 8
I (1074) wifi:Init management short buffer num: 32
I (1079) wifi:Init static tx buffer num: 16
I (1083) wifi:Init tx cache buffer num: 32
I (1086) wifi:Init static tx FG buffer num: 2
I (1090) wifi:Init static rx buffer size: 1600
I (1095) wifi:Init static rx buffer num: 16
I (1098) wifi:Init dynamic rx buffer num: 32
I (1103) wifi_init: rx ba win: 16
I (1106) wifi_init: accept mbox: 6
I (1110) wifi_init: tcpip mbox: 32
I (1114) wifi_init: udp mbox: 32
I (1118) wifi_init: tcp mbox: 6
I (1122) wifi_init: tcp tx win: 65535
I (1126) wifi_init: tcp rx win: 65535
I (1131) wifi_init: tcp mss: 1440
I (1135) wifi_init: WiFi/LWIP prefer SPIRAM
I (1140) wifi_init: WiFi IRAM OP enabled
I (1144) wifi_init: WiFi RX IRAM OP enabled
W (1149) wifi:Password length matches WPA2 standards, authmode threshold changes from OPEN to WPA2
I (1158) wifi:Set ps type: 1, coexist: 0

I (1162) phy_init: phy_version 680,a6008b2,Jun  4 2024,16:41:10
I (1227) wifi:mode : sta (74:4d:bd:9d:b6:30)
I (1227) wifi:enable tsf
W (1227) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:43
I (2520) wifi:new:<10,0>, old:<1,0>, ap:<255,255>, sta:<10,0>, prof:1, snd_ch_cfg:0x0
I (2521) wifi:state: init -> auth (0xb0)
W (2521) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:43
I (2525) wifi:state: auth -> assoc (0x0)
I (2541) wifi:state: assoc -> run (0x10)
I (2761) wifi:connected with ESP-Audio, aid = 3, channel 10, BW20, bssid = fc:2f:ef:ab:db:70
I (2762) wifi:security: WPA2-PSK, phy: bgn, rssi: -39
I (2763) wifi:pm start, type: 1

I (2766) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
W (2775) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (2792) wifi:AP's beacon interval = 204800 us, DTIM period = 1
I (2792) wifi:<ba-add>idx:0 (ifx:0, fc:2f:ef:ab:db:70), tid:0, ssn:3, winSize:64
I (3796) esp_netif_handlers: sta ip: 192.168.1.100, mask: 255.255.255.0, gw: 192.168.1.1
I (3796) PERIPH_WIFI: Got ip:192.168.1.100
I (3798) AUDIO_THREAD: The monitor_task task allocate stack on external memory
I (3807) audio pipeline: Create audio pipeline for audio player
I (3813) audio pipeline: Create audio player audio stream
I (3820) audio pipeline: Register all elements to playback pipeline
I (3826) audio pipeline: Link playback element together raw-->audio_decoder-->i2s_stream-->[codec_chip]
E (3836) gpio: gpio_install_isr_service(502): GPIO isr service already installed
E (3844) DISPATCHER: exe first list: 0x0
I (3849) DISPATCHER: dispatcher_event_task is running...
I (3951) wifi:<ba-add>idx:1 (ifx:0, fc:2f:ef:ab:db:70), tid:1, ssn:0, winSize:64
I (4807) AUDIO_SYS: | Task              | Run Time    | Per | Prio | HWM       | State   | CoreId   | Stack
I (4808) AUDIO_SYS: | monitor_task      | 675         | 0%  | 23   | 4316      | Running | 0        | Extr
I (4817) AUDIO_SYS: | main              | 254004      |12%  | 1    | 1324      | Ready   | 0        | Intr
I (4828) AUDIO_SYS: | IDLE1             | 994708      |49%  | 0    | 700       | Ready   | 1        | Intr
I (4838) AUDIO_SYS: | IDLE0             | 726577      |36%  | 0    | 692       | Ready   | 0        | Intr
I (4848) AUDIO_SYS: | tiT               | 8424        | 0%  | 18   | 1792      | Blocked | 7fffffff | Intr
I (4859) AUDIO_SYS: | esp_periph        | 1842        | 0%  | 5    | 1592      | Blocked | 0        | Intr
I (4869) AUDIO_SYS: | ipc1              | 0           | 0%  | 24   | 540       | Suspended | 1        | Intr
I (4879) AUDIO_SYS: | ipc0              | 0           | 0%  | 1    | 436       | Suspended | 0        | Intr
I (4890) AUDIO_SYS: | wifi              | 7636        | 0%  | 23   | 3452      | Blocked | 0        | Intr
I (4900) AUDIO_SYS: | esp_timer         | 309         | 0%  | 22   | 3092      | Suspended | 0        | Intr
I (4911) AUDIO_SYS: | sys_evt           | 46          | 0%  | 20   | 416       | Blocked | 0        | Intr
I (4921) AUDIO_SYS: | Tmr Svc           | 0           | 0%  | 1    | 3364      | Blocked | 7fffffff | Intr
I (4931) AUDIO_SYS: | audio_player_st | Created
I (4937) AUDIO_SYS: | esp_dispatcher | Created

I (4942) main: Func:monitor_task, Line:29, MEM Total:6377072 Bytes, Inter:135047 Bytes, Dram:135047 Bytes, Dram largest free:94208Bytes

1970-01-01 00:00:04.213 [E]  VolcEngineRTCLite.c:105 ****************** HELLO BOOKA (671221aa298a540183df32d9)(1.56.001.58)(6059fcf26792a8820bc81f13662979d531e5504d) ********************
1970-01-01 00:00:04.227 [E]  Cache.c:270 operation returned status code: 0x00000009
1970-01-01 00:00:04.242 [E]  ThreadPool.c:92 coreid 1 set 1 stack_size 8192 priority 5
I (5075) audio pipeline: Create audio pipeline for recording
I (5078) audio pipeline: Create player audio stream
I (5085) audio pipeline:  Register all player elements to audio pipeline
I (5091) audio pipeline:  Link all player elements to audio pipeline
I (5099) audio pipeline: Create audio pipeline for playback
I (5105) audio pipeline: Create playback audio stream
W (5110) I2S_STREAM_IDF5.x: I2S(2) already startup
I (5116) audio pipeline: Create opus decoder
I (5121) audio pipeline: Register all elements to playback pipeline
I (5128) audio pipeline: Link playback element together raw-->audio_decoder-->i2s_stream-->[codec_chip]
1970-01-01 00:00:05.039 [E]  RoomImplX.c:167 operation returned status code: 0x52000057
1970-01-01 00:00:05.459 [E]  Cache.c:309 operation returned status code: 0x00000009
1970-01-01 00:00:05.465 [E]  RoomImplX.c:167 operation returned status code: 0x52000057
1970-01-01 00:00:05.467 [E]  LiteHttp.c:641 ID 340052878  E_LOGIC : NO need keepAlive
1970-01-01 00:00:05.475 [E]  RoomImplX.c:167 operation returned status code: 0x52000057
1970-01-01 00:00:05.583 [E]  RoomImplX.c:167 operation returned status code: 0x52000057
1970-01-01 00:00:06.042 [E]  rx_net_delay_manager.c:1130
I (6880) volc_rtc: join channel success ************8105400500486185 elapsed 239 ms now 239 ms

I (6880) volc_rtc: join room success

I (6880) volc_rtc: remote user joined  *****

1970-01-01 00:00:06.063 [E]  EngineImplX.c:103 callback pEngineImplX->eventHandler.on_user_joined used too many times 12
I (6895) MODEL_LOADER: The storage free size is 24576 KB
I (6911) MODEL_LOADER: The partition size is 4152 KB
I (6917) MODEL_LOADER: Successfully load srmodels
I (6922) RECORDER_SR: The first wakenet model: wn9_hilexin

I (6929) AFE_SR: afe interface for speech recognition
I (6935) AFE_SR: AFE version: SR_V220727
I (6939) AFE_SR: Initial auido front-end, total channel: 2, mic num: 1, ref num: 1
I (6951) AFE_SR: aec_init: 1, se_init: 0, vad_init: 0(min speech:64, min noise:256)
I (6956) AFE_SR: wakenet_init: 0
I (7017) AFE_SR: afe mode: 0, (Jan  2 2025 19:06:11)
W (7017) RECORDER_SR: Multinet is not enabled in SDKCONFIG
I (7018) AUDIO_RECORDER: RECORDER_CMD_TRIGGER_START
I (7018) main_task: Returned from app_main()
1970-01-01 00:00:06.212 [E]  rx_net_audio_jitterbuffer.c:1266
1970-01-01 00:00:06.219 [E]  rx_net_audio_jitterbuffer.c:1190
1970-01-01 00:00:06.219 [E]  rx_net_delay_manager.c:1130
1970-01-01 00:00:06.219 [E]  rx_net_delay_manager.c:1130
1970-01-01 00:00:06.224 [E]  Counter.c:90 AudioRecevied fps 0
1970-01-01 00:00:07.007 [E]  Counter.c:90 AudioRecevied fps 39
1970-01-01 00:00:08.012 [E]  Counter.c:90 AudioRecevied fps 49
1970-01-01 00:00:09.163 [E]  Counter.c:90 AudioRecevied fps 44
1970-01-01 00:00:10.014 [E]  Counter.c:90 AudioRecevied fps 56
1970-01-01 00:00:11.016 [E]  Counter.c:90 AudioRecevied fps 50
1970-01-01 00:00:12.021 [E]  Counter.c:90 AudioRecevied fps 46
1970-01-01 00:00:13.001 [E]  Counter.c:90 AudioRecevied fps 53
```

## 故障排除

- 出现加入房间房间打印后没有任何声音
 > 请检查 token 的有效性和智能体是否开启, 更多的查看 [常见的集成问题](https://bytedance.larkoffice.com/docx/TEMCdrJ3VouilPxSpjbc6CyUnAh)
- 出现自问自答的现象
 > 需要确认 AEC 是否生效, 可以在 `audiuo_processor.c` 中将宏 `ENABLE_AEC_DEBUG`打开，再次运行，查看录音数据

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
