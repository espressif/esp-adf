音频流
=============

:link_to_translation:`en:[English]`

音频流指的是负责获取和处理音频数据并将处理后的音频发送出去的 :doc:`音频元素 <../framework/audio_element>`。

以下为支持的音频流：

.. contents::
   :local:
   :depth: 1

每个流 (stream) 有一个结构体作为输入参数进行初始化，其返回的 ``audio_element_handle_t`` 句柄用来调用 ``audio_element.h`` 中的函数。大多数流具有 ``AUDIO_STREAM_READER`` （读）和 ``AUDIO_STREAM_WRITER`` （写）两种类型。例如，可使用 :cpp:func:`i2s_stream_init` 和 :cpp:type:`i2s_stream_cfg_t` 来设置 I2S 流的类型。

有关 API 的详细信息，请参阅下文。


.. _api-reference-stream_algorithm:

算法流
----------------

算法流 (algorithm stream) 集成声学回声消除 (AEC)、自动增益控制 (AGC)、噪声抑制 (NS) 等前端算法，用于处理收取的音频，常用于 VoIP、语音识别、关键词唤醒等需要预处理的场景。算法流调用 :component:`esp-sr`，故占用较大内存，且只支持 ``AUDIO_STREAM_READER`` 类型。


应用示例
^^^^^^^^^^^^^^^^^^^

- :example_file:`advanced_examples/algorithm/main/algorithm_examples.c`
- :example_file:`protocols/voip/main/voip_app.c`

.. include:: /_build/inc/algorithm_stream.inc


.. _api-reference-stream_fatfs:

FatFs 流
------------

FatFs 流从 FatFs 文件系统中读取和写入数据，具有读和写两种类型，类型由 :cpp:type:`audio_stream_type_t` 定义。


应用示例
^^^^^^^^^^^^^^^^^^^

- 读类型示例：:example:`player/pipeline_play_sdcard_music`
- 写类型示例：:example:`recorder/pipeline_recording_to_sdcard`

.. include:: /_build/inc/fatfs_stream.inc


.. _api-reference-stream_http:

HTTP 流
-----------

HTTP 流通过 :cpp:func:`esp_http_client` 获取和发送数据，具有读和写两种类型，类型由 :cpp:type:`audio_stream_type_t` 定义。``AUDIO_STREAM_READER`` 支持 HTTP、HTTPS 和 HTTP 流直播流协议 (HTTP Live Stream) 等协议，使用前需要连接网络。


应用示例
^^^^^^^^^^^^^^^^^^^

- 读类型示例

  - :example:`player/pipeline_living_stream`
  - :example:`player/pipeline_http_mp3`

- 写类型示例

  - :example:`recorder/pipeline_raw_http`

.. include:: /_build/inc/http_stream.inc

.. _api-reference-stream_i2s:

I2S 流
----------

I2S 流通过芯片的 I2S、PDM、ADC、DAC 接口接收和发送音频数据，其中 ADC、DAC 功能需要芯片定义 ``SOC_I2S_SUPPORTS_ADC_DAC``。I2S 流还集成自动电平控制 (ALC) 来调节音量，多通道输出和扩展发送音频数据位宽，相关控制位定义在 :cpp:type:`i2s_stream_cfg_t` 中。


应用示例
^^^^^^^^^^^^^^^^^^^

- 读类型示例：:example:`recorder/pipeline_wav_amr_sdcard`
- 写类型示例：:example:`get-started/play_mp3_control`

.. include:: /_build/inc/i2s_stream.inc


.. _api-reference-stream_pwm:

PWM 流
----------

一些成本敏感的场景中，音频信号不用 DAC 进行转换，而是使用 PWM 对信号进行调制并经过滤波电路实现。PWM 流实现了音频数据用芯片的 PWM 进行调制并发送的功能，只有 ``AUDIO_STREAM_WRITER`` 类型。注意，PWM 的数模转换信噪较低。


应用示例
^^^^^^^^^^^^^^^^^^^

- 写类型示例：:example:`player/pipeline_play_mp3_with_dac_or_pwm`


.. include:: /_build/inc/pwm_stream.inc


.. _api-reference-stream_raw:

原始流
----------

原始流 (raw stream) 用于获取连接的前级元素输出数据，或者为后级连接的元素填充数据，本身不建立线程。``AUDIO_STREAM_READER`` 的应用方式为 [i2s] -> [filter] -> [raw] 或 [i2s] -> [codec-amr] -> [raw]，``AUDIO_STREAM_WRITER`` 的应用方式为 [raw]->[codec-mp3]->[i2s]。


应用示例
^^^^^^^^^^^^^^^^^^^

- 读类型示例：:example:`protocols/voip`
- 写类型示例：:example:`advanced_examples/downmix_pipeline`


.. include:: /_build/inc/raw_stream.inc


.. _api-reference-stream_spiffs:

SPIFFS 流
-------------

SPIFFS 流从 SPIFFS 读取和写入音频数据。


应用示例
^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_spiffs_mp3`


.. include:: /_build/inc/spiffs_stream.inc


.. _api-reference-stream_tcp_client:

TCP 客户端流
-----------------

TCP 客户端流 (TCP client stream) 通过 TCP 读取和写入服务器数据。


应用示例
^^^^^^^^^^^^^^^^^^^

- :example:`get-started/pipeline_tcp_client`


.. include:: /_build/inc/tcp_client_stream.inc


.. _api-reference-stream_tone:

提示音流
-----------

提示音流 (tone stream) 读取 :adf_file:`tools/audio_tone/mk_audio_tone.py` 生成的数据，只支持 ``AUDIO_STREAM_READER`` 类型。


应用示例
^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_flash_tone`


.. include:: /_build/inc/tone_stream.inc


.. _api-reference-embed_flash:

嵌入 Flash 流
------------------------

嵌入 Flash 流 (flash-embedding stream) 读取 :adf_file:`tools/audio_tone/mk_embed_flash.py` 生成的数据，只支持 ``AUDIO_STREAM_READER`` 类型。


应用示例
^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_embed_flash_tone`


.. include:: /_build/inc/embed_flash_stream.inc


.. _api-reference-stream_tts:

语音合成流
------------

语音合成流 (TTS stream) 获取 :component:`esp-sr` 的 ``esp_tts_voice`` 数据，只支持 ``AUDIO_STREAM_READER`` 类型。


应用示例
^^^^^^^^^^^^^^^^^^^

- 读类型示例：:example:`player/pipeline_tts_stream`

.. include:: /_build/inc/tts_stream.inc
