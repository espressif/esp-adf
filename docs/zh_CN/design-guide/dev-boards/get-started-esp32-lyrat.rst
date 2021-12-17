ESP32-LyraT V4.3 入门指南
===========================

:link_to_translation:`en:[English]`

本指南旨在向用户介绍 ESP32-LyraT V4.3 音频开发版的功能、配置选项以及如何快速入门。如果您的开发板不是 4.3 版本，请前往 `其他 LyraT 开发板版本`_。

ESP32-LyraT 开发板面向搭载 ESP32 双核芯片的音频应用领域，如 Wi-Fi 音箱、蓝牙音箱、语音遥控器以及所有需要音频功能的智能家居应用。

ESP32-LyraT 是一款立体声音频开发板，如需单声道音频开发板，请前往 :doc:`get-started-esp32-lyrat-mini`。


准备工作
----------

* 1 × :ref:`ESP32 LyraT V4.3 开发板 <get-started-esp32-lyrat-v4.3-board>`
* 2 x 扬声器或 3.5 mm 的耳机（若使用扬声器，建议功率不超过 3 瓦特，另外需要接口为 JST PH 2.0 毫米 2 针的插头，若没有此插头，开发过程中可替换为杜邦母跳线）
* 2 x Micro-USB 2.0 数据线（Type A 转 Micro B）
* 1 × PC（Windows、 Linux 或 Mac OS）

如需立即使用此开发板，请直接前往 `正式开始开发`_。


概述
^^^^^^^

ESP32-LyraT V4.3 是一款基于 `乐鑫 <https://espressif.com>`_ ESP32 的开发板，专为音频应用市场打造，除 ESP32 芯片原有的硬件外，还提供了可用于音频处理硬件和扩展 RAM。具体包括以下硬件：

* ESP32-WROVER 模组
* 音频编解码芯片
* 板载双麦克风
* 耳机输出
* 2 个 3-watt 扬声器输出
* 双辅助输入
* MicroSD 卡槽（一线模式或四线模式）
* 6 个按键（2 个物理按键和 4 个触摸按键）
* JTAG 排针
* 集成 USB-UART 桥接芯片
* 锂电池充电管理

下图展示的是 ESP32-LyraT 的主要组件以及组件之间的连接方式。

.. figure:: ../../../_static/esp32-lyrat-v4.3-block-diagram.jpg
    :alt: ESP32 LyraT block diagram
    :figclass: align-center

    ESP32-LyraT 开发板框图


组件
^^^^^^

以下列表和图片仅涉及 ESP32-LyraT 的主要组件、接口和控制方式，仅展示目前所需的信息。欲访问详细的技术文档，请前往 :doc:`board-esp32-lyrat-v4.3` 和 `ESP32 LyraT V4.3 schematic`_ (PDF)。


ESP32-WROVER 模组
    ESP32-WROVER 模组采用 ESP32 芯片，可实现 Wi-Fi/蓝牙连接和数据处理，同时集成 32 Mbit SPI flash 和 32 Mbit PSRAM，可实现灵活的数据存储。
耳机输出
    输出插槽可连接 3.5 mm 立体声耳机。

    .. 注意::

        该插槽可接入移动电话耳机，只与 OMPT 标准耳机兼容，与 CTIA 耳机不兼容。更多耳机标准信息请访问维基百科 `Phone connector (audio) <https://en.wikipedia.org/wiki/Phone_connector_(audio)#TRRS_standards>`_ 词条。

.. _get-started-esp32-lyrat-v4.3-board:

.. figure:: ../../../_static/esp32-lyrat-v4.3-layout-overview.jpg
    :alt: ESP32 LyraT V4.3 Board Layout Overview
    :figclass: align-center

    ESP32-LyraT V4.3 开发板布局示意图

左侧扬声器输出
    音频输出插槽，采用 2.00 mm / 0.08" 排针间距，建议连接 4 欧姆 3 瓦特扬声器。
右侧扬声器输出
    音频输出插槽，采用 2.00 mm / 0.08" 排针间距，建议连接 4 欧姆 3 瓦特扬声器。
