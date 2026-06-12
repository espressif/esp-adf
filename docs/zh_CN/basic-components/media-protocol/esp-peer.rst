ESP Peer
========

:link_to_translation:`en:[English]`

ESP Peer 实现 WebRTC PeerConnection，覆盖 ICE、DTLS、SCTP 与 RTP/SRTP。TURN 符合 RFC 5766 与 RFC 8656，设备可作为主控端或被控端。音频支持 G.711A、G.711U 与 OPUS，视频支持 H.264 与 MJPEG。数据通道支持可靠或不可靠、有序或无序传输。组件提供默认实现。仅需要建立对等连接、不需要完整 WebRTC 应用编排时，使用本组件即可。

相关链接：

- `组件管理器 <https://components.espressif.com/components/espressif/esp_peer>`__
- `GitHub 仓库 <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_peer>`__
- `peer_demo 例程 <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_peer/examples/peer_demo>`__
- :doc:`esp-webrtc`
