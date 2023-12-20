ESP32-LyraT-Mini V1.2 入门指南
===========================================

:link_to_translation:`en:[English]`

本文档为用户提供 ESP32-LyraT-Mini v1.2 音频开发板的功能描述、配置选项以及如何快速入门使用 ESP32-LyraT 开发板。

ESP32-LyraT 是一个基于双核 ESP32 用于开发音频应用程序的硬件平台，例如 Wi-Fi音箱、蓝牙音箱、语音遥控器以及有一个或多个音频功能的智能家居应用等。

ESP32-LyraT-Mini 是单声道音频开发板，如需立体声音频开发板，请前往 :doc:`get-started-esp32-lyrat`。


准备工作
-------------

* :ref:`ESP32-LyraT-Mini V1.2 开发板 <get-started-esp32-lyrat-mini-v1.2-board>`
* 扬声器或 3.5 mm 的耳机（若使用扬声器，建议功率不超过 3 瓦特，另外需要接口为 JST PH 2.0 毫米 2 针的插头，若没有此插头，开发过程中可替换为杜邦母跳线）
* 两个 Micro-USB 2.0 数据线（Type A 到 Micro B）
* PC（Windows、Linux 或 Mac OS） 

可选组件

* Micro SD 卡
* 锂电池

如需立即使用此开发板，请直接前往 `应用程序开发`_。 

概述
^^^^^^^^

ESP32-LyraT-Mini 1.2 是基于 `乐鑫 <https://espressif.com>`_ ESP32 专为音频应用市场打造的音频开发板。其主要包括了音频编解码芯片，外置扩展 RAM 和双核 ESP32 芯片。 硬件具体包括：

* **ESP32-WROVER-E 模组**
* **音频编解码芯片**
* **ADC 芯片**
* **麦克风**
* **音频输出**
* **1 x 3 瓦特扬声器** 输出
* **MicroSD 卡** 槽（一线模式） 
* **八个键**
* **两个系统指示灯**
* **JTAG** 和 **UART** 测试点
* 集成 **USB-UART 桥接芯片**
* 锂 **电池充电管理**

下图所示的是 ESP32-LyraT-Mini 的主要组件以及组件之间的连接方式。

.. figure:: ../../../_static/esp32-lyrat-mini-v1.2-block-diagram.png
    :alt: ESP32-LyraT-Mini block diagram
    :figclass: align-center

    ESP32-LyraT-Mini 框图


组件
^^^^^^^^^^

本指南涉及到的 ESP32-LyraT-Mini 开发板的主要组件、接口及控制方式见下。详细技术文档请参阅 :doc:`board-esp32-lyrat-mini-v1.2` 和 `ESP32-LyraT-Mini V1.2 原理图`_ (PDF)。ESP32-LyraT-Mini 开发板各组件的详细描述见下表（从右上角起顺时针顺序）。

音频编解码芯片
	音频编解码芯片 `ES8311 <http://www.everest-semi.com/pdf/ES8311%20PB.pdf>`_ 是一款低功耗单声道音频编解码器。它由单通道 ADC、单通道 DAC、低噪声前置放大器、耳机驱动程序、数字音效处理器、模拟混音器和增益函数组成。该芯片通过 I2S 和 I2C 总线与 **ESP32-WROVER-E 模组** 连接提供硬件音频处理功能。
音频输出
	音频输出插槽，可连接 3.5 mm 立体声耳机。（请注意，此开发板输出单声道信号）
扬声器输出
	音频输出插槽，采用 2.00 mm / 0.08" 排针间距，建议连接 4 欧姆 3 瓦特扬声器。

.. _get-started-esp32-lyrat-mini-v1.2-board:

.. figure:: ../../../_static/esp32-lyrat-mini-v1.2-layout-overview.png
    :scale: 70%
    :alt: ESP32 LyraT-Mini V1.2 Board Layout Overview
    :figclass: align-center

    ESP32 LyraT-Mini V1.2 开发板布局概览

USB-UART 接口
	作为 PC 与 ESP32 之间的通信接口。
USB 充电端口
	可用作开发板的供电电源。
待机/充电指示灯
	绿色 **待机** 指示灯亮起表示电源已接入 **USB 充电端口**。红色 **充电** 指示灯亮起表示 **电池座** 上的电池正在充电。
电源开关
	电源开/关按钮：向上拨动按钮则开发板电源开启；向下拨动则电源关闭。
电源指示灯
	红色指示灯亮起表示 **电源开关** 已开启。
ESP32-WROVER-E 模组
    ESP32-WROVER-E 模组采用 ESP32 芯片，可实现 Wi-Fi/蓝牙连接和数据处理，同时集成 4 MB 外部 SPI flash 和 8 MB SPI PSRAM，可实现灵活的数据存储。


应用程序开发
-----------------------------

ESP32-LyraT-Mini 上电之前，请首先确认开发板完好无损。


初始设置
^^^^^^^^^^^^^

设置开发板，运行首个示例应用程序：

1. 连接扬声器到 **扬声器输出**。也可选择连接耳机到 **音频输出**。 
2. 使用 Micro-USB 数据线将 ESP32-LyraT-Mini 开发板的 **两个 USB 端口** 均与 PC 相连。
3. 此时，绿色 **待机指示灯** 应亮起。假设电池未连接，那么红色 **充电指示灯** 将每隔几秒闪烁一次。
4. 向上拨动 **电源开关**。
5. 此时，红色 **电源指示灯** 应亮起。

如果指示灯如上述显示，则此开发板基本完好，可用于下载应用程序。现在，请按下文介绍运行并配置 PC 上的开发工具。


正式开始开发
^^^^^^^^^^^^^^^^^^^^

如果 ESP32 LyraT 的初始设置已检查完成，请准备开发工具。前往 :doc:`index` 查看以下步骤：

* :ref:`get-started-setup-esp-idf` 提供了一套 ESP32 的 C 语言 PC 开发编译环境；
* :ref:`get-started-get-esp-adf` 获取开发音频应用程序的 API；
* :ref:`get-started-set-up-env` 使编译器找到音频应用 API；
* :ref:`get-started-start-project` 提供 ESP32-LyraT-Mini 开发板的音频应用程序示例；
* :ref:`get-started-connect` 准备加载应用程序；
* :ref:`get-started-build` 最后运行应用程序并播放音乐。


修订历史
--------

* 板上模组从 ESP32-WROVER-B 更新为 ESP32-WROVER-E。


其他 LyraT 系列开发板
------------------------------

* :doc:`get-started-esp32-lyrat`
* :doc:`get-started-esp32-lyratd-msc`

相关文档
-----------------

* `ESP32-LyraT-Mini V1.2 原理图`_ (PDF)
* `ESP32-LyraT-Mini V1.2 尺寸图 <https://dl.espressif.com/dl/schematics/Layout_ESP32-LyraT-Mini_V1.2_20220317.pdf>`_ (PDF)
* :doc:`board-esp32-lyrat-mini-v1.2`
* `ESP32 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf>`_ (PDF)
* `ESP32-WROVER-E 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32-wrover-e_esp32-wrover-ie_datasheet_cn.pdf>`_ (PDF)


.. _ESP32-LyraT-Mini V1.2 原理图: https://dl.espressif.com/dl/schematics/SCH_ESP32-LyraT-Mini_V1.2_20220119.pdf
