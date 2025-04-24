*******************
ESP FOURCC 编码介绍
*******************

:link_to_translation:`en:[English]`

FOURCC（Four Character Code，四字符编码）是一种在多媒体领域被广泛使用的标识符，常用于快速识别音视频编码格式、封装格式、像素格式等。乐鑫科技定义了一套完整的 FOURCC 编码标准，称为 ESP FOURCC，该标准可在开源项目 `ESP-GMF <https://github.com/espressif/esp-gmf/tree/main>`__ 中的 `esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__ 头文件中找到，涵盖了音视频编解码、图像、封装格式和各种像素格式。本文将深入介绍 ESP FOURCC 的背景意义、定义原则、实现方式和使用优势。

FOURCC 的背景与意义
---------------------

FOURCC 的历史可以追溯到 1980 年代 Macintosh 系统中的 OSType 机制，它使用四个字符来标识文件类型。到了 1990 年代，QuickTime 将其引入音视频领域，随后成为 MP4 容器、FFmpeg 等主流框架中的核心识别方式之一。

在资源受限的嵌入式系统中，内存和运算效率至关重要，因此采用紧凑的标识符显得尤为关键。若为每种数据格式定义独立的枚举值或常量，往往会引发以下问题：

1. **缺乏标准化**：若各模块独立定义格式标识符，相同的数据格式可能在不同模块中对应不同的枚举值，增加了接口适配难度。
2. **高转换开销**：在模块之间传递数据时，若标识符不一致，则需进行额外的转换操作，导致运行时开销增加。
3. **调试困难**：各模块使用不同标识方式时，在调试中难以直观判断实际数据格式，增加问题定位难度。

相比之下，FOURCC 机制通过统一的 32 位标识符解决了这些问题。它不仅能确保各模块间格式一致性，还可在编译期生成常量，便于快速比较与查找，大幅提升系统的性能与可维护性。

FOURCC 的基本原理与实现
-------------------------

1. FOURCC 的核心机制
~~~~~~~~~~~~~~~~~~~~~

FOURCC 是一种基于 ASCII 字符的紧凑编码方案，其核心原理是将 **连续四个字符** 转换为一个 **32 位无符号整数** （`uint32_t`）。例如字符串 `"H264"` 对应的十六进制值为 `0x48323634`，各字符的 ASCII 值依次组合如下：

- `'H'` → `0x48`
- `'2'` → `0x32`
- `'6'` → `0x36`
- `'4'` → `0x34`

2. ESP FOURCC 的实现
~~~~~~~~~~~~~~~~~~~~~

在 ESP-GMF 中，FOURCC 的定义主要集中在 `esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__ 头文件中，主要包括以下内容：

- **宏定义**

  ESP-GMF 提供了 ``ESP_FOURCC_TO_INT(a, b, c, d)`` 宏，支持在编译期间生成 FOURCC 值。例如：

  .. code:: c

     #define ESP_FOURCC_H264   ESP_FOURCC_TO_INT('H','2','6','4') // 0x48323634

- **辅助函数**

  该头文件中还定义了 ``ESP_FOURCC_TO_STR(fourcc)`` 宏，它会调用 ``gmf_fourcc_to_str`` 函数，将 ``uint32_t`` 类型的 FOURCC 转换为字符串，例如：

  .. code:: c

     printf("Format: %s\n", ESP_FOURCC_TO_STR(0x48323634)); // "H264"

- **FOURCC 类型定义**

  ESP FOURCC v1.0.0 定义了嵌入式多媒体常用的音视频编解码器、封装格式、像素格式等，具体内容详见 `esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__ 文件。

多媒体 FOURCC 编码格式
------------------------

`esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__ 文件定义了常用的视频编解码格式、音频编解码格式、封装格式及图像编码格式。以下为主要类型的汇总表格：

.. list-table:: ESP FOURCC 编码分类与字符表示表
   :header-rows: 1
   :widths: 20 20 20 40
   :align: center

   * - **分类**
     - **FOURCC 示例**
     - **字符表示**
     - **编码标准 / 技术特征**
   * - **视频编解码**
     - ``ESP_FOURCC_H264``
     - ``'H264'``
     - H.264/AVC（包含 start code）
   * -
     - ``ESP_FOURCC_AVC1``
     - ``'AVC1'``
     - H.264（无 start code，MP4 兼容）
   * -
     - ``ESP_FOURCC_H265``
     - ``'H265'``
     - HEVC/H.265（高压缩率）
   * - **封装格式**
     - ``ESP_FOURCC_MP4``
     - ``'MP4 '``
     - MPEG-4 第 14 部分（多轨封装）
   * -
     - ``ESP_FOURCC_OGG``
     - ``'OGG '``
     - Ogg 封装格式
   * -
     - ``ESP_FOURCC_FLV``
     - ``'FLV '``
     - Flash 视频（流式优化）
   * - **图像编码**
     - ``ESP_FOURCC_PNG``
     - ``'PNG '``
     - 无损压缩，支持透明通道
   * -
     - ``ESP_FOURCC_JPEG``
     - ``'JPEG'``
     - 有损压缩，压缩率高
   * -
     - ``ESP_FOURCC_WEBP``
     - ``'WEBP'``
     - 混合有损/无损压缩，适合体积优化
   * - **音频编解码**
     - ``ESP_FOURCC_MP3``
     - ``'MP3 '``
     - MPEG Layer III，兼容性好
   * -
     - ``ESP_FOURCC _AAC``
     - ``'AAC '``
     - 高效压缩的 AAC 编码
   * -
     - ``ESP_FOURCC_OPUS``
     - ``'OPUS'``
     - 低延迟动态码率，适用于实时通信

FOURCC 像素格式
-----------------

`esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__ 中还定义了 RGB、YUV、灰度及特殊格式的 FOURCC 像素格式，以及相应的数据存储方式。以下为常见格式汇总表：

.. list-table:: ESP FOURCC 像素格式分类表
   :header-rows: 1
   :widths: 20 20 20 40
   :align: center

   * - **分类**
     - **FOURCC 示例**
     - **字符表示**
     - **技术特征**
   * - **RGB 格式**
     - ``ESP_FOURCC_RGB16``
     - ``'RGB6'``
     - RGB-5-6-5 小端存储，16 位压缩（低带宽）
   * -
     - ``ESP_FOURCC_RGB24``
     - ``'RGB3'``
     - RGB-8-8-8 打包，24 位真彩色
   * - **YUV 格式**
     - ``ESP_FOURCC_NV12``
     - ``'NV12'``
     - YUV 4:2:0 半平面（Y 分量 + UV 交错）
   * -
     - ``ESP_FOURCC_YUYV``
     - ``'YUYV'``
     - YUV 4:2:2 打包（Y-U-Y-V 交错）
   * -
     - ``ESP_FOURCC_YUV420P``
     - ``'I420'``
     - YUV 4:2:0 平面格式（Y + U + V 三个平面）
   * - **灰度图像**
     - ``ESP_FOURCC_GREY``
     - ``'GREY'``
     - 8 位单通道灰度图像
   * - **特殊格式**
     - ``ESP_FOURCC_RAW16``
     - ``'RAW6'``
     - 16 位原始数据，小端格式存储

实际应用案例
--------------

在 ESP-GMF 框架中，各模块（如视频解码器、音频处理器、图像滤镜等）均使用 FOURCC 编码进行能力声明与接口匹配。例如，视频解码模块会声明其输入/输出所支持的 FOURCC 格式，调度器根据这些编码格式匹配模块间的数据流，从而实现高效连接。
