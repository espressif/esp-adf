核心转储上传服务
========================

:link_to_translation:`en:[English]`

如需调查已出售设备中的 crash 事件，则需要回收回溯信息进行分析。核心转储上传服务 (core dump upload service) 支持通过 HTTP 将存储在设备分区中的回溯信息传输回来。选择 ``ESP_COREDUMP_ENABLE_TO_FLASH``，即可启用此功能。


应用示例
-------------------

以下示例展示了该 API 的实现方式。

* :example:`system/coredump`


.. include:: /_build/inc/coredump_upload_service.inc