Video Ambient LED Strip
=======================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

A video ambient LED strip captures frames, extracts color blocks, and maps scene or screen colors onto LEDs. It is used for display spotlights, venue strips, or TV bias lights. Input can be a live camera or still images on a memory card. Live capture needs calibration and distortion correction. The output maps normalized colors to LED color, brightness, and count.

Compared with an audio-reactive strip, this design depends on image processing and needs more compute. ESP32-P4 includes an ISP, which helps camera color pick. For HDMI sources, ESP32-P4 can use HDMI-to-CSI capture at about 30 fps.

.. only:: html

   .. mermaid::

      flowchart LR
          Src["Camera or image"]
          Color["Slice and pick color"]
          Led["LED color and brightness"]
          Src --> Color
          Color --> Led

Applications
------------

- **Commercial display**: showrooms, shop windows, and brand stores, where spotlights follow product or ambient color
- **Venues**: strips in bars, KTV rooms, or arcades that follow on-screen color
- **Bias lighting**: a strip behind a TV or monitor that extends picture color into the room

Features
--------

- Live camera capture, or still images from a memory card
- Frame slicing, color-block extraction, and color normalization, then mapping onto LEDs
- Camera calibration and distortion correction for live capture
- Multiple effect modes and parameters
- On ESP32-P4, HDMI-to-CSI capture can raise the color-pick frame rate

Hardware and software
---------------------

Common SoCs are ESP32-S3, ESP32-S31, and ESP32-P4. On ESP32-S3, 480P input is about 10 to 15 fps. The LEDs show normalized color blocks, so 720P and 480P look similar on the strip. ESP32-P4 processing is about twice as fast as ESP32-S3. Image processing is in :doc:`../basic-components/effects/esp-image-effects`.

References
----------

- :doc:`../basic-components/effects/esp-image-effects`
- :doc:`../basic-components/video-codec/esp-new-jpeg`
