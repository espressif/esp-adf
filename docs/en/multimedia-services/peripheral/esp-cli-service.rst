ESP CLI Service
===============

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`CLI Service <https://components.espressif.com/components/espressif/esp_cli_service>`__ is a subclass of :doc:`/multimedia-services/service-infra/esp-service` that embeds the ESP-IDF console UART REPL into the service lifecycle. It provides two layers of commands: built-in static system commands, and dynamic service and tool commands driven by the service manager. Interactive debugging and runtime control can be done from a serial terminal.

.. note::

    Requires ESP-IDF >= 5.4.

Feature List
------------

- UART REPL integration: the ``esp_console`` REPL is embedded directly into the service lifecycle, starting when :cpp:func:`esp_service_start` is called and shutting down on stop/destroy
- Built-in system commands: ``sys_heap``, ``sys_chip``, ``sys_uptime``, and ``sys_reboot`` are registered automatically
- Static commands: any ``esp_console_cmd_t`` can be registered at any time, before or after the REPL starts, via :cpp:func:`esp_cli_service_register_static_command`
- Dynamic ``svc`` command: lists, inspects, and controls (``start`` / ``stop`` / ``pause`` / ``resume``) tracked services from the terminal
- Dynamic ``tool`` command: enumerates and invokes the JSON Schema tools exposed by ``esp_service_manager``, supporting both ``key=value`` and ``--json`` argument formats
- All public APIs are protected by an internal mutex and can be safely called from any task

Technical Deep Dive
-------------------

Command Architecture
^^^^^^^^^^^^^^^^^^^^

Commands attached to the REPL fall into three categories: built-in system commands, which are always available; static commands that the application registers via :cpp:func:`esp_cli_service_register_static_command`; and dynamic ``svc`` / ``tool`` commands, which become available only after ``esp_service_manager`` is bound. Once :cpp:func:`esp_service_start` is called, the REPL task is created, and any pending static commands are registered with ``esp_console``.

.. only:: html

   .. mermaid::

      flowchart TD
          UART[UART Terminal] --> REPL[esp_console REPL]
          REPL --> Sys["Built-in sys_* commands"]
          REPL --> Static["Static commands"]
          REPL --> Dyn["Dynamic commands (requires bound manager)"]
          Dyn --> Svc["svc command: tracked services / manager"]
          Dyn --> Tool["tool command: manager tool registry"]

.. code:: c

    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.prompt = "app>";

    esp_cli_service_t *cli = NULL;
    esp_cli_service_create(&cfg, &cli);

    esp_cli_service_track_service(cli, (esp_service_t *)my_svc, "audio");

    const esp_console_cmd_t my_cmd = {
        .command = "version",
        .help    = "Print firmware version",
        .func    = cmd_version,
    };
    esp_cli_service_register_static_command(cli, &my_cmd);

    esp_service_start((esp_service_t *)cli);

svc and tool Commands
^^^^^^^^^^^^^^^^^^^^^

The ``svc`` command (``list`` / ``info`` / ``start`` / ``stop`` / ``pause`` / ``resume``) shows services that are explicitly tracked via :cpp:func:`esp_cli_service_track_service` first, followed by services registered in the bound ``esp_service_manager``. The ``tool`` command (``list`` / ``info`` / ``call``) enumerates the JSON Schema tools exposed by the manager; ``tool call`` serializes ``key=value`` pairs into a JSON object according to the tool's ``inputSchema``, supporting ``string``, ``integer``, ``number``, and ``boolean`` types, while complex types such as ``array`` or ``object`` require ``--json`` to pass raw JSON arguments instead. The manager is bound either through the ``manager`` field in the configuration or by calling :cpp:func:`esp_cli_service_bind_manager` at runtime; both dynamic commands depend on this step.

Application Examples
---------------------

- ``components/esp_cli_service/examples/`` provides a standalone example; run ``idf.py set-target <chip>`` followed by ``idf.py build flash monitor`` to try it on real hardware.

FAQ
---

**Q1: What if no prompt appears in the serial terminal?**

First confirm that :cpp:func:`esp_service_start` has been called (the REPL task is only created at startup), then check that the UART port and baud rate selected in the terminal emulator match the console UART configured in the IDF menuconfig. If the registered commands allocate a large amount of data, the default 4096-byte ``task_stack`` may not be sufficient and should be increased accordingly.

**Q2: Why do the ``svc`` / ``tool`` commands report "manager not bound"?**

Both dynamic commands require ``esp_service_manager`` to be bound first: set ``cfg.manager`` in the configuration, or call :cpp:func:`esp_cli_service_bind_manager` at runtime before using them.

API Reference
--------------

.. include-build-file:: inc/esp_cli_service.inc
