蓝牙服务
=================

:link_to_translation:`en:[English]`

蓝牙服务 (Bluetooth service) 专门用于与蓝牙设备进行交互，支持以下协议：

* 免提规范 (Hands-Free Profile, HFP)：通过免提设备远程控制手机以及二者间的语音连接。
* 高级音频分发框架 (Advanced Audio Distribution Profile, A2DP)：通过蓝牙连接播放多媒体音频。
* 音视频远程控制规范 (Audio Video Remote Control Profile, AVRCP)：与 A2DP 同时使用，用于远程控制耳机、汽车音响系统或扬声器等设备。


应用示例
-------------------

以下示例展示了该 API 的实现方式。

* :example:`player/pipeline_bt_sink`


.. include:: /_build/inc/bluetooth_service.inc
.. include:: /_build/inc/bt_keycontrol.inc
.. include:: /_build/inc/hfp_stream.inc
.. include:: /_build/inc/a2dp_stream.inc