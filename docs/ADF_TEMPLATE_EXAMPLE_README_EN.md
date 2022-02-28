_Note that this is a template for an ESP-ADF example README.md file. When using this template, replace all these emphasised placeholders with example-specific content._

# _Example Title_


_Link to Chinese Version_

- [Chinese Version](./README_CN.md)

_Select one of the following to describe the difficulty level._

- Basic Example: ![alt text](./_static/level_basic.png "Basic Example") - to get started
- Regular Example: ![alt text](./_static/level_regular.png "Regular Example") - demonstrates functionality of ESP-ADF or audio board
- Complex Example: ![alt text](./_static/level_complex.png "Complex Example") - like regular example but requires more configuration steps, interaction with other programs, setting up some cloud accounts, or more user expertise


## Example Brief

- _Introduce the realization of the example program from a functional point of view. For instance, this example uses different decoders to play music in different formats from the microSD card._
- _From a technical point of view, describe the main technical functions demonstrated. For instance, this example demonstrates the use of pipeline and element, and how element obtains data directly from callback._


### Resources

_It's optional, such as RAM, CPU loading._


### Prerequisites

- _It's optional_
- _Guide first-time users to run get started example first_
- _Guide users to learn the background_


### Folder contents

- _It's optional_
- _It is short explanation of remaining files in the project folder and the folder structure. Below is the example of the `play_mp3` folder._

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


## Environment Setup


### Hardware Required

_List all the hardware, such as development boards, speakers, microSD card, LCD module, camera module, Bluetooth speaker, and so on_


### Additional Requirements

- _It's optional_
- _Audio files, SIP server, DLNA app and so on, such as Baidu Cloud's profile or Amazon's token._


## Build and Flash


### Default IDF Branch

_Select either of the two expressions below depending on the actual situation._

This example supports IDF release/v[x.y] and later branches. By default, it runs on ADF's built-in branch `$ADF_PATH/esp-idf`.

This example supports IDF release/v[x.y] and later branches. By default, it runs on IDF release/v[x.y].


### Other Special IDF Branches

- _When required to select a special IDF/ADF version branch, which must be clearly pointed out and described_
- _For example, DU1906 project selects the IDF branch `audio/stack_on_psram_v3.3` to compile_

  ```bach
  cd $IDF_PATH
  git checkout master
  git pull
  git checkout audio/stack_on_psram_v3.3
  git submodule update --init --recursive
  ```


### Configuration

- _Describe important items to configure in menuconfig, such as long file name support for FatFs, selection of compatible audio board, chip type, PSRAM clock, Wi-Fi/LWIP parameters and so on. Below is an example._

  ```
  Component config > FAT Filesystem support > Long filename support
  ```

- _Configuration of other software if required, such as specifying a patch_


### Build and Flash

_Command to build the example_

- Legacy GNU Make command: `make`
- CMake command: `idf.py build`

_Command to flash the example_

- Legacy GNU Make command: `make flash monitor`
- CMake command: `idf.py -p PORT flash monitor`

For full steps to configure and build an ESP-IDF project, please go to [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) and select the chip and version in the upper left corner of the page.


## How to Use the Example


### Example Functionality

- _To explain how to use this example, what features are supported and what results to get. For example, describe what keys or speech commands are supported. Below is the speech interaction with DuerOS._
  - _"小度小度"，"在呢"，"讲个笑话"_
  - _"小度小度"，"在呢"，"上海天气怎么样？"_
  - _"小度小度"，"在呢"，"播放一首歌"_
  - _"小度小度"，"在呢"，"百度百科乐鑫信息科技"_

- _If any other items are needed, such as server, Bluetooth device, app, second chip, do mention them here. Include links if applicable. Explain how to set them up. Below is an example._
  - _Please run the HTTP server of `pipeline_raw_http`. This server is created on PC by running `python server.py` to receive data._ 


### Example Log

The complete log is as follows:

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

- _It's optional_
- _Running process, results, or video links, such as [ESP32-S3 离线语音识别](https://www.bilibili.com/video/BV1Cv411e7g8)_



## Troubleshooting

_It's optional. If there are any likely problems or errors which many users might encounter, mention them here._


## Technical Support and Feedback

Please use the following feedback channels:

- For technical queries, go to the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For a feature request or bug report, create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will get back to you as soon as possible.
