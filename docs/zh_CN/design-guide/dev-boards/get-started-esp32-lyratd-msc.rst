ESP32-LyraTD-MSC V2.2 入门指南
===========================================

:link_to_translation:`en:[English]`

本指南旨在向用户介绍 ESP32-LyraTD-MSC V2.2 音频开发板的功能、配置选项以及如何快速入门。

ESP32-LyraTD-MSC 是一款面向智能音箱和 AI 应用程序的硬件平台，支持声学回声消除 (Acoustic Echo Cancelation, AEC)、 自动语音识别 (Automatic Speech Recognition, ASR)、打断唤醒 (Wake-up Interrupt) 以及语音互动 (Voice Interaction) 功能。

准备工作
-------------

* 1 × :ref:`ESP32-LyraTD-MSC V2.2 board <get-started-esp32-lyratd-msc-v2.2-board>`
* 2 x 扬声器或 3.5 mm 的耳机（若使用扬声器，建议功率不超过 3 瓦特，另外需要接口为 JST PH 2.0 毫米 2 针的插头，若没有此插头，开发过程中可替换为杜邦母跳线）
* 2 x Micro-USB 2.0 数据线（Type A 转 Micro B）
* 1 × PC（Windows、 Linux 或 Mac OS）

如需立即使用此开发板，请直接前往 `正式开始开发`_。


概述
^^^^^^^^

ESP32-LyraTD-MSC V2.2 是一款基于 `乐鑫 <https://espressif.com>`_ ESP32 芯片的音频开发板，专为智能音箱和 AI 应用程序打造， 除 ESP32 芯片原有的硬件外，还增添了数字信号处理、麦克风阵列以及扩展 RAM。 

ESP32-LyraTD-MSC V2.2 由上板和下板两部分组成，上板 (B) 集成三麦克风阵列、功能按键和 LED 灯，下板 (A) 集成 ESP32-WROVER-B 模组、MicroSemi Digital Signal Processing (DSP) 芯片以及电源管理模块。

.. _get-started-esp32-lyratd-msc-v2.2-board:

.. figure:: ../../../_static/esp32-lyratd-msc-v2.2-side.png
    :alt: ESP32-LyraTD-MSC Side View
    :figclass: align-center

    ESP32-LyraTD-MSC 侧面图

具体包括以下硬件：

* ESP32-WROVER-B 模组
* DSP 芯片
* 3 颗支持远场语音唤醒功能的麦克风
* 2 个 3-watt 扬声器输出
* 耳机输出
* MicroSD 卡槽（一线模式或四线模式）
* 围绕开发板边缘分布的 12 个 LED 灯，可独立控制
* 6 个功能按键，可配置不同功能
* 多个端口：I2S、I2C、SPI、JTAG
* 集成 USB-UART 桥接芯片
* 锂电池充电管理

下图展示的是 ESP32-LyraTD-MSC 的主要组件以及组件之间的连接方式。

.. figure:: ../../../_static/esp32-lyratd-msc-v2.2-block-diagram.png
    :alt: ESP32-LyraTD-MSC block diagram
    :figclass: align-center

    ESP32-LyraTD-MSC 开发板框图


组件
^^^^^^^^^^

以下列表和图片仅涉及 ESP32-LyraTD-MSC 的主要组件、接口和控制方式，只展示目前所需的信息，更多细节请参阅 `相关文档`_ 中的原理图。

ESP32-WROVER-B 模组
    The ESP32-WROVER-B 模组采用 ESP32 芯片，可实现 Wi-Fi/蓝牙连接和数据处理，同时集成 32 Mbit SPI flash 和 64 Mbit PSRAM，可实现灵活的数据存储。
DSP 芯片
    `ZL38063 <https://www.microsemi.com/document-portal/doc_download/136798-zl38063-datasheet>`_ DSP 芯片用于自动语音识别应用程序，可从外部麦克风阵列获取音频数据，并通过自身 DAC 端口输出音频信号。
耳机输出
    输出插槽可连接 3.5 mm 立体声耳机。

    .. 注意::

        该插槽可接入移动电话耳机，只与 OMPT 标准耳机兼容，与 CTIA 耳机不兼容。更多耳机标准信息请访问维基百科 `Phone connector (audio) <https://en.wikipedia.org/wiki/Phone_connector_(audio)#TRRS_standards>`_ 词条。

