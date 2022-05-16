Services
********

:link_to_translation:`zh_CN:[中文]`

A service is a software implementation of specific product functions, such as input keys, network configuration management, and battery check. Each service is also an abstraction of hardware. For example, the input key service supports ADC keys and GPIO keys. Services can be reused on different products, and high-level APIs and events allow easy and convenient product development.

For details please refer to descriptions listed below.

.. toctree::
    :maxdepth: 1

    Bluetooth Service <bluetooth_service>

    Input Key Service <input_key_service>

    Wi-Fi Service <wifi_service>

    OTA Service <ota_service>

    DuerOS Service <dueros_service>

    Display Service <display_service>

    Battery Service <battery_service>

    Core Dump Upload Service <coredump_upload_service>


.. include:: /_build/inc/periph_service.inc
.. include:: /_build/inc/audio_service.inc