Event Interface
===============

The ADF provides the Event Interface API to establish communication between Audio Elements in a pipeline. The API is built around FreeRTOS queue. It implements 'listeners' to watch for incoming messages and inform about them with a callback function.


Application Examples
--------------------

Implementation of this API is demonstrated in couple of examples including :example:`get-started/play_mp3`.


API Reference
-------------

.. include:: /_build/inc/audio_event_iface.inc
