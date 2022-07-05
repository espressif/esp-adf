服务
********

:link_to_translation:`en:[English]`

服务 (service) 框架是具体产品功能在软件层面上的实现，如输入按键、网络配置管理、电池检测等功能。每个服务也是对硬件的抽象，如输入按键服务支持 ADC 类型和 GPIO 类型的的按键。服务可以复用在不同产品上，高级 API 和事件可以让用户简单便捷的开发产品。

更多信息请参考以下文档。

.. toctree::
    :maxdepth: 1

    蓝牙服务 <bluetooth_service>

    输入按键服务 <input_key_service>

    Wi-Fi 服务 <wifi_service>

    OTA 服务 <ota_service>

    DuerOS 服务 <dueros_service>

    显示服务 <display_service>

    电池服务 <battery_service>

    核心转储上传服务 <coredump_upload_service>


.. include:: /_build/inc/periph_service.inc
.. include:: /_build/inc/audio_service.inc