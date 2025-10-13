# 共享 SPI 总线双屏显示例程

- [English Version](./README.md)
- 例程难度：![alt text](../../../docs/_static/level_basic.png "初级")


## 例程简介

本例程演示了使用共享 SPI 总线同时驱动两个屏幕的方案。

当前提供了以下两种方法实现不同动画的同时显示（默认解析 AVI 数据流进行显示）：

- 使能 `USE_AVI_PLAYER` ：两个 avi_player 任务同时解析 AVI 文件，解码 JPEG 后显示在屏幕上，帧率较高
- 未使能 `USE_AVI_PLAYER` ：使用 LVGL 创建两个 GIF 对象，可通过 `flash(mmap)`、`flash(littlefs)`、`sdcard`、`psram` 四种方式读取文件并显示

> 在双屏显示方案中，显示资源一般采用 GIF 或 JPEG 数据流。为了保证显示流畅，对屏幕配置、系统参数设置以及存储器读取速度等方面均有较高要求。

**渲染方案**：为了实现更加流畅的动画播放，可参考 [双屏显示方案](doc/dual_eye.md) 确定当前推荐使用的动画渲染方案  
**存储方案**：为了降低读取耗时，增加总体帧率，可参考 [存储优化](doc/storage_use.md) 确定推荐选择的存储方式与相关优化


## 环境配置

### 硬件要求

1. 为了减少引脚使用，例程采用共享 SPI 连接，MOSI、PCLK、D/C、RST、BCLK 引脚均共享使用，每个屏幕设备使用各自的 CS 引脚
2. 可参考引脚连接使用 [ESP32-S3-DecKitC-1](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-devkitc-1/user_guide.html) 等开发板自行连接

### 引脚连接

| ESP Board Pin | LCD Screen1 PIN | LCD Screen2 PIN |
| ------------- | --------------- | --------------- |
|  GPIO_NUM_45  |      SCL        |       SCL       |
|  GPIO_NUM_48  |      SDA        |       SDA       |
|  GPIO_NUM_47  |      DCX        |       DCX       |
|  GPIO_NUM_21  |     RESET       |      RESET      |
|  GPIO_NUM_39  |     LEDA        |      LEDA       |
|  GPIO_NUM_44  |      CS         |       -         |
|  GPIO_NUM_48  |      -          |       CS        |


## 编译和下载

### IDF 默认分支

本例程支持 IDF release/v5.0 及以后的分支，例程默认使用 ADF 的內建分支 `$ADF_PATH/esp-idf`。

为提高 sdcard 的随机读取速度，需要手动增加补丁：

```
cd esp-idf
git apply patch/speed_up_fread_fwrite.patch
```


### 配置

本例程需要准备一张 microSD 卡，并自备 avi 视频文件，然后把 microSD 卡插入开发板备用。  

待读取文件名需要和代码中配置一致：`AVI_FILE_L` 和 `AVI_FILE_R`。

### 编译和下载

请先编译版本并烧录到开发板上，然后运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

退出调试界面使用 ``Ctrl-]``。

有关配置和使用 ESP-IDF 生成项目的完整步骤，请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/release-v5.3/esp32/index.html)。

**注意**：

- 工程默认使用 USB Serial JTAG 查看 log，如果使用 UART ，则需要修改为：`CONFIG_ESP_CONSOLE_UART_DEFAULT`
- 使用 USB Serial JTAG 时，如果系统反复异常重启，则可能无法识别到端口，此时可以手动进入 download 模式（boot 拉低后上电）


## 如何使用例程

### 功能和用法

- 程序默认使能 `USE_AVI_PLAYER` 配置：即解析 AVI 文件，并将 JPEG 解码后送到屏幕显示
- 不使能 `USE_AVI_PLAYER` 时，则使用 LVGL 直接显示，在分辨率较低时较为方便，且可以叠加其他界面，如图标显示等
- 如果想要测试不同文件系统随机读写速度，可使能 `ENABLE_BENCHMARK`，默认不使能

