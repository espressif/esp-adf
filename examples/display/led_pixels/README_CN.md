# LED 像素显示示例
- [English Version](./README.md)
- 例程难度：![alt text](./_static/level_basic.png "初级")

## 例程简介

例程演示了通过 ESP32-C3 ADC 获取音频数据，经过 FFT 和能量计算后输出到 LED 像素矩阵进行显示的过程。例程中实现了部分样式，可在 main.c 用 cnv_set_cur_pattern 进行配置。LED 驱动支持了 WS2812，可使用 SPI、RMT 输出。

### 例程流程

 1. 通过 `SRC_DRV` 获取源数据。例程使用内部 12 位 ADC，MIC 与音源需要保持较近距离。
 2. `CONVERT` 组件会使用在初始化时注册的 source_data_cb 获取源数据。
 3. `CONVERT` 组件会调用在初始化时注册的 PATTERN 回调函数进行 LED 像素的映射。
 4. `CONVERT` 会将映射之后的 LED 数据发到 `PIXEL_RENDERER` 组件。
 5. `PIXEL_RENDERER` 组件根据接收到的数据将 LED 像素点设置为对应的颜色。

### 资源列表

使能 1000 颗灯珠的帧率为 30 fps。

未初始化 FFT 的内存消耗：

| memory_total (byte) |
|---------------------|
| 17840               |

初始化 FFT 的内存消耗：

| memory_total (byte) |
|---------------------|
| 48072               |

### 预备知识

- 外设：SPI / RMT、ADC。
- 算法：FFT。

## LED 像素坐标的建立
LED 像素坐标的建立需要在 `PIXEL_RENDERER` 组件中完成。此组件有两种数据格式可发送（COORD_RGB：包含坐标与 RGB 数据；ONLY_RGB：只包含 RGB 数据）。

### 建立坐标系的三要素
 - x_axis_points，X 轴上 LED 的数量。
 - y_axis_points，Y 轴上 LED 的数量。
 - origin_offset，原点偏移。

### LED 矩阵排列方式
LED 矩阵有多种排列方式，下方列举三种 6 X 7 的 LED 矩阵排列方式，即 y_axis_points 为 6，x_axis_points 为 7。（例程默认以排列一方式建立坐标系）
 - 排列一的坐标原点为第 6 颗灯珠，led_index 为 5，origin_offset 为 5；坐标 (1,0) 为第 ７ 颗灯珠，led_index 为 ６。
 - 排列二的坐标原点为第 1 颗灯珠，led_index 为 0，origin_offset 为 0；坐标 (1,0) 为第 12 颗灯珠，需重定义像素坐标映射。
 - 排列三的坐标原点为第 1 颗灯珠，led_index 为 0，origin_offset 为 0；坐标 (1,0) 为第 ７ 颗灯珠，需重定义像素坐标映射。

```
符号 ｀*｀ 代表 LED 灯珠
符号 `|` 与 `-` 代表电路
下方排列认为左下角灯珠为坐标原点。

排列一：６ X 7
  ->-*   *---*   *---*   *---*
     |   |   |   |   |   |   |
     *   *   *   *   *   *   *
     |   |   |   |   |   |   |
     *   *   *   *   *   *   *
     |   |   |   |   |   |   |
     *   *   *   *   *   *   *
     |   |   |   |   |   |   |
     *   *   *   *   *   *   *
     |   |   |   |   |   |   |
     *---*   *---*   *---*   *->-

排列二：６ X 7
     *---*   *---*   *---*   *->-
     |   |   |   |   |   |   |
     *   *   *   *   *   *   *
     |   |   |   |   |   |   |
     *   *   *   *   *   *   *
     |   |   |   |   |   |   |
     *   *   *   *   *   *   *
     |   |   |   |   |   |   |
     *   *   *   *   *   *   *
     |   |   |   |   |   |   |
  ->-*   *---*   *---*   *---*

排列三：６ X 7
     *---  *---  *---  *---  *---  *---  *->-
     |  |  |  |  |  |  |  |  |  |  |  |  |
     *  |  *  |  *  |  *  |  *  |  *  |  *
     |  |  |  |  |  |  |  |  |  |  |  |  |
     *  |  *  |  *  |  *  |  *  |  *  |  *
     |  |  |  |  |  |  |  |  |  |  |  |  |
     *  |  *  |  *  |  *  |  *  |  *  |  *
     |  |  |  |  |  |  |  |  |  |  |  |  |
     *  |  *  |  *  |  *  |  *  |  *  |  *
     |  |  |  |  |  |  |  |  |  |  |  |  |
  ->-*  ---*  ---*  ---*  ---*  ---*  ---*

```

### 文件结构

