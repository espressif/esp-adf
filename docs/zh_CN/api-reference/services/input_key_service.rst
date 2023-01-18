输入按键服务
=================

:link_to_translation:`en:[English]`

输入按键服务 (input key service) 将 GPIO 输入中断和 ADC 按键功能以事件的形式提供给用户。事件定义的音频产品常用按键功能参见 :cpp:type:`input_key_user_id_t`。


应用示例
-------------------

以下示例展示了该 API 的实现方式。

* :example:`checks/check_board_buttons`
* :example:`player/pipeline_sdcard_mp3_control`
* :example:`protocols/voip`

.. include:: /_build/inc/input_key_service.inc
.. include:: /_build/inc/input_key_com_user_id.inc