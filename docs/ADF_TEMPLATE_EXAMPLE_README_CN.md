_这是 ESP-ADF 例程的 README.md 模版文件，其中斜体内容（两个下划线之间的内容，如 `_XXX_`）为提示文字，请根据例程的实际情况进行替换_


# _例程标题_

_英文版本链接_

- [English Version](./README.md)

_对例程的难易程度进行标记，分为：_

- 例程难度：![alt text](./_static/level_basic.png "初级") - 入门级例程
- 例程难度：![alt text](./_static/level_regular.png "中级")- 介绍 ESP-ADF 或音频开发板的功能
- 例程难度：![alt text](./_static/level_complex.png "高级") - 和中级例程类似，但相比而言需要更多的配置、与其他程序交互、设置云账户，需用户具备更多的专业知识


## 例程简介

- _从功能的角度介绍例子程序的实现。如：此例使用不同的解码器来播放 microSD 卡中不同格式的音乐_
- _从技术角度，描述主要演示的技术功能点是什么。如：演示管道和元素的使用，元素直接从 callback 获取数据_


### 资源列表

_可选项，如 RAM、CPU 负载_


### 预备知识

- _可选项_
- _引导新用户先使用 get started 例程_
- _引导客户学习背景知识_


### 文件结构

- _可选项_
- _介绍示例的文件夹结构和文件，如 `play_mp3` 文件夹_

```
├── components
│   └── my_board
│       ├── my_board_v1_0
│       │   ├── board.c
│       │   ├── board.h
│       │   ├── board_def.h
│       │   └── board_pins_config.c
│       ├── my_codec_driver
│       │   ├── new_codec.h
│       │   └── new_codec.c
│       ├── CMakeLists.txt
│       ├── component.mk
│       └── Kconfig.projbuild
├── CMakeLists.txt
├── example_test.py            Python script used for automated example testing
├── main
│   ├── CMakeLists.txt
│   ├── component.mk           Component make file
│   ├── adf_music.mp3          Test music file
│   └── play_mp3_example.c
├── Makefile                   Makefile used by legacy GNU Make
└── README.md                  This is the file you are currently reading
```


## 环境配置


### 硬件要求

_列举本例程需要的硬件资源，包括开发板、扬声器、microSD 卡、LCD 模块、摄像头模块、蓝牙音箱等_


### 其他要求

- _可选项_
- _音乐文件资源、SIP server、DLNA 应用软件等。如：如百度云的 profile 或亚马逊的 token 认证等_


## 编译和下载


### 默认 IDF 分支

_根据实际情况选择以下两种表述方式中的一种。_

本例程支持 IDF release/v[x.y] 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

本例程支持 IDF release/v[x.y] 及以后的分支，例程默认使用 IDF release/v[x.y] 分支。


### IDF 其他分支

- _可选项，需要选择特殊 IDF 版本分支，均要明确指出_
- _例如，DU1906 例程选择 IDF 分支 `audio/stack_on_psram_v3.3` 来编译，如：_

  ```bash
  cd $IDF_PATH
  git checkout master
  git pull
  git checkout audio/stack_on_psram_v3.3
  git submodule update --init --recursive
  ```


### 配置

- _介绍和设置重要的 menuconfig 项目，如：FatFs 长文件名、开发板、芯片类型、PSRAM 时钟、Wi-Fi/LWIP 参数等_

  ```
  Component config FAT Filesystem support Long filename support
  ```
  
- _其他需要的软件配置，如：指定的 patch_


### 编译和下载

_如何编译例程，命令是什么，如：_

- Legacy GNU Make 命令 `make`
- CMake 命令 `idf.py build`

_如何下载，命令是什么，如：_

- Legacy GNU Make 命令 `make flash monitor`
- CMake 命令 `idf.py -p PORT flash monitor`

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)，并在页面左上角选择芯片和版本，查看对应的文档。

## 如何使用例程


### 功能和用法

- _本节需说明例程如何使用、支持的功能以及可以得到的反馈结果。比如支持哪些按键，支持什么语音命令。下面为 DuerOS 的语音互动命令：_
  - _"小度小度"，"在呢"，"讲个笑话"_
  - _"小度小度"，"在呢"，"上海天气怎么样？"_
  - _"小度小度"，"在呢"，"播放一首歌"_
  - _"小度小度"，"在呢"，"百度百科乐鑫信息科技"_
- _如果存在一些其他的软件配合才能完成的功能，比如 servers、Bluetooth LE device、app、second chip 都需列举出来，若有链接请提供相应链接，并且解释如何进行设置。如：_
  - _首先运行 `pipeline_raw_http` 的 http server，`pipeline_raw_http` 中要求在 PC 上运行 `python server.py`，创建接收数据的服务器。_


### 日志输出

以下为本例程的完整日志。

```c
I (64) boot: Chip Revision: 3
I (35) boot: ESP-IDF v3.3.1-203-g0c1859a5a 2nd stage bootloader
I (35) boot: compile time 21:43:15
I (35) boot: Enabling RNG early entropy source...
I (41) qio_mode: Enabling default flash chip QIO
I (46) boot: SPI Speed      : 80MHz
I (50) boot: SPI Mode       : QIO
I (54) boot: SPI Flash Size : 8MB
I (58) boot: Partition Table:
I (62) boot: ## Label            Usage          Type ST Offset   Length
I (69) boot:  0 nvs              WiFi data        01 02 00009000 00004000
I (76) boot:  1 otadata          OTA data         01 00 0000d000 00002000
I (84) boot:  2 phy_init         RF data          01 01 0000f000 00001000
I (91) boot:  3 ota_0            OTA app          00 10 00010000 00280000
I (99) boot:  4 ota_1            OTA app          00 11 00290000 00280000
I (106) boot:  5 dsp_bin          Unknown data     01 24 00510000 00280000
I (114) boot:  6 profile          Unknown data     01 29 00790000 00001000
I (121) boot:  7 flash_tone       Unknown data     01 27 00791000 00060000
I (129) boot: End of partition table
```


### 参考文献

- _可选项_
- _运行过程、运行结果、视频链接等_


## 故障排除

_可选项，主要用来描述在使用过程中可能存在的问题，以及解决方法_


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。