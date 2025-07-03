# ESP32S3 存储方案指南

## 目录
- [ESP32S3 存储方案指南](#esp32s3-存储方案指南)
  - [目录](#目录)
  - [存储方案概述](#存储方案概述)
  - [Flash 存储方案](#flash-存储方案)
    - [MMAP 方式](#mmap-方式)
      - [特点](#特点)
      - [使用方法（结合 LVGL 使用为例）](#使用方法结合-lvgl-使用为例)
      - [注意事项](#注意事项)
    - [LittleFS 文件系统](#littlefs-文件系统)
      - [特点](#特点-1)
      - [使用方法（结合 LVGL 使用为例）](#使用方法结合-lvgl-使用为例-1)
  - [SD卡存储方案](#sd卡存储方案)
    - [特点](#特点-2)
    - [使用方法（结合 LVGL 使用为例）](#使用方法结合-lvgl-使用为例-2)
    - [性能优化](#性能优化)
      - [使用 SDIO 4 线模式](#使用-sdio-4-线模式)
      - [优化随机读取](#优化随机读取)
    - [SD 卡调试建议](#sd-卡调试建议)
      - [内存对齐](#内存对齐)
      - [调试方法](#调试方法)

## 存储方案概述

Espressif 的存储方案有多种，根据不同的使用场景和性能需求，可以选择以下方案：

| 存储介质 |    读取方式   |      主要优点    |     局限性    |
| ------- | ------------- | --------------- | ------------ |
| FLASH   | 内存映射 MMAP |     读取速度快    | 无法单个文件升级，需要升级整个分区；写入速度较慢；存储空间受限 |
| FLASH   | LittleFS 读取 | 读取速度相对较快，可单个升级文件 | 写入速度较慢；存储空间受限 |
| SDCARD  | FatFs SPI 读取  | 读写速度相对较快，可单独升级文件 | 读写速度慢于 SDIO 接口，且 CPU loading 更高 |
| SDCARD  | FatFs SDIO 读取 | 读写速度均较快，可单独升级文件 | 目前仅 ESP32、ESP32S3、ESP32P4 支持 SDIO host |

## Flash 存储方案

### MMAP 方式

#### 特点
- 读取速度最快
- 适合静态资源文件
- 不支持动态更新单个文件

#### 使用方法（结合 LVGL 使用为例）

1. 在工程中执行如下命令：`idf.py add-dependency "espressif/esp_mmap_assets^1.3.1"`

2. 工程代码实现：
```c
static lv_img_dsc_t pic_gif = {
    .header.cf = LV_IMG_CF_RAW,
    .header.always_zero = 0,
    .header.reserved = 0,
    .header.w = gif_width,
    .header.h = gif_height,
    .data_size = 0,
    .data = NULL,
};

// 获取文件内存映射
const uint8_t *mem = mmap_assets_get_mem(asset_handle, index);
int size = mmap_assets_get_size(asset_handle, index);

// 设置 GIF 源
pic_gif.data = (uint8_t *)mem;
pic_gif.data_size = size;
lv_gif_set_src(gif, &pic_gif);
```

#### 注意事项
- 文件更新需要重新烧录整个分区
- 不支持文件系统的磨损均衡
- 建议用于不常更新的静态资源

### LittleFS 文件系统

#### 特点
- 支持文件系统功能
- 支持动态更新文件
- 具有磨损均衡功能

#### 使用方法（结合 LVGL 使用为例）

1. 在工程中执行如下命令：`idf.py add-dependency "joltwallet/littlefs^1.20.0"`

2. 推荐配置：
```ini
# 在 sdkconfig 中配置
CONFIG_LV_USE_FS_STDIO=y
CONFIG_LV_FS_STDIO_LETTER=68
CONFIG_LV_FS_STDIO_PATH=""
CONFIG_LV_FS_STDIO_CACHE_SIZE=65535
CONFIG_LV_USE_GIF=y
CONFIG_LITTLEFS_CACHE_SIZE=4096
```

3. 使用示例：
```c
// 直接使用文件路径加载 GIF
lv_gif_set_src(gif, "D:/littlefs/demo.gif");
```

## SD卡存储方案

### 特点
- 支持大容量存储，如 64GB
- 适合存储多媒体资源
- 可支持 SPI/SDIO 接口

### 使用方法（结合 LVGL 使用为例）

1. 推荐配置：
```ini
# 在 sdkconfig 中配置
CONFIG_LV_USE_FS_STDIO=y
CONFIG_LV_FS_STDIO_LETTER=68
CONFIG_LV_FS_STDIO_PATH=""
CONFIG_LV_FS_STDIO_CACHE_SIZE=65535
CONFIG_LV_USE_GIF=y
CONFIG_FATFS_VFS_FSTAT_BLKSIZE=4096
```

2. 使用示例：
```c
// 从 SD 卡加载 GIF
lv_gif_set_src(gif, "D:/sdcard/demo.gif");
```

### 性能优化

#### 使用 SDIO 4 线模式
- 推荐使用 SDIO 4 线 40MHz 模式：相比 SDIO 1 线模式，实际读取速度提升约 3 倍

#### 优化随机读取

1. 如果设置 `CONFIG_FATFS_VFS_FSTAT_BLKSIZE`，则可通过修改 `esp-idf/components/newlib/src/heap.c` 中的 `_malloc_r` 函数实现随机读取加速

```c
void* _malloc_r(struct _reent *r, size_t size)
{
#if CONFIG_FATFS_VFS_FSTAT_BLKSIZE
    uint8_t *buf = NULL;
    if (size == CONFIG_FATFS_VFS_FSTAT_BLKSIZE) {
#if SOC_SDMMC_PSRAM_DMA_CAPABLE && CONFIG_SPIRAM
        buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_CACHE_ALIGNED);
#else
        buf = heap_caps_malloc(size, MALLOC_CAP_DMA);
#endif
        if (buf == NULL) {
            buf = heap_caps_malloc_default(size);
        }
        printf("_malloc_r %p, size: %d\n", buf, size);
        return buf;
    }
#endif
    return heap_caps_malloc_default(size);
}
```

2. 如果应用/组件主动调用 `fopen()`, 则可申请满足需求的 buffer，并通过 `setvbuf()` 将其设置到缓冲区内；而每次写入时，**写入数据量小于设置的大小**。详细参考：
- 标准库读取实现：[fread](https://gitee.com/mirrors/newlib-cygwin/blob/newlib-4.4.0/newlib/libc/stdio/fread.c#L227)
- 标准库写入实现：[fwrite](https://gitee.com/mirrors/newlib-cygwin/blob/newlib-4.4.0/newlib/libc/stdio/fvwrite.c#L207)

### SD 卡调试建议

#### 内存对齐
- 确保传入驱动的内存满足 DMA 对齐要求，可打印 `esp_dma_is_buffer_alignment_satisfied` 是否满足要求
- 如果使用 PSRAM，则还需要确定当前芯片下是否具备如下能力： `SOC_SDMMC_PSRAM_DMA_CAPABLE`

#### 调试方法
- 参考 [优化随机读取](#优化随机读取) 申请内存，保证申请大小是 512 整数倍，并通过 `setvbuf()` 将其设置到标准库缓冲区内
- 读取性能调试：[SDMMC 读取实现](https://github.com/espressif/esp-idf/blob/v5.4.1/components/sdmmc/sdmmc_cmd.c#L599)
- 写入性能调试：[SDMMC 写入实现](https://github.com/espressif/esp-idf/blob/v5.4.1/components/sdmmc/sdmmc_cmd.c#L462)
