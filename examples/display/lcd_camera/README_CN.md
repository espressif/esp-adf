# LCD 摄像头回显示例
- [English Version](./README.md)
- 例程难度：![alt text](./_static/level_basic.png "初级")

## 例程简介

本例程演示调用 esp32-camera API 获取一张图片， 最终显示在 LCD 上。示例分辨率为 320 * 240。例程中有两种测试模式
- 从 camera 中获取 YUV422 格式的图片， 然后转换成 RGB565 格式图片并显示在 LCD 上
[camera YUV422] ->  [color converted to RGB565] -> [display on LCD]
- 从 camera 中获取 RGB565 格式的图片并显示在 LCD 上
[camera RGB565] -> [display on LCD]

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中 [例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性) 中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

- 本例程测试的摄像头型号为 OV3660，其他型号请参考 [esp32-camera](https://github.com/espressif/esp32-camera)。

## 编译和下载


### IDF 默认分支

本例程支持 IDF release/v4.4 及以后的分支，例程默认使用 IDF release/v4.4 分支。

### IDF 分支

- IDF release/v4.4 分支切换命令如下：

  ```
  cd $IDF_PATH
  git checkout master
  git pull
  git checkout release/v4.4
  git submodule update --init --recursive
  ```

### 配置

本例程默认选择的开发板是 `ESP32-S3-Korvo-2`。

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)，并在页面左上角选择芯片和版本，查看对应的文档。

## 如何使用例程

### 功能和用法

- 例程运行时可在显示屏上看到摄像头对准的物体。

### 日志输出

以下是本例程的完整日志。

