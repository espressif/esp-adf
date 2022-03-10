# 电池检测服务 (Battery Detection Service) 例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程在演示了 ADF 框架下配置电池服务，在使用电池的应用场景中，通过注册 `battery_service_cb` 回调函数，可以获得电池满电、亏电、阈值报警等事件推送的示例。


## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中[例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性)中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v3.3 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

### 配置

本例程默认选择的开发板是 `ESP32-Korvo-DU1906`。注意，因为此例程需要 GPIO 读取 ADC 电压，所以有些开发板不适用此例程。

```
menuconfig > Audio HAL > ESP32-Korvo-DU1906
```

**注意：**

对于有充电管理芯片的开发板，如果其有 `I2C` 等接口可以读取电池电压、温度等数据，可以通过注册电池服务的 `init` 来初始化电池充电管理芯片，然后通过注册 `vol_get` 接口，来读取电池充电管理芯片的数据。

```c
    vol_monitor_param_t vol_monitor_cfg = {
        .init = adc_init,
        .deinit = adc_deinit,
        .vol_get = vol_read,
        .read_freq = 2,
        .report_freq = 2,
        .vol_full_threshold = CONFIG_VOLTAGE_OF_BATTERY_FULL,
        .vol_low_threshold = CONFIG_VOLTAGE_OF_BATTERY_LOW,
    };
```

可以通过 menuconfig 来配置电池电压阈值。

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。


## 如何使用例程

### 功能和用法

- 例程开始运行后，将自动读取 `ESP32-Korvo-DU1906` 开发板的电池电压数据，打印如下：

```c
rst:0x1 (POWERON_RESET),boot:0x1b (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 16:37:38
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x07284 ( 29316) map
I (122) esp_image: segment 1: paddr=0x000172ac vaddr=0x3ffb0000 size=0x02088 (  8328) load
I (126) esp_image: segment 2: paddr=0x0001933c vaddr=0x40080000 size=0x06cdc ( 27868) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (143) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x15878 ( 88184) map
0x400d0020: _stext at ??:?

I (177) esp_image: segment 4: paddr=0x000358a0 vaddr=0x40086cdc size=0x03874 ( 14452) load
0x40086cdc: prvCopyDataToQueue at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/queue.c:1921

I (190) boot: Loaded app from partition at offset 0x10000
I (190) boot: Disabling RNG early entropy source...
I (190) cpu_start: Pro cpu up.
I (194) cpu_start: Application information:
I (199) cpu_start: Project name:     battery_example
I (204) cpu_start: App version:      v2.2-238-ga496ac5c
I (210) cpu_start: Compile time:     Nov 24 2021 16:37:31
I (216) cpu_start: ELF file SHA256:  870be2024ac17feb...
I (222) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (228) cpu_start: Starting app cpu, entry point is 0x40081604
0x40081604: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (239) heap_init: Initializing. RAM available for dynamic allocation:
I (245) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (252) heap_init: At 3FFB28C0 len 0002D740 (181 KiB): DRAM
I (258) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (264) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (271) heap_init: At 4008A550 len 00015AB0 (86 KiB): IRAM
I (277) cpu_start: Pro cpu start user code
I (295) spi_flash: detected chip: gd
I (296) spi_flash: flash io: dio
W (296) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (306) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (316) BATTERY_EXAMPLE: [1.0] create battery service
I (326) BATTERY_EXAMPLE: [1.1] start battery service
I (326) BATTERY_EXAMPLE: [1.2] start battery voltage report
I (4326) BATTERY_EXAMPLE: got voltage 2077
I (8326) BATTERY_EXAMPLE: got voltage 2078
I (12326) BATTERY_EXAMPLE: got voltage 2078
I (16326) BATTERY_EXAMPLE: got voltage 2077
I (20326) BATTERY_EXAMPLE: got voltage 2078
I (24326) BATTERY_EXAMPLE: got voltage 2078
I (28326) BATTERY_EXAMPLE: got voltage 2078
I (32326) BATTERY_EXAMPLE: got voltage 2078
I (36326) BATTERY_EXAMPLE: got voltage 2078
I (40326) BATTERY_EXAMPLE: got voltage 2077
I (44326) BATTERY_EXAMPLE: got voltage 2076
I (48326) BATTERY_EXAMPLE: got voltage 2078
I (52326) BATTERY_EXAMPLE: got voltage 2077
I (56326) BATTERY_EXAMPLE: got voltage 2078
I (60326) BATTERY_EXAMPLE: got voltage 2078
I (60336) BATTERY_EXAMPLE: [1.3] change battery voltage report freqency
I (62326) BATTERY_EXAMPLE: got voltage 2077
I (64326) BATTERY_EXAMPLE: got voltage 2078
I (66326) BATTERY_EXAMPLE: got voltage 2078
I (68326) BATTERY_EXAMPLE: got voltage 2078
I (70326) BATTERY_EXAMPLE: got voltage 2078
I (72326) BATTERY_EXAMPLE: got voltage 2077
I (74326) BATTERY_EXAMPLE: got voltage 2077
I (76326) BATTERY_EXAMPLE: got voltage 2077
I (78326) BATTERY_EXAMPLE: got voltage 2078
I (80326) BATTERY_EXAMPLE: got voltage 2078
I (82326) BATTERY_EXAMPLE: got voltage 2078
I (84326) BATTERY_EXAMPLE: got voltage 2078
I (86326) BATTERY_EXAMPLE: got voltage 2077
I (88326) BATTERY_EXAMPLE: got voltage 2078
I (90326) BATTERY_EXAMPLE: got voltage 2078
I (92326) BATTERY_EXAMPLE: got voltage 2078
I (94326) BATTERY_EXAMPLE: got voltage 2078
I (96326) BATTERY_EXAMPLE: got voltage 2078
I (98326) BATTERY_EXAMPLE: got voltage 2077
I (100326) BATTERY_EXAMPLE: got voltage 2079
I (102326) BATTERY_EXAMPLE: got voltage 2078
I (104326) BATTERY_EXAMPLE: got voltage 2078
I (106326) BATTERY_EXAMPLE: got voltage 2078
I (108326) BATTERY_EXAMPLE: got voltage 2077
I (110326) BATTERY_EXAMPLE: got voltage 2077
I (112326) BATTERY_EXAMPLE: got voltage 2077
I (114326) BATTERY_EXAMPLE: got voltage 2078
I (116326) BATTERY_EXAMPLE: got voltage 2077
I (118326) BATTERY_EXAMPLE: got voltage 2077
I (120326) BATTERY_EXAMPLE: got voltage 2077
I (120336) BATTERY_EXAMPLE: [2.0] stop battery voltage report
I (120336) BATTERY_EXAMPLE: [2.1] destroy battery service
I (120336) BATTERY_SERVICE: battery service destroyed
I (120336) VOL_MONITOR: vol monitor destroyed

```


