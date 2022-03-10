# 命令行接口 (CLI) 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了 ADF 的 `periph_console` 控制 `esp_audio` API 和其他系统 API 的方法。

在熟记命令的前提下，使用命令行接口 (command-line interface, CLI) 可以在 ADF 项目的快速 debug 分析和快速 feature 测试中有较大的优势。

当前支持的命令如下表：

|序号|命令|描述或用法|关联函数|
| --  | -- | -- | -- |
| 01  | play      |  播放指定序号的音乐，命令为 "play [index or url] [byte_pos]" <br /> 1. "play" <br /> 2. 扫描后播放指定序号的音乐，命令为 "play index_number" <br /> 3. 从指定网址播放，命令为 "play url_path"               |  cli_play |
| 02  | pause     |  暂停播放音乐                                                   |  cli_pause  |
| 03  | resume    |  继续播放                                                                       |  cli_resume  |
| 04  | stop      |  停止播放                                                                 |  cli_stop  |
| 05  | setvol    |  设置音量                                                                  |  cli_set_vol  |
| 06  | getvol    |  获取音量                                                                   |  cli_get_vol  |
| 07  | getpos    |  获取位置，单位为秒                                                    |  get_pos  |
| 08  | seek      |  寻找位置，单位为秒                                                     |  cli_seek  |
| 09  | duration  |  获取音乐时长                                                           |  cli_duration  |
| 10  | tone      |  插入并播放提示音                                                         |  cli_insert_tone  |
| 11  | stone     |  定时关闭提示音                                                         |  cli_stop_tone  |
| 12  | setspeed  |  设置速度                                                                    |  cli_set_speed  |
| 13  | getspeed  |  获取速度                                                                   |  cli_get_speed  |
| 14  | join      |  作为 station 与 Wi-Fi AP 连接                                                   |  wifi_set  |
| 15  | wifi      |  获取已连接的 AP 信息                                              |  wifi_info  |
| 16  | led       |  Lyrat-MSC LED 灯带模式                                                   |  led  |
| 17  | scan      |  扫描 microSD 卡中的音乐文件，命令为 "scan [path]"，如 "scan /sdcard"          |  playlist_sd_scan  |
| 18  | list      |  显示扫描的播放列表                                                        |  playlist_sd_show  |
| 19  | next      |  播放下 x 个文件，命令为 "next [step]"                                    |  playlist_sd_next  |
| 20  | prev      |  播放上 x 个文件，命令为 "prev [step]"                                |  playlist_sd_prev  |
| 21  | mode      |  设置自动播放模式，命令为 "mode [value]"， 0 表示仅播放一次，其他值表示列表内循环播放|  playlist_set_mode  |
| 22  | reboot    |  重启系统                                                                |  sys_reset  |
| 23  | free      |  获取系统空闲内存                                                       |  show_free_mem  |
| 24  | stat      |  显示所有 FreeRTOS 任务处理时间                                    |  run_time_stats  |
| 25  | tasks     |  获取正在运行的任务信息                                          |  task_list  |
| 26  | system    |  获取所有 FreeRTOS 任务状态信息                                    |  task_real_time_states  |


## 环境配置


### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程需要准备一张 microSD 卡，准备一些 AMR、FLAC、OGG、OPUS、MP3、WAV、AAC、TS、M4A 等格式的音频文件，并命名为 `test.amr`、`test.flac`、`test.ogg`、`test.opus`、`test.mp3`、`test.wav`、`test.aac`、`test.ts` 和 `test.m4a` 样式，然后拷贝到 microSD 中，最后插入开发板备用。

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

如果你需要修改录音文件名，本例程建议同时打开 FatFs 长文件名支持。

