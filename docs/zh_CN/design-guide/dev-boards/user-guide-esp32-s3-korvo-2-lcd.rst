=========================
ESP32-S3-Korvo-2-LCD V1.0
=========================

:link_to_translation:`en: [English]`

本指南将帮助您快速上手 ESP32-S3-Korvo-2-LCD 扩展板，并提供该款扩展板的详细信息。

本扩展板需与其他的 :ref:`ESP32-S3-Korvo-2 V3.0 配件 <esp32-s3-korvo-2-v3.0-accessories>` 一同购买，无法单独购买。

ESP32-S3-Korvo-2-LCD 增配 LCD 图形显示器和电容式触摸屏，扩展了 ESP32-S3-Korvo-2 V3.0（以下简称“主板”）的功能。

.. figure:: ../../../_static/esp32-s3-korvo-2-lcd-v1.0-overview.png
    :align: center
    :alt: ESP32-S3-Korvo-2-LCD V1.0

    ESP32-S3-Korvo-2-LCD V1.0

本指南包括如下内容：

- `入门指南`_：简要介绍了开发板和硬件、软件设置指南。
- `硬件概况`_：详细介绍了开发板的硬件。
- `相关文档`_：列出了相关文档的链接。


入门指南
========

ESP32-S3-Korvo-2-LCD 搭载 320x240 分辨率的 2.4 英寸 LCD 图形显示器和 10 点电容式触摸屏。该显示器通过 SPI 总线连接到 ESP32-S3。

组件介绍
--------

.. figure:: ../../../_static/esp32-s3-korvo-2-lcd-v1.0-front.png
    :scale: 60%
    :align: center
    :alt: ESP32-S3-Korvo-2-LCD V1.0 - 正面

    ESP32-S3-Korvo-2-LCD V1.0 - 正面

.. figure:: ../../../_static/esp32-s3-korvo-2-lcd-v1.0-back.png
    :scale: 70%
    :align: center
    :alt: ESP32-S3-Korvo-2-LCD V1.0- 背面

    ESP32-S3-Korvo-2-LCD V1.0 - 背面

以下按照顺时针的顺序依次介绍开发板上的主要组件。**保留** 表示该功能可用，但当前版本并未启用该功能。

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - 主要组件
     - 介绍
   * - LCD 显示器 (LCD Display)
     - 2.4 英寸 320x240 SPI LCD 显示模块，显示驱动器/控制器是 Ilitek ILI934。
   * - Home 键 (Home Key) 
     - （保留）返回首页或上一页。 
   * - 信号连接器 (Signal Connector)
     - 借助 FPC 排线连接扩展板和主板之间的电源线、地线和信号线。
   * - LCD 连接器 (LCD Connector)
     - 将 LCD 显示器连接到扩展板的驱动电路。
   * - TP 连接器 (TP Connector)
     - 将 LCD 显示器连接到扩展板的触摸电路。

开始开发应用
-------------

通电前，请确保开发板完好无损。


必备硬件
^^^^^^^^

- 主板：ESP32-S3-Korvo-2 V3.0
- 扩展板：ESP32-S3-Korvo-2-LCD V1.0
- 两根 USB 2.0 数据线（标准 A 型转 Micro-B 型）
- 安装螺栓和螺丝（用于稳定安装）
- FPC 排线（用于连接主板和扩展板）
- 电脑（Windows、Linux 或 macOS）


硬件设置
^^^^^^^^

按照以下步骤将 ESP32-S3-Korvo-2-LCD 安装到 ESP32-S3-Korvo-2 上：

1. 使用 FPC 线连接两块开发板。
2. 安装螺栓和螺丝，保证安装稳定。 


软件设置
^^^^^^^^

请参考主板的用户指南的 :ref:`软件设置 <esp32-s3-korvo-2-v3.0-software-setup>` 章节。


硬件概况
========


功能框图
--------

ESP32-S3-Korvo-2-LCD 的主要组件和连接方式如下图所示。

.. figure:: ../../../_static/esp32-s3-korvo-2-lcd-v1.0-electrical-block-diagram.png
    :align: center
    :alt: ESP32-S3-Korvo-2-LCD

    ESP32-S3-Korvo-2-LCD


硬件版本
============

无历史版本。


相关文档
========

- :doc:`ESP32-S3-Korvo-2 V3.0 用户指南 <user-guide-esp32-s3-korvo-2>`
- `ESP32-S3-Korvo-2-LCD 原理图 <https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-KORVO-2-LCD_V1.0_20210918.pdf>`_ (PDF)
- `ESP32-S3-Korvo-2-LCD PCB 布局图 <https://dl.espressif.com/dl/schematics/PCB_ESP32-S3-KORVO-2-LCD_V1.0_20210918.pdf>`_ (PDF)

有关本开发板的更多设计文档，请联系我们的商务部门 `sales@espressif.com <sales@espressif.com>`_。