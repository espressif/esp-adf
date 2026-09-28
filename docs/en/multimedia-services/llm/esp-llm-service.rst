ESP LLM Service
===============

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`esp_llm_service <https://components.espressif.com/components/espressif/esp_llm_service>`__ is the unified AI session service. It uses :doc:`/multimedia-services/service-infra/esp-service` as its base class and links media with :doc:`/multimedia-services/media/esp-media-service`. The application creates capture and playback, then links those streams to the LLM (Large Language Model). Uplink frames go to a vendor adapter. Downlink frames go to the application-owned player. The LLM does not create or destroy capture and playback.

For how an application creates an adapter and links capture and playback, see `esp_llm_baidu_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_baidu_agent_app>`__, `esp_llm_coze_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_coze_agent_app>`__, `esp_llm_tongyi_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_tongyi_agent_app>`__, and `esp_llm_volc_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_volc_agent_app>`__.

Features
--------

- Four media streams: ``ESP_LLM_MEDIA_STREAM_AUDIO_UP`` carries the microphone, ``ESP_LLM_MEDIA_STREAM_VIDEO_UP`` carries the camera, ``ESP_LLM_MEDIA_STREAM_AUDIO_DN`` goes to the speaker, and ``ESP_LLM_MEDIA_STREAM_VIDEO_DN`` goes to the display. Enable the required streams in ``streams[]`` before create, and leave the others disabled
- Link the media first, then call ``esp_service_start()`` on the player, the LLM, and capture. Stop in the reverse order. Call ``esp_service_stop()`` before ``esp_llm_service_destroy()``
- ``esp_llm_service_send_text()`` sends text after the session has started
- ``esp_llm_service_send_frame()`` sends one frame. Do not use it for frames that a linked capture is already sending
- ``esp_llm_service_send_custom_data()`` sends a backend-native command
- ``esp_llm_service_cancel_response()`` cancels the current reply and keeps the session
- ``esp_llm_service_enable_uplink_track()`` disables audio or video uplink. Frames of that type are dropped, and capture keeps running
- Optional audio AI sets ``audio_ai_cfg.capture`` to the application's capture and enables wake word with ``feature_mask``. Call ``esp_llm_service_trigger_wakeup()`` on press, ``keep_awake()`` while held, and ``trigger_sleep()`` on release

Adapters
--------

.. list-table::
   :header-rows: 1
   :widths: 16 16 68

   * - Adapter
     - Protocol
     - Capability
   * - :doc:`esp_llm_baidu <esp-llm-baidu>`
     - Baidu RTC
     - Voice chat, with music from the agent during the conversation. Image understanding sends the camera image to the cloud. Image generation displays cloud images. Multimodal does both. Room intercom is separate from the agent scenes
   * - :doc:`esp_llm_coze <esp-llm-coze>`
     - Coze WebSocket
     - Audio and text conversation. Talk directly, or send uplink after a wake word. A wake can barge in on the current reply. No camera image is sent, and no cloud image is displayed
   * - :doc:`esp_llm_tongyi <esp-llm-tongyi>`
     - Tongyi WebSocket
     - Duplex voice conversation. In vision mode the application previews the camera locally and sends one JPEG with ``esp_llm_service_send_frame()``. A prompt in ``frame.user_data`` requests a reply. There is no continuous video and no cloud image display
   * - :doc:`esp_llm_volc <esp-llm-volc>`
     - Volcengine RTC
     - Voice chat. ``esp_llm_volc_send_text_to_tts()`` speaks text and does not send that text through the chat model. Publishing the local camera and subscribing to the remote picture are off by default

How It Works
------------

Media Streams and Linking
^^^^^^^^^^^^^^^^^^^^^^^^^

Create the session with the adapter create function. The application creates, starts, and destroys the microphone, speaker, and camera. The service does not create them. Link first, then start the player, the LLM, and capture. Stop in the reverse order.

.. code:: c

    esp_media_service_link(capture, APP_AUDIO_CAPTURE_STREAM,
                           llm, ESP_LLM_MEDIA_STREAM_AUDIO_UP);
    esp_media_service_link(llm, ESP_LLM_MEDIA_STREAM_AUDIO_DN,
                           player, APP_AUDIO_PLAYER_STREAM);

    esp_service_start(player);
    esp_service_start(llm);
    esp_service_start(capture);

``APP_AUDIO_CAPTURE_STREAM`` and ``APP_AUDIO_PLAYER_STREAM`` are the application's own stream IDs. After the link, the service reads uplink frames from capture and passes them to the adapter, then writes downlink frames to the player. The application does not send that audio again. Do not enable unused streams in ``streams[]``. ``esp_llm_service_send_frame()`` is only for a single frame that has no capture link. Do not call it for frames the linked capture is already sending.

Control Plane
^^^^^^^^^^^^^

``send_text()``, ``send_frame()``, ``send_custom_data()``, and ``cancel_response()`` run after the service has started, and they must not overlap. ``cancel_response()`` stops only the current reply. The session stays up. ``enable_uplink_track()`` may run from an event callback: gated frames of that type are dropped, the call still returns success, and local capture and preview keep running.

Audio AI
^^^^^^^^

Set ``audio_ai_cfg.capture`` to the audio capture the application already created, and enable wake word and related features with ``feature_mask``. Uplink can stay disabled until a wake, and a wake can cancel the current reply. The button sequence is ``trigger_wakeup()`` on press, ``keep_awake()`` while held, and ``trigger_sleep()`` on release.

Examples
--------

- `esp_llm_baidu_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_baidu_agent_app>`__ selects the scene in menuconfig and links capture and playback to the Baidu adapter. Voice chat includes music URL playback
- `esp_llm_coze_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_coze_agent_app>`__ connects to Coze with a personal access token and demonstrates direct talk, wake word, and barge-in
- `esp_llm_tongyi_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_tongyi_agent_app>`__ selects voice only or vision in menuconfig. In vision mode, ``snap`` sends one image and ``snap <prompt>`` asks about that image
- `esp_llm_volc_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_volc_agent_app>`__ enables voice chat only. Video stays off