```
menuconfig > Component config > FAT Filesystem support > Long filename support
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

- 例程开始运行后，终端打印 `esp32>` 等待用户输入指令，打印如下：

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 15:16:34
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 4MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00200000
I (90) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x866c8 (550600) map
I (340) esp_image: segment 1: paddr=0x000966f0 vaddr=0x3ffb0000 size=0x03620 ( 13856) load
I (346) esp_image: segment 2: paddr=0x00099d18 vaddr=0x40080000 size=0x06300 ( 25344) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (359) esp_image: segment 3: paddr=0x000a0020 vaddr=0x400d0020 size=0x1098bc (1087676) map
0x400d0020: _stext at ??:?

I (813) esp_image: segment 4: paddr=0x001a98e4 vaddr=0x40086300 size=0x189c0 (100800) load
0x40086300: esp_flash_erase_region at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/spi_flash/esp_flash_api.c:364

I (878) boot: Loaded app from partition at offset 0x10000
I (878) boot: Disabling RNG early entropy source...
I (879) psram: This chip is ESP32-D0WD
I (884) spiram: Found 64MBit SPI RAM device
I (888) spiram: SPI RAM mode: flash 40m sram 40m
I (893) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (900) cpu_start: Pro cpu up.
I (904) cpu_start: Application information:
I (909) cpu_start: Project name:     cli_app
I (914) cpu_start: App version:      v2.2-201-g6fa02923
I (920) cpu_start: Compile time:     Oct  8 2021 15:16:28
I (926) cpu_start: ELF file SHA256:  678b9a48007e0989...
I (932) cpu_start: ESP-IDF:          v4.2.2
I (937) cpu_start: Starting app cpu, entry point is 0x40082050
0x40082050: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1831) spiram: SPI SRAM memory test OK
I (1831) heap_init: Initializing. RAM available for dynamic allocation:
I (1832) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1838) heap_init: At 3FFB8870 len 00027790 (157 KiB): DRAM
I (1844) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1850) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1857) heap_init: At 4009ECC0 len 00001340 (4 KiB): IRAM
I (1863) cpu_start: Pro cpu start user code
I (1868) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1890) spi_flash: detected chip: gd
I (1891) spi_flash: flash io: dio
W (1891) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1901) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1912) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1952) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1992) SDCARD: CID name SA16G!

I (2452) CONSOLE_EXAMPLE: Start Wi-Fi
I (2452) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (2452) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (2472) wifi:wifi driver task: 3ffca798, prio:23, stack:6656, core=0
I (2472) system_api: Base MAC address is not set
I (2472) system_api: read default base MAC address from EFUSE
I (2482) wifi:wifi firmware version: bb6888c
I (2482) wifi:wifi certification version: v7.0
I (2482) wifi:config NVS flash: enabled
I (2492) wifi:config nano formating: disabled
I (2492) wifi:Init data frame dynamic rx buffer num: 32
I (2492) wifi:Init management frame dynamic rx buffer num: 32
I (2502) wifi:Init management short buffer num: 32
I (2512) wifi:Init static tx buffer num: 16
I (2512) wifi:Init tx cache buffer num: 32
I (2512) wifi:Init static rx buffer size: 1600
I (2522) wifi:Init static rx buffer num: 10
I (2522) wifi:Init dynamic rx buffer num: 32
I (2532) wifi_init: rx ba win: 6
I (2532) wifi_init: tcpip mbox: 32
I (2532) wifi_init: udp mbox: 6
I (2542) wifi_init: tcp mbox: 6
I (2542) wifi_init: tcp tx win: 5744
I (2542) wifi_init: tcp rx win: 5744
I (2552) wifi_init: tcp mss: 1440
I (2552) wifi_init: WiFi IRAM OP enabled
I (2562) wifi_init: WiFi RX IRAM OP enabled
I (2562) phy_init: phy_version 4660,0162888,Dec 23 2020
I (2692) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2702) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (2702) ES8388_DRIVER: init,out:02, in:00
I (2712) ESP_AUDIO_TASK: media_ctrl_task running...,0x3f806b38

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-9-g84df87e-037bef3-09be8fe                |
|                     Compile date: Jul 20 2021-13:51:36                     |
------------------------------------------------------------------------------
I (2742) ESP_AUDIO_CTRL: Func:media_ctrl_create, Line:350, MEM Total:4292044 Bytes, Inter:146012 Bytes, Dram:141120 Bytes

I (2752) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (2762) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2772) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2802) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (2802) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (2812) MP3_DECODER: MP3 init
W (2812) I2S: I2S driver already installed
I (2812) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (2822) CONSOLE_EXAMPLE: Func:cli_setup_player, Line:657, MEM Total:4273924 Bytes, Inter:123648 Bytes, Dram:118756 Bytes

I (2832) CONSOLE_EXAMPLE: esp_audio instance is:0x3f806b38


esp32>
```