左侧扬声器输出
    音频输出插槽，采用 2.00 mm / 0.08" 排针间距，建议连接 4 欧姆 3 瓦特扬声器。
右侧扬声器输出
    音频输出插槽，采用 2.00 mm / 0.08" 排针间距，建议连接 4 欧姆 3 瓦特扬声器。

.. figure:: ../../../_static/esp32-lyratd-msc-v2.2-a-top.png
    :alt: ESP32-LyraTD-MSC V2.2 Lower Board (A) Components
    :figclass: align-center

    ESP32-LyraTD-MSC V2.2 下板 (A) 组件图

USB-UART 接口
    作为 PC 和 ESP32-WROVER-B 模组之间的通信接口。
USB 供电接口
    为开发板供电。
待机/充电指示灯
    绿色 **待机** 指示灯亮起时，表示电源已接入 **Micro USB 接口**；红色 **充电** 指示灯亮起时，表示连接至 **电池接口** 上的电池正在充电。
电源开关
    电源开/关按钮：向右拨动按钮则开发板电源开启，向左拨动则电源关闭。
电源指示灯
    红色指示灯亮起表示 **电源开关** 已开启。

.. figure:: ../../../_static/esp32-lyratd-msc-v2.2-b-top.png
    :alt: ESP32-LyraTD-MSC V2.2 Upper Board (B) Components
    :figclass: align-center

    ESP32-LyraTD-MSC V2.2 上板 (B) 组件图

启动/复位按键
   启动：长按 **Boot** 键，然后点按 **Reset** 键进入烧写模式，此时可通过串行端口上传固件。

   复位：仅按下 **Reset** 键只能重置系统。


应用程序开发
-----------------------------

ESP32-LyraTD-MSC 上电之前，请首先确认开发板完好无损，且上板 (B) 和下板 (A) 紧紧固定在一起。


初始设置
^^^^^^^^^^^^^

设置开发板，以运行首个示例应用程序：

1. 将扬声器连接至 **两个扬声器输出**，或将耳机连接至 **耳机输出**。
2. 插入 Micro-USB 数据线，连接 PC 与 ESP32-LyraTD-MSC 开发板的 **两个 USB 端口**。
3. 此时，绿色 **待机指示灯** 应亮起。若电池未连接，红色 **充电指示灯** 每隔几秒闪烁一次。
4. 向右拨动 **电源开关**。
5. 此时，红色 **电源指示灯** 应亮起。

如果指示灯如上述显示，则初始设置已经完成，开发板可用于下载应用程序。现在，请按下文介绍运行并配置 PC 上的开发工具。


正式开始开发
^^^^^^^^^^^^^^^^^^^^

若已完成初始设置和检查工作，请准备开发工具，请前往 :ref:`get-started-step-by-step` 查看以下步骤：

* **Set up ESP-IDF** 提供一套 ESP32 和 ESP32-S2 芯片的 C 语言 PC 开发编译环境；
* **Get ESP-ADF**  获取开发音频应用程序的 API；
* **Setup Path to ESP-ADF** 使开发框架获取到音频应用 API；
* **Start a Project** 为开发板提供音频应用程序示例；
* **Connect Your Device** 准备加载应用程序；
* **Build the Project** 运行应用程序，播放音乐。


其他 LyraT 系列开发板
------------------------------

* :doc:`get-started-esp32-lyrat`
* :doc:`get-started-esp32-lyrat-mini`

相关文档
-----------------

* `ESP32-LyraTD-MSC V2.2 Schematic Lower Board (A)`_ (PDF)
* `ESP32-LyraTD-MSC V2.2 Schematic Upper Board (B)`_ (PDF)
* `ESP32 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_cn.pdf>`_ (PDF)
* `ESP32-WROVER-B 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32-wrover-b_datasheet_cn.pdf>`_ (PDF)


.. _ESP32-LyraTD-MSC V2.2 Schematic Lower Board (A): https://dl.espressif.com/dl/schematics/ESP32-LyraTD-MSC_A_V2_2-1109A.pdf
.. _ESP32-LyraTD-MSC V2.2 Schematic Upper Board (B): https://dl.espressif.com/dl/schematics/ESP32-LyraTD-MSC_B_V1_1-1109A.pdf
