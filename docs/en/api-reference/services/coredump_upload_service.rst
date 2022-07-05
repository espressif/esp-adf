Core Dump Upload Service
========================

:link_to_translation:`zh_CN:[中文]`

To investigate crashes in some sold devices, the backtrace is needed for analysis. The core dump upload service transmits the backtrace stored in the device partition over HTTP. To enable this feature, select ``ESP_COREDUMP_ENABLE_TO_FLASH``.


Application Example
-------------------

Implementation of this API is demonstrated in the following example:

* :example:`system/coredump`


.. include:: /_build/inc/coredump_upload_service.inc