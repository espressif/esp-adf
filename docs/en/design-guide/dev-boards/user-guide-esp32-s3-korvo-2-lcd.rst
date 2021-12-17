=========================
ESP32-S3-Korvo-2-LCD V1.0
=========================

:link_to_translation:`zh_CN:[中文]`

This user guide provides information on the ESP32-S3-Korvo-2-LCD extension board.

This extension board cannot be bought separately and is usually sold together with other accessories of the ESP32-S3-Korvo-2 development board, which will be referred to as main board below.

Currently, ESP32-S3-Korvo-2-LCD V1.0 is sold as part of the :ref:`ESP32-S3-Korvo-2 V3.0 accessories <esp32-s3-korvo-2-v3.0-accessories>`. 

The ESP32-S3-Korvo-2-LCD extends the functionality of your main board by adding an LCD graphic display and capacitive touchpad.

.. figure:: ../../../_static/esp32-s3-korvo-2-lcd-v1.0-overview.png
    :align: center
    :alt: ESP32-S3-Korvo-2-LCD V1.0
    :figclass: align-center

    ESP32-S3-Korvo-2-LCD V1.0

The document consists of the following major sections:

- `Getting started`_: Overview of the board and hardware/software setup instructions to get started.
- `Hardware Reference`_: More detailed information about the board's hardware.
- Hardware Revision Details: Hardware revision history, known issues, and links to user guides for previous versions (if any) of the board.
- `Related Documents`_: Links to related documentation.


Getting Started
===============

This extension board adds a 2.4” LCD graphic display with the resolution of 320x240 and a 10-point capacitive touchpad. This display is connected to ESP32-S3 over the SPI bus.

Description of Components
-------------------------

.. figure:: ../../../_static/esp32-s3-korvo-2-lcd-v1.0-front.png
    :scale: 60%
    :align: center
    :alt: ESP32-S3-Korvo-2-LCD V1.0 - front
    :figclass: align-center

    ESP32-S3-Korvo-2-LCD V1.0 - front

.. figure:: ../../../_static/esp32-s3-korvo-2-lcd-v1.0-back.png
    :scale: 70%
    :align: center
    :alt: ESP32-S3-Korvo-2-LCD V1.0- back
    :figclass: align-center

    ESP32-S3-Korvo-2-LCD V1.0 - back

The key components of the board are described in a clockwise direction. Reserved means that the functionality is available, but the current version of the board does not use it.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Key Component
     - Description
   * - LCD Display
     - A 2.4” 320x240 SPI LCD display module; the display driver/controller is Ilitek ILI934.
   * - Home Key 
     - (Reserved) Returns to homepage or previous page. 
   * - Signal Connector
     - Connects the power, ground and signal wires between the LCD board and main board with FPC cable.
   * - LCD Connector
     - Connects LCD display to the driver circuit of this board.
   * - TP Connector
     - Connects LCD display to the touch circuit of this board.

Start Application Development
-----------------------------

Before powering up your board, please make sure that it is in good condition with no obvious signs of damage.


Required Hardware
^^^^^^^^^^^^^^^^^

- Main board: ESP32-S3-Korvo-2 V3.0
- ESP32-S3-Korvo-2-LCD V1.0
- Mounting copper standoffs and screws (for stable mounting)
- FPC cable (for connecting main board and LCD board)
- Computer running Windows, Linux, or macOS


Hardware Setup
^^^^^^^^^^^^^^

To mount your ESP32-S3-Korvo-2-LCD onto ESP32-S3-Korvo-2:

1. Connect LCD board to main board with FPC cable.
2. Install copper standoffs and screws for stable mounting.


Software Setup
^^^^^^^^^^^^^^

See Section :ref:`Software Setup <esp32-s3-korvo-2-v3.0-software-setup>` of the main board user guide.


Hardware Reference
==================


Block Diagram
-------------

The block diagram below shows the components of ESP32-S3-Korvo-2-LCD and their interconnections.

.. figure:: ../../../_static/esp32-s3-korvo-2-lcd-v1.0-electrical-block-diagram.png
    :align: center
    :alt: ESP32-S3-Korvo-2-LCD
    :figclass: align-center

    ESP32-S3-Korvo-2-LCD


Hardware Revision Details
=========================

Initial release.


Related Documents
=================

- :doc:`ESP32-S3-Korvo-2 V3.0 <user-guide-esp32-s3-korvo-2>`
- `ESP32-S3-Korvo-2-LCD Schematic <https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-KORVO-2-LCD_V1.0_20210918.pdf>`_ (PDF)
- `ESP32-S3-Korvo-2-LCD PCB layout <https://dl.espressif.com/dl/schematics/PCB_ESP32-S3-KORVO-2-LCD_V1.0_20210918.pdf>`_ (PDF)

For further design documentation for the board, please contact us at `sales@espressif.com <sales@espressif.com>`_.