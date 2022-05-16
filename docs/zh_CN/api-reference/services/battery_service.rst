电池服务
===============

:link_to_translation:`en:[English]`

电池服务 (battery service) 提供了监测和管理电池电压的功能。电池电压和 :cpp:type:`battery_service_event_t` 定义的事件可以通过回调函数与用户进行交互。


应用示例
-------------------

以下示例展示了该 API 的实现方式。

* :example:`system/battery`


.. include:: /_build/inc/battery_service.inc
.. include:: /_build/inc/voltage_monitor.inc