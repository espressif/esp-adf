Console Peripheral
==================

Console Peripheral is used to control the Audio application from the terminal screen. It provides 2 ways do execute command, one sends an event to esp_peripherals (for a command without parameters), another calls a callback function (need parameters). If there is a callback function, no event will be sent.

Please make sure that the lifetime of :cpp:type:`periph_console_cmd_t` must be ensured during console operation, :cpp:func:`periph_console_init` only reference, does not make a copy.

Code example
------------

Please refer to :example_file:`cli/main/console_example.c`.


API Reference
-------------

.. include:: /_build/inc/periph_console.inc
