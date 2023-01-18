Input Key Service
=================

:link_to_translation:`zh_CN:[中文]`

The input key service provides GPIO input interrupts and ADC key functions in the form of events. For what common key functions are defined by the events for audio products, see :cpp:type:`input_key_user_id_t`.


Application Example
-------------------

Implementation of this API is demonstrated in the following example:

* :example:`checks/check_board_buttons`
* :example:`player/pipeline_sdcard_mp3_control`
* :example:`protocols/voip`

.. include:: /_build/inc/input_key_service.inc
.. include:: /_build/inc/input_key_com_user_id.inc