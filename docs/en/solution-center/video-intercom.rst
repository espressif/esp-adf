Video Intercom and Doorbell
===========================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

The video-intercom and doorbell solution runs camera capture, encode/decode, two-way calling, and a display on ESP32-S3 / ESP32-P4. Product forms include a peephole, door lock, doorbell, or care camera. The device captures the door or indoor scene and sends it over WebRTC, RTMP, RTSP, or SIP to a phone or indoor panel. Depending on the protocol, it provides remote video preview, video streaming, or two-way video intercom. Audio uses a 1-to-3-mic array with noise suppression and acoustic echo cancellation. Display interfaces include SPI, 8080, and RGB; the UI can use LVGL.

Product differences are mainly the camera interface (USB, DVP, or MIPI) and panel size. Software structure is similar for a lock, doorbell, and care camera.

.. only:: html

   .. mermaid::

      flowchart LR
          Cam["Camera"]
          Enc["Video codec"]
          Rtc["WebRTC / RTMP / RTSP / SIP"]
          Lcd["Video preview/two-way video intercom"]
          Cam --> Enc
          Enc --> Rtc
          Rtc --> Lcd

Applications
------------

- **Home security**: smart peephole, video doorbell, lock with a screen, and access control
- **Care devices**: infant or elder-care terminals for remote video preview and video intercom
- **Home appliances**: remote video preview and video intercom on pet feeders, indoor monitors, and similar devices
- **Interactive toys**: a toy or desktop robot with video calling

Features
--------

- Two-way video intercom and voice calling
- Transport over WebRTC, RTMP, RTSP, or SIP
- 1-to-3-mic arrays with noise suppression and acoustic echo cancellation
- SPI, 8080, or RGB panels for preview and a simple UI
- Wi-Fi keep-alive: ESP32-S3 standby can reach about 600 µA; ESP32-P4 current depends on the companion Wi-Fi chip
- Calls started from a phone app or mini program

Hardware and software
---------------------

Pick the SoC by resolution and codec: H.264 calling uses ESP32-P4; MJPEG up to 480×800 can use ESP32-S3, and higher resolutions can use ESP32-S31. The camera interface can be USB, DVP, or MIPI.

Real-time calling is in :doc:`esp-webrtc-solution`. H.264 and JPEG are in :doc:`../basic-components/video-codec/esp-h264` and :doc:`../basic-components/video-codec/esp-new-jpeg`. The audio front end is in `ESP-SR <https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/README.html>`__. A doorbell example is `doorbell_demo <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/doorbell_demo>`__.

References
----------

- :doc:`esp-webrtc-solution`
- `doorbell_demo <https://github.com/espressif/esp-webrtc-solution/tree/main/solutions/doorbell_demo>`__
- :doc:`../basic-components/video-codec/esp-h264`
- :doc:`../basic-components/video-codec/esp-new-jpeg`
