# 选择不同解码器播放 HTTP 音频例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例可以选择播放从 HTTP 下载的 AAC、AMR、FLAC、MP3、OGG、OPUS 或 WAV 等格式的音频文件。

完整 HTTP 下载，解码器解码播放的管道如下：

```c
[Music_file_on_HTTP_server] ---> http_stream ---> music_decoder ---> i2s_stream ---> codec_chip ---> [Speaker]
                                                        ▲
                                                ┌───────┴────────┐
                                                │  AAC_DECODER   │
                                                │  AMR_DECODER   │
                                                │  FLAC_DECODER  │
                                                │  MP3_DECODER   │
                                                │  OGG_DECODER   │
                                                │  OPUS_DECODER  │
                                                │  WAV_DECODER   │
                                                └────────────────┘
```

http_stream 从网络上下载程序选定的音频文件，然后由选定的解码器解码，解码后的数据通过 I2S 传输给 codec 芯片播放。


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

```c
menuconfig > Example Configuration > (myssid) WiFi SSID > (myssid) WiFi Password
```

### 编译和下载

对于 `esp32-s3` 芯片编译，使用下面命令来选择 esp32-s3 的默认编译选项。

```c
cp sdkconfig.defaults.esp32s3 sdkconfig
```

对于 `esp32` 芯片编译，使用下面命令来选择 esp32 的默认编译选项。

```c
cp sdkconfig.defaults.esp32 sdkconfig
```

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.3/esp32/index.html)。


## 如何使用例程


### 功能和用法

本例默认的解码器选择是 AAC 格式：

```c
#define SELECT_AAC_DECODER 1
```

- 例程开始运行后，会先尝试连接 Wi-Fi 无线网络，如果连接成功后会播放 `play_http_select_decoder_example.c` 代码中预先设定的 HTTP URI 网络音频，打印如下：

