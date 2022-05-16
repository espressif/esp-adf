Display Service
===============

:link_to_translation:`zh_CN:[中文]`

The display service defines enumeration values for some common display patterns in :cpp:type:`display_pattern_t`, with which you can set the corresponding patterns of LEDs or LED bars.

The currently supported LED drivers are AW2013, WS2812, and IS31x.


Application Example
-------------------

Implementation of this API is demonstrated in the following example:

* :example:`checks/check_display_led`


.. include:: /_build/inc/display_service.inc
.. include:: /_build/inc/led_bar_aw2013.inc
.. include:: /_build/inc/led_bar_is31x.inc
.. include:: /_build/inc/led_bar_ws2812.inc
.. include:: /_build/inc/led_indicator.inc