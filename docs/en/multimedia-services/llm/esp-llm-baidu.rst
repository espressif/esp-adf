ESP LLM Baidu
==============

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`esp_llm_baidu <https://components.espressif.com/components/espressif/esp_llm_baidu>`__ is the Baidu adapter for :doc:`esp-llm-service`. It uses Baidu RTC (BRTC). Synchronize the clock before creating the session. Official documentation: `Baidu RTC <https://cloud.baidu.com/doc/RTC/index.html>`__.

Features
--------

- Voice chat: ``ESP_LLM_BAIDU_SCENE_VOICE_CHAT``. Audio only. No camera and no display. During the conversation the agent can send a music HTTP URL. The application plays it on a separate player stream and mixes it with the voice
- Image understanding: ``ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING``. The camera image is sent to the cloud, either as one snapshot when the cloud asks or as a continuous stream
- Image generation: ``ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION``. The application receives and displays images generated in the cloud
- Multimodal: ``ESP_LLM_BAIDU_SCENE_MULTI_MODAL``. Sends the camera image and displays cloud-generated images at the same time
- Room intercom: ``ESP_LLM_BAIDU_MODE_INTERCOM``, separate from the agent scenes

How It Works
------------

``esp_llm_baidu_create()`` reads ``esp_llm_baidu_cfg_t``. Start from ``ESP_LLM_BAIDU_CFG_AGENT_DEFAULT()``, then set the account and the scene. The structure is in ``esp_llm_baidu.h``. Platform string defaults are in ``esp_llm_baidu_config.h``, including ``workflow``, ``llm``, language, and the engine JSON. Audio and video codec defaults are in ``esp_llm_baidu_defaults.h`` and follow the Kconfig choice.

Agent mode fills ``cfg.agent.account``. Set ``app_id``, ``user_id``, ``server_url``, and ``instance_id``. ``workflow`` defaults to ``VoiceChat``, ``llm`` defaults to ``ernie-4.0``, and ``lang`` defaults to ``zh``. ``license_key`` may be an empty string. Write the scene to ``cfg.agent.scene``. Image understanding also enables the camera entry in ``cfg.service.streams[]``. Image generation enables the display entry. Multimodal enables both. The camera image defaults to one snapshot. For a continuous send, set ``cfg.vision.vision_mode`` to ``ESP_LLM_BAIDU_VISION_MODE_STREAM``.

Select audio in menuconfig under Agent audio profile. The default is OPUS at 16 kHz. With PCM at 16 kHz, the application sends and plays PCM, and the Baidu library uses G.722 on the wire. Select JPEG or H.264 under Video profile. The default is JPEG. ``ESP_LLM_BAIDU_VOICE_INTERRUPT_ENABLE`` controls voice interrupt and is enabled by default. When ``cfg.agent.engine.config_json_raw`` is set, that JSON replaces the engine configuration the adapter generates. Room intercom uses ``ESP_LLM_BAIDU_CFG_INTERCOM_DEFAULT()`` and fills ``app_id``, ``server_url``, ``room_name``, and the certificate path in ``cfg.intercom``.

.. code:: c

    esp_llm_baidu_cfg_t cfg = ESP_LLM_BAIDU_CFG_AGENT_DEFAULT();
    cfg.agent.scene = ESP_LLM_BAIDU_SCENE_VOICE_CHAT_CFG_DEFAULT();
    cfg.agent.account.app_id = CONFIG_BAIDU_APP_ID;
    cfg.agent.account.user_id = CONFIG_BAIDU_USER_ID;
    cfg.agent.account.server_url = CONFIG_BAIDU_SERVER_URL;
    cfg.agent.account.instance_id = CONFIG_BAIDU_INSTANCE_ID;

    esp_llm_service_t *llm = NULL;
    ESP_ERROR_CHECK(esp_llm_baidu_create(&cfg, &llm));

Image generation, image understanding, and multimodal use ``ESP_LLM_BAIDU_SCENE_IMAGE_GENERATION_CFG_DEFAULT()``, ``ESP_LLM_BAIDU_SCENE_IMAGE_UNDERSTANDING_CFG_DEFAULT()``, and ``ESP_LLM_BAIDU_SCENE_MULTI_MODAL_CFG_DEFAULT()``. Link and start order are in :doc:`esp-llm-service`.

Example
-------

`esp_llm_baidu_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_baidu_agent_app>`__ selects the scene in menuconfig. Voice chat includes music URL playback.
