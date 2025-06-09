# Dual Display with Shared SPI Bus Example

- [中文版本](./README_CN.md)
- Example Level: ![alt text](../../../docs/_static/level_basic.png "Basic")


## Example Description

This example demonstrates how to drive two displays simultaneously using one shared SPI bus.

Currently, there are two methods to implement different animations for simultaneous display (default parsing AVI data stream for display):

- Enable `USE_AVI_PLAYER`: Two avi player tasks simultaneously parse avi files, decode JPEG, and display on the screens with high frame rate
- Disable `USE_AVI_PLAYER`: Use LVGL to create two GIF objects, which can read and display files through `flash(mmap)`, `flash(littlefs)`, `sdcard`, or `psram` four ways

> In dual display solutions, display resources generally use GIF or JPEG data streams. To ensure smooth display, there are high requirements for screen configuration, system parameter settings, and memory read speed.

**Rendering Solution**: To achieve smoother animation playback, refer to [Dual Display Solution](doc/dual_eye.md) to determine the currently recommended animation rendering solution  
**Storage Solution**: To reduce read time and increase overall frame rate, refer to [Storage Optimization](doc/storage_use.md) to determine the recommended storage method and related optimizations


## Environment Setup

### Hardware Requirements

1. To minimize pin usage, this solution uses a shared SPI connection where MOSI, PCLK, D/C, RST, and BCLK pins are shared, while each display device uses its own CS pin
2. You can use development boards like [ESP32-S3-DecKitC-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide.html) and connect them yourself

### Pin Connections

| ESP Board Pin | LCD Screen1 PIN | LCD Screen2 PIN |
| ------------- | --------------- | --------------- |
|  GPIO_NUM_45  |      SCL        |       SCL       |
|  GPIO_NUM_48  |      SDA        |       SDA       |
|  GPIO_NUM_47  |      DCX        |       DCX       |
|  GPIO_NUM_21  |     RESET       |      RESET      |
|  GPIO_NUM_39  |     LEDA        |      LEDA       |
|  GPIO_NUM_44  |      CS         |       -         |
|  GPIO_NUM_48  |      -          |       CS        |

## Build and Flash

### IDF Default Branch

This example supports IDF release/v5.0 and later branches. The example uses ADF's built-in branch `$ADF_PATH/esp-idf` by default.

To improve sdcard random read speed, you need to manually apply the patch:

```
cd esp-idf
git apply patch/speed_up_fread_fwrite.patch
```

### Configuration

This example requires a microSD card and an AVI video file. Insert the microSD card into the development board. The file name to be read should match the configuration in the code: `AVI_FILE_L` and `AVI_FILE_R`.

### Build and Flash

First, build the version and flash it to the development board, then run the monitor tool to view the serial output (replace PORT with the port name):

```
idf.py -p PORT flash monitor
```

Use ``Ctrl-]`` to exit the debug interface.

For complete steps on configuring and using ESP-IDF to generate projects, please refer to the [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32/index.html).

**Note**:
- The project uses USB Serial JTAG by default to view logs. If using UART, you need to modify to: `CONFIG_ESP_CONSOLE_UART_DEFAULT`
- When using USB Serial JTAG, if the system repeatedly restarts abnormally, the port may not be recognized. In this case, you can manually enter download mode (pull boot low and power on)


## How to Use the Example

### Features and Usage

- The program enables `USE_AVI_PLAYER` configuration by default: it parses AVI files, decodes JPEG, and sends them to the display.
- When `USE_AVI_PLAYER` is disabled, LVGL is used for direct display. This is more convenient at lower resolutions and allows overlaying other interfaces, such as icon displays.
- To test different file system random read/write speeds, you can enable `ENABLE_BENCHMARK`. It is disabled by default.

### Log Output
The following is the complete log output of this example.

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

## Related Issues

### Accelerating GIF Decoding

- **GIF Decoding Library Time Consumption Statistics**

```c
// File is dual_eyes/managed_components/lvgl__lvgl/src/extra/libs/gif/lv_gif.c -> next_frame_task_cb

// Time consuming main interface
gd_get_frame(gd_GIF *gif); // If dec 240*240, maybe cost 70 ms
// Time consuming secondary interface
gd_render_frame(gd_GIF *gif, uint8_t *buffer); // If dec 240*240, maybe cost 10 ms
```

- **GIF Optimization(Recommended)**: [https://ezgif.com/optimize](https://ezgif.com/optimize), you can choose `Lossy GIF`, `Optimize Transparency`, etc.

- **Optimize GIF Decoding Library**: If the GIF file has no background color, i.e., the animation completely fills the screen and is opaque, you can use the following modification:

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

- **Convert GIF to AVI**: Since the LVGL GIF open-source decoding library is not deeply optimized, the frame rate is low at high resolutions. You can convert using the following command:

```c
.\ffmpeg.exe -i input.gif -q:v 1 -c:v mjpeg -pix_fmt yuvj420p -vtag MJPG -q:v 1 output.avi
```

- **Frame Rate Comparison**

Testing the same animation effect with GIF and AVI files (30 fps, 240×240×2, dual display), actual FPS and CPU Loading are as follows:

|Test Condition|FPS|CPU Loading|
|-------|---|-----------|
|GIF Display: Read from SDCARD|6|32%|
|GIF Display: Read from PSRAM|7|32%|
|GIF Display: Read from PSRAM + Optimized GIF Decoding|8|34%|
|GIF Display: Read from PSRAM + Optimized GIF Decoding + Full Screen DRAM Buffer|11|46%|
|GIF Display: Read from PSRAM + Optimized GIF Decoding + No Screen Refresh|12|47%|
|AVI Display: Read from SDCARD|27|38%|

### Shared SPI Bus Mutual Access

- **Shared SPI Bus Notes**: [Host Features](https://docs.espressif.com/projects/esp-idf/en/v5.4.1/esp32s3/api-reference/peripherals/spi_master.html#id2)

- Related Measures:
  1. When multiple AVI player tasks are playing simultaneously, use xSemaphoreCreateMutex to add a mutex lock for shared devices
  2. When rendering LVGL animations, register the SPI ISR to the same core as the SPI peripheral task, refer to `SPI_ISR_CPU_ID`

### Display Stuttering or Abnormal Display

- Confirm if there are FLASH write operations, such as OTA upgrades. FLASH writes will disable cache and interrupts. If display and FLASH writes cannot be avoided simultaneously, you can enable `CONFIG_SPIRAM_XIP_FROM_PSRAM`
- If you need to write to FLASH when reading files using MMAP method, you can copy the GIF file to PSRAM before displaying


## Technical Support

Please get technical support through the following links:

- For technical support, please visit the [esp32.com](https://esp32.com/viewforum.php?f=20) forum
- For bugs and new feature requests, please create a [GitHub issue](https://github.com/espressif/esp-adf/issues)

We will respond as soon as possible. 
