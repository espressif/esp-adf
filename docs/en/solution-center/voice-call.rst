Voice Intercom and VoIP
=======================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

The voice-intercom and VoIP solution runs pickup, 3A processing, and the call protocol on ESP32 / ESP32-S3, without a separate DSP. The path can be SIP/VoIP on a LAN or the internet, or a carrier number for real inbound and outbound calls. It supports full-duplex one-to-one calls and multi-party conferences. Adding a camera extends it to a video call.

Near-field intercom does not need long pickup range: one microphone plus an audio codec is enough. Conference or far-field products can use a 1-to-3-mic array.

.. only:: html

   .. mermaid::

      flowchart LR
          Mic["Microphone"]
          Afe["NS and AEC"]
          Sip["SIP or VoIP"]
          Spk["Speaker"]
          Mic --> Afe
          Afe --> Sip
          Sip --> Spk

Applications
------------

- **Smart devices**: hands-free calling inside a speaker, appliance, or voice door lock
- **Care and emergency**: one-button help terminal or care alarm, connecting to a care desk or a designated number
- **Conference and field work**: conference phone; hands-free terminals for riding or delivery
- **Child care**: voice calling on a story player or care device, optionally with video

Features
--------

- SIP/VoIP network calls, plus carrier-number dialing and answering
- Full-duplex audio with noise suppression (NS) and acoustic echo cancellation (AEC)
- One-to-one calls or multi-party conferences
- Near-field single microphone, or a 1-to-3-mic array
- Optional voice assistant or simple display (HMI)

Hardware and software
---------------------

A common SoC is ESP32-S3; use ESP32-S31 when classic Bluetooth is required. The audio front end is in `ESP-SR <https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/README.html>`__. SIP and related protocols are in :doc:`../basic-components/media-protocol/esp-media-protocols`. An example is `adf_examples/protocols/voip <https://github.com/espressif/esp-adf/tree/master/adf_examples/protocols/voip>`__.

References
----------

- `ESP-SR audio front end <https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/README.html>`__
- :doc:`../basic-components/media-protocol/esp-media-protocols`
- `VoIP example <https://github.com/espressif/esp-adf/tree/master/adf_examples/protocols/voip>`__
