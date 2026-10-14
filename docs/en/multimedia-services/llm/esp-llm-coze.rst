ESP LLM Coze
=============

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`esp_llm_coze <https://components.espressif.com/components/espressif/esp_llm_coze>`__ is the Coze adapter for :doc:`esp-llm-service`. It uses the Coze WebSocket API through :doc:`/multimedia-services/ai/esp-coze`. Official documentation: `Coze API overview <https://www.coze.cn/docs/developer_guides/coze_api_overview>`__.

Features
--------

- Audio and text conversation. Video is rejected
- Direct talk, or uplink only after a wake word
- A wake can barge in on the current reply
- Audio formats: OPUS or PCM at 16 kHz, and G.711 A-law / mu-law at 8 kHz

How It Works
------------

``esp_llm_coze_create()`` reads ``esp_llm_coze_cfg_t``. Start from ``ESP_LLM_COZE_CFG_DEFAULT()``, then set ``bot_id`` and ``access_token``. The structure is in ``esp_llm_coze.h``. Audio codec defaults are in ``esp_llm_coze_defaults.h`` and follow the Kconfig choice. The ``access_token`` comes from a personal access token or from an OAuth exchange, and both results use the same field. See the `Coze authentication documentation <https://www.coze.cn/open/docs/developer_guides/authentication>`__.

An empty ``ws_base_url`` uses ``wss://ws.coze.cn/v1/chat``. An empty ``conv_create_base_url`` uses ``https://api.coze.cn/v1/conversation/create``. An empty ``user_id`` uses ``esp_llm_user``. An empty ``voice_id`` uses ``7426720361733144585``. ``mode`` defaults to ``ESP_LLM_COZE_MODE_SPEECH_INTERRUPT``. ``ESP_LLM_COZE_MODE_NORMAL`` is the button-triggered conversation. Select ``vad_mode`` in menuconfig under Server VAD silence mode. The default is fast silence detection.

Select audio under Audio profile. The default is OPUS at 16 kHz. The OPUS downlink bitrate defaults to 16000 bit/s, and the downlink frame size defaults to 60 ms. ``downlink_speech_rate`` defaults to 20, in the range -50 to 100. ``websocket_connect_timeout`` defaults to 30000 ms.

Personal Access Token
^^^^^^^^^^^^^^^^^^^^^

A PAT (Personal Access Token) from the Coze open platform is written directly to ``access_token``.

OAuth
^^^^^

The application signs a JWT with the OAuth private key, then exchanges it for a short-lived access token. The component does not perform the exchange.

1. ``esp_coze_jwt_create()`` signs with RS256. ``kid`` is the public-key ID, ``payload`` is the JWT claims JSON, and ``private_key`` is a PEM-encoded RSA private key of at least 2048 bits. It returns a newly allocated JWT that the caller must ``free()``. Fill ``iss``, ``aud``, ``iat``, ``exp``, and ``jti`` as described in the Coze OAuth documentation. ``iat`` and ``exp`` use the device clock, so synchronize time first.
2. ``esp_coze_http_post()`` sends the JWT in ``Authorization: Bearer`` to ``https://api.coze.cn/api/permission/oauth2/token``. Read ``access_token`` from the response JSON and assign it to ``cfg.access_token``. The caller must ``free()`` the response body.

.. code:: c

    esp_llm_coze_cfg_t cfg = ESP_LLM_COZE_CFG_DEFAULT();
    cfg.bot_id = CONFIG_COZE_BOT_ID;
    cfg.access_token = CONFIG_COZE_ACCESS_TOKEN;  /* PAT, or the token from OAuth */

    esp_llm_service_t *llm = NULL;
    ESP_ERROR_CHECK(esp_llm_coze_create(&cfg, &llm));

With wake word enabled, uplink stays disabled until the wake word. Link and start order are in :doc:`esp-llm-service`.

Example
-------

`esp_llm_coze_agent_app <https://github.com/espressif/esp-adf/tree/master/adf_examples/ai_agent/esp_llm_coze_agent_app>`__ uses a personal access token.
