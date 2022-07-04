# 播放 SPIFFS 中的 MP3 文件例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介


本例程在 ADF 框架下实现 SPIFFS 文件系统中 MP3 音频文件的播放演示。


本例程的管道如下图：

```
[flash] ---> spiffs_stream ---> mp3_decoder ---> i2s_stream ---> [codec_chip]
```

### 预备知识

制作 SPIFFS 文件

- 从 [Github/spiffs](https://github.com/igrr/mkspiffs.git) 上克隆 SPIFFS 仓库

```
    git clone https://github.com/igrr/mkspiffs.git
```

- 编译 SPIFFS

```
    cd mkspiffs
    make clean
    make dist CPPFLAGS="-DSPIFFS_OBJ_META_LEN=4"
```

- 拷贝用户音频文件到例程下面的 `tools` 文件夹（本例程已经准备了 `adf_music.mp3` 文件）。

- 运行下面命令，把 `adf_music.mp3` 文件压进 `adf_music.bin` 二进制文件中，然后烧录此文件到 `partition` 中指定分区即可，本例程已经把制作好的文件也放置在 `tools` 文件夹中。

```
    ./mkspiffs -c ./tools -b 4096 -p 256 -s 0x100000 ./tools/adf_music.bin
```

- 创建分区表，如下所示：

  ```
    nvs,      data, nvs,     ,        0x6000,
    phy_init, data, phy,     ,        0x1000,
    factory,  app,  factory, ,        1M,
    storage,  data, spiffs,  0x110000,1M,
  ```

- 下载 SPIFFS bin 文件。当前 `./tools/adf_music.bin` 中只包含 `adf_music.mp3` 文件（所有 MP3 文件最终都会生成一个 bin 文件）。

有关 `spiffs` 的更多信息，请参阅 [SPIFFS 文件系统](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/spiffs.html)。


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。如果选择 `CONFIG_ESP32_C3_LYRA_V2_BOARD`，则需要在 `$ADF_PATH/esp-idf` 目录下应用`idf_v4.4_i2s_c3_pdm_tx.patch`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

运行下面的命令，烧录 `./tools/adf_music.bin` 文件到 `partition` 分区的 `storage` 地址处。

```
python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 115200 write_flash -z 0x110000 ./tools/adf_music.bin
```


> 注: 该 `download` 命令只针对 `esp32` 芯片模块，如果使用其他的芯片硬件，则需要修改 `--chip`。例如，使用 `esp32s3`芯片，命令则为 `python $ADF_PATH/esp-idf/components/esptool_py/esptool/esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 115200 write_flash -z 0x110000 ./tools/adf_music.bin`。

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。

## 如何使用例程

### 功能和用法

- 例程开始运行后，将直接读取 SPIFFS 中的 `/spiffs/adf_music.mp3` 音频文件进行播放，打印如下：

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7556
load:0x40078000,len:13904
ho 0 tail 12 room 4
load:0x40080400,len:5296
0x40080400: _init at ??:?

entry 0x40080710
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 17:32:32
I (29) boot: chip revision: 3
I (33) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) qio_mode: Enabling default flash chip QIO
I (45) boot.esp32: SPI Speed      : 80MHz
I (50) boot.esp32: SPI Mode       : QIO
I (55) boot.esp32: SPI Flash Size : 4MB
I (59) boot: Enabling RNG early entropy source...
I (65) boot: Partition Table:
I (68) boot: ## Label            Usage          Type ST Offset   Length
I (75) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (83) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (90) boot:  2 factory          factory app      00 00 00010000 00100000
I (98) boot:  3 storage          Unknown data     01 82 00110000 00100000
I (105) boot: End of partition table
I (110) boot_comm: chip revision: 3, min. application chip revision: 0
I (117) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0eab4 ( 60084) map
I (143) esp_image: segment 1: paddr=0x0001eadc vaddr=0x3ffb0000 size=0x0153c (  5436) load
I (146) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x2cd40 (183616) map
0x400d0020: _stext at ??:?

