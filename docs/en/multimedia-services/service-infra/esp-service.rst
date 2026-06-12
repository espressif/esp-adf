ESP Service
===========

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`ESP Service <https://components.espressif.com/components/espressif/esp_service>`__ is a three-layer service infrastructure for ESP-IDF. The service base class provides a common lifecycle state machine and event publishing. The service manager adds runtime registration, batch start and stop, and tool invocation. An optional MCP (Model Context Protocol) server then exposes the tools of registered services to a large language model or agent over several transports. Button, Wi-Fi, CLI, and OTA services are all built on this base class.

Feature List
------------

- Lifecycle state machine: \ ``UNINITIALIZED`` → ``INITIALIZED`` → ``RUNNING`` ⇄ ``PAUSED``\ ; all state transitions execute synchronously in the caller's task context
- A vtable-based (\ ``esp_service_ops_t``\ ) subclassing mechanism; a derived service only needs to implement the lifecycle callbacks it requires
- Each service instance is bound to an :doc:`adf-event-hub`, and publishes/subscribes to events via ``esp_service_publish_event()`` / ``esp_service_event_subscribe()``
- Low-power hooks ``on_lowpower_enter`` / ``on_lowpower_exit``\ , which do not trigger state transitions
- ``esp_service_manager`` supports runtime registration/deregistration, lookup by name or category, and batch ``start_all`` / ``stop_all``
- A service registration can carry a JSON-formatted tool description, which the manager automatically parses and exposes for invocation via ``esp_service_manager_invoke_tool()``
- The optional MCP server implements the MCP 2024-11-05 protocol (\ ``tools/list``\ , ``tools/call``\ , ``notifications/tools/list_changed``\ ), supporting six transport methods: HTTP, SSE, WebSocket, UART, STDIO, and SDIO
- Both the service manager and the MCP server are internally protected by mutexes, allowing concurrent calls in a multitasking environment

Technical Deep Dive
--------------------

Three-Layer Model
^^^^^^^^^^^^^^^^^^

ESP Service is divided into three independent layers that can be adopted as needed: \ ``esp_service_t`` is the minimal usable unit, providing lifecycle management and event-publishing capability on its own; \ ``esp_service_manager_t`` provides unified registration and lookup across multiple service instances; the MCP server is then mounted on top of the manager to expose tool invocations to external Agents.

.. only:: html

   .. mermaid::

      classDiagram
          direction TB
          class esp_service_t {
              +state
              +esp_service_start()
              +esp_service_publish_event()
          }
          class esp_service_manager_t {
              +esp_service_manager_register()
              +esp_service_manager_invoke_tool()
          }
          class esp_service_mcp_server_t {
              +tools/list
              +tools/call
          }
          esp_service_manager_t "1" o-- "0..*" esp_service_t
          esp_service_mcp_server_t --> esp_service_manager_t

A service can be implemented using only the base class; the manager is introduced for multi-service orchestration; whether the MCP server is mounted is entirely optional, and the three layers are not mandatorily bound together.

Lifecycle and Subclassing
^^^^^^^^^^^^^^^^^^^^^^^^^

A derived service embeds :cpp:type:`esp_service_t` as the first member of its struct, fills in the required callbacks in :cpp:type:`esp_service_ops_t`, and then calls :cpp:func:`esp_service_init` to complete initialization. The state machine is maintained by the base class; the four lifecycle APIs (:cpp:func:`esp_service_start`, :cpp:func:`esp_service_stop`, :cpp:func:`esp_service_pause`, :cpp:func:`esp_service_resume`) all synchronously invoke the corresponding ``ops`` callback in the caller's task context. If a service requires a long-running background task, it should create the task inside ``on_start`` and return immediately.

.. code:: c

    typedef struct {
        esp_service_t base;  /* Must be the first member */
        /* ... derived fields ... */
    } my_service_t;

    static esp_err_t my_on_start(esp_service_t *base)
    {
        my_service_t *svc = (my_service_t *)base;
        /* Create background task, enable hardware, etc. */
        return ESP_OK;
    }

    static const esp_service_ops_t s_my_ops = {
        .on_start = my_on_start,
    };

    esp_service_config_t cfg = { .name = "my_service" };
    esp_service_init(&svc->base, &cfg, &s_my_ops);

