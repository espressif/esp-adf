SD Card Peripheral
==================

If your board has a SD card connected, use this API to initialize, mount and unmount the card, see functions :cpp:func:`periph_sdcard_init`, :cpp:func:`periph_sdcard_mount` and :cpp:func:`periph_sdcard_unmount`. The data reading / writing is implemented in a separate API described in :ref:`api-reference-stream_fatfs`.


Application Examples
--------------------

Implementation of this API is demonstrated in couple of examples:

* :example:`player/pipeline_sdcard_mp3`
* :example:`player/pipeline_sdcard_wav`
* :example:`recorder/pipeline_wav_sdcard`


API Reference
-------------

.. include:: /_build/inc/periph_sdcard.inc

