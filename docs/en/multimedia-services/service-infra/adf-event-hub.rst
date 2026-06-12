ADF Event Hub
=============

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`ADF Event Hub <https://components.espressif.com/components/espressif/adf_event_hub>`__ is a domain-based publish-subscribe mechanism for ADF and GMF components. Each handle corresponds to one event source domain, such as Wi-Fi or OTA. Publishers deliver events into that domain. Subscribers filter by domain and event identifier, then receive events through a queue or a callback. Each service instance in :doc:`esp-service` automatically creates and binds an event hub to publish domain events for itself and derived services.

Feature List
------------

- Domain-based publish-subscribe: one hub handle represents one event source domain; subscribers filter by ``(event_domain, event_id)``\ , and \ ``ADF_EVENT_ANY_ID`` acts as a wildcard that matches all events
- Two delivery modes: queue mode (non-blocking ``xQueueSend``\ ) and callback mode (executed synchronously in the publisher's task), selectable independently per subscriber
- Startup-order independence: subscribers can complete their subscription before a publisher in the target domain calls ``create()``\ ; the target domain is created automatically
- Reference-counted hub lifecycle: \ ``adf_event_hub_create()`` / ``adf_event_hub_destroy()`` are used in pairs, and a domain is only actually removed once its reference count reaches zero
- Reference-counted envelope delivery: the optional ``release_cb`` is invoked exactly once per successful publish, regardless of whether the delivery path hit any queue subscribers
- Thread-safe: all public APIs are protected by an internal mutex and can be called from any task (ISR context is not supported)
- Observable: \ ``adf_event_hub_get_stats()`` returns the subscriber count and envelope pool usage for each domain, and \ ``adf_event_hub_dump()`` outputs a full status log

Technical Deep Dive
--------------------

Publish-Subscribe Model
^^^^^^^^^^^^^^^^^^^^^^^^

Each domain is identified by a case-sensitive string (such as ``"wifi"``\ ). Publishers and subscribers each hold their own hub handle: a publisher's handle identifies the domain it belongs to, while a subscriber's handle merely identifies the caller itself; the domain to listen on is specified through the ``event_domain`` field of :cpp:func:`adf_event_hub_subscribe`, and defaults to the domain of the subscriber's own hub when set to ``NULL``\ .

.. code:: c

    adf_event_hub_t wifi_hub = NULL;
    adf_event_hub_t app_hub = NULL;
    adf_event_hub_create("wifi", &wifi_hub);
    adf_event_hub_create("app", &app_hub);

    adf_event_subscribe_info_t info = ADF_EVENT_SUBSCRIBE_INFO_DEFAULT();
    info.event_domain = "wifi";
    info.event_id     = 1;
    info.handler      = on_wifi_event;
    adf_event_hub_subscribe(app_hub, &info);

    adf_event_t ev = { .domain = "wifi", .event_id = 1 };
    adf_event_hub_publish(wifi_hub, &ev, NULL, NULL);

Delivery Modes and Reference Release
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Queue mode is used when ``target_queue`` in :cpp:type:`adf_event_subscribe_info_t` is non-null; otherwise the ``handler`` callback mode is used. If both are set, queue mode takes precedence. In queue mode, the publish call only performs a non-blocking ``xQueueSend()``\ , so a full queue on one subscriber only affects that single delivery to it; in callback mode, ``handler`` runs synchronously in the publisher's task and must not block noticeably.

.. only:: html

   .. mermaid::

      sequenceDiagram
          participant Pub as Publisher
          participant Hub as event hub
          participant Sub as Queue Subscriber

          Pub->>Hub: adf_event_hub_publish()
          Hub-)Sub: xQueueSend(delivery)
          Sub->>Hub: adf_event_hub_delivery_done()
          Hub--)Pub: release_cb(payload)

Event payloads are delivered by shallow copy, so heap-allocated payloads must be reclaimed via ``release_cb``\ : when no queue-mode subscriber is hit, \ ``release_cb`` is invoked before :cpp:func:`adf_event_hub_publish` returns; when a queue-mode subscriber is hit, invocation is deferred until after the last call to :cpp:func:`adf_event_hub_delivery_done`, and it is triggered exactly once.

.. code:: c

    static void release_payload(const void *payload, void *ctx)
    {
        free((void *)payload);  /* Invoked exactly once, after the last delivery_done call */
    }

    char *msg = strdup("hello");
    adf_event_t ev = { .domain = "wifi", .event_id = 7, .payload = msg, .payload_len = strlen(msg) + 1 };
    adf_event_hub_publish(wifi_hub, &ev, release_payload, NULL);

.. note::

    After a queue-mode subscriber receives an :cpp:type:`adf_event_delivery_t`, it must call :cpp:func:`adf_event_hub_delivery_done` exactly once for each delivery, otherwise the envelope pool slot will be held and never released.

Lifecycle and Reference Counting
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

:cpp:func:`adf_event_hub_create` manages domains by reference count: calling ``create()`` multiple times for the same domain only increments the reference count, and the domain is actually removed and its subscribers cleaned up only once the count reaches zero (that is, after :cpp:func:`adf_event_hub_destroy` has been called the same number of times). This allows a shared hub to be held jointly by multiple owners, so that any single owner destroying its own reference does not affect the others. ``esp_service_init()`` in :doc:`esp-service` already encapsulates this process, so application code typically does not need to call the event hub's create/destroy interfaces directly.

Application Examples
--------------------

- ``components/adf_event_hub/examples/`` demonstrates several services interacting through the ADF Event Hub; the same code can be built and run both on a PC host (FreeRTOS POSIX simulator) and on ESP-IDF targets.

FAQ
------

**Q1: Can queue mode and callback mode be used on the same subscription at the same time?**

No. When ``target_queue`` and ``handler`` are both set, queue mode takes precedence and ``handler`` is ignored; if both delivery methods are needed, register two separate subscriptions.

API Reference
-------------

.. include-build-file:: inc/adf_event_hub.inc
