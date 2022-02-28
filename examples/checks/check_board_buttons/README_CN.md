# 开发板按键检测例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

ADF 定义了音频开发板常用的 [6 种功能按键](https://github.com/espressif/esp-adf/blob/master/components/input_key_service/include/input_key_com_user_id.h)，即录音键 (Rec)、模式键 (Mode)、 播放键 (Play)、设置键 (Set)、音量减键 (Vol-)、音量加键 (Vol+)，用户可以通过 `input_key_service` 来实现每个按键的短按下、短按下释放、长按下、长按下释放等 [4 种按键事件](https://github.com/espressif/esp-adf/blob/master/components/input_key_service/include/input_key_service.h)。在不同的开发板上，六个按键实现按键检测的方式各有不同，有 [ADC 模拟电压检测](https://github.com/espressif/esp-adf/blob/master/components/audio_board/esp32_s3_korvo2_v3/board_def.h)、[GPIO 中断输入](https://github.com/espressif/esp-adf/blob/master/components/audio_board/lyrat_v4_3/board_def.h)、[电容触摸](https://github.com/espressif/esp-adf/blob/master/components/audio_board/lyrat_v4_3/board_def.h) 等方式。

本例子程序调用 `input_key_service` 使用 6 种功能按键和 4 种按键事件，可用于对开发板按键进行调试和检测。


## 环境配置


### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载


### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。


### 配置

本例程默认选择的开发板是 `ESP32-Lyrat V4.3`，如果需要在其他的开发板上运行此例程，则需要在 menuconfig 中选择开发板的配置，例如选择 `ESP32-Lyrat-Mini V1.1`。

```
menuconfig > Audio HAL > ESP32-Lyrat-Mini V1.1
```


### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程


### 功能和用法

- 例程开始运行后，等待用户按下按键测试，打印如下：

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
I (27) boot: compile time 14:56:47
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 2MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x07f44 ( 32580) map
I (123) esp_image: segment 1: paddr=0x00017f6c vaddr=0x3ffb0000 size=0x020a0 (  8352) load
I (127) esp_image: segment 2: paddr=0x0001a014 vaddr=0x40080000 size=0x06004 ( 24580) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x17770 ( 96112) map
0x400d0020: _stext at ??:?

I (179) esp_image: segment 4: paddr=0x00037798 vaddr=0x40086004 size=0x04538 ( 17720) load
0x40086004: pcTaskGetTaskName at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/tasks.c:2373 (discriminator 1)

I (193) boot: Loaded app from partition at offset 0x10000
I (193) boot: Disabling RNG early entropy source...
I (193) cpu_start: Pro cpu up.
I (197) cpu_start: Application information:
I (202) cpu_start: Project name:     check_board_buttons
I (208) cpu_start: App version:      v2.2-228-g838b0926-dirty
I (214) cpu_start: Compile time:     Nov 12 2021 14:56:40
I (220) cpu_start: ELF file SHA256:  dde162e03a6a6339...
I (226) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (232) cpu_start: Starting app cpu, entry point is 0x40081624
0x40081624: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (243) heap_init: Initializing. RAM available for dynamic allocation:
I (249) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (255) heap_init: At 3FFB2908 len 0002D6F8 (181 KiB): DRAM
I (262) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (268) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (274) heap_init: At 4008A53C len 00015AC4 (86 KiB): IRAM
I (281) cpu_start: Pro cpu start user code
I (299) spi_flash: detected chip: gd
I (299) spi_flash: flash io: dio
W (300) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (309) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (320) CHECK_BUTTON: [ 1 ] Initialize peripherals
I (330) CHECK_BUTTON: [ 2 ] Initialize Button peripheral with board init
I (330) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (340) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (350) CHECK_BUTTON: [ 3 ] Create and start input key service
W (370) PERIPH_TOUCH: _touch_init
W (370) CHECK_BUTTON: [ 4 ] Waiting for a button to be pressed ...

```

- 本例默认使用 `ESP32-Lyrat V4.3`，可以按下音频板上的 [Rec]、[Mode]、[Play]、[Set]、[Vol-]、[Vol+] 按键，检测按键是否正常工作，打印如下：

```c
I (24320) CHECK_BUTTON: [ * ] [Rec] KEY CLICKED
I (24490) CHECK_BUTTON: [ * ] [Rec] KEY CLICK RELEASED
I (25370) CHECK_BUTTON: [ * ] [MODE] KEY CLICKED
I (25510) CHECK_BUTTON: [ * ] [MODE] KEY CLICK RELEASED
I (26920) CHECK_BUTTON: [ * ] [Play] KEY CLICKED
I (27370) CHECK_BUTTON: [ * ] [Play] KEY CLICK RELEASED
I (28420) CHECK_BUTTON: [ * ] [SET] KEY CLICKED
I (28720) CHECK_BUTTON: [ * ] [SET] KEY CLICK RELEASED
I (29320) CHECK_BUTTON: [ * ] [Vol-] KEY CLICKED
I (29770) CHECK_BUTTON: [ * ] [Vol-] KEY CLICK RELEASED
I (31120) CHECK_BUTTON: [ * ] [Vol+] KEY CLICKED
I (31420) CHECK_BUTTON: [ * ] [Vol+] KEY CLICK RELEASED

```

- 同时，本例程也支持按键的长按处理，长按按键并释放，打印如下：

```c
I (43170) CHECK_BUTTON: [ * ] [Rec] KEY CLICKED
I (45200) CHECK_BUTTON: [ * ] [Rec] KEY PRESSED
I (48330) CHECK_BUTTON: [ * ] [Rec] KEY PRESS RELEASED
I (51620) CHECK_BUTTON: [ * ] [MODE] KEY CLICKED
I (53650) CHECK_BUTTON: [ * ] [MODE] KEY PRESSED
I (56260) CHECK_BUTTON: [ * ] [MODE] KEY PRESS RELEASED
I (57670) CHECK_BUTTON: [ * ] [Play] KEY CLICKED
I (59770) CHECK_BUTTON: [ * ] [Play] KEY PRESSED
I (60670) CHECK_BUTTON: [ * ] [Play] KEY PRESS RELEASED
I (62920) CHECK_BUTTON: [ * ] [SET] KEY CLICKED
I (65020) CHECK_BUTTON: [ * ] [SET] KEY PRESSED
I (67270) CHECK_BUTTON: [ * ] [SET] KEY PRESS RELEASED
I (68470) CHECK_BUTTON: [ * ] [Vol-] KEY CLICKED
I (70570) CHECK_BUTTON: [ * ] [Vol-] KEY PRESSED
I (72070) CHECK_BUTTON: [ * ] [Vol-] KEY PRESS RELEASED
I (72820) CHECK_BUTTON: [ * ] [Vol+] KEY CLICKED
I (74920) CHECK_BUTTON: [ * ] [Vol+] KEY PRESSED
I (75820) CHECK_BUTTON: [ * ] [Vol+] KEY PRESS RELEASED

```


### 日志输出

以下为本例程的完整日志。

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
I (27) boot: compile time 14:56:47
I (27) boot: chip revision: 3
I (31) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (38) boot.esp32: SPI Speed      : 40MHz
I (43) boot.esp32: SPI Mode       : DIO
I (47) boot.esp32: SPI Flash Size : 2MB
I (52) boot: Enabling RNG early entropy source...
I (57) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (68) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (83) boot:  2 factory          factory app      00 00 00010000 00100000
I (91) boot: End of partition table
I (95) boot_comm: chip revision: 3, min. application chip revision: 0
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x07f44 ( 32580) map
I (123) esp_image: segment 1: paddr=0x00017f6c vaddr=0x3ffb0000 size=0x020a0 (  8352) load
I (127) esp_image: segment 2: paddr=0x0001a014 vaddr=0x40080000 size=0x06004 ( 24580) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x17770 ( 96112) map
0x400d0020: _stext at ??:?

I (179) esp_image: segment 4: paddr=0x00037798 vaddr=0x40086004 size=0x04538 ( 17720) load
0x40086004: pcTaskGetTaskName at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/tasks.c:2373 (discriminator 1)

I (193) boot: Loaded app from partition at offset 0x10000
I (193) boot: Disabling RNG early entropy source...
I (193) cpu_start: Pro cpu up.
I (197) cpu_start: Application information:
I (202) cpu_start: Project name:     check_board_buttons
I (208) cpu_start: App version:      v2.2-228-g838b0926-dirty
I (214) cpu_start: Compile time:     Nov 12 2021 14:56:40
I (220) cpu_start: ELF file SHA256:  dde162e03a6a6339...
I (226) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (232) cpu_start: Starting app cpu, entry point is 0x40081624
0x40081624: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (243) heap_init: Initializing. RAM available for dynamic allocation:
I (249) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (255) heap_init: At 3FFB2908 len 0002D6F8 (181 KiB): DRAM
I (262) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (268) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (274) heap_init: At 4008A53C len 00015AC4 (86 KiB): IRAM
I (281) cpu_start: Pro cpu start user code
I (299) spi_flash: detected chip: gd
I (299) spi_flash: flash io: dio
W (300) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (309) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (320) CHECK_BUTTON: [ 1 ] Initialize peripherals
I (330) CHECK_BUTTON: [ 2 ] Initialize Button peripheral with board init
I (330) gpio: GPIO[36]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (340) gpio: GPIO[39]| InputEn: 1| OutputEn: 0| OpenDrain: 0| Pullup: 1| Pulldown: 0| Intr:3
I (350) CHECK_BUTTON: [ 3 ] Create and start input key service
W (370) PERIPH_TOUCH: _touch_init
W (370) CHECK_BUTTON: [ 4 ] Waiting for a button to be pressed ...
I (24320) CHECK_BUTTON: [ * ] [Rec] KEY CLICKED
I (24490) CHECK_BUTTON: [ * ] [Rec] KEY CLICK RELEASED
I (25370) CHECK_BUTTON: [ * ] [MODE] KEY CLICKED
I (25510) CHECK_BUTTON: [ * ] [MODE] KEY CLICK RELEASED
I (26920) CHECK_BUTTON: [ * ] [Play] KEY CLICKED
I (27370) CHECK_BUTTON: [ * ] [Play] KEY CLICK RELEASED
I (28420) CHECK_BUTTON: [ * ] [SET] KEY CLICKED
I (28720) CHECK_BUTTON: [ * ] [SET] KEY CLICK RELEASED
I (29320) CHECK_BUTTON: [ * ] [Vol-] KEY CLICKED
I (29770) CHECK_BUTTON: [ * ] [Vol-] KEY CLICK RELEASED
I (31120) CHECK_BUTTON: [ * ] [Vol+] KEY CLICKED
I (31420) CHECK_BUTTON: [ * ] [Vol+] KEY CLICK RELEASED
I (43170) CHECK_BUTTON: [ * ] [Rec] KEY CLICKED
I (45200) CHECK_BUTTON: [ * ] [Rec] KEY PRESSED
I (48330) CHECK_BUTTON: [ * ] [Rec] KEY PRESS RELEASED
I (51620) CHECK_BUTTON: [ * ] [MODE] KEY CLICKED
I (53650) CHECK_BUTTON: [ * ] [MODE] KEY PRESSED
I (56260) CHECK_BUTTON: [ * ] [MODE] KEY PRESS RELEASED
I (57670) CHECK_BUTTON: [ * ] [Play] KEY CLICKED
I (59770) CHECK_BUTTON: [ * ] [Play] KEY PRESSED
I (60670) CHECK_BUTTON: [ * ] [Play] KEY PRESS RELEASED
I (62920) CHECK_BUTTON: [ * ] [SET] KEY CLICKED
I (65020) CHECK_BUTTON: [ * ] [SET] KEY PRESSED
I (67270) CHECK_BUTTON: [ * ] [SET] KEY PRESS RELEASED
I (68470) CHECK_BUTTON: [ * ] [Vol-] KEY CLICKED
I (70570) CHECK_BUTTON: [ * ] [Vol-] KEY PRESSED
I (72070) CHECK_BUTTON: [ * ] [Vol-] KEY PRESS RELEASED
I (72820) CHECK_BUTTON: [ * ] [Vol+] KEY CLICKED
I (74920) CHECK_BUTTON: [ * ] [Vol+] KEY PRESSED
I (75820) CHECK_BUTTON: [ * ] [Vol+] KEY PRESS RELEASED
```


## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
