# 开发板的 LED 显示检测例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

ADF 从功能角度为定义了一系列常见的 [显示模式](https://github.com/espressif/esp-adf/blob/master/components/display_service/include/display_service.h)，在不同硬件环境下，用户可使用统一接口 `display_service_set_pattern` 设置显示模式。目前已添加的驱动 LED 方式有 [PWM](https://github.com/espressif/esp-adf/blob/master/components/display_service/led_indicator/led_indicator.c)、[AW2013](https://github.com/espressif/esp-adf/blob/master/components/display_service/led_bar/led_bar_aw2013.c)、[IS3x](https://github.com/espressif/esp-adf/blob/master/components/display_service/led_bar/led_bar_is31x.c)、[WS2812](https://github.com/espressif/esp-adf/blob/master/components/display_service/led_bar/led_bar_ws2812.c)。

本例程主要演示如何操作 LED 模式，以及验证所有 LED 是否正常工作，更多信息请参考 [ADF 入门指南](https://docs.espressif.com/projects/esp-adf/zh_CN/latest/get-started/index.html)。


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

- 例程开始运行后，循环按顺序依次显示不同的闪烁效果，打印如下：

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
I (27) boot: compile time 15:01:00
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x06c00 ( 27648) map
I (122) esp_image: segment 1: paddr=0x00016c28 vaddr=0x3ffb0000 size=0x020ac (  8364) load
I (125) esp_image: segment 2: paddr=0x00018cdc vaddr=0x40080000 size=0x0733c ( 29500) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x16d48 ( 93512) map
0x400d0020: _stext at ??:?

I (178) esp_image: segment 4: paddr=0x00036d70 vaddr=0x4008733c size=0x03118 ( 12568) load
0x4008733c: xthal_window_spill_nw at /Users/igrokhotkov/e/esp32/hal/hal/windowspill_asm.S:212

I (189) boot: Loaded app from partition at offset 0x10000
I (189) boot: Disabling RNG early entropy source...
I (190) cpu_start: Pro cpu up.
I (193) cpu_start: Application information:
I (198) cpu_start: Project name:     check_display_led
I (204) cpu_start: App version:      v2.2-228-g07116fa5-dirty
I (210) cpu_start: Compile time:     Nov 12 2021 16:20:42
I (217) cpu_start: ELF file SHA256:  b5d39a251613418c...
I (223) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (229) cpu_start: Starting app cpu, entry point is 0x40081618
0x40081618: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (239) heap_init: Initializing. RAM available for dynamic allocation:
I (246) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (252) heap_init: At 3FFB2930 len 0002D6D0 (181 KiB): DRAM
I (258) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (264) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (271) heap_init: At 4008A454 len 00015BAC (86 KiB): IRAM
I (277) cpu_start: Pro cpu start user code
I (295) spi_flash: detected chip: gd
I (296) spi_flash: flash io: dio
W (296) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (306) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (317) CHECK_DISPLAY_LED: [ 1 ] Create display service instance
I (327) CHECK_DISPLAY_LED: LED pattern index is 0
W (327) LED_INDI: The led mode is invalid
I (2337) CHECK_DISPLAY_LED: LED pattern index is 1
I (4337) CHECK_DISPLAY_LED: LED pattern index is 2
W (4337) LED_INDI: The led mode is invalid
I (6337) CHECK_DISPLAY_LED: LED pattern index is 3
I (8337) CHECK_DISPLAY_LED: LED pattern index is 4
I (10337) CHECK_DISPLAY_LED: LED pattern index is 5
W (10337) LED_INDI: The led mode is invalid
I (12337) CHECK_DISPLAY_LED: LED pattern index is 6
W (12337) LED_INDI: The led mode is invalid
I (14337) CHECK_DISPLAY_LED: LED pattern index is 7
W (14337) LED_INDI: The led mode is invalid
I (16337) CHECK_DISPLAY_LED: LED pattern index is 8
W (16337) LED_INDI: The led mode is invalid
I (18337) CHECK_DISPLAY_LED: LED pattern index is 9
W (18337) LED_INDI: The led mode is invalid
I (20337) CHECK_DISPLAY_LED: LED pattern index is 10
W (20337) LED_INDI: The led mode is invalid
I (22337) CHECK_DISPLAY_LED: LED pattern index is 11
W (22337) LED_INDI: The led mode is invalid
I (24337) CHECK_DISPLAY_LED: LED pattern index is 12
W (24337) LED_INDI: The led mode is invalid
I (26337) CHECK_DISPLAY_LED: LED pattern index is 13
I (28337) CHECK_DISPLAY_LED: LED pattern index is 14
I (30337) CHECK_DISPLAY_LED: LED pattern index is 15
W (30337) LED_INDI: The led mode is invalid
I (32337) CHECK_DISPLAY_LED: LED pattern index is 16
W (32337) LED_INDI: The led mode is invalid
I (34337) CHECK_DISPLAY_LED: LED pattern index is 17
W (34337) LED_INDI: The led mode is invalid
I (36337) CHECK_DISPLAY_LED: LED pattern index is 18
W (36337) LED_INDI: The led mode is invalid
I (38337) CHECK_DISPLAY_LED: LED pattern index is 19
W (38337) LED_INDI: The led mode is invalid
I (40337) CHECK_DISPLAY_LED: LED pattern index is 20
I (42337) CHECK_DISPLAY_LED: LED pattern index is 21
I (44337) CHECK_DISPLAY_LED: LED pattern index is 22
W (44337) LED_INDI: The led mode is invalid
I (46337) CHECK_DISPLAY_LED: LED pattern index is 23
W (46337) LED_INDI: The led mode is invalid
I (48337) CHECK_DISPLAY_LED: LED pattern index is 24
W (48337) LED_INDI: The led mode is invalid
I (50337) CHECK_DISPLAY_LED: LED pattern index is 25
W (50337) LED_INDI: The led mode is invalid
I (52337) CHECK_DISPLAY_LED: LED pattern index is 26
W (52337) LED_INDI: The led mode is invalid
I (54337) CHECK_DISPLAY_LED: LED pattern index is 27
I (56337) CHECK_DISPLAY_LED: LED pattern index is 28
I (58337) CHECK_DISPLAY_LED: LED pattern index is 0
W (58337) LED_INDI: The led mode is invalid
I (60337) CHECK_DISPLAY_LED: LED pattern index is 1
I (62337) CHECK_DISPLAY_LED: LED pattern index is 2
W (62337) LED_INDI: The led mode is invalid
I (64337) CHECK_DISPLAY_LED: LED pattern index is 3
I (66337) CHECK_DISPLAY_LED: LED pattern index is 4
I (68337) CHECK_DISPLAY_LED: LED pattern index is 5
W (68337) LED_INDI: The led mode is invalid
I (70337) CHECK_DISPLAY_LED: LED pattern index is 6
W (70337) LED_INDI: The led mode is invalid
I (72337) CHECK_DISPLAY_LED: LED pattern index is 7
W (72337) LED_INDI: The led mode is invalid
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
I (27) boot: compile time 15:01:00
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x06c00 ( 27648) map
I (122) esp_image: segment 1: paddr=0x00016c28 vaddr=0x3ffb0000 size=0x020ac (  8364) load
I (125) esp_image: segment 2: paddr=0x00018cdc vaddr=0x40080000 size=0x0733c ( 29500) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (142) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x16d48 ( 93512) map
0x400d0020: _stext at ??:?

I (178) esp_image: segment 4: paddr=0x00036d70 vaddr=0x4008733c size=0x03118 ( 12568) load
0x4008733c: xthal_window_spill_nw at /Users/igrokhotkov/e/esp32/hal/hal/windowspill_asm.S:212

I (189) boot: Loaded app from partition at offset 0x10000
I (189) boot: Disabling RNG early entropy source...
I (190) cpu_start: Pro cpu up.
I (193) cpu_start: Application information:
I (198) cpu_start: Project name:     check_display_led
I (204) cpu_start: App version:      v2.2-228-g07116fa5-dirty
I (210) cpu_start: Compile time:     Nov 12 2021 16:20:42
I (217) cpu_start: ELF file SHA256:  b5d39a251613418c...
I (223) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (229) cpu_start: Starting app cpu, entry point is 0x40081618
0x40081618: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (239) heap_init: Initializing. RAM available for dynamic allocation:
I (246) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (252) heap_init: At 3FFB2930 len 0002D6D0 (181 KiB): DRAM
I (258) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (264) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (271) heap_init: At 4008A454 len 00015BAC (86 KiB): IRAM
I (277) cpu_start: Pro cpu start user code
I (295) spi_flash: detected chip: gd
I (296) spi_flash: flash io: dio
W (296) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (306) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (317) CHECK_DISPLAY_LED: [ 1 ] Create display service instance
I (327) CHECK_DISPLAY_LED: LED pattern index is 0
W (327) LED_INDI: The led mode is invalid
I (2337) CHECK_DISPLAY_LED: LED pattern index is 1
I (4337) CHECK_DISPLAY_LED: LED pattern index is 2
W (4337) LED_INDI: The led mode is invalid
I (6337) CHECK_DISPLAY_LED: LED pattern index is 3
I (8337) CHECK_DISPLAY_LED: LED pattern index is 4
I (10337) CHECK_DISPLAY_LED: LED pattern index is 5
W (10337) LED_INDI: The led mode is invalid
I (12337) CHECK_DISPLAY_LED: LED pattern index is 6
W (12337) LED_INDI: The led mode is invalid
I (14337) CHECK_DISPLAY_LED: LED pattern index is 7
W (14337) LED_INDI: The led mode is invalid
I (16337) CHECK_DISPLAY_LED: LED pattern index is 8
W (16337) LED_INDI: The led mode is invalid
I (18337) CHECK_DISPLAY_LED: LED pattern index is 9
W (18337) LED_INDI: The led mode is invalid
I (20337) CHECK_DISPLAY_LED: LED pattern index is 10
W (20337) LED_INDI: The led mode is invalid
I (22337) CHECK_DISPLAY_LED: LED pattern index is 11
W (22337) LED_INDI: The led mode is invalid
I (24337) CHECK_DISPLAY_LED: LED pattern index is 12
W (24337) LED_INDI: The led mode is invalid
I (26337) CHECK_DISPLAY_LED: LED pattern index is 13
I (28337) CHECK_DISPLAY_LED: LED pattern index is 14
I (30337) CHECK_DISPLAY_LED: LED pattern index is 15
W (30337) LED_INDI: The led mode is invalid
I (32337) CHECK_DISPLAY_LED: LED pattern index is 16
W (32337) LED_INDI: The led mode is invalid
I (34337) CHECK_DISPLAY_LED: LED pattern index is 17
W (34337) LED_INDI: The led mode is invalid
I (36337) CHECK_DISPLAY_LED: LED pattern index is 18
W (36337) LED_INDI: The led mode is invalid
I (38337) CHECK_DISPLAY_LED: LED pattern index is 19
W (38337) LED_INDI: The led mode is invalid
I (40337) CHECK_DISPLAY_LED: LED pattern index is 20
I (42337) CHECK_DISPLAY_LED: LED pattern index is 21
I (44337) CHECK_DISPLAY_LED: LED pattern index is 22
W (44337) LED_INDI: The led mode is invalid
I (46337) CHECK_DISPLAY_LED: LED pattern index is 23
W (46337) LED_INDI: The led mode is invalid
I (48337) CHECK_DISPLAY_LED: LED pattern index is 24
W (48337) LED_INDI: The led mode is invalid
I (50337) CHECK_DISPLAY_LED: LED pattern index is 25
W (50337) LED_INDI: The led mode is invalid
I (52337) CHECK_DISPLAY_LED: LED pattern index is 26
W (52337) LED_INDI: The led mode is invalid
I (54337) CHECK_DISPLAY_LED: LED pattern index is 27
I (56337) CHECK_DISPLAY_LED: LED pattern index is 28
I (58337) CHECK_DISPLAY_LED: LED pattern index is 0
W (58337) LED_INDI: The led mode is invalid
I (60337) CHECK_DISPLAY_LED: LED pattern index is 1
I (62337) CHECK_DISPLAY_LED: LED pattern index is 2
W (62337) LED_INDI: The led mode is invalid
I (64337) CHECK_DISPLAY_LED: LED pattern index is 3
I (66337) CHECK_DISPLAY_LED: LED pattern index is 4
I (68337) CHECK_DISPLAY_LED: LED pattern index is 5
W (68337) LED_INDI: The led mode is invalid
I (70337) CHECK_DISPLAY_LED: LED pattern index is 6
W (70337) LED_INDI: The led mode is invalid
I (72337) CHECK_DISPLAY_LED: LED pattern index is 7
W (72337) LED_INDI: The led mode is invalid
```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
