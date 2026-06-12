Bluetooth Audio
===============

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

The Bluetooth audio solution covers Classic Bluetooth and LE Audio in one software stack. The device can act as a speaker that receives music from a phone or PC and supports hands-free calling, as a source that sends local audio to a headset or speaker, or as an Auracast broadcast source or sink. Use A2DP playback, AVRCP control, and HFP calling to work with the existing phone Bluetooth ecosystem. The same software stack also supports LE Audio unicast, broadcast, and multi-device synchronized playback.

ESP32-S31 is recommended when the product needs both Classic Bluetooth and Bluetooth 5.4 (LE). ESP32 can be used when only Classic Bluetooth audio is required.

.. only:: html

   .. mermaid::

      flowchart LR
          Phone["Phone or PC"]
          Soc["SoC"]
          Spk["Speaker"]
          Mic["Microphone"]
          Head["Headset or speaker"]
          Phone -->|"A2DP / HFP / LE Audio"| Soc
          Mic --> Soc
          Soc --> Spk
          Soc -->|"A2DP Source / Auracast"| Head

Applications
------------

- **Bluetooth speaker**: a phone or PC sends music over A2DP; the device plays it as an A2DP Sink and supports previous, pause, next, and linked volume
- **Screened head unit**: an in-vehicle or desktop terminal displays track, cover, and lyrics, and handles playback control and dialing on the device
- **Hands-free calling**: HFP outputs downlink audio to the speaker and captures uplink from the microphone, for screened speakers, car head units, and desktop conference terminals
- **Bluetooth source**: the device discovers and connects to a headset or speaker, then sends local or SD card audio to the peer
- **TWS and hearing aids**: LE Audio unicast (CIS) for point-to-point connection or left/right synchronized playback
- **Shared listening**: Auracast broadcast (BIS) from one source to many sinks, for exhibition guides, conference interpretation, public TV listening, and multi-room playback

Features
--------

- Dual-mode Bluetooth: Classic Bluetooth (BR/EDR) and BLE 5.4 on the same product, compatible with existing Bluetooth devices and LE Audio peers
- Classic profiles: A2DP Sink / Source, AVRCP playback control and metadata, HFP hands-free calling, PBAP phonebook and call history
- LE Audio: CIS unicast, BIS / Auracast broadcast, and TMAP telephony and media role combinations
- ``esp_bt_audio`` reports connection, stream, playback control, volume, and call status through one event path
- Optional touch UI: media player, dialer, and volume bar
- Acoustic echo cancellation (AEC) on the call uplink
- Hardware ASRC on ESP32-S31 for sample-rate conversion

Chip comparison
---------------

Bluetooth audio capabilities by chip:

.. list-table::
   :header-rows: 1
   :widths: 22 26 26 26

   * - Item
     - ESP32
     - ESP32-S3
     - ESP32-S31
   * - CPU
     - Dual-core Xtensa LX6, up to 240 MHz
     - Dual-core Xtensa LX7, up to 240 MHz
     - Dual-core RISC-V, up to 320 MHz
   * - SRAM
     - 520 KB
     - 512 KB
     - 512 KB
   * - Classic Bluetooth
     - Yes (v4.2 BR/EDR)
     - No
     - Yes (BR/EDR)
   * - Bluetooth LE
     - Bluetooth LE 4.2
     - Bluetooth 5 (LE)
     - Bluetooth 5.4 (LE)
   * - LE Audio
     - No
     - No
     - Yes (CIS / BIS)
   * - Hardware ASRC
     - No
     - No
     - Yes
   * - I2S
     - 2 controllers
     - 2 controllers
     - 2 controllers, hardware Bluetooth audio sync

Sample-rate conversion loading:

.. list-table::
   :header-rows: 1
   :widths: 28 36 36

   * - Metric
     - ESP32-S31 hardware ASRC
     - ESP32-S3 software SRC
   * - Average CPU loading
     - 0.79%
     - 2.35%
   * - Peak CPU loading
     - 1.86%
     - 14.17%
   * - Average memory
     - 539 B
     - 19400 B
   * - Peak memory
     - 672 B
     - 87000 B

Hardware and software
---------------------

ESP32-S31 supports both Classic Bluetooth and LE Audio, and is the recommended SoC for this solution. ESP32 can be used when the product only needs A2DP, AVRCP, and HFP. ESP32-S3 supports Bluetooth LE only, and does not support Classic Bluetooth or LE Audio.

``esp_bt_audio`` manages Bluetooth roles and events. The audio stream goes through an ESP-GMF pipeline for decode, effects, acoustic echo cancellation, and playback. Board peripherals are initialized by ``esp_board_manager``. See `esp_bt_audio/examples/bt_audio <https://github.com/espressif/esp-gmf/tree/main/packages/esp_bt_audio/examples/bt_audio>`__. The example index is in :doc:`../multimedia-examples/index`.

References
----------

- `Application Solution Series: Audio Applications <https://esp32-s31.espressif.com/en/official-projects/5>`__
- `esp_bt_audio component <https://components.espressif.com/components/espressif/esp_bt_audio>`__
- `ESP Bluetooth Audio documentation <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/esp-bt-audio.html>`__
- `bt_audio example <https://github.com/espressif/esp-gmf/tree/main/packages/esp_bt_audio/examples/bt_audio>`__
- `ESP32 Datasheet <https://documentation.espressif.com/esp32_datasheet_en.pdf>`__
- `ESP32-S3 Datasheet <https://documentation.espressif.com/esp32-s3_datasheet_en.pdf>`__
- `ESP32-S31 Datasheet <https://documentation.espressif.com/esp32-s31_datasheet_en.pdf>`__
