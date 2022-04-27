=====================
ESP32-S3-Korvo-2 V3.0
=====================

:link_to_translation:`en: [English]`

本指南将帮助您快速上手 ESP32-S3-Korvo-2 V3.0，并提供该款开发板的详细信息。

ESP32-S3-Korvo-2 是一款基于 ESP32-S3 芯片的多媒体开发板，搭载双麦克风阵列，支持语音识别和近/远场语音唤醒。同时它还搭载 LCD、摄像头、microSD 卡等外设，可支持基于 JPEG 的视频流处理，满足用户对低成本、低功耗、联网的音视频产品开发需求。

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-overview.png
    :align: center
    :scale: 45%
    :alt: ESP32-S3-Korvo-2 V3.0（板载 ESP32-S3-WROOM-1 模组）
    
    ESP32-S3-Korvo-2 V3.0（板载 ESP32-S3-WROOM-1 模组）

ESP32-S3-Korvo-2 开发板主要由以下几个部分组成：

- 主板：ESP32-S3-Korvo-2
- LCD 扩展板：:doc:`ESP32-S3-Korvo-2-LCD <user-guide-esp32-s3-korvo-2-lcd>`
- 摄像头

本文档主要介绍 ESP32-S3-Korvo-2 主板，有关其他部分的信息请点击相应的链接。

本指南包括如下内容：

- `入门指南`_：简要介绍了开发板和硬件、软件设置指南。
- `硬件参考`_：详细介绍了开发板的硬件。
- `硬件版本`_：介绍硬件历史版本和已知问题，并提供链接至历史版本开发板的入门指南（如有）。
- `相关文档`_：列出了相关文档的链接。


入门指南
========

本小节将简要介绍 ESP32-S3-Korvo-2 V3.0，说明如何在 ESP32-S3-Korvo-2 V3.0 上烧录固件及相关准备工作。


组件介绍
--------

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0.png
    :align: center
    :scale: 70%
    :alt: ESP32-S3-Korvo-2 V3.0（点击放大）
    
    ESP32-S3-Korvo-2 V3.0（点击放大）

