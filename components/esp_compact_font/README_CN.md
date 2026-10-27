# ESP Compact Font — 面向 ESP32 的快速紧凑字体渲染

[![Component Registry](https://components.espressif.com/components/espressif/esp_compact_font/badge.svg)](https://components.espressif.com/components/espressif/esp_compact_font)
[English](./README.md)

**ESP Compact Font 是面向带显示屏的 ESP32 设备的轻量字体渲染组件。** 它帮助你在不依赖 LVGL 的情况下减小字库占用并高效绘制文字。

面向资源受限的嵌入式产品，它支持 UTF-8 文本、运行时字号缩放、可选字形缓存，以及卡拉 OK 式的逐段文字高亮。它特别适合中文界面以及其他需要覆盖大量 CJK 字符的应用。

## 主要特性

- **紧凑字库存储** — GFH 使用 Huffman 编码压缩字形覆盖，支持 1、2、4 位灰度格式。
- **快速字形解码** — 高效解码缩短绘制文字所需的时间。
- **运行时字号缩放** — 把一份字库缩放到不同像素尺寸，而不必为每种字号各存一份字体文件。
- **卡拉 OK 式高亮** — 按高亮色逐步显现文字，与音频播放或其他定时事件同步。
- **可选字形缓存** — 复用已解码字形，减少重复解码。
- **灵活的字库来源** — 从内存或文件系统路径加载，并可选择把字库数据拷入 RAM。
- **不依赖 LVGL** — 可在独立应用中使用，也可与现有图形框架一起使用。
- **多种输出格式** — 可绘制到 RGB565、RGB888、灰度或单色表面。

## 适用场景

ESP Compact Font 适合需要在有限 Flash、RAM 或算力下高效绘制文字的嵌入式产品。

- **智能显示与控制面板：** 菜单、设置、状态信息和设备标签。
- **中文界面：** 在不引入笨重运行时字体引擎的情况下显示 CJK 字符和标点。
- **电子书与 AI 助手：** 在内存受限的设备上显示大量文字。
- **卡拉 OK 与同步文本：** 随播放逐步高亮歌词。
- **嵌入式仪表盘与界面系统：** 高效绘制频繁更新的文字。

## 支持的字体格式

| 格式 | 说明 |
| --- | --- |
| **GFH** | 内置 Huffman 编码字库，支持 1、2、4 bpp 灰度字形。推荐用于大多数应用。 |
| **LVGL 二进制（`.bin`）** | 可选支持由 `lv_font_conv` 生成的字体二进制。不需要链接 LVGL 库。 |
| **GFZ** | 可选的 2 bpp 字体格式，按字形使用 Zstandard 压缩。默认关闭，并需要额外的解码器代码。 |

在优先考虑紧凑存储和快速解码时，推荐使用 GFH。在优先考虑文件体积时，GFZ 提供另一种压缩方式。

技术细节见 [GFH / GFZ 格式与算法](./format.md)。

## 性能对比

下面的主机基准在相同字形上对比 GFH 与 LVGL 字体二进制。编译选项为 `-O2`，并关闭字形缓存。

**测试语料：** 约 9,620 个 CJK 表意文字和标点，使用 36 像素的 Noto Sans CJK 子集。

| 格式 | LVGL 大小 | GFH 大小 | LVGL 解码 | GFH 解码 |
| --- | ---: | ---: | ---: | ---: |
| 1-bpp | 1,335 KB | 788 KB | 2.71 µs/字形 | 1.97 µs/字形 |
| 2-bpp | 1,778 KB | 1,474 KB | 9.14 µs/字形 | 2.88 µs/字形 |
| 4-bpp | 3,066 KB | 2,970 KB | 9.90 µs/字形 | 3.66 µs/字形 |

### ESP32-S3 板端结果

以下结果测自 CPU 主频 240 MHz、PSRAM 频率 80 MHz 的 ESP32-S3。解码前先把嵌入字库源复制到 RAM（`with_ram=1`），并关闭字形缓存。每次均以 36 px 解码全部 9,620 个字形，结果为 `bad=0`。

| 格式 | LVGL 打开 | GFH 打开 | LVGL 完整解码 | GFH 完整解码 | LVGL 解码 | GFH 解码 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1-bpp | 164.131 ms | 88.408 ms | 1,757.046 ms | 1,242.523 ms | 182.64 µs/字形 | 129.16 µs/字形 |
| 2-bpp | 194.160 ms | 135.454 ms | 4,203.172 ms | 1,808.119 ms | 436.92 µs/字形 | 187.95 µs/字形 |
| 4-bpp | 279.157 ms | 233.920 ms | 4,392.224 ms | 2,158.726 ms | 456.57 µs/字形 | 224.39 µs/字形 |

打开时间包含准备 RAM 字库副本的过程；完整解码时间表示整套字库的无缓存解码过程。

对于该语料，GFH 在 1 bpp 下将文件大小减少约 41%。三种测试格式的字形解码也都更快。

**可选 GFZ 基准：** 在同一 2 bpp 语料上，GFZ 测得 1,623 KB 和 3.89 µs/字形。在这次测试中它比 GFH 更大、更慢。

## 资源占用

下面的数据使用同一套 9,620 字、36 px 语料。RAM 是 `esp_compact_font_set_mem_src()` 返回后、字库文件仍留在 Flash（`prefer_ram` 关闭）时库保留的内存。指令大小是 `riscv32-esp-elf-gcc -O2` 去掉未引用函数之后的 `.text`。

### RAM

每个打开的字体保留一份字形索引，每个字形 16 字节。这套语料是 `9,618 × 16 = 153,888` 字节。文件中的索引同样大，RAM 索引建好后就会释放。GFH 还会建立一张 12 位 Huffman 表（16,384 字节）和一张较短的二级表。建表用的 32,768 字节临时缓冲会在打开返回前释放。

| 格式 | 索引 | 解码表 | 常驻 RAM |
| --- | ---: | ---: | ---: |
| GFH 1-bpp | 153,888 | 17,760 | 171,776 |
| GFH 2-bpp | 153,888 | 16,736 | 170,752 |
| GFH 4-bpp | 153,888 | 16,384 | 170,400 |
| GFZ 2-bpp | 153,888 | 41,108 | 195,124 |
| LVGL `.bin` | 307,776 | 0 | 307,944 |

常驻 RAM 含 108 字节的字体句柄和 codec 对象（GFH 与 GFZ 为 20 字节，LVGL 为 60 字节）。GFH 的解码表是 Huffman 查找表。GFZ 的表是按 `ZSTD_DEC_LIT_CAP` 为 4,096 建立的 zstd 工作区。LVGL 同时保留共享索引和每个字形第二条 16 字节记录。以流式方式打开的 GFZ 还会保留 16,384 字节的字典。内存源直接使用文件镜像里的字典。

字形变少时，索引按每个字形 16 字节下降（LVGL `.bin` 为每个字形 32 字节）。16,384 字节的 Huffman 查找表和 41,108 字节的 zstd 工作区与字形数量无关，打开后始终占用。

`prefer_ram` 会在上表之外再复制一份字库文件。解码一个字形时，调用方缓冲区最大为 `80 × 80 = 6,400` 字节。可选缓存每个槽 48 字节（默认 512 槽为 24,576 字节），外加每个已缓存字形的一份 A8 蒙版（`宽 × 高`）。测得的一个 36 px 字形蒙版为 35×34 = 1,190 字节。

### 指令内存

| 构建 | `.text` | `.rodata` |
| --- | ---: | ---: |
| GFH（默认） | 15,186 | 50 |
| ESP32-P4 上的 GFH，含 PIE 辅助函数 | 15,586 | 50 |
| GFH + LVGL `.bin` | 20,378 | 129 |
| GFH + GFZ | 28,356 | 891 |

GFH 始终链接。LVGL 与 GFZ 分别由 `ESP_COMPACT_FONT_LVGL` 和 `ESP_COMPACT_FONT_GFZ` 控制。ESP32-P4 那一行加上缩放与混合辅助函数（208 与 192 字节）。

## 快速开始

基本流程很直接：打开渲染器，加载字库，选择像素尺寸，然后把 UTF-8 文本画到显示缓冲区上。

```c
#include <stdlib.h>
#include "esp_compact_font.h"

static void *my_malloc(size_t size, void *ctx)
{
    (void)ctx;
    return malloc(size);
}

static void my_free(void *ptr, void *ctx)
{
    (void)ctx;
    free(ptr);
}

void draw_text(uint16_t *framebuffer, int width, int height,
               const uint8_t *gfh_data, size_t gfh_size)
{
    esp_compact_font_cfg_t cfg = {0};
    esp_compact_font_handle_t font = NULL;

    cfg.alloc.malloc = my_malloc;
    cfg.alloc.free = my_free;

    if (esp_compact_font_open(&cfg, &font) != ESP_COMPACT_FONT_OK) {
        return;
    }

    if (esp_compact_font_set_mem_src(font, gfh_data, gfh_size)
        == ESP_COMPACT_FONT_OK) {
        esp_compact_font_surface_t dst = {
            .buffer = framebuffer,
            .stride = width * 2,
            .width = width,
            .height = height,
            .format = ESP_COMPACT_FONT_SURFACE_RGB565,
        };

        esp_compact_font_set_pixel(font, 24);

        esp_compact_font_draw_utf8(
            font, &dst, 16, 40, "Hello World",
            ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0, 0, 0), NULL);
    }

    esp_compact_font_close(font);
}
```

把示例中的字库数据、帧缓冲和分配器换成应用自己提供的。该示例使用 RGB565 目标表面和 24 像素的绘制尺寸。

**注意：** 传给 `esp_compact_font_set_mem_src()` 的缓冲区在字库使用期间必须保持有效。

对于频繁绘制的文字，可以启用字形缓存以减少重复解码。更完整的配置见 [字体基准示例](./examples/font_benchmark)。

## 卡拉 OK 式文字高亮

ESP Compact Font 支持按水平方向逐步绘制字形，因此可以实现卡拉 OK 歌词、跟读高亮和同步文字动画，而无需为不同高亮状态各维护一份字库。

基本做法是先用普通颜色画完整字形，再随着播放进度用高亮色重画已唱到的部分。

```c
int advance = 0;

uint32_t normal = ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0xB4, 0xAE, 0xA4);
uint32_t highlight = ESP_COMPACT_FONT_MAKE_COLOR_RGB565(0xC4, 0x3A, 0x2A);

/* 用普通颜色画完整字形。 */
esp_compact_font_draw(font, &dst, x, baseline, cp, normal, &advance);

/* sung_percent 范围为 0 到 100。 */
esp_compact_font_set_draw_range(font, 0, sung_percent);

esp_compact_font_draw(font, &dst, x, baseline, cp, highlight, NULL);
```

`esp_compact_font_set_draw_range()` 把水平绘制窗口设为字形位图宽度的百分比：`0` 是左缘，`100` 是右缘。

绘制范围是**一次性**的：下一次 `esp_compact_font_draw()` 调用会用掉它，然后清除。每次绘制局部字形都要重新设置范围。

- `esp_compact_font_draw()` 把已配置的范围应用到下一次字形绘制。
- `esp_compact_font_draw_utf8()` 把同一个范围应用到字符串中的每个码点。
- `esp_compact_font_draw_glyph()` 忽略绘制范围。

要让整行同步高亮，把播放进度映射到当前字形，并且只绘制新露出的部分。这样可以避免不必要地重画已经高亮的文字。

相关绘制方法见 [字体基准示例](./examples/font_benchmark)。

## 内存与缓存

ESP Compact Font 同时支持内存字库源和文件系统字库源。

- **内存源：** `esp_compact_font_set_mem_src()` 使用调用方提供的缓冲区。缓冲区在使用期间必须保持有效。
- **文件系统源：** `esp_compact_font_set_file_src()` 从文件读取字库数据。GFH、GFZ 和 LVGL 源可以按字形读取，而不必把整份字库拷入内存。
- **加载到 RAM：** `prefer_ram` 选项会在可能时请求把字库镜像拷贝到所配置分配器的内存中。这会多用 RAM，并可以减少绘制时的文件系统访问。
- **字形缓存：** 可选的共享缓存保存已解码字形以便复用。请按可用内存和应用的绘制负载选择容量。

### 典型流程

1. 准备应用所需的字库图集和字形数据。
2. 运行对应的生成工具以产出 GFH 或 GFZ 文件，或用 `lv_font_conv` 生成 LVGL `.bin`。
3. 把生成的文件编进固件，或存到文件系统上。
4. 用 `esp_compact_font_set_mem_src()` 或 `esp_compact_font_set_file_src()` 加载字库。

**应该选哪种格式？**

- **GFH：** 从这里开始，以兼顾文件大小和解码速度。
- **GFZ：** 当文件体积小于 `GFH`、且额外解码器占用可以接受时再考虑。

文件布局、编码细节和压缩算法见 [GFH / GFZ 格式与算法](./format.md)。

## 字库生成工具

在开发电脑上生成紧凑字库文件，然后把它们打包进 ESP-IDF 应用，或在运行时从文件系统加载。

| 工具 | 输出 | 说明 |
| --- | --- | --- |
| [`tools/mk_gfh.py`](./tools/mk_gfh.py) | GFH | 生成 1、2 或 4 bpp 字形覆盖的 Huffman 编码字库。推荐用于大多数应用。 |
| [`tools/mk_gfz.py`](./tools/mk_gfz.py) | GFZ | 生成按字形使用 Zstandard 压缩的 2 bpp 字库。 |
| [`lv_font_conv`](https://github.com/lvgl/lv_font_conv) | LVGL `.bin` | LVGL 官方转换器。本组件解码该文件，并不重新实现编码器。 |


## 示例与文档

| 资源 | 说明 |
| --- | --- |
| [字体基准](./examples/font_benchmark) | 在显示屏上对比字库大小和解码性能。 |
| [测试应用](./test_apps) | 覆盖缓存、解码、绘制和文件系统加载的 Unity 测试。 |
| [GFH / GFZ 格式文档](./format.md) | 详细的格式布局、Huffman 编码、XOR 编码和压缩算法。 |


## 许可证

Espressif Modified MIT。详情见 [LICENSE](./LICENSE)。

可选的 LVGL 二进制解码器保留 LVGL Kft 原有的 MIT 声明。可选的 Zstandard 解码器保留其 BSD-3-Clause 声明。

## 技术支持

技术支持请使用以下链接：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题报告与功能请求：[GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
