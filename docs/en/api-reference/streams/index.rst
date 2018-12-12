Audio Streams
=============

An :doc:`Audio Element <../framework/audio_element>` responsible for acquiring of audio data and then sending the data out after processing, is called the Audio Stream.

The following stream types are supported:

+----------------------+----------------------+
| Stream Name          | Stream Type          |
+======================+======================+
| I2S                  | Reader / Writer      |
+----------------------+----------------------+
| HTTP                 | Reader / Writer      |
+----------------------+----------------------+
| FatFs                | Reader / Writer      |
+----------------------+----------------------+
| Spiffs               | Reader / Writer      |
+----------------------+----------------------+

To set the stream type, use provided structure, e.g. :cpp:type:`i2s_stream_cfg_t` for I2S stream, together with :cpp:type:`audio_stream_type_t` enumerator.

See description below for the API details.


.. _api-reference-stream_i2s:

I2S Stream
----------

When the I2S stream type is "writer", the data may be sent either to a codec chip or to the internal DAC of ESP32. To simplify configuration, two macros are provided to cover each case:

* :cpp:type:`I2S_STREAM_CFG_DEFAULT` - the I2S stream is communicating with a codec chip
* :cpp:type:`I2S_STREAM_INTERNAL_DAC_CFG_DEFAULT` - the stream data are sent to the DAC

Each macro configures several other stream parameters such as sample rate, bits per sample, DMA buffer length, etc.

.. include:: /_build/inc/i2s_stream.inc


.. _api-reference-stream_http:

HTTP Stream
-----------

.. include:: /_build/inc/http_stream.inc


.. _api-reference-stream_fatfs:

FatFs Stream
------------

.. include:: /_build/inc/fatfs_stream.inc


.. _api-reference-stream_spiffs:

Spiffs Stream
-------------

.. include:: /_build/inc/spiffs_stream.inc
