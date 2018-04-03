Audio Streams
=============

An :doc:`Audio Element <../framework/audio_element>` responsible for acquiring of audio data and then sending the data out after processing, is called the Audio Stream.

The following stream types are supported:

+----------------------+----------------------+
| Stream Name          | Stream  Type         |
+======================+======================+
| I2S                  | Reader / Writer      |
+----------------------+----------------------+
| HTTP                 | Reader / :sup:`1`    |
+----------------------+----------------------+
| FatFs                | Reader / Writer      |
+----------------------+----------------------+

1. The **Writer** type for the HTTP stream is yet to be implemented.

To set the configure the stream type use provided structure, e.g. :cpp:type:`i2s_stream_cfg_t` for I2S stream, together with :cpp:type:`audio_stream_type_t` enumerator.

See description below for the API details.


.. _api-reference-stream_i2s:

I2S Stream
----------

.. include:: /_build/inc/i2s_stream.inc


.. _api-reference-stream_http:

HTTP Stream
-----------

.. include:: /_build/inc/http_stream.inc


.. _api-reference-stream_fatfs:

FatFs Stream
------------

.. include:: /_build/inc/fatfs_stream.inc
