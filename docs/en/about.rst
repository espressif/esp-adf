About
=====

This is documentation of `ESP-ADF <https://github.com/espressif/esp-adf>`_, the framework to develop audio applications for `ESP32 <https://espressif.com/en/products/hardware/esp32/overview>`_ chip by `Espressif <https://espressif.com>`_.

The **ESP32** is 2.4 GHz Wi-Fi and Bluetooth combo, 32 bit dual core chip running up to 240 MHz, designed for mobile, wearable electronics, and Internet-of-Things (IoT) applications. It has several peripherals on board including I2S interfaces to easy integrate with dedicated audio chips. These hardware features together with the ESP-ADF software provide a powerful platform to implement audio applications including native wireless networking and powerful user interface.

The **ESP-ADF** provides a range of API components including **Audio Streams**, **Codecs** and **Services** organized in **Audio Pipeline**, all integrated with audio hardware through **Media HAL** and with **Peripherals** onboard of **ESP32**.

.. figure:: ../_static/adf_block_diagram.png
    :align: center
    :alt: Espressif Audio Development Framework
    :figclass: align-center

    Espressif Audio Development Framework

The ESP-ADF also provides integration with **Baidu DauerOS** cloud services. A range of components is coming to provide integration with DeepBrain, Amazon, Google, Alibaba and Turing cloud services.

The **ESP-ADF** builds on well established, FreeRTOS based, Espressif IOT Development Framework `ESP-IDF <https://github.com/espressif/esp-idf>`_. 