```
├── components
│   |── source_drv
│   |   ├── include
│   |   │   └── src_drv_adc.h              <!-- ADC 头文件，负责获取 ADC 源数据 -->
│   |   ├── src_drv_adc.c
│   |   ├── CMakeLists.txt
│   |   └── component.mk
│   |── convert
│   |   ├── cnv_audio
│   |   │   └── cnv_audio.h                <!-- 音频相关，例如：声强/音量计算 -->
│   |   ├── cnv_basic
│   |   │   ├── cnv_debug.h
│   |   │   └── cnv_fft.h                  <!-- FFT 相关，例如：频率计算 -->
│   |   ├── include
│   |   │   ├── cnv_pattern.h              <!-- 灯光样式相关，可自定义源数据 － LED 像素点的映射关系 -->
│   |   │   └── cnv.h                      <!-- CNV 头文件，负责绘制 LED 像素帧 -->
│   |   ├── lib
│   |   │   └── esp32c3
|   │   |       └── libconvert.a
│   |   ├── cnv_pattern.c
│   |   ├── cnv.c
│   |   ├── CMakeLists.txt
│   |   └── component.mk
│   ├── pixel_renderer
│   │   ├── led_driver
│   │   │   ├── ws2812_rmt
|   │   |   |   ├── ws2812_rmt.c
|   │   |   |   └── ws2812_rmt.h           <!-- WS2812 灯珠的 RMT 驱动 -->
│   │   │   └── ws2812_spi
|   │   |       ├── ws2812_spi.c
|   │   |       └── ws2812_spi.h           <!-- WS2812 灯珠的 SPI 驱动 -->
│   │   ├── include
│   │   │   |── pixel_coord.h              <!-- PIXEL 坐标转换为 index -->
│   │   │   |── pixel_dev.h
│   │   │   └── pixel.h                    <!-- PIXEL 头文件，负责控制 LED -->
│   │   ├── pixel_coord.c
│   │   ├── pixel_dev.c
│   │   ├── pixel.c
│   │   ├── CMakeLists.txt
│   │   └── component.mk
│   └── utilis
│       ├── CMakeLists.txt
│       ├── esp_color.c
│       └── esp_color.h                    <!-- 色彩相关，例如：RGB -> HSV / HSV -> RGB -->
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   ├── component.mk
│   ├── Kconfig.projbuild
│   ├── idf_component.yml
│   └── main.c
├── CMakeLists.txt
└── README_CN.md
```

## 环境配置

### 硬件要求

