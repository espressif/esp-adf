Smart Speaker and Alarm Clock
=============================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

Smart speakers and AI alarm clocks share one on/off-cloud voice solution. Wake word, dialogue, multi-source playback, and schedules run on the same software, and the device can ship as a speaker, bedside clock, story player, or desktop assistant. The online path uses RTC or WebSocket to a large-model or voice-cloud service. Earlier ASR + NLU designs may use MQTT or HTTP. Offline mode keeps only the wake word and local command words for home control without a network.

One SoC runs the audio front end, codecs, streaming playback, and peripherals such as Mesh, IR, motors, and a display, without a separate voice DSP. When classic Bluetooth is required, use ESP32-S31; an external voice DSP on ESP32 is also possible.

.. only:: html

   .. mermaid::

      flowchart LR
          Wake["Wake word"]
          Chat["Cloud or on-device chat"]
          Player["Multi-source playback"]
          Alarm["Alarm and schedule"]
          Wake --> Chat
          Chat --> Player
          Alarm --> Player

Applications
------------

- **Home entertainment**: smart speaker or network radio, playing online stations and a local library
- **Children's education**: story player or early-learning terminal, with wake word and cloud content
- **Smart home**: voice assistant or voice-enabled appliance, controlling lights and devices over BLE Mesh or IR
- **Desk and bedside**: AI alarm clock with schedule, weather, and voice reminders

Features
--------

- Wake word, plus noise suppression, acoustic echo cancellation, automatic gain control, and microphone arrays; the wake word can be customized
- Multi-turn online dialogue, or offline command-word control; the online side can connect to mainstream voice-cloud and large-model services
- Playback from Wi-Fi (HTTP, HLS, DLNA, AirPlay), Bluetooth, and local storage, with barge-in, resume after power loss, and fade
- Alarms, schedules, and a display for weather, clock faces, or expressions
- Audio codecs and EQ; IR or Mesh home control as a product extension

Hardware and software
---------------------

Common SoCs for voice products are ESP32-S3, ESP32-S31, and ESP32-P4. Use ESP32-S31 when classic Bluetooth is required. Display products pick the SoC by panel interface and resolution.

The audio front end is documented in `ESP-SR <https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/index.html>`__. Playback and format handling are in :doc:`../basic-components/audio-codec/esp-audio-codec` and :doc:`../basic-components/effects/esp-audio-effects`. Real-time dialogue can follow :doc:`esp-webrtc-solution`. Example projects live in `adf_examples/ai_agent <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent>`__.

References
----------

- `ESP-SR <https://github.com/espressif/esp-sr>`__
- `ESP Audio Codec <https://components.espressif.com/components/espressif/esp_audio_codec>`__
- `ESP-GMF elements <https://github.com/espressif/esp-gmf/tree/main/elements>`__
- `AI Agent examples <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent>`__