### 日志输出
以下为本例程的完整日志。

```c
rst:0x1 (POWERON_RESET),boot:0x1b (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:2
load:0x3fff0030,len:7204
load:0x40078000,len:13212
load:0x40080400,len:4568
0x40080400: _init at ??:?

entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2-1-g379ca2123 2nd stage bootloader
I (27) boot: compile time 16:37:38
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
I (102) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x07284 ( 29316) map
I (122) esp_image: segment 1: paddr=0x000172ac vaddr=0x3ffb0000 size=0x02088 (  8328) load
I (126) esp_image: segment 2: paddr=0x0001933c vaddr=0x40080000 size=0x06cdc ( 27868) load
0x40080000: _WindowOverflow4 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/xtensa/xtensa_vectors.S:1730

I (143) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x15878 ( 88184) map
0x400d0020: _stext at ??:?

I (177) esp_image: segment 4: paddr=0x000358a0 vaddr=0x40086cdc size=0x03874 ( 14452) load
0x40086cdc: prvCopyDataToQueue at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/freertos/queue.c:1921

I (190) boot: Loaded app from partition at offset 0x10000
I (190) boot: Disabling RNG early entropy source...
I (190) cpu_start: Pro cpu up.
I (194) cpu_start: Application information:
I (199) cpu_start: Project name:     battery_example
I (204) cpu_start: App version:      v2.2-238-ga496ac5c
I (210) cpu_start: Compile time:     Nov 24 2021 16:37:31
I (216) cpu_start: ELF file SHA256:  870be2024ac17feb...
I (222) cpu_start: ESP-IDF:          v4.2.2-1-g379ca2123
I (228) cpu_start: Starting app cpu, entry point is 0x40081604
0x40081604: call_start_cpu1 at /hengyongchao/esp-idfs/esp-idf-v4.2.2-psram/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (239) heap_init: Initializing. RAM available for dynamic allocation:
I (245) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (252) heap_init: At 3FFB28C0 len 0002D740 (181 KiB): DRAM
I (258) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (264) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (271) heap_init: At 4008A550 len 00015AB0 (86 KiB): IRAM
I (277) cpu_start: Pro cpu start user code
I (295) spi_flash: detected chip: gd
I (296) spi_flash: flash io: dio
W (296) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (306) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (316) BATTERY_EXAMPLE: [1.0] create battery service
I (326) BATTERY_EXAMPLE: [1.1] start battery service
I (326) BATTERY_EXAMPLE: [1.2] start battery voltage report
I (4326) BATTERY_EXAMPLE: got voltage 2077
I (8326) BATTERY_EXAMPLE: got voltage 2078
I (12326) BATTERY_EXAMPLE: got voltage 2078
I (16326) BATTERY_EXAMPLE: got voltage 2077
I (20326) BATTERY_EXAMPLE: got voltage 2078
I (24326) BATTERY_EXAMPLE: got voltage 2078
I (28326) BATTERY_EXAMPLE: got voltage 2078
I (32326) BATTERY_EXAMPLE: got voltage 2078
I (36326) BATTERY_EXAMPLE: got voltage 2078
I (40326) BATTERY_EXAMPLE: got voltage 2077
I (44326) BATTERY_EXAMPLE: got voltage 2076
I (48326) BATTERY_EXAMPLE: got voltage 2078
I (52326) BATTERY_EXAMPLE: got voltage 2077
I (56326) BATTERY_EXAMPLE: got voltage 2078
I (60326) BATTERY_EXAMPLE: got voltage 2078
I (60336) BATTERY_EXAMPLE: [1.3] change battery voltage report freqency
I (62326) BATTERY_EXAMPLE: got voltage 2077
I (64326) BATTERY_EXAMPLE: got voltage 2078
I (66326) BATTERY_EXAMPLE: got voltage 2078
I (68326) BATTERY_EXAMPLE: got voltage 2078
I (70326) BATTERY_EXAMPLE: got voltage 2078
I (72326) BATTERY_EXAMPLE: got voltage 2077
I (74326) BATTERY_EXAMPLE: got voltage 2077
I (76326) BATTERY_EXAMPLE: got voltage 2077
I (78326) BATTERY_EXAMPLE: got voltage 2078
I (80326) BATTERY_EXAMPLE: got voltage 2078
I (82326) BATTERY_EXAMPLE: got voltage 2078
I (84326) BATTERY_EXAMPLE: got voltage 2078
I (86326) BATTERY_EXAMPLE: got voltage 2077
I (88326) BATTERY_EXAMPLE: got voltage 2078
I (90326) BATTERY_EXAMPLE: got voltage 2078
I (92326) BATTERY_EXAMPLE: got voltage 2078
I (94326) BATTERY_EXAMPLE: got voltage 2078
I (96326) BATTERY_EXAMPLE: got voltage 2078
I (98326) BATTERY_EXAMPLE: got voltage 2077
I (100326) BATTERY_EXAMPLE: got voltage 2079
I (102326) BATTERY_EXAMPLE: got voltage 2078
I (104326) BATTERY_EXAMPLE: got voltage 2078
I (106326) BATTERY_EXAMPLE: got voltage 2078
I (108326) BATTERY_EXAMPLE: got voltage 2077
I (110326) BATTERY_EXAMPLE: got voltage 2077
I (112326) BATTERY_EXAMPLE: got voltage 2077
I (114326) BATTERY_EXAMPLE: got voltage 2078
I (116326) BATTERY_EXAMPLE: got voltage 2077
I (118326) BATTERY_EXAMPLE: got voltage 2077
I (120326) BATTERY_EXAMPLE: got voltage 2077
I (120336) BATTERY_EXAMPLE: [2.0] stop battery voltage report
I (120336) BATTERY_EXAMPLE: [2.1] destroy battery service
I (120336) BATTERY_SERVICE: battery service destroyed
I (120336) VOL_MONITOR: vol monitor destroyed

```


## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
