ESP OTA Service
===============

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`OTA Service <https://components.espressif.com/components/espressif/esp_ota_service>`__ is a subclass of :doc:`/multimedia-services/service-infra/esp-service` that implements a modular, extensible over-the-air firmware upgrade pipeline. It splits the upgrade process into four independent abstraction layers: data source, write target, version checker, and integrity verifier. Transport methods such as HTTP/HTTPS, filesystem, and BLE can be combined with write targets such as the application partition, data partition, and bootloader, without modifying the service implementation.

Feature List
------------

- Multiple data sources for download: HTTP/HTTPS, the local filesystem, and a BLE GATT peripheral that follows the official ESP BLE OTA APP protocol (optional); custom transports such as UART or SPI can also be integrated via the ``esp_ota_service_source_t`` interface
- Multiple write targets: the application partition, raw data partition, and bootloader (optional); custom write targets can also be integrated via the ``esp_ota_service_target_t`` interface
- Version check before download: three built-in checkers — application image header, semantic version header, and JSON manifest — automatically skip items that are already up to date
- Streaming integrity verification: built-in SHA-256 and MD5 verifiers; signature or CRC schemes can also be integrated via the ``esp_ota_service_verifier_t`` interface
- NVS-based resume support: the download offset is saved every few KB, allowing resumption from the breakpoint after a failure or reboot
- Rollback support: ``esp_ota_service_confirm_update()`` / ``esp_ota_service_rollback()`` and pending-verify state detection (requires ``CONFIG_OTA_ENABLE_ROLLBACK``)
- Pause/resume during download: a running session can directly call :cpp:func:`esp_service_pause` / :cpp:func:`esp_service_resume`
- The event bus covers 6 event types — session start, version check, item start/progress/end, and session end — without imposing a reboot policy

Technical Deep Dive
--------------------

Pipeline Abstraction
^^^^^^^^^^^^^^^^^^^^

Each :cpp:type:`esp_ota_upgrade_item_t` describes one partition upgrade, and consists of two mandatory interfaces, ``source`` and ``target``, plus two optional interfaces, ``checker`` and ``verifier``. All four interface types are function-pointer structures that can be freely combined once implemented. The service takes ownership of every component in the list via :cpp:func:`esp_ota_service_set_upgrade_list`, so the caller should not call their ``destroy()`` functions directly afterward.

.. only:: html

   .. mermaid::

      flowchart TD
          Start([Item start]) --> Chk{checker?}
          Chk -- No --> Open[source.open]
          Chk -- Yes --> Check[checker.check]
          Check -- Not newer --> Skip([Skip])
          Check -- Newer version --> Open
          Open --> Vb{verifier?}
          Vb -- No --> TOpen[target.open]
          Vb -- Yes --> VBegin[verifier.verify_begin]
          VBegin -- Rejected --> Skip
          VBegin -- Accepted --> TOpen
          TOpen --> Loop{source.read loop}
          Loop --> Update[verifier.verify_update]
          Update -- Verification failed --> Fail([Abort])
          Update -- Passed --> Write[target.write] --> Loop
          Loop -- EOF --> Vf{verifier?}
          Vf -- No --> Commit[target.commit]
          Vf -- Yes --> VFinish[verifier.verify_finish]
          VFinish -- Verification failed --> Fail
          VFinish -- Passed --> Commit

Calling :cpp:func:`esp_service_start` returns immediately; the worker task runs in the background and reports progress via events, without blocking the caller's task.

.. code:: c

    #include "esp_ota_service_default.h"  /* Aggregates the built-in source/target/checker/verifier headers */

    esp_ota_service_t *svc = NULL;
    esp_ota_service_create(&(esp_ota_service_cfg_t)ESP_OTA_SERVICE_CFG_DEFAULT(), &svc);

    esp_ota_upgrade_item_t item = {
        .uri      = "http://example.com/firmware.bin",
        .source   = http_source,
        .target   = app_target,
        .checker  = manifest_checker,
        .verifier = sha256_verifier,
    };
    esp_ota_service_set_upgrade_list(svc, &item, 1);

    esp_service_event_subscribe((esp_service_t *)svc, &sub);
    esp_service_start((esp_service_t *)svc);