I (204) esp_image: segment 3: paddr=0x0004cd68 vaddr=0x3ffb153c size=0x00ed0 (  3792) load
I (206) esp_image: segment 4: paddr=0x0004dc40 vaddr=0x40080000 size=0x0d144 ( 53572) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (237) boot: Loaded app from partition at offset 0x10000
I (237) boot: Disabling RNG early entropy source...
I (237) cpu_start: Pro cpu up.
I (241) cpu_start: Application information:
I (246) cpu_start: Project name:     play_spiffs_mp3
I (252) cpu_start: App version:      v2.2-230-g83bfd722-dirty
I (258) cpu_start: Compile time:     Nov 16 2021 17:32:27
I (264) cpu_start: ELF file SHA256:  d7ecb4c9ba527922...
I (270) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (276) cpu_start: Starting app cpu, entry point is 0x400818d4
0x400818d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (286) heap_init: Initializing. RAM available for dynamic allocation:
I (293) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (299) heap_init: At 3FFB2C90 len 0002D370 (180 KiB): DRAM
I (306) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (312) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (318) heap_init: At 4008D144 len 00012EBC (75 KiB): IRAM
I (325) cpu_start: Pro cpu start user code
I (342) spi_flash: detected chip: gd
I (342) spi_flash: flash io: qio
W (342) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (353) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (364) SPIFFS_MP3_EXAMPLE: [ 1 ] Mount spiffs
I (464) SPIFFS_MP3_EXAMPLE: [ 2 ] Start codec chip
E (464) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (494) SPIFFS_MP3_EXAMPLE: [3.0] Create audio pipeline for playback
I (494) SPIFFS_MP3_EXAMPLE: [3.1] Create spiffs stream to read data from sdcard
I (494) SPIFFS_MP3_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (524) SPIFFS_MP3_EXAMPLE: [3.3] Create mp3 decoder to decode mp3 file
I (524) SPIFFS_MP3_EXAMPLE: [3.4] Register all elements to audio pipeline
I (534) SPIFFS_MP3_EXAMPLE: [3.5] Link it together [flash]-->spiffs-->mp3_decoder-->i2s_stream-->[codec_chip]
I (544) SPIFFS_MP3_EXAMPLE: [3.6] Set up  uri (file as spiffs, mp3 as mp3 decoder, and default output is i2s)
I (554) SPIFFS_MP3_EXAMPLE: [ 4 ] Set up  event listener
I (554) SPIFFS_MP3_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (564) SPIFFS_MP3_EXAMPLE: [4.2] Listening event from peripherals
I (574) SPIFFS_MP3_EXAMPLE: [ 5 ] Start audio_pipeline
I (604) SPIFFS_MP3_EXAMPLE: [ 6 ] Listen for all pipeline events
I (604) SPIFFS_MP3_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (6364) SPIFFS_STREAM: No more data, ret:0
W (7384) SPIFFS_MP3_EXAMPLE: [ * ] Stop event received
I (7384) SPIFFS_MP3_EXAMPLE: [ 7 ] Stop audio_pipeline
E (7384) AUDIO_ELEMENT: [spiffs] Element already stopped
E (7394) AUDIO_ELEMENT: [mp3] Element already stopped
E (7394) AUDIO_ELEMENT: [i2s] Element already stopped
W (7404) AUDIO_PIPELINE: There are no listener registered
W (7404) AUDIO_ELEMENT: [spiffs] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7414) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7424) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE

```


### 日志输出
以下是本例程的完整日志。

```c
rst:0x1 (POWERON_RESET),boot:0x1f (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0030,len:7556
load:0x40078000,len:13904
ho 0 tail 12 room 4
load:0x40080400,len:5296
0x40080400: _init at ??:?

entry 0x40080710
I (29) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (29) boot: compile time 17:32:32
I (29) boot: chip revision: 3
I (33) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (40) qio_mode: Enabling default flash chip QIO
I (45) boot.esp32: SPI Speed      : 80MHz
I (50) boot.esp32: SPI Mode       : QIO
I (55) boot.esp32: SPI Flash Size : 4MB
I (59) boot: Enabling RNG early entropy source...
I (65) boot: Partition Table:
I (68) boot: ## Label            Usage          Type ST Offset   Length
I (75) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (83) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (90) boot:  2 factory          factory app      00 00 00010000 00100000
I (98) boot:  3 storage          Unknown data     01 82 00110000 00100000
I (105) boot: End of partition table
I (110) boot_comm: chip revision: 3, min. application chip revision: 0
I (117) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0eab4 ( 60084) map
I (143) esp_image: segment 1: paddr=0x0001eadc vaddr=0x3ffb0000 size=0x0153c (  5436) load
I (146) esp_image: segment 2: paddr=0x00020020 vaddr=0x400d0020 size=0x2cd40 (183616) map
0x400d0020: _stext at ??:?

I (204) esp_image: segment 3: paddr=0x0004cd68 vaddr=0x3ffb153c size=0x00ed0 (  3792) load
I (206) esp_image: segment 4: paddr=0x0004dc40 vaddr=0x40080000 size=0x0d144 ( 53572) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (237) boot: Loaded app from partition at offset 0x10000
I (237) boot: Disabling RNG early entropy source...
I (237) cpu_start: Pro cpu up.
I (241) cpu_start: Application information:
I (246) cpu_start: Project name:     play_spiffs_mp3
I (252) cpu_start: App version:      v2.2-230-g83bfd722-dirty
I (258) cpu_start: Compile time:     Nov 16 2021 17:32:27
I (264) cpu_start: ELF file SHA256:  d7ecb4c9ba527922...
I (270) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (276) cpu_start: Starting app cpu, entry point is 0x400818d4
0x400818d4: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (286) heap_init: Initializing. RAM available for dynamic allocation:
I (293) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (299) heap_init: At 3FFB2C90 len 0002D370 (180 KiB): DRAM
I (306) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (312) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (318) heap_init: At 4008D144 len 00012EBC (75 KiB): IRAM
I (325) cpu_start: Pro cpu start user code
I (342) spi_flash: detected chip: gd
I (342) spi_flash: flash io: qio
W (342) spi_flash: Detected size(8192k) larger than the size in the binary image header(4096k). Using the size in the binary image header.
I (353) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (364) SPIFFS_MP3_EXAMPLE: [ 1 ] Mount spiffs
I (464) SPIFFS_MP3_EXAMPLE: [ 2 ] Start codec chip
E (464) gpio: gpio_install_isr_service(438): GPIO isr service already installed
I (494) SPIFFS_MP3_EXAMPLE: [3.0] Create audio pipeline for playback
I (494) SPIFFS_MP3_EXAMPLE: [3.1] Create spiffs stream to read data from sdcard
I (494) SPIFFS_MP3_EXAMPLE: [3.2] Create i2s stream to write data to codec chip
I (524) SPIFFS_MP3_EXAMPLE: [3.3] Create mp3 decoder to decode mp3 file
I (524) SPIFFS_MP3_EXAMPLE: [3.4] Register all elements to audio pipeline
I (534) SPIFFS_MP3_EXAMPLE: [3.5] Link it together [flash]-->spiffs-->mp3_decoder-->i2s_stream-->[codec_chip]
I (544) SPIFFS_MP3_EXAMPLE: [3.6] Set up  uri (file as spiffs, mp3 as mp3 decoder, and default output is i2s)
I (554) SPIFFS_MP3_EXAMPLE: [ 4 ] Set up  event listener
I (554) SPIFFS_MP3_EXAMPLE: [4.1] Listening event from all elements of pipeline
I (564) SPIFFS_MP3_EXAMPLE: [4.2] Listening event from peripherals
I (574) SPIFFS_MP3_EXAMPLE: [ 5 ] Start audio_pipeline
I (604) SPIFFS_MP3_EXAMPLE: [ 6 ] Listen for all pipeline events
I (604) SPIFFS_MP3_EXAMPLE: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
W (6364) SPIFFS_STREAM: No more data, ret:0
W (7384) SPIFFS_MP3_EXAMPLE: [ * ] Stop event received
I (7384) SPIFFS_MP3_EXAMPLE: [ 7 ] Stop audio_pipeline
E (7384) AUDIO_ELEMENT: [spiffs] Element already stopped
E (7394) AUDIO_ELEMENT: [mp3] Element already stopped
E (7394) AUDIO_ELEMENT: [i2s] Element already stopped
W (7404) AUDIO_PIPELINE: There are no listener registered
W (7404) AUDIO_ELEMENT: [spiffs] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7414) AUDIO_ELEMENT: [i2s] Element has not create when AUDIO_ELEMENT_TERMINATE
W (7424) AUDIO_ELEMENT: [mp3] Element has not create when AUDIO_ELEMENT_TERMINATE

```


## 故障排除

```c
I (364) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (376) SPIFFS_MP3_EXAMPLE: [ 1 ] Mount spiffs
E (386) SPIFFS: spiffs partition could not be found
root /spiffs E (386) PERIPH_SPIFFS: Failed to find SPIFFS partition

```
如果出现上述日志中的错误提示，请按照编译下载章节的说明，烧录 `./tools/adf_music.bin` 文件到 `partition` 分区的 `storage` 地址处，即可解决此问题。



## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
