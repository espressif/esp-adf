ESP32-Korvo-DU1906
===================
:link_to_translation:`en:[English]`

本文档介绍了如何使用 ESP32-Korvo-DU1906 开发板。

.. figure:: ../../../_static/esp32-korvo-du1906-v1.1.png
    :align: center
    :scale: 50%
    :alt: esp32-korvo-du1906
    :figclass: align-center

    ESP32-Korvo-DU1906 实物图（点击图片放大）

- `入门指南`_：简要介绍了 ESP32-Korvo-DU1906 和硬件、软件设置指南。

- `开始开发应用`_：详细介绍了 ESP32-Korvo-DU1906 的开发过程。

- `相关文档`_：列出了相关文档的链接。

入门指南
---------

ESP32-Korvo-DU1906 的核心组件包括一个 ESP32-DU1906 蓝牙/Wi-Fi音频 模组，可降低噪声、消除回声，并实现波束形成与检测。ESP32-Korvo-DU1906 集成了电源管理、蓝牙/Wi-Fi 音频模组、编解码器、功率放大器等，包括以下功能：

* ADC
* 麦克风阵列
* SD 卡
* 功能按键
* 指示灯
* 电池恒流恒压线性电源管理芯片
* USB 转 UART
* LCD 接口


硬件准备
~~~~~~~~

* 1 x 装有 Windows、Mac OS 或 Linux 系统的 PC（推荐使用 Linux 操作系统）
* 1 x ESP32-Korvo-DU1906
* 2 x Micro USB 数据线
* 2 x 麦克风 (4 Ohm, 2.5 W)

概述
~~~~

该开发板最大的不同在于添加了语音芯片，核心处理模组 ESP32-DU1906 是一款集 Wi-Fi+Bluetooth+Bluetooth Low Energy 以及音频语音处理的 AI 模组，功能强大、用途广泛，提供业内先进的端到端语音解决方案，以及高效率的一体化 AI 服务能力，同时提供端云一体的设备级 AIOT 平台，大大降低物联网 AI 接入门槛。

DU1906 是百度推出的语音处理旗舰芯片，算法集成度高，一颗芯片同时解决了远场阵列信号实时处理和极低误报率的高精度语音唤醒的产业需求。

ESP32-Korvo-DU1906 的主要组件和连接方式如下图所示。

.. figure:: ../../../_static/esp32-korvo-du1906-v1.1-block-diagram.png
    :align: center
    :scale: 50%
    :alt: esp32-korvo-du1906-block
    :figclass: align-center

    ESP32-Korvo-DU1906 框图（点击图片放大）


组件介绍
~~~~~~~~

本节介绍了目前所需 ESP32-Korvo-DU1906 的主要组件、接口和控件等。更多详细信息，请参考相关文档中提供的原理图。

