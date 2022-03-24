# 从 嵌入 Flash 二进制文件中播放 MP3 文件例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了 ADF 使用音频管道 API 播放存储在 flash 嵌入二进制数据中的 MP3 文件，一般可在项目中作为系统提示音使用。

开发板获取 flash 中存储的 MP3 文件并解码播放的管道如下：

```
[flash] ---> embed_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

### 预备知识

本例程在 `${PROJECT_DIR}/main/` 目录下已经帮助用户生成了例程所需音频文件在 flash 中地址的源代码文件 `audio_embed_tone.h` 和 `embed_tone.cmake` 文件 并更新 `CMakeLists.txt` 和 `conponents.mk`。

如果用户需要生成自己的嵌入二进制文件，则需把用户提示音文件放到 `$PROJECT_DIR/main`文件夹下，然后按照下面命令运行 `mk_embed_audio_bin.py` 脚本。更多的嵌入二进制，请参看[相关章节](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/build-system.html#cmake-embed-data) 

Python 脚本命令如下：

```
python $ADF_PATH/tools/audio_tone/mk_embed_audio_tone.py -p $(ADF_PATH)/examples/player/pipeline_embed_flash_tone/main
```

## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V1.1`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程

### 功能和用法

- 例程开始运行后，将主动连接 Wi-Fi 热点，如连接成功，则获取 HTTP 服务器的在线 MP3 音频进行播放，打印如下：

```c

entry 0x40080694
I (27) boot: ESP-IDF v4.4-329-g3c9657fa4e-dirty 2nd stage bootloader
I (27) boot: compile time 21:51:10
I (27) boot: chip revision: 3
I (32) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (39) boot.esp32: SPI Speed      : 40MHz
I (44) boot.esp32: SPI Mode       : DIO
I (48) boot.esp32: SPI Flash Size : 2MB
I (53) boot: Enabling RNG early entropy source...
I (58) boot: Partition Table:
I (62) boot: ## Label            Usage          Type ST Offset   Length
I (69) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (84) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (103) esp_image: segment 0: paddr=00010020 vaddr=3f400020 size=1ee34h (126516) map
I (157) esp_image: segment 1: paddr=0002ee5c vaddr=3ffb0000 size=011bch (  4540) load
I (159) esp_image: segment 2: paddr=00030020 vaddr=400d0020 size=28008h (163848) map
I (222) esp_image: segment 3: paddr=00058030 vaddr=3ffb11bc size=01244h (  4676) load
I (224) esp_image: segment 4: paddr=0005927c vaddr=40080000 size=0de34h ( 56884) load
I (251) esp_image: segment 5: paddr=000670b8 vaddr=50000000 size=00010h (    16) load
I (258) boot: Loaded app from partition at offset 0x10000
I (258) boot: Disabling RNG early entropy source...
I (271) cpu_start: Pro cpu up.
I (272) cpu_start: Starting app cpu, entry point is 0x400811f4
0x400811f4: call_start_cpu1 at /home/xutao/workspace/esp-adf-internal/esp-idf/components/esp_system/port/cpu_start.c:160

I (0) cpu_start: App cpu up.
I (286) cpu_start: Pro cpu start user code
I (286) cpu_start: cpu freq: 160000000
I (286) cpu_start: Application information:
I (290) cpu_start: Project name:     play_embed_tone
I (296) cpu_start: App version:      v2.4-1-g22217d90-dirty
I (302) cpu_start: Compile time:     May 26 2022 21:51:03
I (308) cpu_start: ELF file SHA256:  d98530bff404d3d7...
I (314) cpu_start: ESP-IDF:          v4.4-329-g3c9657fa4e-dirty
I (321) heap_init: Initializing. RAM available for dynamic allocation:
I (328) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (334) heap_init: At 3FFB2E50 len 0002D1B0 (180 KiB): DRAM
I (340) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (347) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (353) heap_init: At 4008DE34 len 000121CC (72 KiB): IRAM
I (361) spi_flash: detected chip: gd
I (364) spi_flash: flash io: dio
W (368) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (381) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (391) PLAY_EMBED_FLASH_MP3: [ 1 ] Start audio codec chip
I (421) PLAY_EMBED_FLASH_MP3: [ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event
E (421) gpio: gpio_install_isr_service(449): GPIO isr service already installed
I (431) PLAY_EMBED_FLASH_MP3: [2.1] Create mp3 decoder to decode mp3 file and set custom read callback
I (441) PLAY_EMBED_FLASH_MP3: [2.2] Create i2s stream to write data to codec chip
I (451) PLAY_EMBED_FLASH_MP3: [2.3] Register all elements to audio pipeline
I (461) PLAY_EMBED_FLASH_MP3: [2.4] Link it together [embed flash]-->mp3_decoder-->i2s_stream-->[codec_chip]
I (471) PLAY_EMBED_FLASH_MP3: [ 3 ] Set up  event listener
I (471) PLAY_EMBED_FLASH_MP3: [3.1] Listening event from all elements of pipeline
I (481) PLAY_EMBED_FLASH_MP3: [3.2] Listening event from peripherals
I (491) PLAY_EMBED_FLASH_MP3: [ 4 ] Start audio_pipeline
I (521) PLAY_EMBED_FLASH_MP3: [ * ] Receive music info from mp3 decoder, sample_rates=16000, bits=16, ch=1
W (2481) EMBED_FLASH_STREAM: No more data,ret:0 ,info.byte_pos:22284
W (3361) PLAY_EMBED_FLASH_MP3: [ * ] Stop event received
I (3361) PLAY_EMBED_FLASH_MP3: [ 5 ] Stop audio_pipeline
E (3361) AUDIO_ELEMENT: [flash] Element already stopped
E (3361) AUDIO_ELEMENT: [mp3] Element already stopped
E (3371) AUDIO_ELEMENT: [i2s] Element already stopped
W (3371) AUDIO_PIPELINE: There are no listener registered
W (3381) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3391) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE
W (3401) AUDIO_ELEMENT: [flash] Element has not create when AUDIO_ELEMENT_TERMINATE

```

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
