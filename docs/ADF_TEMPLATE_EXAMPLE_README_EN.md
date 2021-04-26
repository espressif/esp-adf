_Note that this is a template for an ESP-ADF example README.md file. When using this template, replace all these emphasised placeholders with example-specific content._

# _Example Title_
_English Version or Chinese Version_
- [Chinese Version](./README_CN.md)

_Add a picture to describe the difficulty level_
- _Basic Example: ![alt text](./_static/level_basic.png "Basic Example") - to get started_
- _Regular Example: ![alt text](./_static/level_regular.png "Regular Example") - demonstrates functionality of ESP-ADF or audio board_
- _Complex Example: ![alt text](./_static/level_complex.png "Complex Example") - like regular example but requires more configuration steps, interaction with other programs, setting up some cloud accounts, or more user expertise_

## Example Brief
- _Introduce the realization of the example program from a functional point of view_
- _From a technical point of view, describe the main technical functions demonstrated. For example, to demonstrate the use of pipeline and element, element obtains data directly from callback_

### Resources
_It's optional, such as RAM, CPU loading_

### Prerequisites
- _It's optional_
- _Guide first-time users to run get started example first_
- _Guide users to learn the background_

### Folder contents
- _It's optional_
- _It is short explanation of remaining files in the project folder, e.g: play_mp3 folder_

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

## Environment Setup

### Hardware Required
_List all the hardware, Such as Boards, speakers, SD card, LCD module, Camera module, Bluetooth speaker, etc._

### Additional Requirements
- _It's optional_
- _Such as Baidu Cloud's profile or Amazon's token authentication, tone bins, music files, etc._
- _Such as SIP server, DLNA APP, etc._

## Example Set Up

### Default IDF Branch
_The default IDF branch is ADF's built-in branch `$ADF_PATH/esp-idf`_

### Other special IDF branches
- _When required to select a special IDF/ADF version branch, which must be clearly pointed out and described_
- _For example, DU1906 project select the IDF branch `audio/stack_on_psram_v3.3` to compile_
```c
cd $IDF_PATH
git checkout master
git pull
git checkout audio/stack_on_psram_v3.3
git submodule update --init --recursive
```

### Configuration
- _Such as select compatible audio board in ` menuconfig > Audio HAL`_
- _Such as other important tips, such as turning on long file name support for fatfs_
```c
Component config > FAT Filesystem support > Long filename support
```

### Build and Flash
_Build the project and flash it to the board, then run monitor tool to view serial output:_
```c
idf.py build -p PORT flash monitor
```

## How to use the Example

### Example Functionality
- _To explain how to use this example, what features are supported and what results would be get_
- _Such as description button functionality_
- _If any other items (server, BLE device, app, second chip, whatever) are needed, mention them here. Include links if applicable. Explain how to set them up_

For example, the DU1906 voice interaction command
    * "小度小度" "在呢" "讲个笑话"
    * "小度小度" "在呢" "上海天气怎么样？"
    * "小度小度" "在呢" "播放一首歌"
    * "小度小度" "在呢" "百度百科乐鑫信息科技"

e.g.
`pipeline_raw_http`, run `python server.py` - place the file `server.py` at root of this example, and make sure this directory is writable

### Example Logs
- _Select the booting log, e.g._
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

### References
_Running results, text description, or video links, etc. (optional)._
e.g. [ESP32-S3 Offline Speech Recognition](https://www.bilibili.com/video/BV1Cv411e7g8)

## Troubleshooting
_It's optional. If there are any likely problems or errors which many users might encounter, mention them here. Remove this section for very simple examples where nothing is likely to go wrong._

## Technical support and feedback

Please use the following feedback channels:

* For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
* For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.