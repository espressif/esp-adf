多媒体协议
================

:link_to_translation:`en:[English]`

多媒体协议包括 SIP、RTSP、RTMP 等流媒体协议，以及 WebRTC PeerConnection 与应用层编排。协议栈实现见 :doc:`esp-media-protocols`。应用程序通过 ``esp_sip_service``、``esp_rtsp_service``、``esp_rtmp_service`` 使用这些协议：``link`` 采集与播放，不搬运帧。媒体服务例程见 :doc:`/multimedia-examples/index` 中的 ``sip_cli``、``rtsp_cli``、``rtsp_push``、``rtmp_cli``。直接调用协议栈的方案见 :doc:`/solution-center/esp-webrtc-solution` 中的 ``rtsp_demo`` / ``rtmp_demo``。

.. toctree::
   :maxdepth: 1

   esp-media-protocols
   esp-peer
   esp-webrtc