- 此时，我可以使用 `join` 指令来连接 Wi-Fi AP，无线路由的 SSID 为 `esp32`，无线路由器的密码为 `esp123456`，命令执行后日志打印显示设备已经成功获取 IP 地址，打印如下：

```c
esp32> join esp32 esp123456
I (40462) CONSOLE_EXAMPLE: Connecting Wi-Fi, SSID:"esp32" PASSWORD:"esp123456"
esp32> I (41682) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (42532) wifi:state: init -> auth (b0)
I (42552) wifi:state: auth -> assoc (0)
I (42562) wifi:state: assoc -> run (10)
I (42572) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (42572) wifi:security: WPA2-PSK, phy: bgn, rssi: -40
I (42572) wifi:pm start, type: 1

W (42592) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (42712) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (44452) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (44452) PERIPH_WIFI: Got ip:192.168.5.187

esp32>
```

- 使用 `scan /sdcard` 命令扫描 microSD 卡中的音频文件，打印如下：

```c
esp32> scan /sdcard
I (92022) PLAYLIST_SDCARD: ID   URL
I (92022) PLAYLIST_SDCARD: 0   file://sdcard/wechat.amr
I (92022) PLAYLIST_SDCARD: 1   file://sdcard/test.mp3
I (92022) PLAYLIST_SDCARD: 2   file://sdcard/DU1906_����_������Ƶ.mp3
I (92042) PLAYLIST_SDCARD: 3   file://sdcard/REC.WAV
I (92042) PLAYLIST_SDCARD: 4   file://sdcard/rec_out.wav
I (92052) PLAYLIST_SDCARD: 5   file://sdcard/rec_out.amr
I (92052) PLAYLIST_SDCARD: 6   file://sdcard/test.aac
I (92062) PLAYLIST_SDCARD: 7   file://sdcard/test_output.mp3
I (92062) PLAYLIST_SDCARD: 8   file://sdcard/1.Lyrat_v4.3_REC_ble_spi_sdcard.WAV
I (92082) PLAYLIST_SDCARD: 9   file://sdcard/start_service.mp3
I (92082) PLAYLIST_SDCARD: 10   file://sdcard/588E81FFFE34F6D92020-05-28-15-48-47.opus
I (92092) PLAYLIST_SDCARD: 11   file://sdcard/588E81FFFE34F6D92020-05-28-16-13-17.opus
I (92092) PLAYLIST_SDCARD: 12   file://sdcard/588E81FFFE34F6D92020-05-28-17-12-43.opus
I (92112) PLAYLIST_SDCARD: 13   file://sdcard/test.wav
I (92112) PLAYLIST_SDCARD: 14   file://sdcard/2.Lyrat_v4.3_REC_spi_sdcard.WAV
I (92122) PLAYLIST_SDCARD: 15   file://sdcard/7c9ebdcf12f0-19700101080024.wav
esp32>
```

- 使用 `play 13` 命令播放 microSD 卡中序号为 13 的 `test.WAV` 音频文件，打印如下：

