Peripherals
***********

There are several peripherals available in the ESP-ADF, ranging from buttons and LEDs to SD Card or Wi-Fi. The peripherals are implemented using common API that is then expanded with peripheral specific functionality. The following description covers common functionality.

.. toctree::
    :maxdepth: 1

    Common API <esp_peripherals>

The peripheral specific functionality is available by calling dedicated functions described below. Some peripherals are available on both :doc:`ESP32-LyraT <../../design-guide/dev-boards/get-started-esp32-lyrat>` and :doc:`ESP32-LyraTD-MSC <../../design-guide/dev-boards/get-started-esp32-lyratd-msc>` development boards, some on a specific board only. The following table provides all implemented peripherals broken down by development board.

.. toctree::
    :maxdepth: 1
    :hidden:

    Wi-Fi <periph_wifi>
    SD Card <periph_sdcard>
    Spiffs <periph_spiffs>
    Console <periph_console>
    Touch <periph_touch>
    Button <periph_button>
    LED <periph_led>
    ADC Button <periph_adc_button>
    LED Controller <periph_is31fl3216>

.. csv-table::
    :header: Name of Peripheral, ESP32-LyraT, ESP32-LyraTD-MSC

    :doc:`Wi-Fi <periph_wifi>`, |YES|, |YES|
    :doc:`SD Card <periph_sdcard>`, |YES|, |YES|
    :doc:`Spiffs <periph_spiffs>`, |YES|, |YES|
    :doc:`Console <periph_console>`, |YES|, |YES|
    :doc:`Touch <periph_touch>`, |YES|,
    :doc:`Button <periph_button>`, |YES|,
    :doc:`LED <periph_led>`, |YES|,
    :doc:`ADC Button <periph_adc_button>`, , |YES|
    :doc:`LED Controller <periph_is31fl3216>`, , |YES|


.. |YES| image:: ../../../_static/yes-checkm.png