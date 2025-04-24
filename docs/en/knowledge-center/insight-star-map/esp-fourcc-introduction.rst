**************************
Introduction to ESP FOURCC
**************************

:link_to_translation:`zh_CN:[中文]`

FOURCC (Four Character Code) is a widely used identifier in the multimedia domain, typically employed for the rapid identification of audio and video codecs, container formats, pixel formats, and more. Espressif Systems defines a comprehensive FOURCC encoding standard, called ESP FOURCC, which is specified in the open-source project `ESP-GMF <https://github.com/espressif/esp-gmf/tree/main>`__, specifically in the `esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__ header file. This standard covers audio, video, image, container formats, and various pixel formats. This article provides an in-depth overview of the background, significance, definition principles, implementation methods, and advantages of ESP FOURCC.

Background and Significance of FOURCC
--------------------------------------

The history of FOURCC can be traced back to the 1980s with the OSType mechanism in the Macintosh system, where four-character identifiers were used to represent file types. In the 1990s, QuickTime adopted this approach in the audio-video domain, which later became a core identification method in MP4 containers, FFmpeg, and other mainstream frameworks.

In embedded systems, where resources are constrained and memory and computational efficiency are crucial, using compact identifiers is essential. Defining unique enumeration values or constants for each module to represent data formats often leads to the following issues:

1. **Lack of standardization**: If different modules define their own format identifiers independently, the same data format may have different enumeration values across different modules, increasing interface adaptation complexity.
2. **High conversion overhead**: When transferring data between modules, inconsistencies in format identifiers require additional conversion operations, leading to unnecessary runtime overhead.
3. **Difficult debugging**: When different modules use different identification methods, it becomes challenging to intuitively confirm the actual data format during debugging, increasing the complexity of issue resolution.

In contrast, the FOURCC mechanism resolves these issues by using a unified 32-bit identifier. This not only ensures consistency in data formats across modules but also allows for compile-time constant generation, facilitating quick comparisons and lookups, thereby significantly enhancing overall system performance and maintainability.

Fundamental Principles and Implementation of FOURCC
----------------------------------------------------

1. Core Mechanism of FOURCC
~~~~~~~~~~~~~~~~~~~~~~~~~~~

FOURCC is a compact encoding scheme based on ASCII characters. The core principle is to generate a **32-bit unsigned integer** (`uint32_t`) from **four consecutive characters**. For example, the string `"H264"` corresponds to the hexadecimal value `0x48323634`, where each character’s ASCII code is combined in sequence:

- `'H'` → `0x48`
- `'2'` → `0x32`
- `'6'` → `0x36`
- `'4'` → `0x34`

2. Implementation of ESP FOURCC
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In ESP-GMF, FOURCC definitions are primarily found in the `esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__ header file, which includes the following components:

-  **Macro Definition**

  ESP-GMF defines the macro ``ESP_FOURCC_TO_INT(a, b, c, d)``, allowing developers to generate FOURCC values at compile time. This macro converts four input characters into a 32-bit integer. For example:

   .. code:: c

      #define ESP_FOURCC_H264   ESP_FOURCC_TO_INT('H','2','6','4') // 0x48323634

-  **Helper Functions**

  The `esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__  header provides a macro ``ESP_FOURCC_TO_STR(fourcc)``, which calls ``gmf_fourcc_to_str``. This function converts a ``uint32_t`` FOURCC value into a string representation. For example:

   .. code:: c

      printf("Format: %s\n", ESP_FOURCC_TO_STR(0x48323634)); // "H264"

-  **FOURCC Definitions**

  ESP FOURCC v1.0.0 defines commonly used types in multimedia applications for embedded devices, including audio and video codecs, container formats, and pixel formats. For specific details, refer to `esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__ .

Multimedia FOURCC Formats
----------------------------