.. figure:: ../../../_static/esp32-korvo-du1906-v1.1-layout.png
    :align: center
    :scale: 50%
    :alt: esp32-korvo-du1906-components
    :figclass: align-center

    ESP32-Korvo-DU1906 组件（点击图片放大）

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Key Componenet
     - Description
   * - ESP32-DU1906 
     - ESP32-DU1906 模组是一款功能强大的通用型 Wi-Fi/蓝牙音频通信模组，适用范围广泛，主要面向低功耗传感器网络和语音编码/解码、音乐流及智能语音助手客户端等一系列要求较高的应用场景。
   * - SPI LCD 连接器
     - ESP32-Korvo-DU1906 上有一个 2.54 mm 间距的连接器用来连接 SPI LCD。
   * - 音频模数转换器 (Audio ADC)
     - ESP32-Korvo-DU1906 上有两颗 ES7243 高性能模数转换器。一颗用来采集数字音频功率放大器（Audio PA）的输出，一个用来采集（Line-in 的输入）。两个模数转换器都可以用作声学回声消除（AEC）。
   * - 耳机插孔 (Line-in/out connector)
     - 两个耳机插孔分别用来连接音频数模转换器的 Line-out 输出和音频模数转换器的 Line-in 的输入。当音频数模转换器的 Line-out 耳机插孔插入设备后，ESP32-Korvo-DU1906 上的数字音频功率放大器 (Audio PA) 会被关掉。
   * - 扬声器输出端口 (Speaker Connector)
     - 可通过数字音频功率放大器（Audio PA）支持两个外部扬声器实现立体声功能。
   * - 音频数模转换器 (Audio DAC)
     - ES7148 立体声数模转换器可以将音频数字信号转换为模拟音频输出。
   * - 数字音频功率放大器 (Audio PA)
     - TAS5805M 是一款具有低功率耗散和丰富声音的高效立体声闭环D类放大器，可以将音频数字信号转换为高功率模拟音频输出发送至外部扬声器进行播放。当音频数模转换器的 Line-out 耳机插孔插入设备后，ESP32-Korvo-DU1906 上的数字音频功率放大器（Audio PA）会被关掉。
   * - 电池连接端口 (Battery Connector)
     - 可以连接电池。
   * - 电池充电管理芯片 (Battery Charger)
     - 恒流/恒压线性电源管理芯片 AP5056 可以用于单节锂离子电池的充电管理。
   * - 电源开关 (PWR Slide Switch)
     - 开发板电源开关，turn on 为开启供电，turn off 为关闭供电。
   * - USB-UART 转换器 (USB to UART)    
     - CP2102N 支持 USB 到 UART 的转换，方便软件下载与调试。
   * - Micro-USB 调试接口 (DBG USB)
     - 通用 USB 通信端口，用于 PC 端与 ESP32-DU1906 模组的通信。
   * - 电源输入 (PWR USB)
     - 为整个系统提供电源。建议使用至少 5 V / 2 A 电源适配器供电，保证供电稳定。
   * - 充电指示 LED
     - 指示电池的状态。电池连接后，BAT_CHRG 指示灯亮红灯（表示正在充电），BAT_STBY 指示灯亮绿灯（表示电量已充满）。若未连接电池，默认 BAT_CHRG（红色），BAT_STBY（绿色）。
   * - 电源指示 LED
     - 指示供电状态。上电后，两个指示灯 (SYS_3V3, SYS_5) 都亮红灯。      
   * - 按键 (Buttons)
     - ESP32-Korvo-DU1906 上有 4 个功能按键、1 个重启按键和 1 个 Boot 选择按键。
   * - TF 卡连接器 (SD Card Slot)
     - 用于连接标准 TF 卡。
   * - 调试接口 (ESP_I2C Connector/DSP_I2C Connnector)
     - 开发板预留了两组 I2C 调试接口，分别为 ESP_I2C Connector and DSP_I2C Connnector，以供用户调试代码。
   * - 麦克阵列 (Mic)
     - ESP32-Korvo-DU1906 上有三个板载数字麦克风。三个麦克风的拾音孔呈正三角分布并且相互之间的距离 60 mm。麦克风阵列配合 DSP 可以实现降低噪声、回声消除，并且实现波束形成与检测功能。     
   * - 红外发射/接收器 (IR TX/RX)
     - ESP32-Korvo-DU1906 上有红外发射和接收器各一个，可以配合 ESP32 的红外遥控器使用。
   * - FPC 连接器 (FPC Connector for Mic)
     - ESP32-Korvo-DU1906 上有两个 FPC 连接器分别用来连接 SPI LCD 显示屏和外部麦克风阵列。         
   * - RGB LED
     - ESP32-Korvo-DU1906 上有两个 RGB LED 可供用户配置用来做状态行为指示。
   * - 麦克阵列拨码开关 (Slide Switch for Mic)
     - ESP32-Korvo-DU1906 上预留了外部麦克阵列子板接口，当使用外部麦克阵列子板的时候麦克阵列拨码开关需要全部保持 OFF 状态，当使用板载麦克阵列的时候麦克拨码开关需要全部保持 ON 的状态。


开始开发应用
--------------

通电前，请确保 ESP32-Korvo-DU1906 开发板完好无损。

初始设置
~~~~~~~~

设置开发板，运行首个示例应用程序：

1. 将 4 欧姆扬声器接至两个 **扬声器输出端口**，或将耳机接至 **Line-out 输出口**。
2. 使用两根 Micro-USB 数据线连接 PC 与 ESP32-Korvo-DU1906 的两个 **USB 接口**。
3. 如果电池已连接，则 **充电指示 LED** 将亮红灯。
4. 将 **电源开关** 拨至左侧。
5. **电源指示 LED** 应亮红灯。

如果指示灯如上述显示，则该开发板基本完好，可以开始上传应用。现在，请按照下文介绍，运行并配置 PC 上的开发工具。

开发应用
~~~~~~~~~

如果已检查确认完成初始设置，请准备开发工具。前往 :doc:`index` 查看以下步骤：

* **Set up ESP-IDF** 提供了一套 ESP32（以及 ESP32-S2）系列芯片的 C 语言开发框架；
* **Get ESP-ADF** 安装音频应用程序的 API；
* **Setup Path to ESP-ADF** 使开发框架获取到音频应用的 API；
* **Start a Project** 提供 ESP32-Korvo-DU1906 开发板的音频应用程序示例；
* **Connect Your Device** 准备加载应用程序；
* **Build the Project** 最后运行应用程序并播放音乐。


相关开发板
----------

* :doc:`get-started-esp32-lyrat`
* :doc:`get-started-esp32-lyrat-mini`
* :doc:`get-started-esp32-lyratd-msc`


内含组件和包装
---------------

零售订单
~~~~~~~~

如购买样品，每个 ESP32-Korvo-DU1906 底板将以塑料包装盒或零售商选择的其他方式包装。

零售订单请前往 https://www.espressif.com/zh-hans/products/devkits/esp32-korvo-du1906。


相关文档
--------

* `ESP32-Korvo-DU1906 原理图`_ (PDF)
* `ESP32 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_cn.pdf>`_ (PDF)
* `百度 IOT 技能后台 <https://cloud.baidu.com/doc/SHC/s/Gk7bh9rxo>`_
* `ESP32-DU1906 & ESP32-DU1906-U 技术规格书 <https://www.espressif.com/sites/default/files/documentation/esp32-du1906_esp32-du1906-u_datasheet_cn.pdf>`_ (PDF)

.. _ESP32-Korvo-DU1906 原理图: https://dl.espressif.com/dl/schematics/ESP32-Korvo-DU1906-schematics.pdf
