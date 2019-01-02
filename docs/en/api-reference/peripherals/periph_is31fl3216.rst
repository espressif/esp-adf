LED Controller Peripheral
=========================

This peripheral is applicable to IS31Fl3216 chip that is a light LED controller with an audio modulation mode. It can store data of 8 Frames with internal RAM to play small animations automatically.
You can also use it to control a number of LEDs connected to GPIOs. If you want to use the IS31Fl3216, see functions :cpp:func:`periph_is31fl3216_init`, :cpp:func:`periph_is31fl3216_set_blink_pattern`, :cpp:func:`periph_is31fl3216_set_duty`, :cpp:func:`periph_is31fl3216_set_state`.


Application Examples
--------------------

Implementation of this API is demonstrated in :example:`checks/check_msc_leds` example.


API Reference
-------------

.. include:: /_build/inc/periph_is31fl3216.inc
