ESP WebRTC
==========

:link_to_translation:`en:[English]`

ESP WebRTC 将信令、对等连接与媒体系统组合在一起。配置音视频编解码后，启动与停止接口完成建连、采集发送与接收渲染。采集由 GMF 采集组件完成，渲染由音视频播放组件完成。仅需要建立对等连接时，见 :doc:`esp-peer`。

信令接口屏蔽具体协议差异，仓库提供下列实现：

.. list-table::
   :header-rows: 1
   :widths: 24 46 30

   * - 实现
     - 简介
     - 相关链接
   * - ``apprtc_signal``
     - 提供 AppRTC 风格的 WebSocket 信令，用于门铃与视频通话等方案
     - `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc/impl/apprtc_signal>`__
   * - ``janus_signal``
     - 通过 Janus HTTP 信令发布到 VideoRoom
     - `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc/impl/janus_signal>`__
   * - ``kvs_signaling``
     - 提供 Amazon Kinesis Video Streams 信令
     - `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc/impl/kvs_signaling>`__
   * - ``whip_signal``
     - 提供 WHIP 推流信令
     - `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc/impl/whip_signal>`__

相关链接：

- `GitHub 仓库 <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc>`__
- :doc:`/solution-center/esp-webrtc-solution`
- :doc:`/multimedia-examples/index`
