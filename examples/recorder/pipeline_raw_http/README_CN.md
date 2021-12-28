# 录制 WAV 文件并上传到 HTTP 服务器例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了使用 HTTP 流上传语音数据到 Python 创建的 HTTP 服务器并保存为 WAV 文件的过程。

开发板和 HTTP 服务器正确运行后，当按下音频板上的 [Rec] 按键时，将录制语音数据并通过 HTTP 流上传数据到 HTTP 服务器，当释放 [Rec] 按键时，停止录音。同时 PC 端 HTTP 服务器将收到的数据写入以接收时间为名称的 WAV 文件中。

本例程的完整的管道如下：

```c
mic ---> codec_chip ---> i2s_stream ---> http_stream >>>> [Wi-Fi] >>>> http_server ---> wav_file
```


## 环境配置


### 硬件要求

本例程可在标有绿色复选框的开发板上运行。请记住，如下面的 *配置* 一节所述，可以在 `menuconfig` 中选择开发板。

| 开发板名称 | 开始入门 | 芯片 | 兼容性 |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |


## 编译和下载


### IDF 默认分支

本例程默认 IDF 为 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```c
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

本例程需要先配置 Wi-Fi 连接信息，通过运行 `menuconfig > Example Configuration` 填写 `Wi-Fi SSID` 和 `Wi-Fi Password`。

```c
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

本例程还需要配置 HTTP 服务器（个人 PC）的 URI，请确保开发板和 HTTP 服务器处在同一个 Wi-Fi 局域网中，如 HTTP 服务器的 LAN IP 地址是 `192.168.5.72` 那么在 `menuconfig > Example Configuration` 配置为 `http://192.168.5.72:8000/upload`。

```c
menuconfig > Example Configuration > (http://192.168.5.72:8000/upload) Server URL to send data
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

- 例程需要先运行 Python 脚本，需要 python 2.7，并且开发板和 HTTP 服务器连接在同一个 Wi-Fi 网络中。
- 文件 server.py 在此示例的根目录下，并确保此目录可写，运行 `python server.py`，Python 脚本运行 log 如下：

```c
python2 server.py
Serving HTTP on 192.168.5.72 port 8000
```

- 例程开始运行后，会先连接 Wi-Fi，Wi-Fi 连接成功后，按下 [Rec] 键录音，这时录制的声音会被上传到 HTTP 服务器的目录下，文件命名内容为当天的时间日期、音频采样率和通道数，并用下划线隔开，如` 20210918T070420Z_16000_16_2.wav`。

- 开发板的日志如下：

```c
I (0) cpu_start: App cpu up.
I (526) heap_init: Initializing. RAM available for dynamic allocation:
I (533) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (539) heap_init: At 3FFB9AD8 len 00026528 (153 KiB): DRAM
I (545) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (552) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (558) heap_init: At 40095C68 len 0000A398 (40 KiB): IRAM
I (564) cpu_start: Pro cpu start user code
I (247) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (277) REC_RAW_HTTP: [ 1 ] Initialize Button Peripheral & Connect to wifi network
I (297) wifi:wifi driver task: 3ffc2c98, prio:23, stack:3584, core=0
I (347) wifi:wifi firmware version: 5f8804c
I (347) wifi:config NVS flash: enabled
I (347) wifi:config nano formating: disabled
I (347) wifi:Init dynamic tx buffer num: 32
I (347) wifi:Init data frame dynamic rx buffer num: 32
I (357) wifi:Init management frame dynamic rx buffer num: 32
I (357) wifi:Init management short buffer num: 32
I (367) wifi:Init static rx buffer size: 1600
I (367) wifi:Init static rx buffer num: 10
I (367) wifi:Init dynamic rx buffer num: 32
W (377) phy_init: failed to load RF calibration data (0xffffffff), falling back to full calibration
I (547) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 2
I (567) wifi:mode : sta (94:b9:7e:65:c2:44)
I (1907) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2887) wifi:state: init -> auth (b0)
I (2897) wifi:state: auth -> assoc (0)
I (2907) wifi:state: assoc -> run (10)
I (2917) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (2917) wifi:security type: 4, phy: bgn, rssi: -32
I (2917) wifi:pm start, type: 1