```c
I (24) boot: ESP-IDF v4.4.3-173-g1f63dc70b8-dirty 2nd stage bootloader
I (25) boot: compile time 16:41:13
I (25) boot: chip revision: 0
I (28) boot.esp32s3: Boot SPI Speed : 80MHz
I (33) boot.esp32s3: SPI Mode       : DIO
I (38) boot.esp32s3: SPI Flash Size : 2MB
I (42) boot: Enabling RNG early entropy source...
I (48) boot: Partition Table:
I (51) boot: ## Label            Usage          Type ST Offset   Length
I (59) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (66) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (74) boot:  2 factory          factory app      00 00 00010000 00100000
I (81) boot: End of partition table
I (85) esp_image: segment 0: paddr=00010020 vaddr=3c030020 size=0f900h ( 63744) map
I (105) esp_image: segment 1: paddr=0001f928 vaddr=3fc98cf0 size=006f0h (  1776) load
I (106) esp_image: segment 2: paddr=00020020 vaddr=42000020 size=29680h (169600) map
I (141) esp_image: segment 3: paddr=000496a8 vaddr=3fc993e0 size=04324h ( 17188) load
I (145) esp_image: segment 4: paddr=0004d9d4 vaddr=40374000 size=14ce4h ( 85220) load
I (174) boot: Loaded app from partition at offset 0x10000
I (174) boot: Disabling RNG early entropy source...
I (186) opi psram: vendor id : 0x0d (AP)
I (186) opi psram: dev id    : 0x02 (generation 3)
I (186) opi psram: density   : 0x03 (64 Mbit)
I (190) opi psram: good-die  : 0x01 (Pass)
I (194) opi psram: Latency   : 0x01 (Fixed)
I (199) opi psram: VCC       : 0x01 (3V)
I (204) opi psram: SRF       : 0x01 (Fast Refresh)
I (209) opi psram: BurstType : 0x01 (Hybrid Wrap)
I (215) opi psram: BurstLen  : 0x01 (32 Byte)
I (220) opi psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (226) opi psram: DriveStrength: 0x00 (1/1)
W (231) PSRAM: DO NOT USE FOR MASS PRODUCTION! Timing parameters will be updated in future IDF version.
I (241) spiram: Found 64MBit SPI RAM device
I (246) spiram: SPI RAM mode: sram 80m
I (250) spiram: PSRAM initialized, cache is in normal (1-core) mode.
I (257) cpu_start: Pro cpu up.
I (261) cpu_start: Starting app cpu, entry point is 0x40375794
0x40375794: call_start_cpu1 at /home/xutao/workspace/esp-adf-internal-dev/esp-idf/components/esp_system/port/cpu_start.c:148

I (0) cpu_start: App cpu up.
I (687) spiram: SPI SRAM memory test OK
I (696) cpu_start: Pro cpu start user code
I (696) cpu_start: cpu freq: 240000000
I (696) cpu_start: Application information:
I (699) cpu_start: Project name:     lcd_camera
I (704) cpu_start: App version:      v2.4-153-g28bae448-dirty
I (710) cpu_start: Compile time:     Dec  1 2022 11:24:51
I (716) cpu_start: ELF file SHA256:  f212c71453f16740...
I (722) cpu_start: ESP-IDF:          v4.4.3-173-g1f63dc70b8-dirty
I (729) heap_init: Initializing. RAM available for dynamic allocation:
I (737) heap_init: At 3FC9EA58 len 000315A8 (197 KiB): D/IRAM
I (743) heap_init: At 3FCD8000 len 00011710 (69 KiB): D/IRAM
I (749) heap_init: At 3FCE9710 len 00005724 (21 KiB): STACK/DRAM
I (756) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (762) heap_init: At 600FE000 len 00002000 (8 KiB): RTCRAM
I (768) spiram: Adding pool of 8192K of external SPI memory to heap allocator
I (1015) spi_flash: detected chip: gd
I (1016) spi_flash: flash io: dio
W (1016) spi_flash: Detected size(16384k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (1027) sleep: Configure to isolate all GPIO pins in sleep state
I (1033) sleep: Enable automatic switching of GPIO sleep configuration
I (1040) esp_apptrace: Initialized TRAX on CPU0
I (1046) cpu_start: Starting scheduler on PRO CPU.
I (498) esp_apptrace: Initialized TRAX on CPU1
I (0) cpu_start: Starting scheduler on APP CPU.
I (1071) spiram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1081) TCA9554: Detected IO expander device at 0x70, name is: TCA9554A
I (1111) gpio: GPIO[2]| InputEn: 0| OutputEn: 1| OpenDrain: 0| Pullup: 0| Pulldown: 0| Intr:0
I (1511) s3 ll_cam: DMA Channel=4
I (1511) cam_hal: cam init ok
I (1511) sccb: pin_sda 17 pin_scl 18
I (1521) camera: Detected camera at address=0x3c
I (1521) camera: Detected OV3660 camera
I (1521) camera: Camera PID=0x3660 VER=0x00 MIDL=0x00 MIDH=0x00
I (1851) s3 ll_cam: node_size: 3840, nodes_per_line: 1, lines_per_node: 6
I (1851) s3 ll_cam: dma_half_buffer_min:  3840, dma_half_buffer: 15360, lines_per_half_buffer: 24, dma_buffer_size: 30720
I (1851) cam_hal: buffer_size: 30720, half_buffer_size: 15360, node_buffer_size: 3840, node_cnt: 8, total_cnt: 10
I (1861) cam_hal: Allocating 153600 Byte frame buffer in PSRAM
I (1881) cam_hal: cam config ok
I (1891) ov3660: Calculated VCO: 320000000 Hz, PLLCLK: 160000000 Hz, SYSCLK: 40000000 Hz, PCLK: 20000000 Hz
I (1931) LCD_Camera: Taking picture...
I (2071) LCD_Camera: Picture taken! The size was: 153600 bytes, w:320, h:240
I (2111) LCD_Camera: Func:app_main, Line:128, MEM Total:8303867 Bytes, Inter:229183 Bytes, Dram:229183 Bytes

I (2121) LCD_Camera: Taking picture...
I (2161) LCD_Camera: Picture taken! The size was: 153600 bytes, w:320, h:240
I (2201) LCD_Camera: Func:app_main, Line:128, MEM Total:8303867 Bytes, Inter:229183 Bytes, Dram:229183 Bytes

I (2211) LCD_Camera: Taking picture...
I (2251) LCD_Camera: Picture taken! The size was: 153600 bytes, w:320, h:240
I (2291) LCD_Camera: Func:app_main, Line:128, MEM Total:8303867 Bytes, Inter:229183 Bytes, Dram:229183 Bytes

I (2301) LCD_Camera: Taking picture...
I (2341) LCD_Camera: Picture taken! The size was: 153600 bytes, w:320, h:240
I (2381) LCD_Camera: Func:app_main, Line:128, MEM Total:8303867 Bytes, Inter:229183 Bytes, Dram:229183 Bytes
```

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
