ESP Media Service
==================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`ESP Media Service <https://components.espressif.com/components/espressif/esp_media_service>`__ defines a common media interface for audio and video services in ESP-ADF, based on :doc:`/multimedia-services/service-infra/esp-service`. An application creates a service, configures the media stream, and then links a source stream to a sink stream. Once linked and started, media frames flow through the read and write interfaces, so the application does not need to forward frames by hand. Audio and video capture and playback are documented in :doc:`/basic-components/esp-gmf/index`.

Feature List
------------

- A unified audio/video service model with three roles: source, sink, and source-sink (``ESP_MEDIA_ROLE_SRC`` / ``SINK`` / ``SRC_SINK``)
- Multi-media endpoints based on a stream ID (``esp_media_stream_id_t``); a single service can expose multiple streams at the same time
- Request negotiation at link time: a sink can express requirements to the source through ``esp_media_service_request_t`` (for example, whether a global cache is needed)
- The provider read interface (``esp_media_provider_t``) is decoupled from the track manager write interface
- A built-in default in-memory track manager (``esp_media_track_mngr_t``) that supports either per-track independent caching or global arrival-order caching
- Service lifecycle is uniformly managed through :doc:`/multimedia-services/service-infra/esp-service`, with media-related operations layered on top of the base class as an independent vtable (``esp_media_service_ops_t``)

Technical Deep Dive
--------------------

Service Roles and Linking
^^^^^^^^^^^^^^^^^^^^^^^^^^

ESP Media Service declares whether it is a source, a sink, or both, through ``get_role`` in :cpp:type:`esp_media_service_ops_t`; :cpp:func:`esp_media_service_link` verifies that the roles of the selected source and sink are compatible when linking, and then passes the source's provider to the sink.

.. only:: html

   .. mermaid::

      flowchart TD
          Create["Create service"] --> Config["Configure service"]
          Config --> Link["Link source/sink streams"]
          Link --> Start["Start service"]
          Start --> Flow["Media frames flow"]
          Flow --> Stop["Stop service"]
          Stop --> Unlink["Unlink and destroy"]

.. code:: c

    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    esp_media_service_link(src_service, stream, sink_service, stream);

    esp_service_start(sink_service);
    esp_service_start(src_service);

Once the link is established, media data flows from the provider exported by the source service to the sink; removing the link requires calling :cpp:func:`esp_media_service_unlink`, which clears the provider currently set on the sink stream.

Provider and Track Manager
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The delivery of media data relies on two mutually decoupled interfaces: at link time, the sink obtains a read-only ``esp_media_provider_t`` from the source service, and acquires and releases frames through :cpp:func:`esp_media_provider_acquire_frame` / :cpp:func:`esp_media_provider_release_frame`; the source service, in turn, holds an ``esp_media_track_mngr_t``, produces frames through its write API, and exposes the provider handle it exports to downstream consumers.

.. code:: c

    /* Source service side: create the track manager, register a track, and export the provider */
    esp_media_track_mngr_cfg_t cfg = { .max_track_num = 2 };
    esp_media_track_mngr_create(&cfg, &svc->mngr);
    esp_media_track_mngr_add_track(svc->mngr, &audio_track);
    esp_media_track_mngr_get_provider(svc->mngr, &svc->provider);

.. code:: c

    /* Sink side: acquire and release a frame */
    esp_media_frame_t frame = {0};
    if (esp_media_provider_acquire_frame(&sink->provider, &frame, timeout_ms) == ESP_OK) {
        process_frame(&frame);
        esp_media_provider_release_frame(&sink->provider, &frame);
    }

.. warning::

    A frame acquired through :cpp:func:`esp_media_provider_acquire_frame` must be released by calling :cpp:func:`esp_media_provider_release_frame`; after it is released, ``frame.data`` must no longer be accessed.

Track Manager Cache Modes
^^^^^^^^^^^^^^^^^^^^^^^^^^

The default ``esp_media_track_mngr_t`` supports two payload ownership modes: ``ESP_MEDIA_TRACK_CACHE_INTERNAL``, in which the manager copies and holds the frame data itself, and ``ESP_MEDIA_TRACK_CACHE_USER``, which caches only the frame metadata while the payload remains owned by the user and is returned through the ``frame_release`` callback after it has been consumed. A global cache is also supported, letting multiple tracks share a single queue ordered by arrival time, which is suitable for scenarios such as RTMP where audio and video are interleaved; enabling the global cache must be configured before tracks are added.

Stop and Abort Ordering
^^^^^^^^^^^^^^^^^^^^^^^^

The media interface is designed to tolerate the stop sequence: when the source service stops, it should call ``esp_media_track_write_abort()``, which notifies downstream consumers through the ``ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT`` event; when the sink stops, it should first set a local stop flag, then call :cpp:func:`esp_media_provider_abort` to wake up any blocked read, wait for the task to exit and release any frames it has acquired, and only then unlink. If a track manager is shared by multiple services through linking, it must be unlinked before reset or destroy is performed on it; as long as any task still holds an acquired frame or is blocked on the queue, reset or destroy must not be performed on the track manager.

Application Examples
---------------------

Complete source/sink examples are in the examples directory of the `esp_media_service component repository <https://components.espressif.com/components/espressif/esp_media_service>`__. See :doc:`/multimedia-services/service-infra/esp-service` for the service base class.

FAQ
------

**Q1: Can a service implement only one of ``get_provider`` or ``set_provider``?**

Not necessarily. A service with the role ``ESP_MEDIA_ROLE_SRC_SINK`` can implement both at the same time, acting as a source for downstream consumers as well as a sink for upstream producers; :cpp:func:`esp_media_service_link` only checks role compatibility for the selected pair of source/sink services.

API Reference
--------------

.. include-build-file:: inc/esp_media_service.inc

.. include-build-file:: inc/esp_media_provider.inc

.. include-build-file:: inc/esp_media_track_mngr.inc
