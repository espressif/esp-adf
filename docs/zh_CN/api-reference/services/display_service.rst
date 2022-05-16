显示服务
===============

:link_to_translation:`en:[English]`

显示服务 (display service) 在 :cpp:type:`display_pattern_t` 中定义了一些常用的显示样式枚举值，帮助用户设置 LED 或者 LED 灯条的对应样式。

已经支持的 LED 驱动芯片有 AW2013、WS2812、IS31x。


应用示例
-------------------

以下示例展示了该 API 的实现方式。

* :example:`checks/check_display_led`


.. include:: /_build/inc/display_service.inc
.. include:: /_build/inc/led_bar_aw2013.inc
.. include:: /_build/inc/led_bar_is31x.inc
.. include:: /_build/inc/led_bar_ws2812.inc
.. include:: /_build/inc/led_indicator.inc