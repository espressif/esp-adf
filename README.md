# Espressif Audio Development Framework

[![Documentation Status](https://readthedocs.com/projects/espressif-esp-adf/badge/?version=latest)](https://docs.espressif.com/projects/esp-adf/en/latest/?badge=latest)

Espressif Systems Audio Development Framework (ESP-ADF) is the official audio development framework for the [ESP32](https://espressif.com/en/products/hardware/esp32/overview) and [ESP32-S2](https://www.espressif.com/en/products/socs/esp32-s2) SoCs.

## Overview

ESP-ADF supports development of audio applications for the Espressif Systems SoCs in the most comprehensive way. With ESP-ADF, you can easily add features, develop audio applications from simple to complex:

- Music player or recorder supports audio formats such as MP3, AAC, FLAC, WAV, OGG, OPUS, AMR, TS, EQ, Downmixer, Sonic, ALC, G.711 and etc.
- Play music from sources: HTTP, HLS (HTTP Live Streaming), SPIFFS, SDCARD, A2DP-Source, A2DP-Sink, HFP and etc.
- Integrate Media services such as: DLNA, VoIP and etc.
- Internet Radio
- Voice recognition and integration with online services such as Alexa, DuerOS and etc.

As a general, the ESP-ADF features will be supported as shown below:

<div align="center"><img src="docs/_static/adf_block_diagram.png" alt ="ADF Block Diagram" align="center" /></div>

## Developing with the ESP-ADF

### Quick Start

You need one of ESP-IDF versions described in [ESP-ADF Releases](https://github.com/espressif/esp-adf/releases), one of audio boards below and headphones.

**Note:**  If this is your first exposure to ESP-IDF, proceed to **Getting Started** documentation specific for [ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) or [ESP32-S2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html) SoCs.

Click on one of audio boards shown below to set up and start using ESP-ADF.

### Hardware

Espressif Systems has released a number of boards for ESP-ADF to develop audio applications, including:

| ESP32-LyraT Development Board | ESP32-LyraTD-MSC Development Board |
|:----:|:----:|
| [<img src="docs/_static/esp32-lyrat-v4.2-side.jpg" width="380" alt ="ESP32-LyraT Development Board" align="center" />](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [<img src="docs/_static/esp32-lyratd-msc-v2.2.jpg" width="380" alt ="ESP32-LyraTD-MSC Development Board" align="center" />](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |
| [Getting Started with ESP32-LyraT](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html) | [Getting Started with ESP32-LyraTD-MSC](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html) |

| ESP32-LyraT-Mini Development Board | ESP32-S2-Kaluga-1 Kit (ESP-LyraT-8311A) |
|:----:|:----:|
| [<img src="docs/_static/esp32-lyrat-mini-v1.2.png" width="380" alt ="ESP32-LyraT-Mini Development Board" align="center" />](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | [<img src="docs/_static/esp32-s2-kaluga-1-kit.png" width="380" alt ="ESP32-LyraT-Mini Development Board" align="center" />](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) |
| [Getting Started with ESP32-LyraT-Mini](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html) | [Getting Started with ESP32-S2-Kaluga-1 Kit](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html) |

| ESP32-Korvo-DU1906 Development Board |
| :----: |
| [<img src="docs/_static/esp32-korvo-du1906-v1.1.png" width="380" alt ="ESP32-Korvo-DU1906 Development Board" align="center" />](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) | 
| [Getting Started with ESP32-Korvo-DU1906](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html) |

#### ESP32-LyraT

An open-source development board, supporting Espressif Systems’ ADF and featuring voice wake-up, a wake-up button and an audio player. Designed for smart speakers and smart-home applications.

[<div align="center"><img src="docs/_static/esp32-lyrat-v4.3-layout-overview.jpg" width="700" alt ="ESP32-LyraT Development Board Overview" align="center" /></div>](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html)

* [Getting Started with ESP32-LyraT](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat.html)
* [ESP32-LyraT V4.3 Hardware Reference](https://docs.espressif.com/projects/esp-adf/en/latest/design-guide/board-esp32-lyrat-v4.3.html)
* [ESP32-LyraT Schematic (PDF)](https://dl.espressif.com/dl/schematics/esp32-lyrat-v4.3-schematic.pdf)

#### ESP32-LyraTD-MSC

Designed for smart speakers and AI applications. Supports Acoustic Echo Cancellation (AEC), Automatic Speech Recognition (ASR), Wake-up Interrupt and Voice Interaction.

[<div align="center"><img src="docs/_static/esp32-lyratd-msc-v2.2-a-top.png" width="740" alt ="ESP32-LyraTD-MSC Development Board Overview" align="center" /></div>](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html)

* [Getting Started with ESP32-LyraTD-MSC](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyratd-msc.html)
* [ESP32-LyraTD-MSC Schematic Lower Board A (PDF) ](https://dl.espressif.com/dl/schematics/ESP32-LyraTD-MSC_A_V2_2-1109A.pdf), [Upper Board B (PDF)](https://dl.espressif.com/dl/schematics/ESP32-LyraTD-MSC_B_V1_1-1109A.pdf)

#### ESP32-LyraT-Mini

An open-source mono development board. Designed for connected smart speakers and smart-home audio applications.

[<div align="center"><img src="docs/_static/esp32-lyrat-mini-v1.2-layout-overview.png" width="740" alt ="ESP32-LyraT-Mini Development Board Overview" align="center" /></div>](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html)

* [Getting Started with ESP32-LyraT-Mini](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-lyrat-mini.html)
* [ESP32-LyraT-Mini Schematic (PDF) ](https://dl.espressif.com/dl/schematics/SCH_ESP32-LYRAT-MINI_V1.2_20190605.pdf)

#### ESP32-S2-Kaluga-1 Kit (ESP-LyraT-8311A)

The ESP32-S2-Kaluga-1 multimedia kit is based on ESP32-S2 and features several extensions including ESP-LyraT-8311A audio board (below) as well as camera, LCD, and touch pad boards.

[<div align="center"><img src="docs/_static/esp-lyrat-8311a-v1.2-layout-front.png" width="740" alt ="ESP-LyraT-8311A Extension Board" align="center" /></div>](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html)

* [Getting Started with ESP32-S2-Kaluga-1 Kit](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/hw-reference/esp32s2/user-guide-esp32-s2-kaluga-1-kit.html)
* [ESP32-S2-Kaluga-1 Schematic (PDF) ](https://dl.espressif.com/dl/schematics/ESP-LyraT-8311A_V1_2_SCH_20200421A.pdf)

#### ESP32-Korvo-DU1906

ESP32-Korvo-DU1906 is an Espressif audio development board with an ESP32-DU1906 module as its core, integrating Wi-Fi, Bluetooth, Bluetooth Low Energy RF and voice/speech signal processing functions. 

[<div align="center"><img src="docs/_static/esp32-korvo-du1906-v1.1.png" width="600" alt ="ESP32-Korvo-DU1906 Development Board" align="center" /></div>](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html)

* [Getting Started with ESP32-Korvo-DU1906](https://docs.espressif.com/projects/esp-adf/en/latest/get-started/get-started-esp32-korvo-du1906.html)
* [ESP32-Korvo-DU1906 Schematic (PDF) ](https://dl.espressif.com/dl/schematics/ESP32-Korvo-DU1906-schematics.pdf)

#### Examples

Check folder [examples](examples) that contains sample applications to demonstrate API features of the ESP-ADF.

# Resources

* [Documentation](https://docs.espressif.com/projects/esp-adf/en/latest/index.html) for the latest version of https://docs.espressif.com/projects/esp-adf/. This documentation is built from the [docs directory](docs) of this repository.
* The [esp32.com forum](https://esp32.com/) is a place to ask questions and find community resources. On the forum there is a [section dedicated to ESP-ADF](https://esp32.com/viewforum.php?f=20) users.
* [Check the Issues section on github](https://github.com/espressif/esp-adf/issues) if you find a bug or have a feature request. Please check existing Issues before opening a new one.
* If you're interested in contributing to ESP-ADF, please check the [Contributions Guide](https://esp-idf.readthedocs.io/en/latest/contribute/index.html).
