AI Companion Intercom (VoCat)
=============================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

VoCat (ESP-VoCat-S31, Chinese name 喵伴) is Espressif's desktop AI companion demo. It integrates voice interaction, vision, and Bluetooth audio on one device for companion, intercom, and multimodal products. This revision upgrades the original ESP-VoCat: the main board uses ESP32-S31-WROOM-1 and adds a DVP camera. The base board, microphone board, round display, speaker, battery, touch keys, and enclosure remain compatible.

.. only:: html

   .. mermaid::

      flowchart LR
          Mic["Microphone"]
          Cam["Camera"]
          Soc["ESP32-S31"]
          Lcd["Round display"]
          Spk["Speaker"]
          Bt["Bluetooth audio"]
          Mic --> Soc
          Cam --> Soc
          Soc --> Lcd
          Soc --> Spk
          Soc --> Bt

Applications
------------

- **Desktop companion**: round display for expressions and video, touch-key input, as a desktop voice assistant or companion form factor
- **Family intercom**: local or cloud voice chat, with the camera for simple video interaction
- **Bluetooth speaker**: classic Bluetooth and BLE Audio on ESP32-S31, extended to a desktop speaker or broadcast-audio demo
- **On-device vision**: photo upload, frame analysis, gesture, color, and object recognition for multimodal demos

Features
--------

- Microphone capture and speaker playback, connected to a cloud large-model session or an on-device voice pipeline
- Round display for expressions and preview; touch keys, IMU, SD card, and rotating-base communication are retained
- DVP camera for capture and frame analysis; on-device gesture recognition, OpenCV color detection, and YOLO11 object detection
- ES8389 codec for capture and playback; besides a classic Bluetooth speaker, LE Audio / Auracast can be added
- Hardware JPEG and PPA image acceleration, with higher PSRAM bandwidth, for Wi-Fi 6 video streaming and high-frame-rate display on the round panel

Hardware and software
---------------------

The SoC is ESP32-S31. Audio uses ES8389; vision uses an SC101IOT DVP camera. Real-time calls can reuse :doc:`esp-webrtc-solution`. Voice-chat integration is covered in :doc:`../multimedia-services/ai/esp-coze`. Hardware design files are published on OSHWHub. Software can combine the public components and examples listed below.

References
----------

- Hardware (open source): `ESP-VoCat-S31 <https://oshwhub.com/esp-college/project_fqjutmff>`__
- Demo video: `VoCat on ESP32-S31 <https://www.bilibili.com/video/BV1azLv6tEuj/>`__
- Chip datasheet: `ESP32-S31 Datasheet <https://documentation.espressif.com/esp32-s31_datasheet_en.pdf>`__
