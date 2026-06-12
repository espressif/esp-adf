ESP Wi-Fi Service
=================

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`Wi-Fi Service <https://components.espressif.com/components/espressif/esp_wifi_service>`__ unifies credential storage, provisioning interaction, automatic connection, network selection, and quality probing in a device's networking workflow into a single set of :doc:`/multimedia-services/service-infra/esp-service` service interfaces. With it, applications no longer need to separately maintain credential storage, SoftAP/Web provisioning, BluFi provisioning, reconnection, and multi-AP selection logic, making it possible to build stable, field-serviceable networked products more quickly.

Feature List
------------

- Profile management: maintains multiple sets of Wi-Fi credentials, supporting addition, update, enabling, disabling, deletion, and cleanup; provisioning channels and connection selection share the same profile manager
- Pluggable storage: reuses the NVS, file system, dual-partition raw flash, or custom storage adapter layers from :doc:`/multimedia-services/service-infra/esp-config-manager`, and supports encryption callbacks to protect saved credentials
- Multi-channel provisioning: HTTP SoftAP/Web UI, BluFi, and application-defined custom provisioning flows, all of which write to the same shared profile
- Automatic startup strategy: when the service starts, it automatically enters the connection selection flow or starts the configured provisioning flow, depending on whether an enabled profile exists
- Intelligent selection and switching: selects a more suitable AP based on user priority, signal quality, historical connectivity, and a temporary blacklist, and re-evaluates after disconnection or link degradation
- Network quality probing: supports connectivity, latency, and throughput degradation detection to handle the "connected but the service is unusable" scenario
- Optional MCP tool support: once both ``CONFIG_ESP_MCP_ENABLE`` and ``CONFIG_WIFI_SERVICE_MCP_ENABLE`` are enabled, the MCP server from :doc:`/multimedia-services/service-infra/esp-service` can be used to remotely query status, manage profiles, and trigger provisioning/connection

Technical Deep Dive
-------------------

Profile Management and Storage
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each Wi-Fi credential is stored as an :cpp:type:`esp_wifi_service_profile_t`\ (SSID, password, priority, enable flag), which is centrally managed and persisted by ``esp_wifi_service_profile_mgr_t``. The storage layer of the profile manager directly reuses the storage adapter interface from :doc:`/multimedia-services/service-infra/esp-config-manager`; an ``esp_config_storage_t`` handle must be prepared before creation:

.. code:: c

    esp_config_storage_nvs_t nvs_cfg = {
        .nvs_namespace = "wifi_store",
        .key_primary   = "profile_p",
        .key_backup    = "profile_b",
    };
    esp_config_storage_t profile_store = NULL;
    esp_config_storage_init_nvs(&nvs_cfg, &profile_store);

    esp_wifi_service_profile_mgr_cfg_t profile_cfg = {
        .max_profiles = 8,
        .storage      = profile_store,
    };
    esp_wifi_service_profile_mgr_t profile_manager = NULL;
    esp_wifi_service_profile_mgr_init(&profile_cfg, &profile_manager);

The same ``profile_manager`` handle must be passed to both the Wi-Fi Service and the configuration of each provisioning channel, so that credentials written by provisioning are immediately visible to the connection selection logic.

Provisioning Channels
^^^^^^^^^^^^^^^^^^^^^

If at least one enabled profile exists when the service starts, it goes directly into the connection selection flow; otherwise, it starts all the provisioning channels configured in ``prov_list``. HTTP SoftAP/Web UI and BluFi are provided as built-in channels, and applications can also implement a custom provisioning flow that writes to the same profile manager:

- The HTTP channel starts a SoftAP, DNS captive portal, HTTP server, and a default or custom Web UI; credentials are submitted via ``POST /prov/profiles``
- The BluFi channel sends network information via Bluetooth from the phone side; credentials also land in the shared profile manager, and this channel depends on ``CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE`` and the underlying BluFi protocol stack
- Both channels can be enabled in parallel; once provisioning finishes, the selector logic takes over the connection

.. code:: c

    esp_wifi_service_prov_t *http_agent = NULL;
    esp_wifi_service_prov_http_config_t http_cfg = {
        .name             = "http",
        .port             = 80,
        .profile_manager  = profile_manager,
        .default_priority = 10,
    };
    esp_wifi_service_prov_http_create(&http_cfg, &http_agent);

    esp_wifi_service_config_t cfg = {
        .name            = "wifi_service",
        .profile_manager = profile_manager,
        .prov_list       = &http_agent,
        .prov_num        = 1,
    };
    esp_wifi_service_t *svc = NULL;
    esp_wifi_service_create(&cfg, &svc);
    esp_service_start((esp_service_t *)svc);

Selection and Switching
^^^^^^^^^^^^^^^^^^^^^^^

In multi-network environments, the selector automatically decides which AP to connect to: during re-evaluation, it first scans the surrounding APs, keeps only the candidates that match a saved and enabled profile, and then ranks them by user priority, signal quality, the most recent record of successful access, and a temporary blacklist, rather than simply choosing the AP with the strongest signal.

.. only:: html

   .. mermaid::

      flowchart TD
          Init[Service initialization] --> Check{Enabled profile exists}
          Check -- Yes --> Selector[Start selector]
          Check -- No --> Prov[Start provisioning channel]
          Prov --> Save[Receive and save credentials]
          Save --> Selector
          Selector --> Scan[Scan and rank candidate APs]
          Scan --> Decide{Switch needed}
          Decide -- No --> Keep[Keep current connection]
          Decide -- Yes --> Switch[Switch or fail over]
          Switch --> Probe[Probe quality after connecting]
          Probe --> Decide2{Probe failed or degraded}
          Decide2 -- No --> Keep
          Decide2 -- Yes --> Scan

When no usable candidate network is found, the selector retries scanning according to a built-in backoff table (\ ``1000, 5000, 10000, 20000, 30000`` ms\ ), which can be overridden via ``selector_policy.retry``; applications can also call :cpp:func:`esp_wifi_service_request_connect` to skip scanning and re-evaluation and connect directly to a saved SSID, which is suitable for command-line tools or remote management scenarios.

Network Quality Probing
^^^^^^^^^^^^^^^^^^^^^^^

Being connected to Wi-Fi while the service remains unusable is a common field issue: the device has obtained an IP address, but access to cloud endpoints fails, latency is too high, or throughput is insufficient. Quality probing covers connectivity checks (accessing a specified URL to determine external connectivity) and latency/throughput checks (measuring request duration and actual throughput); handling is triggered only after consecutive failures or sustained degradation, to avoid switching networks because of a single transient fluctuation. Once degradation is confirmed, the current BSSID is temporarily added to the blacklist, and the selector re-selects a candidate network.

Application Examples
---------------------

- The example under the ``examples`` directory demonstrates the minimal integration flow for NVS profile storage, HTTP provisioning, and a custom selector policy; refer to the component repository for the complete code.

FAQ
---

**Q1: Does setting max_connect_retry to 0 cause it to retry forever?**

Yes.\ ``0`` preserves the original behavior of retrying continuously; when set to a non-zero value, automatic re-evaluation stops once consecutive connection failures reach that count, and the counter only resets after :cpp:func:`esp_wifi_service_request_connect` or :cpp:func:`esp_wifi_service_request_reeval` is called.

API Reference
-------------

.. include-build-file:: inc/esp_wifi_service.inc

.. include-build-file:: inc/esp_wifi_service_profile_mgr.inc
