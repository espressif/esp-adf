ESP32-LyraT V4.3 Getting Started Guide
======================================

:link_to_translation:`zh_CN:[中文]`

This guide provides users with functional descriptions, configuration options for ESP32-LyraT V4.3 audio development board, as well as how to get started with the ESP32-LyraT board. Check section `Other Versions of LyraT`_, if you have different version of this board.

The ESP32-LyraT is a hardware platform designed for the dual-core ESP32 audio applications, e.g., Wi-Fi or Bluetooth audio speakers, speech-based remote controllers, connected smart-home appliances with one or more audio functionality, etc. 

The ESP32-LyraT is a stereo audio board. If you are looking for a mono audio board, intended for lower end applications, check :doc:`get-started-esp32-lyrat-mini`.


What You Need
-------------

* 1 × :ref:`ESP32 LyraT V4.3 board <get-started-esp32-lyrat-v4.3-board>`
* 2 x Speaker or headphones with a 3.5 mm jack. If you use a speaker, it is recommended to choose one no more than 3 watts, and JST PH 2.0 2-Pin plugs are needed. In case you do not have this type of plug it is also fine to use Dupont female jumper wires during development.
* 2 x Micro-USB 2.0 cables, Type A to Micro B
* 1 × PC loaded with Windows, Linux or Mac OS

If you like to start using this board right now, go directly to section `Start Application Development`_.


Overview
^^^^^^^^

The ESP32-LyraT V4.3 is an audio development board produced by `Espressif <https://espressif.com>`_ built around ESP32. It is intended for audio applications, by providing hardware for audio processing and additional RAM on top of what is already onboard of the ESP32 chip. The specific hardware includes:

* ESP32-WROVER-E Module
* Audio Codec Chip
* Dual Microphones on board
* Headphone output
* 2 x 3-watt Speaker output
* Dual Auxiliary Input
* MicroSD Card slot (1 line or 4 lines)
* Six buttons (2 physical buttons and 4 touch buttons)
* JTAG header
* Integrated USB-UART Bridge Chip
* Li-ion Battery-Charge Management

The block diagram below presents main components of the ESP32-LyraT and interconnections between components.

.. figure:: ../../../_static/esp32-lyrat-v4.3-block-diagram.jpg
    :alt: ESP32 LyraT block diagram
    :figclass: align-center

    ESP32-LyraT Block Diagram


Components
^^^^^^^^^^

The following list and figure describe key components, interfaces and controls of the ESP32-LyraT used in this guide. This covers just what is needed now. For detailed technical documentation of this board, please refer to :doc:`board-esp32-lyrat-v4.3` and `ESP32 LyraT V4.3 schematic`_ (PDF).


ESP32-WROVER-E Module
    The ESP32-WROVER-E module contains ESP32 chip to provide Wi-Fi / Bluetooth connectivity and data processing power as well as integrates 4 MB external SPI flash and an additional 8 MB PSRAM for flexible data storage.
Headphone Output
    Output socket to connect headphones with a 3.5 mm stereo jack.

    .. note::

        The socket may be used with mobile phone headsets and is compatible with OMPT standard headsets only. It does not work with CTIA headsets. Please refer to `Phone connector (audio) <https://en.wikipedia.org/wiki/Phone_connector_(audio)#TRRS_standards>`_ on Wikipedia.

.. _get-started-esp32-lyrat-v4.3-board:

.. figure:: ../../../_static/esp32-lyrat-v4.3-layout-overview-with-wrover-e-module.jpg
    :alt: ESP32 LyraT V4.3 Board Layout Overview
    :figclass: align-center

    ESP32-LyraT V4.3 Board Layout Overview

Left Speaker Output
    Output socket to connect a speaker. The 4-ohm and 3-watt speaker is recommended. The pins have a 2.00 mm / 0.08" pitch.
Right Speaker Output
    Output socket to connect a speaker. The 4-ohm and 3-watt speaker is recommended. The pins have a 2.00 mm / 0.08" pitch.
Boot/Reset Press Keys
    Boot: holding down the **Boot** button and momentarily pressing the **Reset** button initiates the firmware upload mode. Then user can upload firmware through the serial port. Reset: pressing this button alone resets the system.
