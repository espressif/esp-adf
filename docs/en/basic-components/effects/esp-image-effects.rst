ESP Image Effects
=================

:link_to_translation:`zh_CN:[中文]`

ESP Image Effects is a lightweight image processing component provided by Espressif for ESP series SoCs. It integrates basic image processing capabilities such as color space conversion, rotation, scaling, and cropping, providing a unified, efficient image processing interface for image capture, display, encoding, AI vision, and other applications.

The component uses a unified API architecture, supports a wide range of pixel formats such as RGB and YUV, and processes images of arbitrary resolution, and is compatible with mainstream color space standards such as BT.601, BT.709, and BT.2020. It also supports rotation at any angle, image scaling, region cropping, and high-performance color conversion, and can automatically select an optimized algorithm based on the format and resolution, reducing data conversion overhead while maintaining processing efficiency.

ESP Image Effects is optimized at the algorithm level for embedded platforms and combines efficient memory management with hardware acceleration to achieve high performance, low power consumption, and low memory usage. It is widely applicable to smart cameras, AI vision, industrial inspection, IoT image processing, and other embedded vision products.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_image_effects>`__
- `GitHub Repository <https://github.com/espressif/esp-adf-libs/tree/master/esp_image_effects>`__
- `Tech Blog <https://developer.espressif.com/blog/2025/08/announcing_esp_image_effects>`__
