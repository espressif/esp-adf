# 使用封装器操作 NVS 的例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

ADF 在外部 PSRAM 中创建和使用了较多堆栈任务，任务运行时候禁止进行 flash 操作。

此示例演示了 ADF 此类任务如何使用 `esp_dispatcher` 和 `nvs_action` 两种方式进行 NVS 的写入和读取的操作。


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

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v4.2/esp32/index.html)。

## 如何使用例程

### 功能和用法

例程开始运行后，首先在 NVS 中写入字符串 “Hello, ESP dispatcher!”， 然后从 NVS 读取该字符串并打印出来，代码延迟一段时间后，再写入一个 `uint8_t` 类型的数，然后再读取出来，打印如下：

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 15:47:53
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 2MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00100000
I (90) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0c800 ( 51200) map
I (131) esp_image: segment 1: paddr=0x0001c828 vaddr=0x3ffb0000 size=0x02114 (  8468) load
I (135) esp_image: segment 2: paddr=0x0001e944 vaddr=0x40080000 size=0x016d4 (  5844) load
0x40080000: _WindowOverflow4 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (141) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x1eb24 (125732) map
0x400d0020: _stext at ??:?

I (199) esp_image: segment 4: paddr=0x0003eb4c vaddr=0x400816d4 size=0x0ce1c ( 52764) load
0x400816d4: heap_caps_realloc at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/heap/heap_caps.c:345 (discriminator 1)

I (233) boot: Loaded app from partition at offset 0x10000
I (233) boot: Disabling RNG early entropy source...
I (233) psram: This chip is ESP32-D0WD
I (239) spiram: Found 64MBit SPI RAM device
I (243) spiram: SPI RAM mode: flash 40m sram 40m
I (248) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (255) cpu_start: Pro cpu up.
I (259) cpu_start: Application information:
I (264) cpu_start: Project name:     nvs_dispatcher_example
I (270) cpu_start: App version:      v2.2-210-g86396c1f-dirty
I (276) cpu_start: Compile time:     Nov  3 2021 15:47:45
I (282) cpu_start: ELF file SHA256:  c7db10882e76faf0...
I (288) cpu_start: ESP-IDF:          v4.2.2
I (293) cpu_start: Starting app cpu, entry point is 0x40081a68
0x40081a68: call_start_cpu1 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1188) spiram: SPI SRAM memory test OK
I (1188) heap_init: Initializing. RAM available for dynamic allocation:
I (1188) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1194) heap_init: At 3FFB2B78 len 0002D488 (181 KiB): DRAM
I (1201) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1207) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1214) heap_init: At 4008E4F0 len 00011B10 (70 KiB): IRAM
I (1220) cpu_start: Pro cpu start user code
I (1225) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1246) spi_flash: detected chip: gd
I (1247) spi_flash: flash io: dio
W (1247) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1257) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1268) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
E (1308) DISPATCHER: exe first list: 0x0
I (1308) DISPATCHER: dispatcher_event_task is running...
I (1308) DISPATCHER_EXAMPLE: Task create

I (1308) DISPATCHER_EXAMPLE: Sync invoke begin
I (1318) DISPATCHER_EXAMPLE: Write Hello, ESP dispatcher! to NVS ...
I (1328) DISPATCHER_EXAMPLE: Operation Done
I (1328) DISPATCHER_EXAMPLE: Committing updates in NVS ...
I (1338) DISPATCHER_EXAMPLE: Operation Done
I (1338) DISPATCHER_EXAMPLE: Sync invoke end with 0

I (1348) DISPATCHER_EXAMPLE: Async invoke begin
I (1348) DISPATCHER_EXAMPLE: async invoke end

I (1348) DISPATCHER_EXAMPLE: Read from NVS ...
I (1358) DISPATCHER_EXAMPLE: Operation Done
I (1368) DISPATCHER_EXAMPLE: Read result: Hello, ESP dispatcher!
I (3358) DISPATCHER_EXAMPLE: Write and Read NVS with dispatcher and actions.
I (3358) DISPATCHER_EXAMPLE: Open
I (3358) DISPATCHER_EXAMPLE: Set a u8 number
I (3368) DISPATCHER_EXAMPLE: Commit
I (3368) DISPATCHER_EXAMPLE: Read from nvs
I (3368) DISPATCHER_EXAMPLE: Read 171 from nvs
I (3378) DISPATCHER_EXAMPLE: Close
W (3378) DISPATCHER_EXAMPLE: Task exit