I (2977) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4777) REC_RAW_HTTP: [ 2 ] Start codec chip
E (4777) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (4797) REC_RAW_HTTP: [3.0] Create audio pipeline for recording
I (4797) REC_RAW_HTTP: [3.1] Create http stream to post data to server
I (4807) REC_RAW_HTTP: [3.2] Create i2s stream to read audio data from codec chip
I (4817) REC_RAW_HTTP: [3.3] Register all elements to audio pipeline
I (4817) REC_RAW_HTTP: [3.4] Link it together [codec_chip]-->i2s_stream->http_stream-->[http_server]
W (4847) PERIPH_TOUCH: _touch_init
I (4847) REC_RAW_HTTP: [ 4 ] Press [Rec] button to record, Press [Mode] to exit
I (11447) REC_RAW_HTTP: [ * ] [Rec] input key event, resuming pipeline ...
Total bytes written: 141312
I (14937) REC_RAW_HTTP: [ * ] [Rec] key released, stop pipeline ...
W (14937) AUDIO_ELEMENT: IN-[http] AEL_IO_ABORT
W (14937) HTTP_STREAM: No output due to stopping
I (14937) REC_RAW_HTTP: [ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker
E (14957) TRANS_TCP: tcp_poll_read select error 104, errno = Connection reset by peer, fd = 54
I (14957) REC_RAW_HTTP: [ + ] HTTP client HTTP_STREAM_FINISH_REQUEST
W (14967) AUDIO_PIPELINE: Without stop, st:1
W (14967) AUDIO_PIPELINE: Without wait stop, st:1
```

- HTTP 服务器的日志如下：

```c
python2 server.py
Serving HTTP on 192.168.5.72 port 8000
Audio information, sample rates: 16000, bits: 16, channel(s): 2
Total bytes received: 141312
192.168.5.187 - - [18/Sep/2021 15:04:20] "POST /upload HTTP/1.1" 200 -
```

- 最后按 [Mode] 键，退出例程。

```c
W (197635) REC_RAW_HTTP: [ * ] [Set] input key event, exit the demo ...
I (197635) REC_RAW_HTTP: [ 5 ] Stop audio_pipeline
W (197635) AUDIO_PIPELINE: Without stop, st:1
W (197635) AUDIO_PIPELINE: Without wait stop, st:1
W (197645) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197655) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197655) AUDIO_PIPELINE: There are no listener registered
W (197675) AUDIO_PIPELINE: There are no listener registered
W (197675) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197685) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
I (197695) wifi:state: run -> init (0)
I (197695) wifi:pm stop, total sleep time: 186442255 us / 194856817 us

I (197695) wifi:new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (197705) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
I (197715) wifi:flush txq
I (197715) wifi:stop sw txq
I (197725) wifi:lmac stop hw txq
I (197725) wifi:Deinit lldesc rx mblock:10
```


### 日志输出

以下为本例程的完整日志。

```c
I (0) cpu_start: App cpu up.
I (526) heap_init: Initializing. RAM available for dynamic allocation:
I (533) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (539) heap_init: At 3FFB9AD8 len 00026528 (153 KiB): DRAM
I (545) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (552) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (558) heap_init: At 40095C68 len 0000A398 (40 KiB): IRAM
I (564) cpu_start: Pro cpu start user code
I (247) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (277) REC_RAW_HTTP: [ 1 ] Initialize Button Peripheral & Connect to wifi network
I (297) wifi:wifi driver task: 3ffc2c98, prio:23, stack:3584, core=0
I (347) wifi:wifi firmware version: 5f8804c
I (347) wifi:config NVS flash: enabled
I (347) wifi:config nano formating: disabled
I (347) wifi:Init dynamic tx buffer num: 32
I (347) wifi:Init data frame dynamic rx buffer num: 32
I (357) wifi:Init management frame dynamic rx buffer num: 32
I (357) wifi:Init management short buffer num: 32
I (367) wifi:Init static rx buffer size: 1600
I (367) wifi:Init static rx buffer num: 10
I (367) wifi:Init dynamic rx buffer num: 32
W (377) phy_init: failed to load RF calibration data (0xffffffff), falling back to full calibration
I (547) phy: phy_version: 4180, cb3948e, Sep 12 2019, 16:39:13, 0, 2
I (567) wifi:mode : sta (94:b9:7e:65:c2:44)
I (1907) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1
I (2887) wifi:state: init -> auth (b0)
I (2897) wifi:state: auth -> assoc (0)
I (2907) wifi:state: assoc -> run (10)
I (2917) wifi:connected with esp32, aid = 2, channel 11, BW20, bssid = fc:ec:da:b7:11:c7
I (2917) wifi:security type: 4, phy: bgn, rssi: -32
I (2917) wifi:pm start, type: 1

