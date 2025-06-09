# ESP32S3 双屏显示方案

## 目录
- [ESP32S3 双屏显示方案](#esp32s3-双屏显示方案)
  - [目录](#目录)
  - [方案概述](#方案概述)
  - [技术规格](#技术规格)
    - [屏幕参数与目标帧率](#屏幕参数与目标帧率)
    - [硬件连接](#硬件连接)
  - [方案分析](#方案分析)
    - [大分辨率屏幕方案](#大分辨率屏幕方案)
      - [性能需求分析](#性能需求分析)
      - [带宽计算](#带宽计算)
      - [渲染方案](#渲染方案)
    - [小分辨率屏幕方案](#小分辨率屏幕方案)
      - [性能需求分析](#性能需求分析-1)
      - [带宽计算](#带宽计算-1)
      - [渲染方案](#渲染方案-1)
  - [常见问题与解决方案](#常见问题与解决方案)
    - [显示撕裂问题](#显示撕裂问题)
    - [双屏同步显示](#双屏同步显示)
    - [性能优化建议](#性能优化建议)
  - [性能优化配置](#性能优化配置)

## 方案概述

本文档主要介绍 ESP32S3 双屏显示方案，支持双屏同显/异显模式，适用于不同分辨率的显示需求，并针对需要进行优化。

## 技术规格

### 屏幕参数与目标帧率
- 屏幕分辨率：240×240（大屏）/ 128×128（小屏）
- 颜色格式：RGB565（16位色深）
- 接口类型：SPI/QSPI
- 目标帧率：20fps（单屏）

### 硬件连接

为了减少引脚使用，双屏复用同一路 SPI 总线 (MOSI、PCLK、D/C、RST、BCLK 引脚共享，每个屏幕独立 CS 引脚)。

```
    ESP Board                     LCD Screen1                     LCD Screen2
+---------------+              +----------------+              +----------------+
|               |              |                |              |                |
|           3V3 +------------->| VCC            |------------->| VCC            |
|               |              |                |              |                |
|           GND +------------->| GND            |------------->| GND            |
|               |              |                |              |                |
| MOSI(DATA[0]) +------------->| MOSI           |------------->| MOSI           |
|               |              |                |              |                |
|          PCLK +------------->| PCLK           |------------->| PCLK           |
|               |              |                |              |                |
|           D/C +------------->| D/C            |------------->| D/C            |
|               |              |                |              |                |
|           RST +------------->| RST            |------------->| RST            |
|               |              |                |              |                |
|      BK_LIGHT +------------->| BCKL           |------------->| BLCK           |
|               |              |                |              |                |
|           CS1 +------------->| CS             |              |                |
|               |              +----------------+              |                |
|           CS2 +--------------------------------------------->| CS             |
|               |                                              |                |
+---------------+                                              +----------------+
```

## 方案分析

### 大分辨率屏幕方案

#### 性能需求分析
- 屏幕分辨率：240×240 及以上
- 双屏异显总帧率：40fps（25ms/帧），防撕裂需要达到 60fps 及以上
- 单帧数据量：约 15KB
- 单帧数据读取延迟：假定 4ms，越短越好

#### 带宽计算
1. SPI 总线需求：
   - 基础需求：240×240×16×40×1.2 = 45Mbps
   - 防撕裂需求：240×240×16×60×1.2 = 66Mbps
   - 推荐配置：SPI 80MHz 或 QSPI 20MHz
> **部分屏幕无法支持 80M CLK，需要查看数据手册进一步确认**

2. 存储访问需求：
   - 带宽需求：15KB/4ms = 3.75MB/s
   - 推荐方案：
     - 使用 SDIO 40MHz 4-line 接口
     - 小文件直接加载到 PSRAM 后读取
     - 使用 MMAP 读取 Flash 数据

#### 渲染方案
- 推荐使用 AVI 播放方案
  - 支持 320×240 分辨率下 66fps 解码
  - 基于 JPEG 解码优化
  - 依赖组件：
    - [AVI Player](https://components.espressif.com/components/espressif/avi_player/versions/1.0.0)
    - [ESP New JPEG](https://components.espressif.com/components/espressif/esp_new_jpeg/versions/0.6.1)

### 小分辨率屏幕方案

#### 性能需求分析
- 屏幕分辨率：240×240 以下，如 128×128 等
- 双屏异显总帧率：40fps（25ms/帧）
- 单帧数据量：约 5KB
- 单帧数据读取延迟：假定 4ms，越短越好

#### 带宽计算
1. SPI 总线需求：
   - 基础需求：128×128×16×40×1.2 = 13Mbps
   - 防撕裂需求：128×128×16×60×1.2 = 19Mbps
   - 推荐配置：SPI 20MHz 或 SPI 40MHz

2. 存储访问需求：
   - 带宽需求：5KB/4ms = 1.25MB/s
   - 推荐方案：一般文件较小，可拷贝到 PSRAM 进行显示

#### 渲染方案
- 如需结合 LVGL 叠加图层，则可以使用 GIF 解码
- 仍推荐使用 JPEG 解码（可封装成 AVI 格式）

## 常见问题与解决方案

### 显示撕裂问题
- **硬件方案**：使用屏幕 TE 信号实现防撕裂
  - 参考文档：[LCD Screen Tearing](https://docs.espressif.com/projects/esp-iot-solution/zh_CN/latest/display/lcd/lcd_screen_tearing.html#te)
- **软件方案（不推荐）**：模拟 TE 信号
  - 使用高精度硬件定时器输出 60fps 同步信号
  - 修改初始偏移时间，调整到与硬件 TE 信号一致的时序上

### 双屏同步显示
- **同显方案**：
  - 解码一次，刷屏两次
  - 共享 SPI 总线同时刷新（需屏幕支持）
- **异显方案**：
  - 确保解码任务在同一核心运行
  - 设置相同的任务优先级
  - 如果仍无法避免，可尝试使用信号量同步解码与刷屏任务

### 性能优化建议
- 使用 PSRAM 缓存 GIF/AVI 文件数据
- 优化存储介质读取速度，如使用 SDIO 4-line 40M

## 性能优化配置

为提升系统整体性能，可通过提高运存 PSRAM 频率以及存储 SPI SDIO 总线频率，减少访存时间。相关文档参考：

- [QSPI flash 芯片的高性能模式](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-reference/peripherals/spi_flash/spi_flash_optional_feature.html#qspi-flash)
- [SPI Flash 和片外 SPI RAM 配置](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-guides/flash_psram_config.html#spi-flash-spi-ram)

推荐的 sdkconfig 如下：

```ini
# 性能优化
CONFIG_COMPILER_OPTIMIZATION_PERF=y

# 120MHz Flash 配置（需确认硬件支持）
CONFIG_IDF_EXPERIMENTAL_FEATURES=y
CONFIG_BOOTLOADER_FLASH_DC_AWARE=y
CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
CONFIG_ESPTOOLPY_FLASHFREQ_120M=y
CONFIG_SPI_FLASH_HPM_ENA=y

# 120M PSRAM 配置（需要配合 120M flash 使用）
CONFIG_SPIRAM=y
CONFIG_SPIRAM_SPEED_120M=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=64
CONFIG_SPIRAM_TIMING_TUNING_POINT_VIA_TEMPERATURE_SENSOR=y
CONFIG_SPIRAM_TIMING_MEASURE_TEMPERATURE_INTERVAL_SECOND=5
```

> **注意：**需要确认具体硬件型号是否支持 120MHz Flash，同时建议进行批量测试验证
