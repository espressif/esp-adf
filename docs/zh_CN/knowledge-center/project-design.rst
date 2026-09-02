项目设计
**************

:link_to_translation:`en:[English]`

多媒体项目的选型依据是输入、输出和芯片型号。输入包括麦克风、摄像头、存储、网络和用户操作，输出包括扬声器、屏幕、存储、网络和状态指示。

输入与输出
================

输入

* 模拟麦克风通过外置 Codec 完成模数转换。数字麦克风使用 PDM 或 I2S 接口。
* 摄像头使用 DVP、MIPI CSI 或 USB 接口。
* 本地音视频文件存储在 microSD 或片内 Flash。
* 网络流媒体通过 Wi-Fi 或以太网获取。
* 无线音频使用经典蓝牙或 LE Audio。
* 用户操作来自按键或触摸屏。

输出

* 外置 Codec 或片上 DAC 驱动耳机、功放和扬声器。
* LCD 显示画面和 UI。
* 录音、录像写入 microSD 或 Flash。
* 推流使用 Wi-Fi、以太网或 WebRTC。
* 经典蓝牙或 LE Audio 向耳机或音箱发送音频。
* LED、灯带或振动用于状态指示。

主控

芯片完成采集、编解码与网络收发。ESP32-P4 使用硬件 JPEG、H.264 和 PPA 降低 CPU 占用。


芯片与模组
================

按功能选择型号。推荐使用带 PSRAM 的芯片或模组。Wi-Fi、蓝牙与媒体同时运行时，内部 RAM 通常不足。芯片与模组筛选见 `产品选项工具 <https://products.espressif.com/#/product-selector>`__。

.. list-table::
   :header-rows: 1
   :widths: 36 28 36

   * - 场景
     - 建议型号
     - 说明
   * - 纯音频播放、网络电台
     - ESP32、ESP32-S2、ESP32-C3
     - 外置 Codec 音质更高；ESP32、ESP32-S2 提供片上 DAC
   * - 经典蓝牙音频
     - ESP32、ESP32-S31
     - ESP32、ESP32-S31 支持经典蓝牙
   * - 语音唤醒、AEC、双麦
     - ESP32-S3、ESP32-S31、ESP32-P4
     - 需要 PSRAM
   * - 音视频播放、对讲、高分辨率编解码
     - ESP32-P4
     - 更大 PSRAM；MIPI CSI / DSI；硬件 JPEG / H.264 / PPA
   * - 灯带等低成本外设
     - ESP32-C3
     - 算力需求低；无 PSRAM

开发板列表见 :doc:`/multimedia-boards/index`。


软件组成
================

板卡外设由 ``esp_board_manager`` 和 ``esp_codec_dev`` 初始化，见 :doc:`/basic-components/hardware-support/esp-board-manager` 与 :doc:`/basic-components/hardware-support/esp-codec-dev`。

媒体处理使用 GMF 处理链（pipeline）串接处理单元（element），见 :doc:`/basic-components/esp-gmf/index`。

播放、采集、联网等产品能力由 :doc:`/multimedia-services/index` 提供。

.. only:: html

   .. mermaid::

      flowchart LR
          App["应用与产品服务"] --> Gmf["GMF 处理链"]
          App --> Board["esp_board_manager 与 esp_codec_dev"]
          Gmf --> Board
          Board --> Soc["SoC 与外设"]


硬件形态
================

最少外围
----------------

仅使用 I2S、PDM 或 DAC，即可构成外围最少的产品。数字麦克风采集语音后，通过 Wi-Fi 与云服务交互。

.. figure:: ../../_static/audio-project-minimum-voice-service.jpg
    :alt: 音频项目示例：向云服务发送语音命令
    :figclass: align-center

    音频项目示例：向云服务发送语音命令

片上 DAC 用于网络电台一类播放器，音质低于外置 Codec。

.. figure:: ../../_static/audio-project-minimum-internet-radio.jpg
    :alt: 音频项目示例：联网广播播放器
    :figclass: align-center

    音频项目示例：联网广播播放器


典型整机
----------------

需要更高音质或更多接口时，使用外置 I2S Codec 处理模拟输入输出。Codec 提供前置放大、耳机放大和多路模拟口。音视频同时工作时增加摄像头与 LCD。

开发板示例见 :doc:`/multimedia-boards/dev-boards/user-guide-esp32-s3-korvo-2` 和 `ESP32-P4 Function EV <https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32p4/esp32-p4x-function-ev-board/user_guide.html>`__。

.. figure:: ../../_static/audio-project-typical-example.jpg
    :alt: 典型音频项目示例
    :figclass: align-center

    典型音频项目示例


方案对照
================

整机形态与软硬件组成见 :doc:`/solution-center/index`。

* :doc:`/solution-center/smart-speaker-alarm`
* :doc:`/solution-center/picture-book`
* :doc:`/solution-center/voice-call`
* :doc:`/solution-center/video-intercom`
* :doc:`/solution-center/esp-webrtc-solution`
* :doc:`/solution-center/bluetooth-audio`
* :doc:`/solution-center/audio-led-strip`
* :doc:`/solution-center/video-led-strip`
* :doc:`/solution-center/ai-companion`