本例程支持的开发板在 `$ADF_PATH/examples/README_CN.md` 文档中 [例程与乐鑫音频开发板的兼容性表格](../../README_CN.md#例程与乐鑫音频开发板的兼容性) 中有标注，表格中标有绿色复选框的开发板均可运行本例程。请记住，如下面的 [配置](#配置) 一节所述，可以在 `menuconfig` 中选择开发板。

- ESP32-C3-Lyra-v2.0 开发板、WS2812 灯带。
- 此例程选择 `ADC1_CHANNEL_0` 采集 ADC 数据。
- 此例程选择 `GPIO_NUM_7` 发送 WS2812 命令，即 JP2 接口的 GPIO7 连接 LED 的 data 引脚。

## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v4.4 分支，例程默认使用 IDF release/v4.4 分支。

### 配置

本例程默认选择的开发板是 `ESP32-C3-Lyra-v2.0`。

```
menuconfig > Audio HAL > ESP32-C3-Lyra-v2.0
```

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出（替换 PORT 为端口名称）：

```
idf.py set-target esp32c3
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)，并在页面左上角选择芯片和版本，查看对应的文档。

## 如何使用例程

以下配置都可以使用 `menuconfig` 配置。
### cnv.h 中可更改配置
```
#define   CNV_AUDIO_SAMPLE          (CONFIG_EXAMPLE_AUDIO_SAMPLE)        /*!< 音频采样率，默认为 16000 */
#define   CNV_N_SAMPLES             (CONFIG_EXAMPLE_N_SAMPLE)            /*!< FFT 采样点数，默认为 256 */
#define   CNV_MIN_SOUND_ENERGY      (CONFIG_EXAMPLE_MIN_SOUND_ENERGY)    /*!< 最小声音能量阈值，低于此值被认定为静音环境，默认 10000000 */
#define   CNV_TOTAL_LEDS            (CONFIG_EXAMPLE_TOTAL_LEDS)          /*!< LED 总数，默认 30 */
#define   CNV_SOURCE_DATA_DEBUG     (CONFIG_EXAMPLE_SOURCE_DATA_DEBUG)   /*!< 原始数据显示 >!*/
#define   CNV_FFT_DEBUG             (CONFIG_EXAMPLE_FFT_DEBUG)           /*!< FFT数据显示 >!*/
```

### pixel_renderer.h 中可更改配置
```
#define   PIXEL_RENDERER_TX_CHANNEL (CONFIG_EXAMPLE_LED_TX_CHANNEL)      /*!< 通道，ESP32C3 默认使用 CHANNEL_0 */
#define   PIXEL_RENDERER_CTRL_IO    (CONFIG_EXAMPLE_LED_CTRL_IO)         /*!< 信号控制引脚，ESP32C3 默认使用 GPIO_NUM_7 */
```

### 日志输出

以下是本例程的完整日志。

```c
--- idf_monitor on /dev/ttyUSB0 115200 ---
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---
�ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x1 (POWERON),boot:0xd (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd6100,len:0x16cc
load:0x403ce000,len:0x930
load:0x403d0000,len:0x2d28
entry 0x403ce000
I (30) boot: ESP-IDF v4.4.1-149-g009e66e6252-dirty 2nd stage bootloader
I (30) boot: compile time 17:40:50
I (30) boot: chip revision: 3
I (34) boot.esp32c3: SPI Speed      : 80MHz
I (39) boot.esp32c3: SPI Mode       : DIO
I (43) boot.esp32c3: SPI Flash Size : 2MB
I (48) boot: Enabling RNG early entropy source...
I (53) boot: Partition Table:
I (57) boot: ## Label            Usage          Type ST Offset   Length
I (64) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (72) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (79) boot:  2 factory          factory app      00 00 00010000 00100000
I (87) boot: End of partition table
I (91) esp_image: segment 0: paddr=00010020 vaddr=3c020020 size=0e460h ( 58464) map
I (109) esp_image: segment 1: paddr=0001e488 vaddr=3fc8bc00 size=01530h (  5424) load
I (110) esp_image: segment 2: paddr=0001f9c0 vaddr=40380000 size=00658h (  1624) load
I (117) esp_image: segment 3: paddr=00020020 vaddr=42000020 size=1c694h (116372) map
I (143) esp_image: segment 4: paddr=0003c6bc vaddr=40380658 size=0b3a8h ( 45992) load
I (152) esp_image: segment 5: paddr=00047a6c vaddr=50000010 size=00010h (    16) load
I (155) boot: Loaded app from partition at offset 0x10000
I (156) boot: Disabling RNG early entropy source...
I (172) cpu_start: Pro cpu up.
I (180) cpu_start: Pro cpu start user code
I (180) cpu_start: cpu freq: 160000000
I (180) cpu_start: Application information:
I (183) cpu_start: Project name:     led_pixels
I (188) cpu_start: App version:      v2.4-53-g031d238c-dirty
I (194) cpu_start: Compile time:     Jun 29 2022 17:40:46
I (200) cpu_start: ELF file SHA256:  9f1695a0b854d493...
I (206) cpu_start: ESP-IDF:          v4.4.1-149-g009e66e6252-dirty
I (213) heap_init: Initializing. RAM available for dynamic allocation:
I (220) heap_init: At 3FC8E440 len 00031BC0 (198 KiB): DRAM
I (227) heap_init: At 3FCC0000 len 0001F060 (124 KiB): STACK/DRAM
I (233) heap_init: At 50000020 len 00001FE0 (7 KiB): RTCRAM
I (240) spi_flash: detected chip: generic
I (245) spi_flash: flash io: dio
W (248) spi_flash: Detected size(4096k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (262) sleep: Configure to isolate all GPIO pins in sleep state
I (268) sleep: Enable automatic switching of GPIO sleep configuration
I (276) cpu_start: Starting scheduler.
I (280) WS2812: MEM Total:321096 Bytes
W (285) CONVERT: The RGB color array is not set, Please use API 'bottom_color_array_set' Settings
I (410) CONVERT: Percentage of sound intensity: 0 %
I (424) CONVERT: Percentage of sound intensity: 0 %
I (437) CONVERT: Percentage of sound intensity: 0 %
```

## 故障排除

- 数据的流向：SRC_DRV --> CONVERT --> PIXEL_RENDERER。
- 源数据输入检测：DEBUG 原始数据（例如：ADC 能否录取正常的 1KHz / 2KHz 等音频），可在 `menuconfig` 中开启 `SOURCE_DATA_DEBUG` 打印 `source_data[]`。
- FFT检测：可在 `menuconfig` 中开启 `FFT_DEBUG` 获取 FFT 操作之后的数据。（注意：频率计算公式：采样率 * X 轴坐标 / 采样点数 = 频率）
- LED驱动检测：确认 SPI / RMT 引脚配置正确并能生成正确的 WS2812 命令。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。