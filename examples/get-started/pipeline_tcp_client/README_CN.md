# 从 TCP 流中播放 MP3 文件例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程在 ADF 框架下实现在线 TCP 流的 MP3 音频播放演示。

本例程的管道如下图：

```
[tcp_server] ---> tcp_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v5.0 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程需要先配置 Wi-Fi 连接信息，通过运行 `menuconfig > Example Configuration` 填写 `Wi-Fi SSID` 和 `Wi-Fi Password`。

```
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

本例程需要配置 TCP 服务器的信息（IP 地址和端口号），通过运行 `menuconfig > Example Configuration` 填写 `TCP URL` 和 `TCP PORT`。

```
menuconfig > Example Configuration > (192.168.5.72) TCP URL > (8080) TCP PORT

```

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.3/esp32/index.html)。


## 如何使用例程

### 功能和用法

- 例程需要先运行 TCP 服务器的 Python 脚本，需要 python 3.x，并且开发板和 TCP 服务器连接在同一个 Wi-Fi 网络中，还需要准备一首 MP3 音乐命名为 `esp32.mp3` 拷贝到 TCP 服务器脚本同一文件夹下面，Python 脚本运行日志如下：

```
python tcp_server.py
Get the esp32.mp3 size is 1453677
starting listen on ip 192.168.5.72, port 8080
waiting for client to connect

```

- 例程开始运行后，将主动连接 Wi-Fi 热点，如连接成功则去连接 TCP 服务器，获取 MP3 音频进行播放，打印如下：

```c
I (0) cpu_start: App cpu up.
I (525) heap_init: Initializing. RAM available for dynamic allocation:
I (532) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (538) heap_init: At 3FFB7AA8 len 00028558 (161 KiB): DRAM
I (544) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (551) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (557) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (563) cpu_start: Pro cpu start user code
I (582) spi_flash: detected chip: gd
I (582) spi_flash: flash io: dio
W (583) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (592) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (639) TCP_CLIENT_EXAMPLE: [ 1 ] Start codec chip
I (659) TCP_CLIENT_EXAMPLE: [2.0] Create audio pipeline for playback
I (659) TCP_CLIENT_EXAMPLE: [2.1] Create i2s stream to write data to codec chip
I (689) TCP_CLIENT_EXAMPLE: [2.2] Create mp3 decoder to decode mp3 file
I (689) TCP_CLIENT_EXAMPLE: [2.2] Create tcp client stream to read data
I (689) TCP_CLIENT_EXAMPLE: [2.3] Register all elements to audio pipeline
I (699) TCP_CLIENT_EXAMPLE: [2.4] Link it together tcp-->mp3-->i2s
I (709) TCP_CLIENT_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (709) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (2879) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4639) TCP_CLIENT_EXAMPLE: [ 4 ] Set up  event listener
I (4639) TCP_CLIENT_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (4639) TCP_CLIENT_EXAMPLE: [4.2] Listening event from peripherals
I (4649) TCP_CLIENT_EXAMPLE: [ 5 ] Start audio_pipeline
I (4699) TCP_CLIENT_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=48000, bits=16, ch=2
E (94929) AUDIO_ELEMENT: IN-[tcp] AEL_STATUS_ERROR_INPUT
E (94929) AUDIO_ELEMENT: [tcp] ERROR_PROCESS, AEL_IO_FAIL
W (94929) AUDIO_ELEMENT: [tcp] audio_element_on_cmd_error,3

```


### 日志输出

- 以下为本例程的完整日志。

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
I (27) boot: compile time 11:20:38
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 4MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 phy_init         RF data          01 01 0000d000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00300000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x23954 (145748) map
I (167) esp_image: segment 1: paddr=0x0003397c vaddr=0x3ffb0000 size=0x0336c ( 13164) load
I (172) esp_image: segment 2: paddr=0x00036cf0 vaddr=0x40080000 size=0x09328 ( 37672) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (190) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0x9eee8 (650984) map
0x400d0020: _stext at ??:?

I (438) esp_image: segment 4: paddr=0x000def10 vaddr=0x40089328 size=0x0e1b4 ( 57780) load
0x40089328: xPortStartScheduler at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/port.c:317

I (476) boot: Loaded app from partition at offset 0x10000
I (476) boot: Disabling RNG early entropy source...
I (477) cpu_start: Pro cpu up.
I (480) cpu_start: Application information:
I (485) cpu_start: Project name:     tcp_client_example
I (491) cpu_start: App version:      v2.2-252-g0cc4fd6d
I (497) cpu_start: Compile time:     Nov 26 2021 15:26:12
I (503) cpu_start: ELF file SHA256:  f0bc8f5381316fac...
I (509) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (515) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (525) heap_init: Initializing. RAM available for dynamic allocation:
I (532) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (538) heap_init: At 3FFB7AA8 len 00028558 (161 KiB): DRAM
I (544) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (551) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (557) heap_init: At 400974DC len 00008B24 (34 KiB): IRAM
I (563) cpu_start: Pro cpu start user code
I (582) spi_flash: detected chip: gd
I (582) spi_flash: flash io: dio
W (583) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (592) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (639) TCP_CLIENT_EXAMPLE: [ 1 ] Start codec chip
I (659) TCP_CLIENT_EXAMPLE: [2.0] Create audio pipeline for playback
I (659) TCP_CLIENT_EXAMPLE: [2.1] Create i2s stream to write data to codec chip
I (689) TCP_CLIENT_EXAMPLE: [2.2] Create mp3 decoder to decode mp3 file
I (689) TCP_CLIENT_EXAMPLE: [2.2] Create tcp client stream to read data
I (689) TCP_CLIENT_EXAMPLE: [2.3] Register all elements to audio pipeline
I (699) TCP_CLIENT_EXAMPLE: [2.4] Link it together tcp-->mp3-->i2s
I (709) TCP_CLIENT_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (709) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (2879) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (4639) TCP_CLIENT_EXAMPLE: [ 4 ] Set up  event listener
I (4639) TCP_CLIENT_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (4639) TCP_CLIENT_EXAMPLE: [4.2] Listening event from peripherals
I (4649) TCP_CLIENT_EXAMPLE: [ 5 ] Start audio_pipeline
I (4699) TCP_CLIENT_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=48000, bits=16, ch=2
E (94929) AUDIO_ELEMENT: IN-[tcp] AEL_STATUS_ERROR_INPUT
E (94929) AUDIO_ELEMENT: [tcp] ERROR_PROCESS, AEL_IO_FAIL
W (94929) AUDIO_ELEMENT: [tcp] audio_element_on_cmd_error,3
```

- TCP 服务器端的日志如下：

```c
python2 tcp_server.py
Get the esp32.mp3 size is 1453677
starting listen on ip 192.168.5.72, port 8080
waiting for client to connect
total size  [1024/1453677]
total size  [2048/1453677]
total size  [3072/1453677]
total size  [4096/1453677]
total size  [5120/1453677]
total size  [6144/1453677]
total size  [7168/1453677]
total size  [8192/1453677]
total size  [9216/1453677]

...

total size  [1445888/1453677]
total size  [1446912/1453677]
total size  [1447936/1453677]
total size  [1448960/1453677]
total size  [1449984/1453677]
total size  [1451008/1453677]
total size  [1452032/1453677]
total size  [1453056/1453677]
total size  [1453677/1453677]
total size  [1453677/1453677]
get all data for esp32.mp3
close client connect

```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
