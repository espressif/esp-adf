ESP Peripherals
===============

This library simplifies the management of peripherals, by pooling and monitoring in a single task, adding basic functions to send and receive events. And it also provides APIs to easily integrate new peripherals.

.. note::

    Note that if you do not intend to integrate new peripherals into esp_peripherals, you are only interested in simple api ``esp_periph_init``, ``esp_periph_start``, ``esp_periph_stop`` and ``esp_periph_destroy``.  If you want to integrate new peripherals, please refer to :doc:`Periph Button <./periph_button>` source code

Examples
--------

Please refer to :example_file:`player/pipeline_http_mp3/main/play_http_mp3_example.c`.


API Reference
-------------

.. include:: /_build/inc/esp_peripherals.inc