### 日志输出
以下为本例程的完整日志。

```c
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x15 (USB_UART_CHIP_RESET),boot:0xa (SPI_FAST_FLASH_BOOT)
Saved PC:0x42012b8b

SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2820,len:0x560
load:0x403c8700,len:0x4
load:0x403c8704,len:0xb00
load:0x403cb700,len:0x28f4
entry 0x403c8878
I (114) octal_psram: vendor id    : 0x0d (AP)
I (114) octal_psram: dev id       : 0x02 (generation 3)
I (114) octal_psram: density      : 0x03 (64 Mbit)
I (114) octal_psram: good-die     : 0x01 (Pass)
I (114) octal_psram: Latency      : 0x01 (Fixed)
I (115) octal_psram: VCC          : 0x01 (3V)
I (115) octal_psram: SRF          : 0x01 (Fast Refresh)
I (115) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
I (115) octal_psram: BurstLen     : 0x01 (32 Byte)
I (115) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (116) octal_psram: DriveStrength: 0x00 (1/1)
I (117) MSPI Timing: PSRAM timing tuning index: 5
I (117) esp_psram: Found 8MB PSRAM device
I (117) esp_psram: Speed: 80MHz
I (117) cpu_start: Multicore app
I (126) cpu_start: Pro cpu start user code
I (126) cpu_start: cpu freq: 240000000 Hz
I (126) app_init: Application information:
I (126) app_init: Project name:     dual_eye
I (126) app_init: App version:      v2.7-106-g8c41df11c-dirty
I (126) app_init: Compile time:     Jun 25 2025 17:32:39
I (127) app_init: ELF file SHA256:  6f5c635a9...
I (127) app_init: ESP-IDF:          v5.4-dirty
I (127) efuse_init: Min chip rev:     v0.0
I (127) efuse_init: Max chip rev:     v0.99
I (127) efuse_init: Chip rev:         v0.2
I (127) heap_init: Initializing. RAM available for dynamic allocation:
I (128) heap_init: At 3FC9F408 len 0004A308 (296 KiB): RAM
I (128) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
I (128) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (128) heap_init: At 600FE11C len 00001ECC (7 KiB): RTCRAM
I (128) esp_psram: Adding pool of 8192K of PSRAM memory to heap allocator
I (129) spi_flash: detected chip: generic
I (129) spi_flash: flash io: qio
I (129) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (130) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (130) main_task: Started on CPU0
I (131) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (131) main_task: Calling app_main()
I (194) LITTLE_FS: Partition size: total: 6291456, used: 2908160
I (194) LITTLE_FS: Initializing LittleFS successfully
I (286) mmap_assets: Partition 'assets' successfully created, version: 1.3.1
I (287) MMAP: Stored files:6
I (287) MMAP: Name:[angry_l.avi], mem:[0x3c8700b6], size:[272500 bytes], w:[0], h:[0]
I (287) MMAP: Name:[angry_r.avi], mem:[0x3c8b292c], size:[272618 bytes], w:[0], h:[0]
I (287) MMAP: Name:[blink_100.gif], mem:[0x3c8f5218], size:[388945 bytes], w:[100], h:[100]
I (287) MMAP: Name:[blink_128.gif], mem:[0x3c95416b], size:[465906 bytes], w:[128], h:[128]
I (288) MMAP: Name:[flicker.gif], mem:[0x3c9c5d5f], size:[1348206 bytes], w:[128], h:[128]
I (288) MMAP: Name:[love.gif], mem:[0x3cb0efcf], size:[136643 bytes], w:[100], h:[106]
I (336) SDCARD: Filesystem mounted
Name: SC32G
Type: SDHC
Speed: 40.00 MHz (limit: 40.00 MHz)
Size: 30436MB
CSD: ver=2, sector_size=512, capacity=62333952 read_bl_len=9
SSR: bus_width=1
I (337) main_task: Returned from app_main()
I (337) DUAL_LCD: Setting LCD backlight: 100%
I (338) gc9a01: LCD panel create success, version: 1.2.0
I (359) gc9a01: LCD panel create success, version: 1.2.0
I (583) DUAL_LCD: Display init done
_malloc_r 0x3fcad328, size: 4096
_malloc_r 0x3fcb1330, size: 4096
_malloc_r 0x3fcae5ac, size: 4096
I (1748) DUAL_EYE_PLAYER: FPS(0x3c07c550): 28.1
_malloc_r 0x3fcb25b4, size: 4096
I (1807) DUAL_EYE_PLAYER: FPS(0x3c07c5a0): 28.4
_malloc_r 0x3fcad328, size: 4096
I (2850) DUAL_EYE_PLAYER: FPS(0x3c07c550): 27.2
_malloc_r 0x3fcae32c, size: 4096
I (2909) DUAL_EYE_PLAYER: FPS(0x3c07c5a0): 27.2
I (3339) : ┌───────────────────┬──────────┬─────────────┬─────────┬──────────┬───────────┬────────────┬───────┐
I (3340) : │ Task              │ Core ID  │ Run Time    │ CPU     │ Priority │ Stack HWM │ State      │ Stack │
I (3340) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (3341) : │ IDLE0             │ 0        │ 999692      │  49.93% │ 0        │ 820       │ Ready      │ Extr  │
I (3341) : │ esp_timer         │ 0        │ 1308        │   0.07% │ 22       │ 3348      │ Suspended  │ Extr  │
I (3342) : │ ipc0              │ 0        │ 0           │   0.00% │ 24       │ 532       │ Suspended  │ Extr  │
I (3342) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (3343) : │ avi_player        │ 1        │ 302217      │  15.10% │ 5        │ 948       │ Ready      │ Extr  │
I (3343) : │ avi_player        │ 1        │ 300918      │  15.03% │ 5        │ 956       │ Ready      │ Extr  │
I (3343) : │ IDLE1             │ 1        │ 263085      │  13.14% │ 0        │ 732       │ Ready      │ Extr  │
I (3344) : │ lcd_draw_right    │ 1        │ 67616       │   3.38% │ 10       │ 2988      │ Blocked    │ Extr  │
I (3344) : │ lcd_draw_left     │ 1        │ 64700       │   3.23% │ 10       │ 2928      │ Blocked    │ Extr  │
I (3350) : │ player_task       │ 1        │ 1738        │   0.09% │ 3        │ 1728      │ Blocked    │ Extr  │
I (3351) : │ system_task       │ 1        │ 728         │   0.04% │ 15       │ 3276      │ Running    │ Extr  │
I (3352) : │ ipc1              │ 1        │ 0           │   0.00% │ 3        │ 476       │ Suspended  │ Extr  │
I (3353) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
I (3354) : │ Tmr Svc           │ 7fffffff │ 0           │   0.00% │ 1        │ 1364      │ Blocked    │ Extr  │
I (3354) : └───────────────────┴──────────┴─────────────┴─────────┴──────────┴───────────┴────────────┴───────┘
I (3355) MONITOR: Func:system_task, Line:25, MEM Total:7881468 Bytes, Inter:313951 Bytes, Dram:313951 Bytes

_malloc_r 0x3fcad328, size: 4096
I (3951) DUAL_EYE_PLAYER: FPS(0x3c07c550): 27.2
_malloc_r 0x3fcb2ad8, size: 4096
I (4010) DUAL_EYE_PLAYER: FPS(0x3c07c5a0): 27.2
_malloc_r 0x3fcae5ac, size: 4096
I (5053) DUAL_EYE_PLAYER: FPS(0x3c07c550): 27.2
_malloc_r 0x3fcad328, size: 4096
I (5112) DUAL_EYE_PLAYER: FPS(0x3c07c5a0): 27.2
_malloc_r 0x3fcad328, size: 4096
I (6154) DUAL_EYE_PLAYER: FPS(0x3c07c550): 27.2
_malloc_r 0x3fcb2ad8, size: 4096
I (6213) DUAL_EYE_PLAYER: FPS(0x3c07c5a0): 27.2
_malloc_r 0x3fcae5ac, size: 4096
I (7256) DUAL_EYE_PLAYER: FPS(0x3c07c550): 27.2
_malloc_r 0x3fcad328, size: 4096
I (7315) DUAL_EYE_PLAYER: FPS(0x3c07c5a0): 27.2

```