```c
I (123222) CONSOLE_EXAMPLE: app_audio play
I (123232) CONSOLE_EXAMPLE: play index= 13, URI:file://sdcard/test.wav, byte_pos:0
I (123232) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (123242) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (123242) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffe0ed4
I (123252) ESP_AUDIO_TASK: It's a decoder
I (123262) ESP_AUDIO_TASK: 1.CUR IN:[IN_file],CODEC:[DEC_wav],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (123262) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4268264 Bytes, Inter:118056 Bytes, Dram:113164 Bytes
[0I (123272) ESP_AUDIO_TASK: 2.Handles,IN:0x3f806e9c,CODEC:0x3f807f14,FILTER:0x3f808d88,OUT:0x3f808704
m
I (123292) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (123292) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (123302) AUDIO_PIPELINE: link el->rb, el:0x3f806e9c, tag:IN_file, rb:0x3f80909c
I (123312) AUDIO_PIPELINE: link el->rb, el:0x3f807f14, tag:DEC_wav, rb:0x3f80b8dc
I (123312) AUDIO_PIPELINE: link el->rb, el:0x3f808d88, tag:Audio_forge, rb:0x3f80d91c
I (123322) ESP_AUDIO_TASK: 3. Previous starting...
I (123332) AUDIO_ELEMENT: [IN_file-0x3f806e9c] Element task created
I (123342) AUDIO_ELEMENT: [IN_file] AEL_MSG_CMD_RESUME,state:1
I (123342) AUDIO_ELEMENT: [DEC_wav-0x3f807f14] Element task created
I (123352) AUDIO_ELEMENT: [DEC_wav] AEL_MSG_CMD_RESUME,state:1
I (123352) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (123362) FATFS_STREAM: File size: 17432920 byte, file position: 0
I (123372) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f806e9c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (123372) CODEC_ELEMENT_HELPER: The element is 0x3f807f14. The reserve data 2 is 0x0.
I (123392) WAV_DECODER: a new song playing
I (123392) ESP_AUDIO_TASK: Received music info then on play
I (123412) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (123412) AUDIO_ELEMENT: [Audio_forge-0x3f808d88] Element task created
I (123422) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (123422) AUDIO_FORGE: audio_forge opened
I (123422) AUDIO_ELEMENT: [OUT_iis-0x3f808704] Element task created
I (123442) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (123442) I2S_STREAM: AUDIO_STREAM_WRITER
I (123442) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f807f14] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (123462) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f808d88] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (123472) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f808704] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (123492) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0
I (123492) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:938, MEM Total:4191004 Bytes, Inter:101240 Bytes, Dram:96348 Bytes

I (123502) ESP_AUDIO_CTRL: Exit play procedure, ret:0
esp32> I (123502) CONSOLE_EXAMPLE: esp_auido status:1,err:0


esp32> W (214072) FATFS_STREAM: No more data, ret:0
I (214072) AUDIO_ELEMENT: IN-[IN_file] AEL_IO_DONE,0
I (214072) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f806e9c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (214122) AUDIO_ELEMENT: IN-[DEC_wav] AEL_IO_DONE,-2
I (214132) DEC_WAV: Closed
I (214132) ESP_AUDIO_TASK: Received last pos: 17432920 bytes
I (214132) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f807f14] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (214172) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (214172) AUDIO_FORGE: audio forge closed
I (214172) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f808d88] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (214232) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (214262) ESP_AUDIO_TASK: Received last time: 90794 ms
I (214262) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f808704] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (214282) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 0, src:0, is_stopping:0
I (214292) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:938, MEM Total:4219308 Bytes, Inter:101336 Bytes, Dram:96444 Bytes

W (214292) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (214302) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (214302) CONSOLE_EXAMPLE: esp_auido status:4,err:0

I (214322) ESP_AUDIO_CTRL: Total:17432920, get duration:90796 ms by bps:1536000
I (214322) CONSOLE_EXAMPLE: [ * ] End of time:90794 ms, duration:90796 ms
I (214332) CONSOLE_EXAMPLE: Func:esp_audio_state_task, Line:522, MEM Total:4239008 Bytes, Inter:116632 Bytes, Dram:111740 Bytes


esp32>

```
- 用户可以自行尝试其他的命令，也可以使用 `help` 命令来获取命令帮助。

```c
esp32> help
----------------------
Perpheral console HELP
----------------------
play    Play music, cmd:"play [index or url] [byte_pos]",
            e.g. 1."play"; 2. play with index after scan,"play index_number"; 3.play with specific url, "play url_path"
pause   Pause
resume  Resume
stop    Stop player
setvol  Set volume
getvol  Get volume
getpos  Get position by seconds
seek    Seek position by second
duration    Get music duration
tone    Insert tone to play
stone   Stop tone by a timer
setspeed    Set speed
getspeed    Get speed
join    Join WiFi AP as a station
wifi    Get connected AP information
led     Lyrat-MSC led bar pattern
scan    Scan sdcard music file, cmd: "scan [path]",e.g. "scan /sdcard"
list    Show scanned playlist
next    Next x file to play, cmd: "next [step]"
prev    Previous x file to play, cmd: "prev [step]"
mode    Set auto play mode, cmd:"mode [value]", 0:once; others: playlist loop all
reboot  Reboot system
free    Get system free memory
stat    Show processor time of all FreeRTOS tasks
tasks   Get information about running tasks
system  Get freertos all task states information
esp32>

```


