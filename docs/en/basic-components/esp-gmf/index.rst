ESP-GMF General Multimedia Framework
====================================

:link_to_translation:`zh_CN:[中文]`

`ESP-GMF <https://github.com/espressif/esp-gmf>`__ (Espressif General Multimedia Framework) is Espressif's lightweight processing framework for IoT multimedia. It uses elements as the basic processing units, connects them in a pipeline, and transfers audio, video, and general data through ports and a data bus.

Architecture, APIs, and examples are in the `ESP-GMF documentation <https://docs.espressif.com/projects/esp-gmf/en/latest/>`__.

.. list-table::
   :header-rows: 1
   :widths: 22 12 46 20

   * - Component
     - Layer
     - Description
     - Related links
   * - ``gmf_core``
     - Core
     - Defines the processing chain, processing unit, data port, data bus, and FOURCC
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-core/index.html>`__ · `Component <https://components.espressif.com/components/espressif/gmf_core>`__
   * - ``gmf_io``
     - Elements
     - Provides external interfaces for files, HTTP, embedded Flash, I2S PDM, and codec devices
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-elements/gmf-io.html>`__ · `Component <https://components.espressif.com/components/espressif/gmf_io>`__
   * - ``gmf_audio``
     - Elements
     - Provides audio codecs, sample-rate, channel, and bit-depth conversion, plus equalization, mixing, fade, automatic level control, and dynamic range control
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-elements/gmf-audio.html>`__ · `Component <https://components.espressif.com/components/espressif/gmf_audio>`__
   * - ``gmf_video``
     - Elements
     - Provides video codecs, plus scaling, rotation, overlay, and frame-rate conversion, with PPA acceleration when available
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-elements/gmf-video.html>`__ · `Component <https://components.espressif.com/components/espressif/gmf_video>`__
   * - ``gmf_ai_audio``
     - Elements
     - Provides acoustic echo cancellation, noise suppression, automatic gain control, voice activity detection, wake-word detection, and command-word recognition
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-elements/gmf-ai-audio.html>`__ · `Component <https://components.espressif.com/components/espressif/gmf_ai_audio>`__
   * - ``gmf_misc``
     - Elements
     - Copies one input to multiple outputs for splitting and parallel processing
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-elements/gmf-misc.html>`__ · `Component <https://components.espressif.com/components/espressif/gmf_misc>`__
   * - ``gmf_loader``
     - Packages
     - Registers processing units and external interfaces into the framework pool from menuconfig, and applies default parameters
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/gmf-loader.html>`__ · `Component <https://components.espressif.com/components/espressif/gmf_loader>`__
   * - ``gmf_app_utils``
     - Packages
     - Provides application initialization and debug helpers for codecs, I2C, SD cards, Wi-Fi, and the command line
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/gmf-app-utils.html>`__ · `Component <https://components.espressif.com/components/espressif/gmf_app_utils>`__
   * - ``esp_capture``
     - Packages
     - Captures audio and video through a source, processing path, and sink, with encoding, overlay, and local storage
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/esp-capture.html>`__ · `Component <https://components.espressif.com/components/espressif/esp_capture>`__
   * - ``esp_player``
     - Packages
     - Demuxes, decodes, and renders audio and video in one instance; input can be a local file, HTTP(S), HLS, or an external frame
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/esp-player.html>`__ · `Component <https://components.espressif.com/components/espressif/esp_player>`__
   * - ``esp_audio_simple_player``
     - Packages
     - Plays audio by selecting the external interface and decoder from the URI scheme and file extension
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/esp-audio-simple-player.html>`__ · `Component <https://components.espressif.com/components/espressif/esp_audio_simple_player>`__
   * - ``esp_audio_render``
     - Packages
     - Mixes multiple PCM streams and writes the result through a callback; processing chains can run before and after mixing
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/esp-audio-render.html>`__ · `Component <https://components.espressif.com/components/espressif/esp_audio_render>`__
   * - ``esp_video_render``
     - Packages
     - Composites video and UI onto LCD, LVGL, or a frame buffer; supports multiple inputs and dirty-region updates
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/esp-video-render.html>`__ · `Component <https://components.espressif.com/components/espressif/esp_video_render>`__
   * - ``esp_bt_audio``
     - Packages
     - Provides Classic Bluetooth and LE Audio interfaces covering A2DP, HFP, AVRCP, and optional broadcast roles
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/esp-bt-audio.html>`__ · `Component <https://components.espressif.com/components/espressif/esp_bt_audio>`__
   * - ``esp_asrc``
     - Packages
     - Converts sample rate, bit depth, and channels; uses the ASRC peripheral when present, otherwise a software implementation
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/esp-asrc.html>`__ · `Component <https://components.espressif.com/components/espressif/esp_asrc>`__
   * - ``gmf_fft``
     - Packages
     - Provides a fixed-point Q15 real FFT and IFFT for lengths from 32 to 8192, with PIE vector optimizations and a C implementation
     - `Docs <https://docs.espressif.com/projects/esp-gmf/en/latest/gmf-framework/gmf-package/gmf-fft.html>`__ · `Component <https://components.espressif.com/components/espressif/gmf_fft>`__
