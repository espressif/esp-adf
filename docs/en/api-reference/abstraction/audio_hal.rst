Audio HAL
=========

Abstraction layer for audio board hardware, serves as an interface between the user application and the hardware driver for specific audio board like :doc:`ESP32 LyraT <../../get-started/index>`.

The API provides data structures to configure sampling rates of ADC and DAC signal conversion, data bit widths, I2C stream parameters, and selection of signal channels connected to ADC and DAC. It also contains several specific functions to e.g. initialize the audio board, :cpp:func:`audio_hal_init`, control the volume, :cpp:func:`audio_hal_get_volume` and :cpp:func:`audio_hal_set_volume`.


API Reference
-------------

.. include:: /_build/inc/audio_hal.inc
