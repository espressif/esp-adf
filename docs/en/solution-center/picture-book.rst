Picture Book and E-Book
=======================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

The picture-book and e-book solution adds camera recognition on top of a story player. After the camera reads OID dots or artwork on a page, the device plays the matching audio and can translate, follow-read, or explain. Product forms include a reading pen, picture-book machine, dictionary pen, and talking e-book. One SoC runs audio codecs, the voice front end, the camera, and image compression, with 2.4 GHz Wi-Fi and Bluetooth 5 (LE).

Recognition results can drive local content or request a cloud picture-book library and a large-model Q and A session, so tap-to-read and explanation run as one flow.

.. only:: html

   .. mermaid::

      flowchart LR
          Cam["Camera"]
          Oid["OID or image recognition"]
          Player["Local or cloud playback"]
          Chat["Voice Q and A"]
          Cam --> Oid
          Oid --> Player
          Chat --> Player

Applications
------------

- **Early learning**: reading pen, picture-book machine, or story player, with tap-to-read, page-turn reading, or finger tracking
- **K12 study**: dictionary pen, word trainer, or translation aid, looking up scanned words or sentences and supporting follow-reading
- **Interactive reading**: talking e-book or interactive picture-book player, with a display for text or illustrations

Features
--------

- Offline OID recognition, or cloud-side picture-book image recognition
- Tap-to-read, page-turn reading, finger tracking, and local picture-book playback
- Online Q and A, encyclopedia lookup, and large-model study help
- Speaker-grade playback control: barge-in, resume after power loss, and fade
- Optional display for text, translation, or expressions

Hardware and software
---------------------

Common SoCs are ESP32-S3, ESP32-S31, and ESP32-P4, because the product needs both a camera and voice interaction. Reading pens can use an SPI camera; dictionary pens often use a DVP camera. Use ESP32-S31 when classic Bluetooth is required.

Playback and format handling are in :doc:`../basic-components/audio-codec/esp-audio-codec`. DVP camera drivers are in `esp-video-components <https://github.com/espressif/esp-video-components>`__. Voice-chat examples are in `adf_examples/ai_agent <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent>`__.

References
----------

- `ESP Audio Codec <https://components.espressif.com/components/espressif/esp_audio_codec>`__
- `ESP-GMF elements <https://github.com/espressif/esp-gmf/tree/main/elements>`__
- `esp-video-components <https://github.com/espressif/esp-video-components>`__
- `AI Agent examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent>`__
