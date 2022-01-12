# GUI 显示示例
- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")

## 例程简介

本例程使用 LVGL 图形库绘制了一个音乐播放器的界面，支持触摸屏控制。本例程参考 LVGL 原始 lv_demo_music 工程。

## 环境配置

### 硬件要求

本例程可在标有绿色复选框的开发板上运行。请记住，如下面的 *配置* 一节所述，可以在 `menuconfig` 中选择开发板。

| 开发板名称 | 开始入门 | 芯片 | 兼容性 |
|-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------:|:--------------------------------------------------------------------:|:-----------------------------------------------------------------:|
| ESP32-LyraT | [![alt text](../../../docs/_static/esp32-lyrat-v4.3-side-small.jpg "ESP32-LyraT")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |
| ESP32-LyraTD-MSC | [![alt text](../../../docs/_static/esp32-lyratd-msc-v2.2-small.jpg "ESP32-LyraTD-MSC")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |
| ESP32-LyraT-Mini | [![alt text](../../../docs/_static/esp32-lyrat-mini-v1.2-small.jpg "ESP32-LyraT-Mini")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |
| ESP32-Korvo-DU1906 | [![alt text](../../../docs/_static/esp32-korvo-du1906-v1.1-small.jpg "ESP32-Korvo-DU1906")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | <img src="../../../docs/_static/ESP32.svg" height="85" alt="ESP32"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |
| ESP32-S2-Kaluga-1 Kit | [![alt text](../../../docs/_static/esp32-s2-kaluga-1-kit-small.png "ESP32-S2-Kaluga-1 Kit")](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) | <img src="../../../docs/_static/ESP32-S2.svg" height="100" alt="ESP32-S2"> | ![alt text](../../../docs/_static/no-button.png "开发板不兼容此例程") |
|ESP32-S3-Korvo-2 | [![alt text](../../../docs/_static/esp32-s3-korvo-2-v3.0-small.png "ESP32-Korvo-2")](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/user-guide-esp32-s3-korvo-2.html)) | <img src="../../../docs/_static/ESP32-S3.svg" height="100" alt="ESP32-S3"> | ![alt text](../../../docs/_static/yes-button.png "开发板兼容此例程") |

## 编译和下载


### IDF 默认分支

本例程支持分支至少是 IDF release/v4.4， 例程默认使用的是 IDF release/v4.4

### IDF 分支

本例程使用 release v4.4 分支，切换命令如下：

```c
cd $IDF_PATH
git checkout master
git pull
git checkout release/v4.4
git submodule update --init --recursive
```

### 配置

本例程选择的开发板是 ` ESP32-S3-Korvo-2 v3`。

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.4/esp32s3/index.html)。

## 如何使用例程


### 功能和用法

例程运行时会首先进入欢迎界面，待进入播放界面后，向上滑动 `ALL TRACKS` 会出现播放列表，点击一首模拟的音乐，再向下滑动折叠的播放界面，即可看到音乐正在播放。

### 日志输出

以下为本例程的完整日志。

```c
I (25) boot: ESP-IDF v4.4-dev-3930-g214d62b9ad-dirty 2nd stage bootloader
I (25) boot: compile time 21:30:04
I (25) boot: chip revision: 0
I (29) boot.esp32s3: Boot SPI Speed : 80MHz
I (33) boot.esp32s3: SPI Mode       : DIO
I (38) boot.esp32s3: SPI Flash Size : 2MB
I (43) boot: Enabling RNG early entropy source...
W (48) bootloader_random: RNG for ESP32-S3 not currently supported
I (55) boot: Partition Table:
I (59) boot: ## Label            Usage          Type ST Offset   Length
I (66) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (73) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (81) boot:  2 factory          factory app      00 00 00010000 00100000
I (88) boot: End of partition table
I (93) esp_image: segment 0: paddr=00010020 vaddr=3c050020 size=702c8h (459464) map
I (183) esp_image: segment 1: paddr=000802f0 vaddr=3fc93f90 size=0290ch ( 10508) load
I (186) esp_image: segment 2: paddr=00082c04 vaddr=40374000 size=0d414h ( 54292) load
I (201) esp_image: segment 3: paddr=00090020 vaddr=42000020 size=4906ch (299116) map
I (255) esp_image: segment 4: paddr=000d9094 vaddr=40381414 size=02b78h ( 11128) load
I (258) esp_image: segment 5: paddr=000dbc14 vaddr=50000000 size=00010h (    16) load
I (267) boot: Loaded app from partition at offset 0x10000
I (267) boot: Disabling RNG early entropy source...
W (273) bootloader_random: RNG for ESP32-S3 not currently supported
I (291) cpu_start: Pro cpu up.
I (291) cpu_start: Starting app cpu, entry point is 0x4037534c
0x4037534c: call_start_cpu1 at /Users/maojianxin/workspace/esp-adf-internal-dev/esp-idf/components/esp_system/port/cpu_start.c:156

I (0) cpu_start: App cpu up.
I (305) cpu_start: Pro cpu start user code
I (305) cpu_start: cpu freq: 160000000
I (305) cpu_start: Application information:
I (308) cpu_start: Project name:     lv_demos
I (313) cpu_start: App version:      v2.2-296-g830d5002-dirty
I (319) cpu_start: Compile time:     Dec 28 2021 21:39:20
I (325) cpu_start: ELF file SHA256:  db1ef929fed12d0e...
I (331) cpu_start: ESP-IDF:          v4.4-dev-3930-g214d62b9ad-dirty
I (338) heap_init: Initializing. RAM available for dynamic allocation:
I (346) heap_init: At 3FCAA8A8 len 00035758 (213 KiB): D/IRAM
I (352) heap_init: At 3FCE0000 len 0000EE34 (59 KiB): STACK/DRAM
I (359) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (365) heap_init: At 600FE000 len 00002000 (8 KiB): RTCRAM
I (372) spi_flash: detected chip: gd
I (376) spi_flash: flash io: dio
W (379) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (393) sleep: Configure to isolate all GPIO pins in sleep state
I (400) sleep: Enable automatic switching of GPIO sleep configuration
I (407) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (468) gpio: GPIO[2]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
W (468) I2C_BUS: i2c_bus_create:57: I2C bus has been already created, [port:0]
```

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。