.. important::

    After calling :cpp:func:`esp_ota_service_set_upgrade_list`, ownership of every ``source``, ``target``, ``checker``, and ``verifier`` in the list is transferred to the service, even if the call fails; do not call their ``destroy()`` functions directly afterward — :cpp:func:`esp_ota_service_destroy` will release them uniformly.

Choosing the Right Combination
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Component selection for common scenarios: for HTTP OTA combined with manifest version checking and SHA-256 verification, use ``esp_ota_service_source_http_create()`` + ``esp_ota_service_checker_manifest_create()`` + ``esp_ota_service_verifier_sha256_create()`` + ``esp_ota_service_target_app_create()``; the ``verifier`` can be omitted when only version checking is needed without streaming verification; for SD card or USB flash drive deployment, use ``esp_ota_service_source_fs_create()`` together with the same application target; for data partition upgrades, use ``esp_ota_service_checker_data_version_create()`` and ``esp_ota_service_target_data_create()`` instead. For batch upgrades across multiple partitions, a single session can configure separate items for the application partition and data partition, sharing the same event stream.

Events and Progress
^^^^^^^^^^^^^^^^^^^

After subscribing via :cpp:func:`esp_service_event_subscribe`, the ``payload`` field of the ``adf_event_t`` received in each callback points to an :cpp:type:`esp_ota_service_event_t`. The ``id`` field must be read first, followed by accessing the corresponding union branch: ``ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK`` reports the version check result, ``ESP_OTA_SERVICE_EVT_ITEM_PROGRESS`` updates the downloaded byte count approximately once per second, ``ESP_OTA_SERVICE_EVT_ITEM_END`` carries the success/skip/failure result of a single partition, and ``ESP_OTA_SERVICE_EVT_SESSION_END`` carries the summary statistics for the entire session. The caller can also directly poll the download progress percentage of the current item using :cpp:func:`esp_ota_service_get_progress`.

Resume and Rollback
^^^^^^^^^^^^^^^^^^^

When ``CONFIG_OTA_ENABLE_RESUME`` (enabled by default) is turned on, the service saves the download offset to NVS at a fixed byte interval. For data sources that do not implement ``seek()`` (such as BLE) or targets that do not implement ``set_write_offset()`` (the data partition or bootloader), the ``resumable`` field of the corresponding item must be set to ``false``; otherwise :cpp:func:`esp_ota_service_set_upgrade_list` returns ``ESP_ERR_NOT_SUPPORTED``. When ``CONFIG_OTA_ENABLE_ROLLBACK`` is enabled, after the new firmware boots it should first check whether it is in the pending-verify state using :cpp:func:`esp_ota_service_is_pending_verify`, and call :cpp:func:`esp_ota_service_confirm_update` to cancel the rollback timer once self-testing passes; otherwise every reboot will trigger an automatic rollback.

Application Examples
---------------------

- ``examples/ota_http/`` demonstrates HTTP + manifest version checking + SHA-256 verification + resume support
- ``examples/ota_fs/`` demonstrates offline deployment from an SD card/USB flash drive and batch upgrades across multiple partitions
- ``examples/ota_ble/`` demonstrates BLE GATT firmware push in scenarios without Wi-Fi

FAQ
---

**Q1: resumable is set to true, but ESP_ERR_NOT_SUPPORTED is still returned. Why?**

The data source does not implement ``seek()``, or the write target does not implement ``set_write_offset()``. The data partition and bootloader targets erase the entire region during ``open()``, so they inherently do not support resume; currently, only the application partition target, together with data sources that support random-access ``seek``, can resume.

**Q2: How can bricking be avoided after a failed upgrade?**

Application partition upgrades rely on the A/B dual-image mechanism, allowing a fallback to the old partition on failure. However, bootloader OTA (``CONFIG_OTA_ENABLE_BOOTLOADER_OTA``) writes through a staging partition and provides no atomicity guarantee; a power loss during the copy process carries a risk of bricking the device, so additional evaluation and a fallback plan are required before mass production.

API Reference
-------------

.. include-build-file:: inc/esp_ota_service.inc

.. include-build-file:: inc/esp_ota_service_default.inc
