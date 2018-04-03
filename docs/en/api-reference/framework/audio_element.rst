Audio Element
=============

The basic building block for the application programmer developing with ADF is the ``audio_element`` object. Every decoder, encoder, filter, input stream, or output stream is in fact an Audio Element. 

This API has been designed and then used to implement Audio Elements provided by ADF.

The general functionality of an Element is to take some data on input, processes it, and output to a the next. Each Element is run as a separate task. To enable control on particular stages of the data lifecycle from the input, during processing and up to the output, the ``audio_element`` object provides possibility to trigger callbacks per stage. There are seven types of available callback functions: open, seek, process, close, destroy, read and write, and they are defined in :cpp:type:`audio_element_cfg_t`. Particular Elements typically use a subset of all avialable callbacks. For instance the :doc:`../codecs/mp3_decoder` is using open, process, close and destroy callback functions.

The available Audio Element types intended for development with this API are listed in description of :doc:`audio_common.h <audio_common>` header file under :cpp:type:`audio_element_type_t` enumerator.


API Reference
-------------

.. include:: /_build/inc/audio_element.inc