以下按照顺时针的顺序依次介绍开发板上的主要组件。

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - 主要组件
     - 介绍
   * - ESP32-S3-WROOM-1 模组
     - ESP32-S3-WROOM-1 模组是两款通用型 Wi-Fi + 低功耗蓝牙 MCU 模组，搭载 ESP32-S3 系列芯片。除具有丰富的外设接口外，模组还拥有强大的神经网络运算能力和信号处理能力，适用于 AIoT 领域的多种应用场景，例如唤醒词检测和语音命令识别、人脸检测和识别、智能家居、智能家电、智能控制面板、智能扬声器等。
   * - 左侧麦克风 (Left Microphone)
     - 板载麦克风，连接至 ADC。
   * - 音频模数转换器 (Audio ADC Chip）
     - `ES7210 <http://www.everest-semi.com/pdf/ES7210%20PB.pdf>`_ 是一款用于麦克风阵列应用的高性能、低功耗 4 通道音频模数转换器，非常适合音乐和语音应用。此外，ES7210 也可以用作声学回声消除 (AEC)。
   * - 音频编解码芯片 (Audio Codec Chip)
     - 音频编解码器芯片 `ES8311 <http://www.everest-semi.com/pdf/ES8311%20PB.pdf>`_ 是一种低功耗单声道音频编解码器，包含单通道 ADC、单通道 DAC、低噪声前置放大器、耳机驱动器、数字音效、模拟混音和增益功能。它通过 I2S 和 I2C 总线与 ESP32-S3-WROOM-1 模组连接，以提供独立于音频应用程序的硬件音频处理。
   * - 音频功率放大器 (Audio PA Chip)
     - NS4150 是一款低 EMI、3 W 单声道 D 类音频功率放大器，用于放大来自音频编解码芯片的音频信号，以驱动扬声器。
   * - 右侧麦克风 (Right Microphone)
     - 板载麦克风，连接至 ADC。
   * - 扬声器输出端口 (Speaker Output Port)
     - 可通过音频功率放大器的支持，实现外部扬声器播放功能。
   * - USB-to-UART 桥接器 (USB-to-UART Bridge Chip)
     - 单芯片 USB-UART 桥接器 CP2102N 为软件下载和调试提供高达 3 Mbps 的传输速率。
   * - USB-to-UART 端口 (USB-to-UART Port)
     - 用于 PC 端与 ESP32-S3-WROOM-1 模组的通信。
   * - USB 电源端口 (USB Power Port)
     - 为整个系统提供电源。建议使用至少 5V/2A 电源适配器供电，保证供电稳定。
   * - 电池接口 (Battery Socket)
     - 用于连接单节锂离子电池的两针接口。
   * - 电源开关 (Power Switch)
     - 电源拨动开/关：向下拨动开启开发板电源，向上拨动关闭开发板电源。
   * - 电池充电管理芯片 (Battery Charger)
     - 恒流/恒压线性电源管理芯片 AP5056 可以用于单节锂离子电池的充电管理，为通过 Micro USB 端口连接到电池接口的电池充电。
   * - 功能按键 (Function Press Keys)
     - 六个按键，分别为 REC、MUTE、PLAY、SET、VOL- 和 VOL+，与 ESP32-S3-WROOM-1 模组连接，借助该 UI 和专用的 API 可以开发和测试音频应用程序。
   * - Boot/Reset 按键 (Boot/Reset Press Keys)
     - | Boot：长按 Boot 键时，再按 Reset 键可启动固件上传模式，然后便可通过串口上传固件。
       | Reset：单独按下此按键会重置系统。
   * - microSD 插槽 (MicroSD Slot)
     - 本开发板支持一线模式的 microSD 卡，可以存储或播放 microSD 卡中的音频文件。
   * - LCD 连接器 (LCD Connector)
     - 一款 0.5 mm 间距的 FPC 连接器，用以连接 LCD 扩展板。
   * - 系统 LED (System LEDs)
     - 两个通用 LED（绿色和红色），由 ESP32-S3-WROOM-1 模组控制，可借助专用的 API 为音频应用程序做状态行为指示。
   * - 摄像头连接器 (Camera Connector)
     - 通过连接器外接摄像头模组至开发板，实现图像传输。


开始开发应用
-------------

通电前，请确保开发板完好无损。

必备硬件
^^^^^^^^

- 1 x ESP32-S3-Korvo-2 V3.0
- 1 x 扬声器
- 2 x USB 2.0 数据线（标准 A 型转 Micro-B 型）
- 1 x 电脑（Windows、Linux 或 macOS）

.. note::

  请确保使用适当的 USB 数据线。部分数据线仅可用于充电，无法用于数据传输和编程。

可选硬件
^^^^^^^^

- 1 x microSD 卡
- 1 x 锂离子电池

.. note::

  请务必使用内置保护电路的锂离子电池。

硬件设置
^^^^^^^^

1. 连接扬声器至 **扬声器输出** 端口。
2. 插入 USB 数据线，分别连接 PC 与开发板的两个 USB 端口。
3. 此时，绿色待机指示灯应亮起。若电池未连接，红色充电指示灯每隔几秒闪烁一次。
4. 打开 **电源开关**。
5. 此时，红色电源指示灯应亮起。


.. _esp32-s3-korvo-2-v3.0-software-setup:

软件设置
^^^^^^^^

请前往 :doc:`../../get-started/index`，在 :ref:`get-started-step-by-step` 小节查看如何快速设置开发环境，并将 :adf:`应用程序示例 <examples>` 烧录至您的开发板。

内含组件和包装
---------------

.. _esp32-s3-korvo-2-v3.0-accessories:

可分开购买主板或主板配件，其中配件包含：

- LCD 扩展板：ESP32-S3-Korvo-2-LCD
- 摄像头
- 连接器 

  - 20 针 FPC 线

- 紧固件：
  
  - 安装螺栓 (x8)
  - 螺丝 (x4)

零售订单
^^^^^^^^

如购买样品，每个开发板将以防静电袋或零售商选择的其他方式包装。

零售订单请前往 https://www.espressif.com/zh-hans/company/contact/buy-a-sample。


批量订单
^^^^^^^^

如批量购买，开发板将以大纸板箱包装。

批量订单请前往 https://www.espressif.com/zh-hans/contact-us/sales-questions。


硬件参考
========


功能框图
--------

ESP32-S3-Korvo-2 V3.0 的主要组件和连接方式如下图所示。

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-electrical-block-diagram.png
    :align: center
    :scale: 55%
    :alt: ESP32-S3-Korvo-2 V3.0 电气功能框图
    
    ESP32-S3-Korvo-2 V3.0 电气功能框图


供电说明
--------

USB 与电池供电
^^^^^^^^^^^^^^

主电源为 5 V，由 USB 提供。辅助电源为 3.7 V，由电池提供，为可选项。USB 供电使用专用的数据线，与用于上传应用程序的 USB 数据线分开。为了近一步减少来自 USB 的噪音，可使用电池代替 USB。

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-usb-ps.png
    :align: center
    :scale: 40%
    :alt: ESP32-S3-Korvo-2 V3.0 - USB 电源供电
    
    ESP32-S3-Korvo-2 V3.0 - USB 电源供电

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-battery-ps.png
    :align: center
    :scale: 40%
    :alt: ESP32-S3-Korvo-2 V3.0 - 电池供电
    
    ESP32-S3-Korvo-2 V3.0 - 电池供电

如下图所示，当 USB 供电和电池供电同时存在时，VBUS 为高电平，Q14 处于截止状态，VBAT 自动与系统电源切断。此时，USB 为系统供电。

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-ps-options.png
    :align: center
    :scale: 40%
    :alt: ESP32-S3-Korvo-2 V3.0 - 供电选项
    
    ESP32-S3-Korvo-2 V3.0 - 供电选项

音频和数字独立供电
^^^^^^^^^^^^^^^^^^^^^^^^

ESP32-S3-Korvo-2 V3.0 可为音频组件和 ESP 模组提供相互独立的电源，可降低数字组件给音频信号带来的噪声并提高组件的整体性能。

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-digital-ps.png
    :align: center
    :scale: 40%
    :alt: ESP32-S3-Korvo-2 V3.0 - 数字供电
    
    ESP32-S3-Korvo-2 V3.0 - 数字供电

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-audio-ps.png
    :align: center
    :scale: 40%
    :alt: ESP32-S3-Korvo-2 V3.0 - 音频供电
    
    ESP32-S3-Korvo-2 V3.0 - 音频供电


GPIO 分配列表
-----------------------

下表为 ESP32-S3-WROOM-1 模组管脚的 GPIO 分配列表，用于控制开发板的特定组件或功能。

.. list-table:: ESP32-S3-WROOM-1 GPIO 分配
   :header-rows: 1
   :widths: 10 10 10 10 10 10 10 10 10 10

   * - 管脚 [#one]_
     - 管脚名称
     - ES8311
     - ES7210
     - 摄像头
     - LCD
     - 按键
     - microSD 卡
     - IO 扩展
     - 其他
   * - 3
     - EN
     - 
     - 
     - 
     - 
     - EN_KEY
     - 
     - 
     - 
   * - 4
     - IO4
     - 
     - 
     - 
     - 
     - 
     - DATA0
     - 
     - 
   * - 5
     - IO5
     - 
     - 
     - 
     - 
     - REC, MUTE, PLAY, SET, VOL-, VOL+
     - 
     - 
     - 
   * - 6
     - IO6
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - BAT_MEAS_ADC
   * - 7
     - IO7
     - 
     - 
     - 
     - 
     - 
     - CMD
     - 
     - 
   * - 8
     - IO15
     - 
     - 
     - 
     - 
     - 
     - CLK
     - 
     - 
   * - 9
     - IO16
     - I2S0_MCLK
     - MCLK
     - 
     - 
     - 
     - 
     - 
     - 
   * - 10
     - IO17
     - I2C_SDA
     - I2C_SDA
     - SIOD
     - TP_I2C_SDA
     - 
     - 
     - I2C_SDA
     - 
   * - 11
     - IO18
     - I2C_CLK
     - I2C_CLK
     - SIOC
     - TP_I2C_CLK
     - 
     - 
     - I2C_CLK
     - 
   * - 12
     - IO8
     - I2S0_DSDIN
     - 
     - 
     - 
     - 
     - 
     - 
     - 
   * - 13
     - IO19
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - ESP_USB_DM (Reserve)
   * - 14
     - IO20
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - ESP_USB_DP (Reserve)
   * - 15
     - IO3
     - 
     - 
     - D5
     - 
     - 
     - 
     - 
     - 
   * - 16
     - IO46
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - NC
   * - 17
     - IO9
     - I2S0_SCLK
     - SCLK
     - 
     - 
     - 
     - 
     - 
     - 
   * - 18
     - IO10
     - 
     - SDOUT
     - 
     - 
     - 
     - 
     - 
     - 
   * - 19
     - IO11
     - 
     - 
     - PCLK
     - 
     - 
     - 
     - 
     - 
   * - 20
     - IO12
     - 
     - 
     - D6
     - 
     - 
     - 
     - 
     - 
   * - 21
     - IO13
     - 
     - 
     - D2
     - 
     - 
     - 
     - 
     - 
   * - 22
     - IO14
     - 
     - 
     - D4
     - 
     - 
     - 
     - 
     - 
   * - 23
     - IO21
     - 
     - 
     - VSYNC
     - 
     - 
     - 
     - 
     - 
   * - 24
     - IO47
     - 
     - 
     - D3
     - 
     - 
     - 
     - 
     - 
   * - 25
     - IO48
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - PA_CTRL
   * - 26
     - IO45
     - I2S0_LRCK
     - LRCK
     - 
     - 
     - 
     - 
     - 
     - 
   * - 27
     - IO0
     - 
     - 
     - 
     - LCD_SPI_SDA
     - BOOT_KEY
     - 
     - 
     - 
   * - 28
     - IO35
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - NC
   * - 29
     - IO36
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - NC
   * - 30
     - IO37
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - NC
   * - 31
     - IO38
     - 
     - 
     - HREF
     - 
     - 
     - 
     - 
     - 
   * - 32
     - IO39
     - 
     - 
     - D9
     - 
     - 
     - 
     - 
     - 
   * - 33
     - IO40
     - 
     - 
     - XCLK
     - 
     - 
     - 
     - 
     - 
   * - 34
     - IO41
     - 
     - 
     - D8
     - 
     - 
     - 
     - 
     - 
   * - 35
     - IO42
     - 
     - 
     - D7
     - 
     - 
     - 
     - 
     - 
   * - 36
     - RXD0
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - ESP0_UART0_RX
   * - 37
     - TXD0
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     - ESP0_UART0_TX
   * - 38
     - IO2
     - 
     - 
     - 
     - LCD_SPI_DC
     - 
     - 
     - 
     - 
   * - 39
     - IO1
     - 
     - 
     - 
     - LCD_SPI_CLK
     - 
     - 
     - 
     - 
   * - 41
     - EPAD
     - 
     - 
     - 
     - 
     - 
     - 
     - 
     -

.. [#one] 管脚 - ESP32-S3-WROOM-1 模组管脚号，不含 GND 和供电管脚。

分配给 IO 扩展器的 GPIO 被近一步分配为多个 GPIO。

.. list-table:: IO 扩展器 GPIO 分配
   :header-rows: 1
   :widths: 10 10 10 10

   * - IO 扩展器管脚
     - 管脚名称
     - LCD
     - 其他
   * - 4
     - P0
     - 
     - PA_CTRL
   * - 5
     - P1
     - LCD_CTRL
     - 
   * - 6
     - P2
     - LCD_RST
     - 
   * - 7
     - P3
     - LCD_CS
     - 
   * - 9
     - P4
     - TP_INT
     - 
   * - 10
     - P5
     - 
     - PERI_PWR_ON
   * - 11
     - P6
     - 
     - LED1
   * - 12
     - P7
     - 
     - LED2

连接器
---------

摄像头连接器
^^^^^^^^^^^^^^^^

===  =============  =============
No.  摄像头信号       ESP32-S3 管脚
===  =============  =============
1    SIOD           GPIO17
2    SIOC           GPIO18 
3    D5             GPIO3
4    PCLK           GPIO11
5    D6             GPIO12
6    D2             GPIO13
7    D4             GPIO14
8    VSYNC          GPIO21
9    D3             GPIO47
10   HREF           GPIO38
11   D9             GPIO39
12   XCLK           GPIO40
13   D8             GPIO41
14   D7             GPIO42
===  =============  =============

LCD 连接器
^^^^^^^^^^^^^^^^

===  ===========  =============
No.  LCD 信号      ESP32-S3 管脚
===  ===========  =============
1    TP_I2C_SDA   GPIO17
2    TP_I2C_CLK   GPIO18 
3    LCD_SPI_SDA  GPIO0 
4    LCD_SPI_DC   GPIO2
5    LCD_SPI_CLK  GPIO1 
===  ===========  =============

===  ============  ============
No.  LCD 信号       扩展器管脚
===  ============  ============
1    ESP_LCD_CTRL   P1
2    ESP_LCD_RST   P2
3    ESP_LCD_CS    P3
4    ESP_TP_INT    P4
===  ============  ============

AEC 电路
--------

AEC 电路为 AEC 算法提供参考信号。

ESP32-S3-Korvo-2 回声参考信号源有两路兼容设计，一路是 Codec (ES8311) DAC 输出 (DAC_AOUTLN/DAC_AOUTLP)，一路是 PA (NS4150) 输出 (PA_OUTL+/PA_OUTL-)。默认推荐将 Codec (ES8311) DAC 输出 (DAC_AOUTLN/DAC_AOUTLP) 作为回声参考信号，同时将下图中电阻 R132、R140 NC。

回声参考信号通过 ADC (ES7210) 的 ADC_MIC3P/ADC_MIC3N 采集后送回给 ESP32-S3 用于 AEC 算法。

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-aec-codec-o.png
    :align: center
    :scale: 60%
    :alt: ESP32-S3-Korvo-2 V3.0 - AEC Codec DAC 输出（点击放大）
    
    ESP32-S3-Korvo-2 V3.0 - AEC Codec DAC 输出（点击放大）

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-aec-pa-o.png
    :align: center
    :scale: 30%
    :alt: ESP32-S3-Korvo-2 V3.0 - AEC PA 输出 （点击放大）
    
    ESP32-S3-Korvo-2 V3.0 - AEC PA 输出（点击放大）

.. figure:: ../../../_static/esp32-s3-korvo-2-v3.0-aec-signal-collection.png
    :align: center
    :scale: 60%
    :alt: ESP32-S3-Korvo-2 V3.0 - AEC 参考信号采集（点击放大）
    
    ESP32-S3-Korvo-2 V3.0 - 参考信号采集（点击放大）

硬件设置选项
----------------------

自动下载
^^^^^^^^^^^^^^^^^^^^^^

可以通过两种方式使 ESP 开发板进入下载模式：

- 手动按下 Boot 和 RST 键，然后先松开 RST，再松开 Boot 键。
- 由软件自动执行下载。软件利用串口的 DTR 和 RTS 信号来控制 ESP 开发板的 EN、IO0 管脚的状态。详情请参见 `ESP32-S3-Korvo-2 V3.0 原理图`_ (PDF)。


ESP 管脚测试点分配
-------------------

本节介绍了 ESP32-S3-Korvo-2 V3.0 板上可分配的测试点。

测试点是裸通孔，具有标准的 2.54 毫米/0.1 英寸间距。您可能需要接入排针或排针插孔，从而连接外部硬件。

编解码器测试点/J15
^^^^^^^^^^^^^^^^^^^^
===  ============  =============
No.  编解码器管脚    ESP32-S3 管脚
===  ============  =============
1    MCLK          GPIO16
2    SCLK          GPIO9 
3    LRCK          GPIO45 
4    DSDIN         GPIO8
5    ASDOUT        – 
6    GND           –
===  ============  =============

ADC 测试点/J16
^^^^^^^^^^^^^^^^^

===  ==========  =============
No.  ADC 管脚     ESP32-S3 管脚
===  ==========  =============
1    MCLK        GPIO16
2    SCLK        GPIO9
3    LRCK        GPIO45 
4    SDOUT       GPIO10
5    INT         –
6    GND         –
===  ==========  =============

UART 测试点/J17
^^^^^^^^^^^^^^^^^^

===  ==========
No.  UART 管脚   
===  ==========
1    3.3V        
2    TXD         
3    RXD        
4    IO0
5    EN          
6    GND         
===  ==========

I2C 测试点/J18
^^^^^^^^^^^^^^

===  ==========  =============
No.  I2C 管脚     ESP32-S3 管脚
===  ==========  =============
1    3.3V        –
2    CLK         GPIO18
3    SDA         GPIO17
4    GND         –
===  ==========  =============

硬件版本
============

无历史版本。

相关文档
========

- `ESP32-S3 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_cn.pdf>`_ (PDF)
- `ESP32-S3-WROOM-1/1U 技术规格书 <https://www.espressif.com/en/sites/default/files/documentation/esp32-s3-wroom-1_datasheet_cn.pdf>`_ (PDF)
- `ESP32-S3-Korvo-2 V3.0 原理图`_ (PDF)
- `ESP32-S3-Korvo-2 V3.0 PCB 布局图 <https://dl.espressif.com/dl/schematics/PCB_ESP32-S3-KORVO-2_V3.0_20210918.pdf>`_ (PDF)

有关本开发板的更多设计文档，请联系我们的商务部门 `sales@espressif.com <sales@espressif.com>`_。

.. _ESP32-S3-Korvo-2 V3.0 原理图: https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-KORVO-2_V3_0_20210918.pdf