Audio Codec Chip
    The Audio Codec Chip, `ES8388 <http://www.everest-semi.com/pdf/ES8388%20DS.pdf>`_, is a low power stereo audio codec with a headphone amplifier. It consists of 2-channel ADC, 2-channel DAC, microphone amplifier, headphone amplifier, digital sound effects, analog mixing and gain functions. It is interfaced with **ESP32-WROVER-E Module** over I2S and I2S buses to provide audio processing in hardware independently from the audio application.
USB-UART Port
    Functions as the communication interface between a PC and the **ESP32-WROVER-E Module**.
USB Power Port
    Provides the power supply for the board.
Standby / Charging LEDs
    The **Standby** green LED indicates that power has been applied to the **Micro USB Port**. The **Charging** red LED indicates that a battery connected to the **Battery Socket** is being charged.
Power Switch
    Power on/off knob: toggling it to the left powers the board on; toggling it to the right powers the board off.
Power On LED
    Red LED indicating that **Power On Switch** is turned on.


Start Application Development
-----------------------------

Before powering up the ESP32-LyraT, please make sure that the board has been received in good condition with no obvious signs of damage.


Initial Setup
^^^^^^^^^^^^^

Prepare the board for loading of the first sample application:

1. Connect speakers to the **Right** and **Left Speaker Output**. Connecting headphones to the **Headphone Output** is an option.
2. Plug in the Micro-USB cables to the PC and to **both USB ports** of the ESP32 LyraT.
3. The **Standby LED** (green) should turn on. Assuming that a battery is not connected, the **Charging LED** (red) will blink every couple of seconds.
4. Toggle left the **Power On Switch**.
5. The red **Power On LED** should turn on.

If this is what you see on the LEDs, the board should be ready for application upload. Now prepare the PC by loading and configuring development tools what is discussed in the next section.


Develop Applications
^^^^^^^^^^^^^^^^^^^^

Once the board is initially set up and checked, you can start preparing the development tools. The Section :ref:`get-started-step-by-step` will walk you through the following steps:

* **Set up ESP-IDF** to get a common development framework for the ESP32 (and ESP32-S2) chips in C language;
* **Get ESP-ADF**  to install the API specific to audio applications;
* **Set up env** to make the framework aware of the audio specific API;
* **Start a Project** that will provide a sample audio application for the board;
* **Connect Your Device** to prepare the application for loading;
*  **Build the Project** to finally run the application and play some music.


Summary of Key Changes from LyraT V4.2
--------------------------------------

* Changed the integrated module to ESP32-WROVER-E from ESP32-WROVER.
* Removed Red LED indicator light.
* Introduced headphone jack insert detection.
* Replaced single Power Amplifier (PA) chip with two separate chips.
* Updated power management design of several circuits: Battery Charging, ESP32, MicorSD, Codec Chip and PA.
* Updated electrical implementation design of several circuits: UART, Codec Chip, Left and Right Microphones, AUX Input, Headphone Output, MicroSD, Push Buttons and Automatic Upload.


Other Versions of LyraT
-----------------------

* :doc:`get-started-esp32-lyrat-v4.2`
* :doc:`get-started-esp32-lyrat-v4`

Other Boards from LyraT Family
------------------------------

* :doc:`get-started-esp32-lyrat-mini`
* :doc:`get-started-esp32-lyratd-msc`

Related Documents
-----------------

* :doc:`board-esp32-lyrat-v4.3`
* `ESP32 LyraT V4.3 schematic`_ (PDF)
* `ESP32-LyraT V4.3 Component Layout`_ (PDF)
* `ESP32 Datasheet <https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf>`_ (PDF)
* `ESP32-WROVER-E Datasheet <https://www.espressif.com/sites/default/files/documentation/esp32-wrover-e_esp32-wrover-ie_datasheet_en.pdf>`_ (PDF)


.. _ESP32 LyraT V4.3 schematic: https://dl.espressif.com/dl/schematics/ESP32-LYRAT_V4.3-20220119.pdf
.. _ESP32-LyraT V4.3 Component Layout: https://dl.espressif.com/dl/schematics/ESP32-LyraT_v4.3_component_layout.pdf
