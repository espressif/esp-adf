# 字体基准示例

- [English Version](./README.md)

- 例程难度：![alt text](../../../../docs/_static/level_regular.png "中级") - 对比 GFH 与 LVGL 字库，并在 board-manager 面板上成对绘制

## 例程简介

- 本例程用同一套汉字语料打开 LVGL `.bin` 与 GFH 字库，在关闭字形缓存时计时整库解码，并打印 `BENCH` / `COMPARE` 日志。随后在面板上按多种像素尺寸成对绘制，并做卡拉 OK 式的逐段高亮。
- 技术上演示 `esp_compact_font_open`、内存字库源、`esp_compact_font_decode_at`、运行时 `esp_compact_font_set_pixel`、绑定同一份字形缓存，以及用 `esp_compact_font_set_draw_range` 做高亮。

### 资源占用

#### 主机端参考数据

`prepare_fonts.py` 生成的语料（9620 个字形，36 px）在主机上的参考数据。编译选项为 `-O2`，关闭缓存。`./host_bench.sh` 会核对下列文件大小，并确认每个字形都能解码。

| bpp | LVGL KB | GFH KB | LVGL µs/字 | GFH µs/字 |
| --- | ------: | -----: | ---------: | --------: |
| 1   |    1335 |    788 |       2.71 |      1.97 |
| 2   |    1778 |   1474 |       9.14 |      2.88 |
| 4   |    3066 |   2970 |       9.90 |      3.66 |

#### ESP32-S3 板端参考数据

测试平台为主频 240 MHz、PSRAM 频率 80 MHz 的 ESP32-S3。解码前先把字库源复制到 RAM（`with_ram=1`），并关闭字形缓存。每一行都以 36 px 解码全部 9620 个字形。

| bpp | LVGL 打开 ms | GFH 打开 ms | LVGL 解码 ms | GFH 解码 ms | LVGL µs/字形 | GFH µs/字形 |
| --- | -----------: | ----------: | -----------: | ----------: | -----------: | ----------: |
| 1   |      164.131 |      88.408 |     1757.046 |    1242.523 |       182.64 |      129.16 |
| 2   |      194.160 |     135.454 |     4203.172 |    1808.119 |       436.92 |      187.95 |
| 4   |      279.157 |     233.920 |     4392.224 |    2158.726 |       456.57 |      224.39 |

“打开 ms”包含准备 RAM 字库副本的时间；“解码 ms”表示完整的无缓存解码过程；“µs/字形”为该时间除以 9620。具体耗时会受开发板、内存配置、编译器和 ESP-IDF 版本影响。

### 预备知识

- ESP-IDF release/v5.4 或更新版本，并已设置 `IDF_PATH`
- 了解 [`esp_compact_font`](../../README_CN.md)
- 具备 `esp_board_manager` 支持、且提供 `display_lcd` 的板级定义

### 文件结构

```text
font_benchmark/
├── main/
│   └── app_main.c              解码基准与 LCD 绘制
├── fonts/                      prepare_fonts.py 生成的字库
├── prepare_fonts.py            下载、子集化并转换语料
├── host_bench.c                主机端解码对照
├── host_bench.sh               编译主机对照程序并检查语料
├── host_codec.ld               保留主机端 codec 段
├── partitions.csv
├── sdkconfig.defaults
└── README.md
```

## 环境配置

### 硬件要求

- 支持 board-manager 并提供 `display_lcd` 的 ESP 开发板

### 其他要求

配置工程前先生成字库。构建过程不会下载字体。

```bash
python3 prepare_fonts.py
```

输出在 `fonts/`：`lvgl_{1,2,4}bpp.bin` 与 `gfh_{1,2,4}bpp.gfh`。构建时会嵌入这些文件。

在 PC 上检查同一套文件（无需 LCD）：

```bash
./host_bench.sh
```

脚本会把 `host_bench.c` 与组件源文件一起编译，对 `fonts/` 运行，并在字库缺失、字形解码失败或文件大小与上表不一致时失败。

## 编译和下载

### 默认 IDF 分支

本例程支持 IDF release/v5.4 及以后分支。默认使用 ADF 内建分支 `$ADF_PATH/esp-idf`。

### 配置

`sdkconfig.defaults` 打开性能优化、SPIRAM 分配、自定义分区表，以及 LVGL `.bin` 支持。

编译前生成 board-manager 配置：

```bash
idf.py gen-bmgr-config -b <your_board_name>
```

本例程需要打开 LVGL `.bin`。`sdkconfig.defaults` 里已经设置：

```text
ESP Compact Font > LVGL bin font
```

### 编译和下载

```bash
idf.py set-target esp32p4
idf.py gen-bmgr-config -b <your_board_name>
idf.py build flash monitor
```

有关配置和使用 ESP-IDF 生成项目的完整步骤，请前往 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)。

## 如何使用例程

### 例程功能

上电后，例程对每种 bpp 解码两遍（先把字库拷进 RAM，再就地读取 Flash），每种 codec 打一行 `BENCH`，每一对打一行 `COMPARE`。然后通过 `esp_board_manager` 打开 LCD，并在 22、36、48、64 像素之间循环。每种尺寸先画一行 LVGL、一行 GFH，再同时做高亮。

`with_ram=1` 表示拷贝到 RAM。`with_ram=0` 表示直接读嵌入的字库。`bad` 是解码失败的字形数。

### 参考

- 组件说明：[esp_compact_font](../../README_CN.md)

## 故障排除

- **`BENCH bit=N codec=lvgl missing` 或 `codec=gfh missing`**：先运行 `python3 prepare_fonts.py`，再重新编译，把字库嵌进固件。
- **`./host_bench.sh` 提示 `atlas not ready`**：`fonts/` 下的文件缺失或过小。在本目录运行 `prepare_fonts.py`。
- **LCD 初始化失败 / 没有 `display_lcd` 句柄**：对提供 `display_lcd` 的板子执行 `idf.py gen-bmgr-config -b <your_board_name>`，然后重新编译。

## 技术支持与反馈

请通过以下渠道反馈：

- 技术问题请到 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 功能需求或缺陷请提交 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
