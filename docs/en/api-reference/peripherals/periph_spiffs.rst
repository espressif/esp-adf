Spiffs Peripheral
==================

Use this API to initialize, mount and unmount spiffs partition, see functions :cpp:func:`periph_spiffs_init`, :cpp:func:`periph_spiffs_mount` and :cpp:func:`periph_spiffs_unmount`. The data reading / writing is implemented in a separate API described in :ref:`api-reference-stream_spiffs`.


Application Example
-------------------

Implementation of this API is demonstrated in :example:`filter/pipeline_spiffs_amr_resample` example.


API Reference
-------------

.. include:: /_build/inc/periph_spiffs.inc