I (2977) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (4777) REC_RAW_HTTP: [ 2 ] Start codec chip
E (4777) gpio: gpio_install_isr_service(412): GPIO isr service already installed
I (4797) REC_RAW_HTTP: [3.0] Create audio pipeline for recording
I (4797) REC_RAW_HTTP: [3.1] Create http stream to post data to server
I (4807) REC_RAW_HTTP: [3.2] Create i2s stream to read audio data from codec chip
I (4817) REC_RAW_HTTP: [3.3] Register all elements to audio pipeline
I (4817) REC_RAW_HTTP: [3.4] Link it together [codec_chip]-->i2s_stream->http_stream-->[http_server]
W (4847) PERIPH_TOUCH: _touch_init
I (4847) REC_RAW_HTTP: [ 4 ] Press [Rec] button to record, Press [Mode] to exit
I (11447) REC_RAW_HTTP: [ * ] [Rec] input key event, resuming pipeline ...
Total bytes written: 141312
I (14937) REC_RAW_HTTP: [ * ] [Rec] key released, stop pipeline ...
W (14937) AUDIO_ELEMENT: IN-[http] AEL_IO_ABORT
W (14937) HTTP_STREAM: No output due to stopping
I (14937) REC_RAW_HTTP: [ + ] HTTP client HTTP_STREAM_POST_REQUEST, write end chunked marker
E (14957) TRANS_TCP: tcp_poll_read select error 104, errno = Connection reset by peer, fd = 54
I (14957) REC_RAW_HTTP: [ + ] HTTP client HTTP_STREAM_FINISH_REQUEST
W (14967) AUDIO_PIPELINE: Without stop, st:1
W (14967) AUDIO_PIPELINE: Without wait stop, st:1
W (197635) REC_RAW_HTTP: [ * ] [Set] input key event, exit the demo ...
I (197635) REC_RAW_HTTP: [ 5 ] Stop audio_pipeline
W (197635) AUDIO_PIPELINE: Without stop, st:1
W (197635) AUDIO_PIPELINE: Without wait stop, st:1
W (197645) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197655) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197655) AUDIO_PIPELINE: There are no listener registered
W (197675) AUDIO_PIPELINE: There are no listener registered
W (197675) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (197685) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
I (197695) wifi:state: run -> init (0)
I (197695) wifi:pm stop, total sleep time: 186442255 us / 194856817 us

I (197695) wifi:new:<11,0>, old:<11,0>, ap:<255,255>, sta:<11,0>, prof:1
W (197705) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
I (197715) wifi:flush txq
I (197715) wifi:stop sw txq
I (197725) wifi:lmac stop hw txq
I (197725) wifi:Deinit lldesc rx mblock:10
```

HTTP 服务器端的完整日志如下：

```c
python2 server.py
Serving HTTP on 192.168.5.72 port 8000
Audio information, sample rates: 16000, bits: 16, channel(s): 2
Total bytes received: 141312
192.168.5.187 - - [18/Sep/2021 15:04:20] "POST /upload HTTP/1.1" 200 -
```


## 故障排除

如果开发板无法上传语音到 HTTP 服务器，那么请检查下面的配置：
1. 开发板的 Wi-Fi 配置是否正确。
2. 开发板是否已经连接 Wi-Fi 并成功获取 IP 地址。
3. 服务器端的 HTTP 脚本 URI 是否已经正确配置。
4. 服务器端的 HTTP 脚本是否已经正确运行。
5. 开发板和 HTTP 服务器是否在同一个 Wi-Fi 网络下。


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。