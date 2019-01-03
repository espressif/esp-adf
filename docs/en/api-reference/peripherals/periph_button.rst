Button Peripheral
=================

To control application flow you may use buttons connected and read through the ESP32 GPIOs. This API provides functions to initialize sepecifc GPIOs and obtain information on button events such as when it has been pressed, when released, when pressed for a long time and released after long press. To get information on particular event, establish a callback function with :cpp:func:`button_dev_add_tap_cb` or :cpp:func:`button_dev_add_press_cb`.


Application Example
-------------------

Implementation of this API is demonstrated in :example:`recorder/pipeline_raw_http` example.


API Reference
-------------

.. include:: /_build/inc/periph_button.inc
