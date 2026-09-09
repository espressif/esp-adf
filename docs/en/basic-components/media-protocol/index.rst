Media Protocol
==============

:link_to_translation:`zh_CN:[中文]`

Media protocols include SIP, RTSP, and RTMP streaming, plus WebRTC PeerConnection and application-level orchestration. The protocol stack is :doc:`esp-media-protocols`. Applications use these protocols through ``esp_sip_service``, ``esp_rtsp_service``, and ``esp_rtmp_service``: they link capture and playback and do not forward frames. Media service examples are ``sip_cli``, ``rtsp_cli``, ``rtsp_push``, and ``rtmp_cli`` in :doc:`/multimedia-examples/index`. Examples that call the protocol stack directly are ``rtsp_demo`` / ``rtmp_demo`` in :doc:`/solution-center/esp-webrtc-solution`.

.. toctree::
   :maxdepth: 1

   esp-media-protocols
   esp-peer
   esp-webrtc
