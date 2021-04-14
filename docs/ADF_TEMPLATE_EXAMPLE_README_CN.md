_这是一个 ESP-ADF 例子程序的 README.md 模版文件，替换里面斜体内容和举例可以使用该模版_

# _例程标题_
_中英文切换的链接，默认英文版本_
- [English Version](./README.md)

_对 example 的难易程度进行标记，分为：_
- 基础 ![alt text](./_static/level_basic.png "基础")
- 普通 ![alt text](./_static/level_regular.png "普通")
- 复杂 ![alt text](./_static/level_complex.png "复杂")

## 例程简介
- _从功能的角度介绍例子程序的实现_
- _比如，此例是使用不同解码器来播放 sdcard 中不同格式的音乐_

- _从技术角度，描述主要演示的技术功能点是什么_
- 比如，演示 pipeline 和 element 的使用，element 直接从 callback 获取数据

### 资源列表
_可选项，RAM，CPU 负载_

### 预备知识
- _可选项_
- _引导新用户先使用 get started 例程_
- _引导客户学习背景知识_

### 文件结构
- _可选项_
- _介绍示例的文件夹结构和文件，如下：play_mp3_
```c
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
- _列举本例程需要的硬件资源，包括开发板、喇叭、SDCARD、LCD 模块、摄像头模块、蓝牙音箱等_

### 其他要求
- _可选项_
- _音乐文件资源，SIP server、 DLNA APP 等。如： 如百度云的 profile 或亚马逊的 token 认证等_

## 编译和下载

### IDF 默认分支
_默认 IDF 为 ADF 的內建分支 `$ADF_PATH/esp-idf`_

### IDF 其他分支
- _可选项，需要选择特殊 IDF 版本分支，均要明确指出_
- _例如，DU1906 例程选择 IDF 分支 `audio/stack_on_psram_v3.3` 来编译， 如：_
```c
cd $IDF_PATH
git checkout master
git pull
git checkout audio/stack_on_psram_v3.3
git submodule update --init --recursive
```

### 配置
- _介绍和设置重要的 menuconfig 项目，如：FATFS长文件名，开发板，芯片类型，PSRAM时钟，Wi-Fi/LWIP参数等_
```c
Component config FAT Filesystem support Long filename support
```

- _其他需要的软件配置，如：指定的 patch_

### 编译和下载
_怎么编译 example，命令是什么，如：_
- Legacy GNU Make 命令 `make`
- cmake 命令 `idf.py build`


_怎么下载，命令是什么，如：_
- Legacy GNU Make 命令 `make flash monitor`
- cmake 命令 `idf.py -p PORT flash monitor`

## 如何使用例程

### 功能和用法
- _这部分要说明例程是如何使用，支持的功能，使用方法，以及可以得到的反馈结果_
- _比如支持哪些按键，支持什么语音命令。如下 dueros 的语音互动命令：_
- "小度小度"，"在呢"，"讲个笑话"
- "小度小度"，"在呢"，"上海天气怎么样？"
- "小度小度"，"在呢"，"播放一首歌"
- "小度小度"，"在呢"，"百度百科乐鑫信息科技"

- _如果存在一些其他的软件配合才能完成的功能，比如 (servers，BLE device，app，second chip，links) 等，都要列举出来，如首先运行 pipeline_raw_http 的 http server。如：`pipeline_raw_http` 中要求在 PC 上运行 `python server.py`，创建接收数据的服务器_

### 日志输出
_选取启动到初始化完成的 log， 示例如下：_
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
_可选项，运行的结果，运行过程的链接等_

## Troubleshooting
_可选项，主要用来描述在使用过程中可能存在的问题，以及解决方法_

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。