```c
entry 0x40080710
I (29) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (29) boot: compile time 15:45:30
I (29) boot: chip revision: 3
I (32) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (39) qio_mode: Enabling default flash chip QIO
I (44) boot.esp32: SPI Speed      : 80MHz
I (49) boot.esp32: SPI Mode       : QIO
I (53) boot.esp32: SPI Flash Size : 4MB
I (58) boot: Enabling RNG early entropy source...
I (63) boot: Partition Table:
I (67) boot: ## Label            Usage          Type ST Offset   Length
I (74) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (82) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (89) boot:  2 factory          factory app      00 00 00010000 0016e360
I (97) boot: End of partition table
I (101) boot_comm: chip revision: 3, min. application chip revision: 0
I (108) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2a4e4 (173284) map
I (168) esp_image: segment 1: paddr=0x0003a50c vaddr=0x3ffb0000 size=0x0335c ( 13148) load
I (173) esp_image: segment 2: paddr=0x0003d870 vaddr=0x40080000 size=0x027a8 ( 10152) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (178) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xabac8 (703176) map
0x400d0020: _stext at ??:?

I (391) esp_image: segment 4: paddr=0x000ebaf0 vaddr=0x400827a8 size=0x14880 ( 84096) load
0x400827a8: _xt_user_exit at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:730

I (433) boot: Loaded app from partition at offset 0x10000
I (433) boot: Disabling RNG early entropy source...
I (433) cpu_start: Pro cpu up.
I (437) cpu_start: Application information:
I (442) cpu_start: Project name:     pipeline_http_select_decoder_ex
I (449) cpu_start: App version:      v2.2-201-g6fa02923
I (455) cpu_start: Compile time:     Sep 30 2021 15:45:28
I (461) cpu_start: ELF file SHA256:  a1c69de73ff06992...
I (467) cpu_start: ESP-IDF:          v4.2.2
I (472) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (482) heap_init: Initializing. RAM available for dynamic allocation:
I (489) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (495) heap_init: At 3FFB7A98 len 00028568 (161 KiB): DRAM
I (501) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (508) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (514) heap_init: At 40097028 len 00008FD8 (35 KiB): IRAM
I (520) cpu_start: Pro cpu start user code
I (537) spi_flash: detected chip: gd
I (538) spi_flash: flash io: qio
W (538) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (549) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (579) HTTP_SELECT_AAC_EXAMPLE: [ 1 ] Start audio codec chip
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.0] Create audio pipeline for playback
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.1] Create http stream to read data
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.2] Create aac decoder to decode aac file
I (1119) HTTP_SELECT_AAC_EXAMPLE: [2.3] Create i2s stream to write data to codec chip
I (1139) HTTP_SELECT_AAC_EXAMPLE: [2.4] Register all elements to audio pipeline
I (1139) HTTP_SELECT_AAC_EXAMPLE: [2.5] Link it together http_stream-->aac_decoder-->i2s_stream-->[codec_chip]
I (1149) HTTP_SELECT_AAC_EXAMPLE: [2.6] Set up  uri (http as http_stream, aac as aac_decoder, and default output is i2s)
I (1159) HTTP_SELECT_AAC_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (1169) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (3049) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (5079) HTTP_SELECT_AAC_EXAMPLE: [ 4 ] Set up  event listener
I (5079) HTTP_SELECT_AAC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (5079) HTTP_SELECT_AAC_EXAMPLE: [4.2] Listening event from peripherals
I (5089) HTTP_SELECT_AAC_EXAMPLE: [ 5 ] Start audio_pipeline
I (6079) HTTP_SELECT_AAC_EXAMPLE: [ * ] Receive music info from aac decoder, sample_rates=44100, bits=16, ch=2
W (191799) HTTP_STREAM: No more data,errno:0, total_bytes:2994446, rlen = 0
W (193299) HTTP_SELECT_AAC_EXAMPLE: [ * ] Stop event received
I (193299) HTTP_SELECT_AAC_EXAMPLE: [ 6 ] Stop audio_pipeline and release all resources
E (193299) AUDIO_ELEMENT: [http] Element already stopped
E (193309) AUDIO_ELEMENT: [aac] Element already stopped
E (193309) AUDIO_ELEMENT: [i2s] Element already stopped
W (193319) AUDIO_PIPELINE: There are no listener registered
W (193329) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193339) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193339) AUDIO_ELEMENT: [aac] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193359) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (193359) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3

```


### 日志输出

以下为本例程的完整日志。

