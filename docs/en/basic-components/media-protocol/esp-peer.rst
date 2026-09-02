ESP Peer
========

:link_to_translation:`zh_CN:[中文]`

ESP Peer implements a WebRTC peer connection covering ICE, DTLS, SCTP, and RTP/SRTP. TURN follows RFC 5766 and RFC 8656. The device can act as the controlling or controlled role. Audio codecs are G.711A, G.711U, and OPUS; video codecs are H.264 and MJPEG. The data channel supports reliable or unreliable delivery, and ordered or unordered delivery. A default implementation is provided. Use this component when only a peer connection is required, without the full WebRTC application stack in :doc:`esp-webrtc`.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_peer>`__
- `GitHub Repository <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_peer>`__
- `peer_demo example <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_peer/examples/peer_demo>`__
- :doc:`esp-webrtc`
