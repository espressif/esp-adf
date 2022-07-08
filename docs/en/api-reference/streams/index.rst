Audio Streams
=============

:link_to_translation:`zh_CN:[中文]`

The Audio Stream refers to an :doc:`Audio Element <../framework/audio_element>` that is responsible for acquiring of audio data and then sending the data out after processing.

The following stream types are supported:

.. contents::
   :local:
   :depth: 1

Each stream is initialized with a structure as an input, and the returned ``audio_element_handle_t`` handle is used to call the functions in ``audio_element.h``. Most streams have two types, ``AUDIO_STREAM_READER`` (reader) and ``AUDIO_STREAM_WRITER`` (writer). For example, to set the I2S stream type, use :cpp:func:`i2s_stream_init` and :cpp:type:`i2s_stream_cfg_t`.

See description below for the API details.


.. _api-reference-stream_algorithm:

Algorithm Stream
----------------

The algorithm stream integrates front-end algorithms such as acoustic echo cancellation (AEC), automatic gain control (AGC), and noise suppression (NS) to process the received audio. It is often used in audio preprocessing scenarios, including VoIP, speech recognition, and keyword wake-up. The stream calls :component:`esp-sr` and thus occupies large memory. The stream only supports the ``AUDIO_STREAM_READER`` type.


Application Example
^^^^^^^^^^^^^^^^^^^

- :example_file:`advanced_examples/algorithm/main/algorithm_examples.c`
- :example_file:`protocols/voip/main/voip_app.c`

.. include:: /_build/inc/algorithm_stream.inc


.. _api-reference-stream_fatfs:

FatFs Stream
------------

The FatFs stream reads and writes data from FatFs. It has two types: "reader" and "writer". The type is defined by :cpp:type:`audio_stream_type_t`.


Application Example
^^^^^^^^^^^^^^^^^^^

- Reader example: :example:`player/pipeline_play_sdcard_music`
- Writer example: :example:`recorder/pipeline_recording_to_sdcard`

.. include:: /_build/inc/fatfs_stream.inc


.. _api-reference-stream_http:

HTTP Stream
-----------

The HTTP stream obtains and sends data through :cpp:func:`esp_http_client`. The stream has two types: "reader" and "writer", and the type is defined by :cpp:type:`audio_stream_type_t`. ``AUDIO_STREAM_READER`` supports HTTP, HTTPS, HTTP Live Stream, and other protocols. Make sure the network is connected before using the stream.


Application Example
^^^^^^^^^^^^^^^^^^^

- Reader example

  - :example:`player/pipeline_living_stream`
  - :example:`player/pipeline_http_mp3`

- Writer example

  - :example:`recorder/pipeline_raw_http`

.. include:: /_build/inc/http_stream.inc

.. _api-reference-stream_i2s:

I2S Stream
----------

The I2S stream receives and transmits audio data through the chip's I2S, PDM, ADC, and DAC interfaces. To use the ADC and DAC functions, the chip needs to define ``SOC_I2S_SUPPORTS_ADC_DAC``. The stream integrates automatic level control (ALC) to adjust volume, multi-channel output, and sending audio data with extended bit width. The relevant control bits are defined in :cpp:type:`i2s_stream_cfg_t`.


Application Example
^^^^^^^^^^^^^^^^^^^

- Reader example: :example:`recorder/pipeline_wav_amr_sdcard`
- Writer example: :example:`get-started/play_mp3_control`

.. include:: /_build/inc/i2s_stream.inc


.. _api-reference-stream_pwm:

PWM Stream
----------

In some cost-sensitive scenarios, the audio signal is not converted by the DAC but is modulated by the PWM (pulse width modulation) and then implemented by a filter circuit. The PWM stream modulates the audio signal with the chip's PWM and sends out the processed audio. It only has the ``AUDIO_STREAM_WRITER`` type. Note that the digital-to-analog conversion by PWM has a lower signal-to-noise ratio.


Application Example
^^^^^^^^^^^^^^^^^^^

- Writer example: :example:`player/pipeline_play_mp3_with_dac_or_pwm`


.. include:: /_build/inc/pwm_stream.inc


.. _api-reference-stream_raw:

Raw Stream
----------

The raw stream is used to obtain the output data of the previous element of the connection or to provide the data for the next element of the connection. It does not create a thread. For ``AUDIO_STREAM_READER``, the connection is [i2s] -> [filter] -> [raw] or [i2s] -> [codec-amr] -> [raw]. For ``AUDIO_STREAM_WRITER``, the connection is [raw] ->[codec-mp3]->[i2s].


Application Example
^^^^^^^^^^^^^^^^^^^

- Reader example: :example:`protocols/voip`
- Writer example: :example:`advanced_examples/downmix_pipeline`


.. include:: /_build/inc/raw_stream.inc


.. _api-reference-stream_spiffs:

SPIFFS Stream
-------------

The SPIFFS stream reads and writes audio data from or into SPIFFS.

Application Example
^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_spiffs_mp3`


.. include:: /_build/inc/spiffs_stream.inc


.. _api-reference-stream_tcp_client:

TCP Client Stream
-----------------

The TCP client stream reads and writes server data over TCP.


Application Example
^^^^^^^^^^^^^^^^^^^

- :example:`get-started/pipeline_tcp_client`


.. include:: /_build/inc/tcp_client_stream.inc


.. _api-reference-stream_tone:

Tone Stream
-----------

The tone stream reads the data generated by :adf_file:`tools/audio_tone/mk_audio_tone.py`. It only supports the ``AUDIO_STREAM_READER`` type.


Application Example
^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_flash_tone`


.. include:: /_build/inc/tone_stream.inc


.. _api-reference-embed_flash:

Flash-Embedding Stream
----------------------

The flash-embedding stream reads the data generated by :adf_file:`tools/audio_tone/mk_embed_flash.py`. It only supports the ``AUDIO_STREAM_READER`` type.


Application Example
^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_embed_flash_tone`


.. include:: /_build/inc/embed_flash_stream.inc


.. _api-reference-stream_tts:

TTS Stream
----------

The tex-to-speech stream (TTS stream) obtains the ``esp_tts_voice`` data of :component:`esp-sr`. It only supports the ``AUDIO_STREAM_READER`` type.


Application Example
^^^^^^^^^^^^^^^^^^^

- Reader example: :example:`player/pipeline_tts_stream`

.. include:: /_build/inc/tts_stream.inc