The `esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__  file defines multimedia formats covering video and audio codecs, containers, and image codecs. Below is a list of commonly used formats:

.. list-table:: ESP FOURCC Classification and Character Representation Table
   :header-rows: 1
   :widths: 20 20 20 40
   :align: center

   * - **Category**
     - **FOURCC Example**
     - **Character Representation**
     - **Encoding Standard/Technical Features**
   * - **Video Codecs**
     - ``ESP_FOURCC_H264``
     - ``'H264'``
     - H.264/AVC (with start codes)
   * -
     - ``ESP_FOURCC_AVC1``
     - ``'AVC1'``
     - H.264 (without start codes, MP4 compatible)
   * -
     - ``ESP_FOURCC_H265``
     - ``'H265'``
     - HEVC/H.265 (high compression efficiency)
   * - **Container Formats**
     - ``ESP_FOURCC_MP4``
     - ``'MP4 '``
     - MPEG-4 Part 14 (multi-track support)
   * -
     - ``ESP_FOURCC_OGG``
     - ``'OGG '``
     - Ogg Container
   * -
     - ``ESP_FOURCC_FLV``
     - ``'FLV '``
     - Flash Video (optimized for streaming)
   * - **Image Codecs**
     - ``ESP_FOURCC_PNG``
     - ``'PNG '``
     - Lossless compression (supports transparency)
   * -
     - ``ESP_FOURCC_JPEG``
     - ``'JPEG'``
     - Lossy compression (high compression ratio)
   * -
     - ``ESP_FOURCC_WEBP``
     - ``'WEBP'``
     - Hybrid lossy/lossless compression (size optimized)
   * - **Audio Codecs**
     - ``ESP_FOURCC_MP3``
     - ``'MP3 '``
     - MPEG Layer III (widely compatible)
   * -
     - ``ESP_FOURCC _AAC``
     - ``'AAC '``
     - Advanced Audio Coding (efficient compression)
   * -
     - ``ESP_FOURCC_OPUS``
     - ``'OPUS'``
     - Low-latency dynamic bitrate (optimized for real-time communication)


FOURCC Pixel Formats
-----------------------

`esp_fourcc.h <https://github.com/espressif/esp-gmf/blob/main/gmf_core/helpers/include/esp_fourcc.h>`__  defines various FOURCC pixel formats, including RGB, YUV, grayscale, and special formats. Each format's data storage method is also specified. The following table lists some commonly used formats:

.. list-table:: ESP FOURCC Pixel Format Classification Table
   :header-rows: 1
   :widths: 20 20 20 40
   :align: center

   * - **Category**
     - **FOURCC Example**
     - **Character Representation**
     - **Technical Features**
   * - **RGB Formats**
     - ``ESP_FOURCC_RGB16``
     - ``'RGB6'``
     - RGB-5-6-5 little-endian, 16-bit compression (low bandwidth)
   * -
     - ``ESP_FOURCC_RGB24``
     - ``'RGB3'``
     - RGB-8-8-8 packed, 24-bit true color
   * - **YUV Formats**
     - ``ESP_FOURCC_NV12``
     - ``'NV12'``
     - YUV 4:2:0 semi-planar (Y plane + interleaved UV)
   * -
     - ``ESP_FOURCC_YUYV``
     - ``'YUYV'``
     - YUV 4:2:2 packed (Y-U-Y-V interleaved)
   * -
     - ``ESP_FOURCC_YUV420P``
     - ``'I420'``
     - YUV 4:2:0 planar format (Y + U + V planes)
   * - **Gray**
     - ``ESP_FOURCC_GREY``
     - ``'GREY'``
     - 8-bit single-channel grayscale
   * - **Special Formats**
     - ``ESP_FOURCC_RAW16``
     - ``'RAW6'``
     - 16-bit raw data (little-endian storage)


Practical Application Cases
------------------------------

In the ESP-GMF framework, various modules (such as video decoders, audio processors, and image filters) utilize FOURCC codes for capability declaration and interface matching. For example, a video decoder may declare its supported FOURCC formats for input and output. The upper-layer scheduler uses FOURCC codes to ensure correct data flow between modules. This mechanism enhances system robustness and facilitates the expansion of new functionalities.
