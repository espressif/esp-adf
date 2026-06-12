ESP Button Service
==================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`Button Service <https://components.espressif.com/components/espressif/esp_button_service>`__ is a :doc:`/multimedia-services/service-infra/esp-service` wrapper for buttons. It discovers button devices registered in Board Manager, registers callbacks for a selected set of events, and forwards button actions as typed service events. The application does not need to maintain a list of device names.

Feature List
------------

- Zero-configuration auto-discovery: iterates over all ``ESP_BOARD_DEVICE_TYPE_BUTTON`` devices registered in ``esp_board_manager``, supporting both single-GPIO buttons and ADC multi-button groups
- Fine-grained event registration: uses the ``event_mask`` bitmask to precisely select which ``iot_button`` callbacks to install, reducing unnecessary overhead
- Lifecycle-aware forwarding: events are only published while the service is in the running state; forwarding is automatically suppressed when the service is stopped, paused, or enters low power mode, and automatically resumes when the service resumes
- Typed payload: each event carries an ``esp_button_service_payload_t`` containing the board-level device label, facilitating dispatch handling in multi-button scenarios
- Inherits the standard lifecycle and event subscription interfaces provided by :doc:`/multimedia-services/service-infra/esp-service`, requiring no additional learning of new calling conventions

Technical Deep Dive
--------------------

Auto-Discovery and Event Forwarding
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:cpp:func:`esp_button_service_create` scans the device registry of ``esp_board_manager``, initializes each button device, and registers the selected ``iot_button`` callbacks according to ``cfg->event_mask``; after calling :cpp:func:`esp_service_start`, every button action is published through the service's event bus, and all subscribers can respond to it.

.. only:: html

   .. mermaid::

      flowchart TD
          BM[esp_board_manager button devices] --> BS[esp_button_service]
          BS -->|iot_button callback| BS
          BS -->|esp_service_publish_event| App[Application subscribers]

.. code:: c

    esp_board_manager_init();

    esp_button_service_cfg_t cfg = {
        .name       = "esp_button_service",
        .event_mask = ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT,
    };
    esp_button_service_t *svc = NULL;
    esp_button_service_create(&cfg, &svc);

    esp_service_t *base = (esp_service_t *)svc;
    adf_event_subscribe_info_t sub = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    sub.event_id = ADF_EVENT_ANY_ID;
    sub.handler  = on_button_event;
    esp_service_event_subscribe(base, &sub);

    esp_service_start(base);

Event Mask and Event IDs
^^^^^^^^^^^^^^^^^^^^^^^^

The service defines 10 event IDs (``ESP_BUTTON_SERVICE_EVT_PRESS_DOWN`` and others, with values starting from ``1``), each corresponding to a bitmask constant ``ESP_BUTTON_SERVICE_EVT_MASK_xxx``; ``event_mask`` only affects callback registration and does not affect the event IDs themselves. ``ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT`` enables six commonly used button interactions by default: ``PRESS_DOWN``, ``PRESS_UP``, ``SINGLE_CLICK``, ``DOUBLE_CLICK``, ``LONG_PRESS_START``, and ``LONG_PRESS_UP``.

Event Payload
^^^^^^^^^^^^^

Each ``ESP_BUTTON_SERVICE_EVT_*`` event carries a heap-allocated :cpp:type:`esp_button_service_payload_t`, which is automatically freed after all subscribers have received it; the application layer does not need to free it manually:

.. code:: c

    typedef struct {
        const char *label;  /* Board-level device name, valid for the lifetime of the process */
    } esp_button_service_payload_t;

For a single-GPIO button, ``label`` is the board-level device name of that button; for an ADC multi-button group, ``label`` is taken from ``button_labels[]`` in the board-level device configuration (``dev_button_config_t``) rather than the device name itself, so that different voltage-level buttons on the same ADC channel can be distinguished.

Application Examples
---------------------

- ``components/esp_button_service/examples/button_svc_example`` demonstrates a mixed setup of a single GPIO button and an ADC multi-button group, along with the complete event subscription flow.

FAQ
------

**Q1: What happens if the event mask ``event_mask`` is set to 0?**

This is equivalent to using ``ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT``; :cpp:type:`esp_button_service_cfg_t` states that when ``event_mask`` is ``0``, it falls back to the default mask.

API Reference
--------------

.. include-build-file:: inc/esp_button_service.inc