## 相关 issue

### 加速 GIF 解码

- **GIF 解码库耗时统计**
```c
// File is dual_eyes/managed_components/lvgl__lvgl/src/extra/libs/gif/lv_gif.c -> next_frame_task_cb

// Time consuming main interface
gd_get_frame(gd_GIF *gif); // If dec 240*240, maybe cost 70 ms
// Time consuming secondary interface
gd_render_frame(gd_GIF *gif, uint8_t *buffer); // If dec 240*240, maybe cost 10 ms
```

- **优化 GIF 文件（推荐）**：[https://ezgif.com/optimize](https://ezgif.com/optimize)，可以选择 `Lossy GIF`，`Optimize Transparency` 等方式

- **优化 GIF 解码库**：如果 GIF 文件本身没有任何背景颜色，即动画完全填充整个屏幕，且不透明，则可以使用如下修改：

```c
// File is dual_eyes/managed_components/lvgl__lvgl/src/extra/libs/gif/gifdec.c
static void
dispose(gd_GIF *gif)
{
    return;
    int i, j, k;
    uint8_t *bgcolor;
    ...
}
```

- **GIF 转化为 AVI**：由于 LVGL GIF 开源解码库并没有深度优化，大分辨率下帧率较低，可通过如下命令进行转化：
```c
.\ffmpeg.exe -i input.gif -q:v 1 -c:v mjpeg -pix_fmt yuvj420p -vtag MJPG -q:v 1 output.avi
```

- **帧率对比**

测试同样动画效果的 GIF 和 AVI 文件（30 fps，240×240×2，双屏显示），实际 FPS 与 CPU Loading 如下：

|测试条件|FPS|CPU Loading|
|-------|---|-----------|
|GIF 显示：读取 SDCARD|6|32%|
|GIF 显示：读取 PSRAM|7|32%|
|GIF 显示：读取 PSRAM + 优化 GIF 解码|8|34%|
|GIF 显示：读取 PSRAM + 优化 GIF 解码 + 全屏 DRAM buffer|11|46%|
|GIF 显示：读取 PSRAM + 优化 GIF 解码 + 不刷屏|12|47%|
|AVI 显示：读取 SDCARD|27|38%|

### 共享 SPI 总线互斥访问

- **共享 SPI 总线注意事项**：[主机特性](https://docs.espressif.com/projects/esp-idf/zh_CN/v5.4.1/esp32s3/api-reference/peripherals/spi_master.html#id2)

- 相关措施：
  1. avi player 多任务同时播放时，使用 xSemaphoreCreateMutex 为共享设备添加互斥锁
  2. LVGL 动画渲染时，将 SPI ISR 注册到与 SPI 外设相关任务相同的内核，参考 `SPI_ISR_CPU_ID`

### 画面卡顿或者显示异常

- 确认是否有 FLASH 写入操作，如 OTA 升级等。FLASH 写入时会关闭 cache 和中断，如无法避免显示和 FLASH 写入同时进行，可以使能 `CONFIG_SPIRAM_XIP_FROM_PSRAM`
- 如果使用 MMAP 方式读取文件时需要写入 FLASH，则可以将 GIF 文件拷贝至 PSRAM 再进行显示

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 故障和新功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