启动/复位按键
    启动: 长按 **Boot** 键，然后按下 **Reset** 键进入烧写模式，此时可通过串行端口上传固件。复位：仅按下 **Reset** 键只能重置系统。
音频编解码芯片
    `ES8388 <http://www.everest-semi.com/pdf/ES8388%20DS.pdf>`_ 音频编解码芯片是一款低功耗立体声编解码器，它由双通道 ADC、双通道 DAC、麦克风放大器、耳机放大器、数字音效处理器、模拟混音和增益控制功能组成。该芯片通过 I2S 和 I2C 总线与 **ESP32-WROVER 模组** 连接，可在芯片内独立完成音频处理，无需依赖音频应用软件。
USB-UART 接口
    作为 PC 和 ESP32 WROVER 模组之间的通信接口。
USB 供电接口
    为开发板供电。
待机/充电指示灯
    绿色 **待机** 指示灯亮起时，表示电源已接入 **Micro USB 接口**；红色 **充电** 指示灯亮起时，表示连接至 **电池接口** 上的电池正在充电。
电源开关
    电源开/关按钮：向左拨动按钮则开发板电源开启，向右拨动则电源关闭。
电源指示灯
    红色指示灯亮起表示 **电源开关** 已开启。


应用程序开发
---------------

ESP32-LyraT 上电之前，请首先确认开发板完好无损。


初始设置
^^^^^^^^^^^^^

设置开发板，运行首个示例应用程序：

1. 连接扬声器至 **两个扬声器输出**，或将耳机连接至 **耳机输出**。
2. 插入 Micro-USB 数据线，连接 PC 与 ESP32-LyraT 开发板的 **两个 USB 端口**。
3. 此时，绿色 **待机指示灯** 应亮起。若电池未连接，红色 **充电指示灯** 每隔几秒闪烁一次。
4. 向左拨动 **电源开关**。
5. 此时，红色 **电源指示灯** 应亮起。

如果指示灯如上述显示，则初始设置已经完成，开发板可用于下载应用程序。现在，请按下文介绍运行并配置 PC 上的开发工具。


正式开始开发
^^^^^^^^^^^^

若已完成初始设置，请准备开发工具。请前往 :ref:`get-started-step-by-step` 查看以下步骤的：

* **Set up ESP-IDF** 提供一套 ESP32 和 ESP32-S2 芯片的 C 语言 PC 开发编译环境；
* **Get ESP-ADF** 获取开发音频应用程序的 API；
* **Setup Path to ESP-ADF** 使开发框架获取到音频应用 API；
* **Start a Project** 为开发板提供音频应用程序示例；
* **Connect Your Device** 准备加载应用程序；
* **Build the Project** 运行应用程序，播放音乐。

与 LyraT V4.2 相比的主要变化
----------------------------

* 移除红色 LED 指示灯；
* 增添耳机插孔插入检测功能； 
* 使用两枚独立芯片代替单个功率放大器；
* 更新一些电路的功率管理设计：电池充电、ESP32、MicroSD、编解码芯片以及功率放大器；
* 更新一些电路的电器实施设计：UART、编解码芯片、左右两侧麦克风、AUX 输入、耳机输出、MicroSD、按键以及自动上传。


其他 LyraT 开发板版本
-----------------------

* :doc:`get-started-esp32-lyrat-v4.2`
* :doc:`get-started-esp32-lyrat-v4`

其他 LyraT 系列开发板
-----------------------

* :doc:`get-started-esp32-lyrat-mini`
* :doc:`get-started-esp32-lyratd-msc`

相关文档
----------

* :doc:`board-esp32-lyrat-v4.3`
* `ESP32 LyraT V4.3 schematic`_ (PDF)
* `ESP32-LyraT V4.3 Component Layout`_ (PDF)
* `ESP32 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_cn.pdf>`_ (PDF)
* `ESP32-WROVER 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32_wrover_datasheet_cn.pdf>`_ (PDF)


.. _ESP32 LyraT V4.3 schematic: https://dl.espressif.com/dl/schematics/esp32-lyrat-v4.3-schematic.pdf
.. _ESP32-LyraT V4.3 Component Layout: https://dl.espressif.com/dl/schematics/ESP32-LyraT_v4.3_component_layout.pdf
