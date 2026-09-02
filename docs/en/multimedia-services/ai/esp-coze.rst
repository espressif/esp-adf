ESP Coze
=========

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`ESP Coze <https://components.espressif.com/components/espressif/esp_coze>`__ is an open-source component for ESP-IDF based on the Coze platform. It communicates with Coze services over WebSocket and provides three independent entry points: streaming conversation, text-to-speech, and speech recognition. It is suited to voice assistants and spoken-question answering on ESP32-class devices.

Feature List
------------

- Streaming conversation (Chat): full-duplex voice + text interaction, with support for subtitles, custom parameters, and conversation ID management
- Text-to-speech (TTS): submit text and receive decoded audio through a callback
- Automatic speech recognition (ASR): continuously stream audio upstream, with support for either server-side VAD or application-driven end-of-utterance detection
- The three entry points are independent of one another, each maintaining its own WebSocket connection, and can be used in parallel at the same time
- A unified set of audio codec types (OPUS, G.711A, G.711U, PCM) is shared by all three modules using the same encoding constants
- Two authentication methods, PAT or OAuth/JWT, with helper APIs provided for JWT signing and HTTP POST
- Kconfig options for tuning the WebSocket task's priority, core affinity, buffer size, and various timeout durations

Technical Deep Dive
--------------------

Chat (Streaming Conversation)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``esp_coze_chat_init`` uses ``esp_coze_chat_config_t`` to create a session handle, without establishing a connection yet. Calling ``esp_coze_chat_start`` connects the WebSocket, waits for the connection to be established, and pushes the initial ``chat.update``\ . When the session ends, call ``esp_coze_chat_stop`` followed by ``esp_coze_chat_deinit``\ .

.. code:: c

    esp_coze_chat_config_t cfg = ESP_COZE_CHAT_DEFAULT_CONFIG();
    cfg.bot_id          = COZE_BOT_ID;
    cfg.access_token    = COZE_ACCESS_TOKEN;
    cfg.enable_subtitle = true;
    cfg.audio_callback  = on_downlink_audio;
    cfg.event_callback  = on_chat_event;

    esp_coze_chat_handle_t chat = NULL;
    esp_coze_chat_init(&cfg, &chat);
    esp_coze_chat_start(chat);
    /* Continuously feed encoded microphone frames using esp_coze_chat_send_audio_data() */
    esp_coze_chat_stop(chat);
    esp_coze_chat_deinit(chat);

Upstream audio is sent via ``esp_coze_chat_send_audio_data``, which internally base64-encodes it and forwards it as an ``input_audio_buffer.append`` event; ``esp_coze_chat_send_audio_complete`` marks the end of an utterance in barge-in mode, and ``esp_coze_chat_audio_data_clearup`` clears audio already buffered on the server. Downstream content is obtained through two kinds of callbacks: ``audio_callback`` receives only the decoded audio bytes, while all other notifications (subtitles, errors, raw events, etc.) are dispatched through the ``esp_coze_chat_event_callback_t`` callback as an ``esp_coze_chat_event_t``\ ; if ``audio_callback`` is not set, downstream audio is instead delivered via ``ESP_COZE_CHAT_EVENT_AUDIO_DATA`` through the event callback.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Event
     - event_data
   * - ``ESP_COZE_CHAT_EVENT_CHAT_COMPLETED``
     - None (end of one conversation turn)
   * - ``ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED`` / ``_STOPED``
     - None (server-side VAD determines the start/end of speech)
   * - ``ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT``
     - Subtitle text (requires ``enable_subtitle = true``\ )
   * - ``ESP_COZE_CHAT_EVENT_CHAT_ERROR``
     - Error JSON text
   * - ``ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA``
     - Raw JSON with an unrecognized ``event_type``, for the application to parse itself

To adjust the voice, custom parameters, or conversation ID while a session is in progress, call ``esp_coze_set_chat_config_voice_id``\ , ``esp_coze_set_chat_config_parameters``\ , or ``esp_coze_set_chat_config_conversation_id``, and then call ``esp_coze_chat_update_chat`` to push the new configuration to the server for it to take effect.

TTS (Text-to-Speech)
^^^^^^^^^^^^^^^^^^^^^

TTS is a streamlined entry point that only performs speech synthesis. Its API shape is similar to Chat's, but with fewer configuration options: ``esp_coze_tts_init`` creates a handle, ``esp_coze_tts_start`` establishes the connection and pushes the initial ``chat.update``\ , and after that, each call to ``esp_coze_tts_send_text`` submits a piece of text to be synthesized. The decoded audio is continuously delivered via ``audio_callback``, and ``ESP_COZE_TTS_EVENT_CHAT_COMPLETED`` is received when a round of synthesis completes.

