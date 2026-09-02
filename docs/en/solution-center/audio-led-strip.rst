Audio Reactive LED Strip
========================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

An audio-reactive LED strip captures ambient sound, applies noise reduction, gain, and FFT, then maps volume and frequency to LED color, brightness, and how many LEDs are lit. The strip can be a standalone mood light or a light ring on a speaker. Wi-Fi is used for provisioning and effect presets. The on-chip ADC plus an analog microphone is enough to drive the effect; an external ADC can be added for higher sample precision.

Any ESP32-series SoC except ESP8266 can run this, with low CPU use. ESP32-C3 is common for a standalone strip. When the strip is part of a speaker, use the speaker SoC (ESP32 or ESP32-S3).

.. only:: html

   .. mermaid::

      flowchart LR
          Mic["Microphone"]
          Fft["Noise reduction and FFT"]
          Led["LED color and brightness"]
          Mic --> Fft
          Fft --> Led

Applications
------------

- **Home lighting**: wall or bedside accent lighting in a living room, bedroom, or play area
- **Venues**: sound-to-light strips in bars, KTV rooms, or arcades
- **Bias lighting**: a strip behind a TV or monitor that follows program audio
- **Appliance rings**: a breathing light or ring on a speaker

Features
--------

- On-chip ADC with an analog microphone; default sample rate 16 kHz, about 12-bit effective depth
- Noise reduction, filtering/gain, and FFT on the captured audio
- Mapping of the spectrum to LED color, brightness, and count, from a few LEDs to hundreds
- Multiple effect modes and parameters
- Can share the speaker SoC and run only as a background light ring

Hardware and software
---------------------

ESP32-C3 is recommended for a standalone strip: the reactive algorithm and Wi-Fi share one chip, with a simple BOM. Speaker products keep ESP32 or ESP32-S3 as the main SoC; the strip uses little CPU. See `adf_examples/display/led_pixels <https://github.com/espressif/esp-adf/tree/master/adf_examples/display/led_pixels>`__.

References
----------

- `led_pixels example <https://github.com/espressif/esp-adf/tree/master/adf_examples/display/led_pixels>`__
