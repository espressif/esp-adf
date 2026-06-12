ESP WebRTC
==========

:link_to_translation:`zh_CN:[中文]`

ESP WebRTC combines signaling, peer connection, and the media system. After audio and video codecs are configured, start and stop operations establish the connection, capture and send media, and render received streams. Capture is handled by the GMF capture component; rendering is handled by the audio and video player. Use :doc:`esp-peer` when only a peer connection is required.

Signaling hides protocol differences. The repository provides the following implementations:

.. list-table::
   :header-rows: 1
   :widths: 24 46 30

   * - Implementation
     - Description
     - Related links
   * - ``apprtc_signal``
     - AppRTC-style WebSocket signaling for doorbell and video-call solutions
     - `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc/impl/apprtc_signal>`__
   * - ``janus_signal``
     - Publishes to a Janus VideoRoom over HTTP signaling
     - `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc/impl/janus_signal>`__
   * - ``kvs_signaling``
     - Provides Amazon Kinesis Video Streams signaling
     - `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc/impl/kvs_signaling>`__
   * - ``whip_signal``
     - Provides WHIP ingest signaling
     - `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc/impl/whip_signal>`__

Related links:

- `GitHub Repository <https://github.com/espressif/esp-webrtc-solution/tree/main/components/esp_webrtc>`__
- :doc:`/solution-center/esp-webrtc-solution`
- :doc:`/multimedia-examples/index`