### 日志输出

以下为本例程的完整日志。

```c
ets Jul 29 2019 12:21:46

rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7152
load:0x40078000,len:13664
load:0x40080400,len:4632
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 15:16:34
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 4MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00200000
I (90) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x866c8 (550600) map
I (340) esp_image: segment 1: paddr=0x000966f0 vaddr=0x3ffb0000 size=0x03620 ( 13856) load
I (346) esp_image: segment 2: paddr=0x00099d18 vaddr=0x40080000 size=0x06300 ( 25344) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (359) esp_image: segment 3: paddr=0x000a0020 vaddr=0x400d0020 size=0x1098bc (1087676) map
0x400d0020: _stext at ??:?

I (813) esp_image: segment 4: paddr=0x001a98e4 vaddr=0x40086300 size=0x189c0 (100800) load
0x40086300: esp_flash_erase_region at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/spi_flash/esp_flash_api.c:364

I (878) boot: Loaded app from partition at offset 0x10000
I (878) boot: Disabling RNG early entropy source...
I (879) psram: This chip is ESP32-D0WD
I (884) spiram: Found 64MBit SPI RAM device
I (888) spiram: SPI RAM mode: flash 40m sram 40m
I (893) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (900) cpu_start: Pro cpu up.
I (904) cpu_start: Application information:
I (909) cpu_start: Project name:     cli_app
I (914) cpu_start: App version:      v2.2-201-g6fa02923
I (920) cpu_start: Compile time:     Oct  8 2021 15:16:28
I (926) cpu_start: ELF file SHA256:  678b9a48007e0989...
I (932) cpu_start: ESP-IDF:          v4.2.2
I (937) cpu_start: Starting app cpu, entry point is 0x40082050
0x40082050: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1831) spiram: SPI SRAM memory test OK
I (1831) heap_init: Initializing. RAM available for dynamic allocation:
I (1832) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1838) heap_init: At 3FFB8870 len 00027790 (157 KiB): DRAM
I (1844) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1850) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1857) heap_init: At 4009ECC0 len 00001340 (4 KiB): IRAM
I (1863) cpu_start: Pro cpu start user code
I (1868) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1890) spi_flash: detected chip: gd
I (1891) spi_flash: flash io: dio
W (1891) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (1901) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1912) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1952) SDCARD: Using 1-line SD mode, 4-line SD mode,  base path=/sdcard
I (1992) SDCARD: CID name SA16G!

I (2452) CONSOLE_EXAMPLE: Start Wi-Fi
I (2452) gpio: GPIO[19]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
E (2452) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (2472) wifi:wifi driver task: 3ffca798, prio:23, stack:6656, core=0
I (2472) system_api: Base MAC address is not set
I (2472) system_api: read default base MAC address from EFUSE
I (2482) wifi:wifi firmware version: bb6888c
I (2482) wifi:wifi certification version: v7.0
I (2482) wifi:config NVS flash: enabled
I (2492) wifi:config nano formating: disabled
I (2492) wifi:Init data frame dynamic rx buffer num: 32
I (2492) wifi:Init management frame dynamic rx buffer num: 32
I (2502) wifi:Init management short buffer num: 32
I (2512) wifi:Init static tx buffer num: 16
I (2512) wifi:Init tx cache buffer num: 32
I (2512) wifi:Init static rx buffer size: 1600
I (2522) wifi:Init static rx buffer num: 10
I (2522) wifi:Init dynamic rx buffer num: 32
I (2532) wifi_init: rx ba win: 6
I (2532) wifi_init: tcpip mbox: 32
I (2532) wifi_init: udp mbox: 6
I (2542) wifi_init: tcp mbox: 6
I (2542) wifi_init: tcp tx win: 5744
I (2542) wifi_init: tcp rx win: 5744
I (2552) wifi_init: tcp mss: 1440
I (2552) wifi_init: WiFi IRAM OP enabled
I (2562) wifi_init: WiFi RX IRAM OP enabled
I (2562) phy_init: phy_version 4660,0162888,Dec 23 2020
I (2692) wifi:mode : sta (94:b9:7e:65:c2:44)
I (2702) gpio: GPIO[21]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (2702) ES8388_DRIVER: init,out:02, in:00
I (2712) ESP_AUDIO_TASK: media_ctrl_task running...,0x3f806b38

----------------------------- ESP Audio Platform -----------------------------
|                                                                            |
|                 ESP_AUDIO-v1.7.0-9-g84df87e-037bef3-09be8fe                |
|                     Compile date: Jul 20 2021-13:51:36                     |
------------------------------------------------------------------------------
I (2742) ESP_AUDIO_CTRL: Func:media_ctrl_create, Line:350, MEM Total:4292044 Bytes, Inter:146012 Bytes, Dram:141120 Bytes

I (2752) AUDIO_HAL: Codec mode is 3, Ctrl:1
I (2762) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2772) I2S: DMA Malloc info, datalen=blocksize=1200, dma_buf_count=3
I (2802) I2S: APLL: Req RATE: 48000, real rate: 47999.961, BITS: 16, CLKM: 1, BCK_M: 8, MCLK: 12287990.000, SCLK: 1535998.750000, diva: 1, divb: 0
I (2802) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (2812) MP3_DECODER: MP3 init
W (2812) I2S: I2S driver already installed
I (2812) LYRAT_V4_3: I2S0, MCLK output by GPIO0
I (2822) CONSOLE_EXAMPLE: Func:cli_setup_player, Line:657, MEM Total:4273924 Bytes, Inter:123648 Bytes, Dram:118756 Bytes

I (2832) CONSOLE_EXAMPLE: esp_audio instance is:0x3f806b38


I (40462) CONSOLE_EXAMPLE: Connecting Wi-Fi, SSID:"esp32" PASSWORD:"esp123456"
esp32> I (41682) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (42532) wifi:state: init -> auth (b0)
I (42552) wifi:state: auth -> assoc (0)
I (42562) wifi:state: assoc -> run (10)
I (42572) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (42572) wifi:security: WPA2-PSK, phy: bgn, rssi: -40
I (42572) wifi:pm start, type: 1

W (42592) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (42712) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (44452) esp_netif_handlers: sta ip: 192.168.5.187, mask: 255.255.255.0, gw: 192.168.5.1
I (44452) PERIPH_WIFI: Got ip:192.168.5.187

I (92022) PLAYLIST_SDCARD: ID   URL
I (92022) PLAYLIST_SDCARD: 0   file://sdcard/wechat.amr
I (92022) PLAYLIST_SDCARD: 1   file://sdcard/test.mp3
I (92022) PLAYLIST_SDCARD: 2   file://sdcard/DU1906_����_������Ƶ.mp3
I (92042) PLAYLIST_SDCARD: 3   file://sdcard/REC.WAV
I (92042) PLAYLIST_SDCARD: 4   file://sdcard/rec_out.wav
I (92052) PLAYLIST_SDCARD: 5   file://sdcard/rec_out.amr
I (92052) PLAYLIST_SDCARD: 6   file://sdcard/test.aac
I (92062) PLAYLIST_SDCARD: 7   file://sdcard/test_output.mp3
I (92062) PLAYLIST_SDCARD: 8   file://sdcard/1.Lyrat_v4.3_REC_ble_spi_sdcard.WAV
I (92082) PLAYLIST_SDCARD: 9   file://sdcard/start_service.mp3
I (92082) PLAYLIST_SDCARD: 10   file://sdcard/588E81FFFE34F6D92020-05-28-15-48-47.opus
I (92092) PLAYLIST_SDCARD: 11   file://sdcard/588E81FFFE34F6D92020-05-28-16-13-17.opus
I (92092) PLAYLIST_SDCARD: 12   file://sdcard/588E81FFFE34F6D92020-05-28-17-12-43.opus
I (92112) PLAYLIST_SDCARD: 13   file://sdcard/test.wav
I (92112) PLAYLIST_SDCARD: 14   file://sdcard/2.Lyrat_v4.3_REC_spi_sdcard.WAV
I (92122) PLAYLIST_SDCARD: 15   file://sdcard/7c9ebdcf12f0-19700101080024.wav
I (123222) CONSOLE_EXAMPLE: app_audio play
I (123232) CONSOLE_EXAMPLE: play index= 13, URI:file://sdcard/test.wav, byte_pos:0
I (123232) ESP_AUDIO_CTRL: Enter play procedure, src:0
I (123242) ESP_AUDIO_CTRL: Play procedure, URL is ok, src:0
I (123242) ESP_AUDIO_CTRL: Request_CMD_Queue CMD:0, Available:5, que:0x3ffe0ed4
I (123252) ESP_AUDIO_TASK: It's a decoder
I (123262) ESP_AUDIO_TASK: 1.CUR IN:[IN_file],CODEC:[DEC_wav],RESAMPLE:[48000],OUT:[OUT_iis],rate:0,ch:0,pos:0
I (123262) ESP_AUDIO_CTRL: Func:_ctrl_play, Line:771, MEM Total:4268264 Bytes, Inter:118056 Bytes, Dram:113164 Bytes
[0I (123272) ESP_AUDIO_TASK: 2.Handles,IN:0x3f806e9c,CODEC:0x3f807f14,FILTER:0x3f808d88,OUT:0x3f808704
m
I (123292) ESP_AUDIO_TASK: 2.2 Update all pipeline
I (123292) ESP_AUDIO_TASK: 2.3 Linked new pipeline
I (123302) AUDIO_PIPELINE: link el->rb, el:0x3f806e9c, tag:IN_file, rb:0x3f80909c
I (123312) AUDIO_PIPELINE: link el->rb, el:0x3f807f14, tag:DEC_wav, rb:0x3f80b8dc
I (123312) AUDIO_PIPELINE: link el->rb, el:0x3f808d88, tag:Audio_forge, rb:0x3f80d91c
I (123322) ESP_AUDIO_TASK: 3. Previous starting...
I (123332) AUDIO_ELEMENT: [IN_file-0x3f806e9c] Element task created
I (123342) AUDIO_ELEMENT: [IN_file] AEL_MSG_CMD_RESUME,state:1
I (123342) AUDIO_ELEMENT: [DEC_wav-0x3f807f14] Element task created
I (123352) AUDIO_ELEMENT: [DEC_wav] AEL_MSG_CMD_RESUME,state:1
I (123352) ESP_AUDIO_TASK: Blocking play until received AEL_MSG_CMD_REPORT_MUSIC_INFO
I (123362) FATFS_STREAM: File size: 17432920 byte, file position: 0
I (123372) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f806e9c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (123372) CODEC_ELEMENT_HELPER: The element is 0x3f807f14. The reserve data 2 is 0x0.
I (123392) WAV_DECODER: a new song playing
I (123392) ESP_AUDIO_TASK: Received music info then on play
I (123412) ESP_AUDIO_TASK: On event play, status:UNKNOWN, 0
I (123412) AUDIO_ELEMENT: [Audio_forge-0x3f808d88] Element task created
I (123422) AUDIO_ELEMENT: [Audio_forge] AEL_MSG_CMD_RESUME,state:1
I (123422) AUDIO_FORGE: audio_forge opened
I (123422) AUDIO_ELEMENT: [OUT_iis-0x3f808704] Element task created
I (123442) AUDIO_ELEMENT: [OUT_iis] AEL_MSG_CMD_RESUME,state:1
I (123442) I2S_STREAM: AUDIO_STREAM_WRITER
I (123442) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f807f14] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (123462) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f808d88] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (123472) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f808704] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_RUNNING
I (123492) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_RUNNING, 0, src:0, is_stopping:0
I (123492) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:938, MEM Total:4191004 Bytes, Inter:101240 Bytes, Dram:96348 Bytes

I (123502) ESP_AUDIO_CTRL: Exit play procedure, ret:0
esp32> I (123502) CONSOLE_EXAMPLE: esp_auido status:1,err:0


esp32> W (214072) FATFS_STREAM: No more data, ret:0
I (214072) AUDIO_ELEMENT: IN-[IN_file] AEL_IO_DONE,0
I (214072) ESP_AUDIO_TASK: Recv Element[IN_file-0x3f806e9c] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (214122) AUDIO_ELEMENT: IN-[DEC_wav] AEL_IO_DONE,-2
I (214132) DEC_WAV: Closed
I (214132) ESP_AUDIO_TASK: Received last pos: 17432920 bytes
I (214132) ESP_AUDIO_TASK: Recv Element[DEC_wav-0x3f807f14] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (214172) AUDIO_ELEMENT: IN-[Audio_forge] AEL_IO_DONE,-2
I (214172) AUDIO_FORGE: audio forge closed
I (214172) ESP_AUDIO_TASK: Recv Element[Audio_forge-0x3f808d88] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (214232) AUDIO_ELEMENT: IN-[OUT_iis] AEL_IO_DONE,-2
I (214262) ESP_AUDIO_TASK: Received last time: 90794 ms
I (214262) ESP_AUDIO_TASK: Recv Element[OUT_iis-0x3f808704] MSG,type:20000,cmd:8,len:4,status:AEL_STATUS_STATE_FINISHED
I (214282) ESP_AUDIO_TASK: ESP_AUDIO status is AEL_STATUS_STATE_FINISHED, 0, src:0, is_stopping:0
I (214292) ESP_AUDIO_TASK: Func:media_ctrl_task, Line:938, MEM Total:4219308 Bytes, Inter:101336 Bytes, Dram:96444 Bytes

W (214292) ESP_AUDIO_TASK: Destroy the old pipeline, FINISHED
W (214302) ESP_AUDIO_TASK: The old pipeline destroyed, FINISHED
I (214302) CONSOLE_EXAMPLE: esp_auido status:4,err:0

I (214322) ESP_AUDIO_CTRL: Total:17432920, get duration:90796 ms by bps:1536000
I (214322) CONSOLE_EXAMPLE: [ * ] End of time:90794 ms, duration:90796 ms
I (214332) CONSOLE_EXAMPLE: Func:esp_audio_state_task, Line:522, MEM Total:4239008 Bytes, Inter:116632 Bytes, Dram:111740 Bytes


esp32>

----------------------
Perpheral console HELP
----------------------
play    Play music, cmd:"play [index or url] [byte_pos]",
            e.g. 1."play"; 2. play with index after scan,"play index_number"; 3.play with specific url, "play url_path"
pause   Pause
resume  Resume
stop    Stop player
setvol  Set volume
getvol  Get volume
getpos  Get position by seconds
seek    Seek position by second
duration    Get music duration
tone    Insert tone to play
stone   Stop tone by a timer
setspeed    Set speed
getspeed    Get speed
join    Join WiFi AP as a station
wifi    Get connected AP information
led     Lyrat-MSC led bar pattern
scan    Scan sdcard music file, cmd: "scan [path]",e.g. "scan /sdcard"
list    Show scanned playlist
next    Next x file to play, cmd: "next [step]"
prev    Previous x file to play, cmd: "prev [step]"
mode    Set auto play mode, cmd:"mode [value]", 0:once; others: playlist loop all
reboot  Reboot system
free    Get system free memory
stat    Show processor time of all FreeRTOS tasks
tasks   Get information about running tasks
system  Get freertos all task states information
esp32>

```


## 故障排除

- 要运行 `stat` 命令，必须在 `menuconfig > Component Config > FreeRTOS > Enable FreeRTOS to collect run time stats` 中启用 `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`。
- 要运行 `tasklist` 命令，必须在 `menuconfig > Component Config > FreeRTOS > Enable FreeRTOS trace facility and Enable FreeRTOS stats formatting functions` 中启用 `CONFIG_FREERTOS_USE_TRACE_FACILITY`。
- 要运行 AAC 解码器，`CONFIG_FREERTOS_HZ` 应设置为 1000 Hz。
- 如果选择开发板 `ESP32-S2-kaluga-1`，则不支持 microSD 卡。


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
