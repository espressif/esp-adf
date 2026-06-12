ESP Live Photo
==============

:link_to_translation:`zh_CN:[中文]`

ESP Live Photo is a Live Photo (Motion Photo) generation and parsing component provided by Espressif for ESP series SoCs. It lets a device directly create and parse motion photo files compatible with smartphones, packaging a static JPEG image and a short MP4 video into a single media file.

The component supports the complete Live Photo lifecycle, including JPEG and MP4 composition, XMP metadata generation, MP4 offset location, and file parsing, and it also supports extracting the JPEG cover image and MP4 video from a Live Photo file. The generated files are directly compatible with phone photo albums that support Live Photo or Motion Photo, without requiring server- or PC-side post-processing.

ESP Live Photo uses a lightweight design and is suitable for smart cameras, smart doorbells, AI cameras, and other IoT devices. Typical uses include capturing photos with a moving moment, recording events, and sharing images.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_live_photo>`__
- `GitHub Repository <https://github.com/espressif/esp-adf/tree/master/components/esp_live_photo>`__