.. code:: c

    esp_coze_tts_config_t cfg = ESP_COZE_TTS_DEFAULT_CONFIG();
    cfg.bot_id         = COZE_BOT_ID;
    cfg.access_token   = COZE_ACCESS_TOKEN;
    cfg.audio_callback = on_audio;

    esp_coze_tts_handle_t tts = NULL;
    esp_coze_tts_init(&cfg, &tts);
    esp_coze_tts_start(tts);
    esp_coze_tts_send_text(tts, "Hello");  /* Example text to synthesize */
    /* After waiting for a number of ESP_COZE_TTS_EVENT_CHAT_COMPLETED events */
    esp_coze_tts_stop(tts);
    esp_coze_tts_deinit(tts);

ASR (Automatic Speech Recognition)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

ASR only performs recognition: encoded audio frames are continuously streamed upstream via ``esp_coze_asr_send_audio``, and recognition results are returned as ``esp_coze_asr_event_t`` events: ``ESP_COZE_ASR_EVENT_TRANSCRIPT_UPDATE`` carries an intermediate transcript, while ``ESP_COZE_ASR_EVENT_TRANSCRIPT_COMPLETED`` carries the final result. The end-of-utterance detection method is determined by ``esp_coze_asr_turn_detection_t``\ : ``ESP_COZE_ASR_TURN_DETECTION_SERVER_VAD`` (the default) has the server determine the start/end of speech based on ``vad_prefix_padding_ms`` / ``vad_silence_duration_ms`` and push ``SPEECH_STARTED`` / ``SPEECH_STOPPED``\ ; ``ESP_COZE_ASR_TURN_DETECTION_NONE`` requires the application to actively call ``esp_coze_asr_send_audio_complete`` after each utterance ends.

.. code:: c

    esp_coze_asr_config_t cfg = ESP_COZE_ASR_DEFAULT_CONFIG();
    cfg.bot_id         = COZE_BOT_ID;
    cfg.access_token   = COZE_ACCESS_TOKEN;
    cfg.event_callback = on_asr;

    esp_coze_asr_handle_t asr = NULL;
    esp_coze_asr_init(&cfg, &asr);
    esp_coze_asr_start(asr);
    /* Continuously call esp_coze_asr_send_audio(asr, frame, frame_len) */
    esp_coze_asr_send_audio_complete(asr);
    esp_coze_asr_stop(asr);
    esp_coze_asr_deinit(asr);

Authentication
---------------

The Chat, TTS, and ASR configuration structures all require an ``access_token``\ , i.e. a Coze PAT (Personal Access Token) or a bearer token generated via OAuth/JWT; see the `Coze authentication documentation <https://www.coze.cn/open/docs/developer_guides/authentication>`__ for details. When using the OAuth/JWT method, the component provides two helper APIs: ``esp_coze_jwt_create`` signs the JWT header and payload with RS256 and returns a token string ready to use; ``esp_coze_http_post`` issues a synchronous HTTP POST request, used to exchange credentials for the final access token via Coze's OAuth endpoint. Business credentials such as the PAT and Bot ID are outside the scope of the component's configuration and are recommended to be managed in the application's own Kconfig, following the approach used in the ``coze_ws_app`` example.

Application Examples
---------------------

- `coze_ws_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/coze_ws_app>`__ demonstrates the complete Chat integration flow under two interaction modes—button press and wake word—and shows how to configure both the PAT and OAuth/JWT authentication methods

FAQ
----

**Q1: Can Chat, TTS, and ASR run at the same time?**

Yes. Each of the three uses its own independent WebSocket client instance and does not interfere with the others; when running multiple of them in parallel, pay attention to memory usage and task priority.

**Q2: How should I choose between PAT and OAuth/JWT?**

PAT is simple to configure and is suitable for development, debugging, and personal projects—it is used directly as the ``access_token``\ . OAuth/JWT requires an additional private-key signing and token-exchange flow (\ ``esp_coze_jwt_create`` + ``esp_coze_http_post``\ ), making it suitable for authenticating fleets of devices in production environments.

**Q3: Can time-consuming processing be done inside the callbacks?**

No. All callbacks run in the context of the WebSocket client task and should return as quickly as possible; time-consuming processing should copy or hand off the data to a queue for another task to complete.

API Reference
--------------

See the component headers for the public API:

- `esp_coze_chat.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_chat.h>`__
- `esp_coze_tts.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_tts.h>`__
- `esp_coze_asr.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_asr.h>`__
- `esp_coze_common.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_common.h>`__
- `esp_coze_jwt.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_jwt.h>`__
- `esp_coze_http.h <https://github.com/espressif/esp-adf/blob/master/components/esp_coze/include/esp_coze_http.h>`__