The base class automatically publishes the ``ESP_SERVICE_EVENT_STATE_CHANGED`` event (with ID ``UINT16_MAX - 1``\ ) after every successful state transition; when a derived service defines its own event enumeration, it must not use this value or the wildcard ``UINT16_MAX``\ . Domain events (such as OTA progress or button actions) are published over the same event bus; see :doc:`adf-event-hub` for the publish/subscribe usage.

.. note::

    The low-power hooks are invoked directly by :cpp:func:`esp_service_lowpower_enter` / :cpp:func:`esp_service_lowpower_exit`, bypassing the state machine, and are suitable for suspending peripheral resources such as radios or LEDs.

Service Manager
^^^^^^^^^^^^^^^

``esp_service_manager_t`` maintains a service registry, where each entry is described by :cpp:type:`esp_service_registration_t`: a mandatory ``service`` instance, an optional category string ``category``\ (queried via ``find_by_category``), and an optional pair of ``tool_desc`` / ``tool_invoke``\ . When both are set, the manager parses the JSON tool description array in ``tool_desc`` and routes calls to :cpp:func:`esp_service_manager_invoke_tool` to the ``tool_invoke`` callback; when both are empty, only lifecycle management is performed.

.. code:: c

    esp_service_manager_t *mgr;
    esp_service_manager_create(NULL, &mgr);

    esp_service_manager_register(mgr, &(esp_service_registration_t){
        .service  = (esp_service_t *)my_service,
        .category = "audio",
    });

    esp_service_manager_start_all(mgr);

A tool description is a JSON array in which each item contains ``name``\ , ``description``\ , and ``inputSchema``\ :

.. code:: json

    [
      {
        "name": "player_service_play",
        "description": "Start audio playback",
        "inputSchema": { "type": "object", "properties": {} }
      }
    ]

CLI Service uses the manager to implement its ``svc`` / ``tool`` commands; see :doc:`/multimedia-services/peripheral/esp-cli-service` for details.

MCP Server (Optional)
^^^^^^^^^^^^^^^^^^^^^

After enabling ``CONFIG_ESP_MCP_ENABLE``, an MCP server can be created to expose the tools registered on the manager to external Agents via JSON-RPC 2.0. The server itself is decoupled from the transport method: :cpp:func:`esp_service_manager_as_tool_provider` wraps the manager as a tool provider, which is then supplied together with a concrete transport instance.

.. only:: html

   .. mermaid::

      flowchart TD
          LLM["LLM / AI Agent"] --> MCP[MCP Server]
          MCP --> MGR[Service Manager]
          MGR --> S1[Service A]
          MGR --> S2[Service B]

Each supported transport method corresponds to its own Kconfig option: HTTP (\ ``POST /mcp``\ ), SSE streaming, WebSocket, UART, STDIO, and SDIO, all sharing ``esp_service_mcp_trans_t`` as a unified interface; one or more can be selected depending on the target device's connectivity.

.. code:: c

    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_trans_http_create(&http_cfg, &transport);

    esp_service_mcp_server_config_t cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &cfg.tool_provider);
    cfg.transport = transport;

    esp_service_mcp_server_t *server = NULL;
    esp_service_mcp_server_create(&cfg, &server);
    esp_service_mcp_server_start(server);

Application Examples
---------------------

- ``components/esp_service/examples/mock_services/`` demonstrates the combination of the service manager with all MCP transport methods, along with a host-side Python test script.
- :example:`services_hub` demonstrates a combination of multiple services—Wi-Fi Service, OTA Service, CLI Service, and Button Service—integrated with ``esp_board_manager`` in a production-style usage.

FAQ
------

**Q1: Must a service be registered with esp_service_manager to be used?**

No. \ ``esp_service_t`` on its own is sufficient for initialization, start/stop, and event publishing; the manager is only needed when cross-service orchestration or MCP tool invocation is required.

**Q2: Can the MCP server enable multiple transport methods at the same time?**

Each ``esp_service_mcp_server_t`` instance is bound to a single transport instance; when multiple transport methods need to be provided simultaneously, create multiple server instances that share the same tool provider.

API Reference
--------------

.. include-build-file:: inc/esp_service.inc

.. include-build-file:: inc/esp_service_manager.inc

.. include-build-file:: inc/esp_service_mcp_server.inc