```c
entry 0x40080710
I (29) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (29) boot: compile time 15:45:30
I (29) boot: chip revision: 3
I (32) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (39) qio_mode: Enabling default flash chip QIO
I (44) boot.esp32: SPI Speed      : 80MHz
I (49) boot.esp32: SPI Mode       : QIO
I (53) boot.esp32: SPI Flash Size : 4MB
I (58) boot: Enabling RNG early entropy source...
I (63) boot: Partition Table:
I (67) boot: ## Label            Usage          Type ST Offset   Length
I (74) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (82) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (89) boot:  2 factory          factory app      00 00 00010000 0016e360
I (97) boot: End of partition table
I (101) boot_comm: chip revision: 3, min. application chip revision: 0
I (108) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x2a4e4 (173284) map
I (168) esp_image: segment 1: paddr=0x0003a50c vaddr=0x3ffb0000 size=0x0335c ( 13148) load
I (173) esp_image: segment 2: paddr=0x0003d870 vaddr=0x40080000 size=0x027a8 ( 10152) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (178) esp_image: segment 3: paddr=0x00040020 vaddr=0x400d0020 size=0xabac8 (703176) map
0x400d0020: _stext at ??:?

I (391) esp_image: segment 4: paddr=0x000ebaf0 vaddr=0x400827a8 size=0x14880 ( 84096) load
0x400827a8: _xt_user_exit at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:730

I (433) boot: Loaded app from partition at offset 0x10000
I (433) boot: Disabling RNG early entropy source...
I (433) cpu_start: Pro cpu up.
I (437) cpu_start: Application information:
I (442) cpu_start: Project name:     pipeline_http_select_decoder_ex
I (449) cpu_start: App version:      v2.2-201-g6fa02923
I (455) cpu_start: Compile time:     Sep 30 2021 15:45:28
I (461) cpu_start: ELF file SHA256:  a1c69de73ff06992...
I (467) cpu_start: ESP-IDF:          v4.2.2
I (472) cpu_start: Starting app cpu, entry point is 0x400819d4
0x400819d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (482) heap_init: Initializing. RAM available for dynamic allocation:
I (489) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (495) heap_init: At 3FFB7A98 len 00028568 (161 KiB): DRAM
I (501) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (508) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (514) heap_init: At 40097028 len 00008FD8 (35 KiB): IRAM
I (520) cpu_start: Pro cpu start user code
I (537) spi_flash: detected chip: gd
I (538) spi_flash: flash io: qio
W (538) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (549) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (579) HTTP_SELECT_AAC_EXAMPLE: [ 1 ] Start audio codec chip
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.0] Create audio pipeline for playback
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.1] Create http stream to read data
I (1109) HTTP_SELECT_AAC_EXAMPLE: [2.2] Create aac decoder to decode aac file
I (1119) HTTP_SELECT_AAC_EXAMPLE: [2.3] Create i2s stream to write data to codec chip
I (1139) HTTP_SELECT_AAC_EXAMPLE: [2.4] Register all elements to audio pipeline
I (1139) HTTP_SELECT_AAC_EXAMPLE: [2.5] Link it together http_stream-->aac_decoder-->i2s_stream-->[codec_chip]
I (1149) HTTP_SELECT_AAC_EXAMPLE: [2.6] Set up  uri (http as http_stream, aac as aac_decoder, and default output is i2s)
I (1159) HTTP_SELECT_AAC_EXAMPLE: [ 3 ] Start and wait for Wi-Fi network
E (1169) gpio: gpio_install_isr_service(438): GPIO isr service already installed
W (3049) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:4
I (5079) HTTP_SELECT_AAC_EXAMPLE: [ 4 ] Set up  event listener
I (5079) HTTP_SELECT_AAC_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (5079) HTTP_SELECT_AAC_EXAMPLE: [4.2] Listening event from peripherals
I (5089) HTTP_SELECT_AAC_EXAMPLE: [ 5 ] Start audio_pipeline
I (6079) HTTP_SELECT_AAC_EXAMPLE: [ * ] Receive music info from aac decoder, sample_rates=44100, bits=16, ch=2
W (191799) HTTP_STREAM: No more data,errno:0, total_bytes:2994446, rlen = 0
W (193299) HTTP_SELECT_AAC_EXAMPLE: [ * ] Stop event received
I (193299) HTTP_SELECT_AAC_EXAMPLE: [ 6 ] Stop audio_pipeline and release all resources
E (193299) AUDIO_ELEMENT: [http] Element already stopped
E (193309) AUDIO_ELEMENT: [aac] Element already stopped
E (193309) AUDIO_ELEMENT: [i2s] Element already stopped
W (193319) AUDIO_PIPELINE: There are no listener registered
W (193329) AUDIO_ELEMENT: [http] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193339) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193339) AUDIO_ELEMENT: [aac] Element has not create when AUDIO_ELEMENT_TERMINATE
W (193359) PERIPH_WIFI: Wi-Fi disconnected from SSID esp32, auto-reconnect disabled, reconnect after 1000 ms
W (193359) PERIPH_WIFI: WiFi Event cb, Unhandle event_base:WIFI_EVENT, event_id:3
```

## 故障排除

如果你遇上开发板无法正常播放网络音频的问题，那么请检查下面的配置：
1. 开发板的 Wi-Fi 配置是否正确。
2. 开发板是否已经连接 Wi-Fi 并获取 IP 地址成功。
3. 服务器端的 HTTP URI 是否可以正常访问。 
4. 对于 HTTP 播放出现卡顿问题，有能力定位问题的用户，可参照 [Wi-Fi Menuconfig](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/wifi.html#psram) 和 [Lwip Performance Optimization](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/lwip.html#performance-optimization) 手动优化配置。
5. 一般建议使用默认 sdkconfig.default 配置。


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
