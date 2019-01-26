Equalizer
=========

Provided in this API equalizer supports:

* fixed number of ten (10) bands;
* four sample rates: 11025 Hz, 22050 Hz, 44100 Hz and 48000 Hz.

The center frequencies of bands are shown in table below.

.. csv-table::
    :header: Band Index, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
 
    Frequency, 31 Hz, 62 Hz, 125 Hz, 250 Hz, 500 Hz, 1 kHz, 2 kHz, 4 kHz, 8 kHz, 16 kHz

Default gain of each band is -13 dB. To set the gains of all bands use structure :cpp:type:`equalizer_cfg`. To set the gain of individual band use function :cpp:func:`equalizer_set_gain_info`.


Application Example
-------------------

Implementation of this API is demonstrated in the :example:`audio_processing/equalizer` example.


API Reference
-------------

.. include:: /_build/inc/equalizer.inc