```

### 日志输出
以下为本例程的完整日志。

```c
entry 0x400806f4
I (27) boot: ESP-IDF v4.2.2 2nd stage bootloader
I (27) boot: compile time 15:47:53
I (27) boot: chip revision: 3
I (30) boot_comm: chip revision: 3, min. bootloader chip revision: 0
I (37) boot.esp32: SPI Speed      : 40MHz
I (42) boot.esp32: SPI Mode       : DIO
I (46) boot.esp32: SPI Flash Size : 2MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (75) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (82) boot:  2 factory          factory app      00 00 00010000 00100000
I (90) boot: End of partition table
I (94) boot_comm: chip revision: 3, min. application chip revision: 0
I (101) esp_image: segment 0: paddr=0x00010020 vaddr=0x3f400020 size=0x0c800 ( 51200) map
I (131) esp_image: segment 1: paddr=0x0001c828 vaddr=0x3ffb0000 size=0x02114 (  8468) load
I (135) esp_image: segment 2: paddr=0x0001e944 vaddr=0x40080000 size=0x016d4 (  5844) load
0x40080000: _WindowOverflow4 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/freertos/xtensa/xtensa_vectors.S:1730

I (141) esp_image: segment 3: paddr=0x00020020 vaddr=0x400d0020 size=0x1eb24 (125732) map
0x400d0020: _stext at ??:?

I (199) esp_image: segment 4: paddr=0x0003eb4c vaddr=0x400816d4 size=0x0ce1c ( 52764) load
0x400816d4: heap_caps_realloc at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/heap/heap_caps.c:345 (discriminator 1)

I (233) boot: Loaded app from partition at offset 0x10000
I (233) boot: Disabling RNG early entropy source...
I (233) psram: This chip is ESP32-D0WD
I (239) spiram: Found 64MBit SPI RAM device
I (243) spiram: SPI RAM mode: flash 40m sram 40m
I (248) spiram: PSRAM initialized, cache is in low/high (2-core) mode.
I (255) cpu_start: Pro cpu up.
I (259) cpu_start: Application information:
I (264) cpu_start: Project name:     nvs_dispatcher_example
I (270) cpu_start: App version:      v2.2-210-g86396c1f-dirty
I (276) cpu_start: Compile time:     Nov  3 2021 15:47:45
I (282) cpu_start: ELF file SHA256:  c7db10882e76faf0...
I (288) cpu_start: ESP-IDF:          v4.2.2
I (293) cpu_start: Starting app cpu, entry point is 0x40081a68
0x40081a68: call_start_cpu1 at /hengyongchao/audio/esp-idfs/esp-idf-v4.2.2/components/esp32/cpu_start.c:287

I (0) cpu_start: App cpu up.
I (1188) spiram: SPI SRAM memory test OK
I (1188) heap_init: Initializing. RAM available for dynamic allocation:
I (1188) heap_init: At 3FFAE6E0 len 00001920 (6 KiB): DRAM
I (1194) heap_init: At 3FFB2B78 len 0002D488 (181 KiB): DRAM
I (1201) heap_init: At 3FFE0440 len 00003AE0 (14 KiB): D/IRAM
I (1207) heap_init: At 3FFE4350 len 0001BCB0 (111 KiB): D/IRAM
I (1214) heap_init: At 4008E4F0 len 00011B10 (70 KiB): IRAM
I (1220) cpu_start: Pro cpu start user code
I (1225) spiram: Adding pool of 4096K of external SPI memory to heap allocator
I (1246) spi_flash: detected chip: gd
I (1247) spi_flash: flash io: dio
W (1247) spi_flash: Detected size(8192k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1257) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (1268) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
E (1308) DISPATCHER: exe first list: 0x0
I (1308) DISPATCHER: dispatcher_event_task is running...
I (1308) DISPATCHER_EXAMPLE: Task create

I (1308) DISPATCHER_EXAMPLE: Sync invoke begin
I (1318) DISPATCHER_EXAMPLE: Write Hello, ESP dispatcher! to NVS ...
I (1328) DISPATCHER_EXAMPLE: Operation Done
I (1328) DISPATCHER_EXAMPLE: Committing updates in NVS ...
I (1338) DISPATCHER_EXAMPLE: Operation Done
I (1338) DISPATCHER_EXAMPLE: Sync invoke end with 0

I (1348) DISPATCHER_EXAMPLE: Async invoke begin
I (1348) DISPATCHER_EXAMPLE: async invoke end

I (1348) DISPATCHER_EXAMPLE: Read from NVS ...
I (1358) DISPATCHER_EXAMPLE: Operation Done
I (1368) DISPATCHER_EXAMPLE: Read result: Hello, ESP dispatcher!
I (3358) DISPATCHER_EXAMPLE: Write and Read NVS with dispatcher and actions.
I (3358) DISPATCHER_EXAMPLE: Open
I (3358) DISPATCHER_EXAMPLE: Set a u8 number
I (3368) DISPATCHER_EXAMPLE: Commit
I (3368) DISPATCHER_EXAMPLE: Read from nvs
I (3368) DISPATCHER_EXAMPLE: Read 171 from nvs
I (3378) DISPATCHER_EXAMPLE: Close
W (3378) DISPATCHER_EXAMPLE: Task exit

```

## 技术支持
请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
