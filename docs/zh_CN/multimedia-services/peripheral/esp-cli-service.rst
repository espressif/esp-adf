ESP CLI Service
=======================

:link_to_translation:`en:[English]`

简介
----------

`CLI Service <https://components.espressif.com/components/espressif/esp_cli_service>`__ 是 :doc:`/multimedia-services/service-infra/esp-service` 的子类，把 ESP-IDF 控制台的串口交互嵌入服务生命周期。它提供两层命令：内置静态系统命令，以及由服务管理器驱动的动态服务命令与工具命令。通过串口终端即可完成交互式调试与运行时控制。

.. note::

    需要 ESP-IDF >= 5.4。

功能清单
----------

- UART REPL 集成：\ ``esp_console`` REPL 直接嵌入服务生命周期，调用 :cpp:func:`esp_service_start` 时启动，停止/销毁时关闭
- 内置系统命令：自动注册 ``sys_heap``\ 、\ ``sys_chip``\ 、\ ``sys_uptime``\ 、\ ``sys_reboot``
- 静态命令：通过 :cpp:func:`esp_cli_service_register_static_command` 在 REPL 启动前后随时注册任意 ``esp_console_cmd_t``
- 动态 ``svc`` 命令：从终端列出、查看和控制（\ ``start`` / ``stop`` / ``pause`` / ``resume``\ ）已跟踪的服务
- 动态 ``tool`` 命令：枚举并调用 ``esp_service_manager`` 暴露的 JSON Schema 工具，支持 ``key=value`` 和 ``--json`` 两种参数传入方式
- 所有公开 API 由内部互斥锁保护，可从任意任务安全调用

技术拆解
----------

命令体系
^^^^^^^^^^^^

REPL 上挂载的命令分为三类：内置系统命令始终可用；应用通过 :cpp:func:`esp_cli_service_register_static_command` 注册的静态命令；以及在绑定 ``esp_service_manager`` 后才可用的 ``svc`` / ``tool`` 动态命令。调用 :cpp:func:`esp_service_start` 后 REPL 任务被创建，所有待注册的静态命令随即注册到 ``esp_console``\ 。

.. only:: html

   .. mermaid::

      flowchart TD
          UART[UART 终端] --> REPL[esp_console REPL]
          REPL --> Sys["内置 sys_* 命令"]
          REPL --> Static["静态命令"]
          REPL --> Dyn["动态命令（需绑定 manager）"]
          Dyn --> Svc["svc 命令：已跟踪服务 / manager"]
          Dyn --> Tool["tool 命令：manager 工具注册表"]

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

svc 与 tool 命令
^^^^^^^^^^^^^^^^^^^^

``svc`` 命令（\ ``list`` / ``info`` / ``start`` / ``stop`` / ``pause`` / ``resume``\ ）优先展示通过 :cpp:func:`esp_cli_service_track_service` 显式跟踪的服务，其次是绑定的 ``esp_service_manager`` 中登记的服务。\ ``tool`` 命令（\ ``list`` / ``info`` / ``call``\ ）枚举管理器暴露的 JSON Schema 工具；\ ``tool call`` 会按工具的 ``inputSchema`` 把 ``key=value`` 键值对序列化成 JSON 对象，支持 ``string``\ 、\ ``integer``\ 、\ ``number``\ 、\ ``boolean`` 类型，遇到 ``array`` 或 ``object`` 等复杂类型时改用 ``--json`` 传入原始 JSON 参数。管理器通过配置中的 ``manager`` 字段或运行期调用 :cpp:func:`esp_cli_service_bind_manager` 绑定，两种动态命令都依赖这一步。

应用示例
----------

- ``components/esp_cli_service/examples/`` 提供独立示例，执行 ``idf.py set-target <chip>`` 后 ``idf.py build flash monitor`` 即可上板体验。

FAQ
------

**Q1：串口终端上没有出现提示符怎么办？**

先确认已调用 :cpp:func:`esp_service_start`\ （REPL 任务只在启动时创建），再检查终端模拟器选择的 UART 端口和波特率是否匹配 IDF menuconfig 中配置的 console UART；如果注册的命令分配了较多数据，默认 4096 字节的 ``task_stack`` 未必够用，需要适当增大。

**Q2：为什么 ``svc`` / ``tool`` 命令提示 “manager not bound”？**

这两类动态命令都需要先绑定 ``esp_service_manager``\ ：在配置中设置 ``cfg.manager``\ ，或在运行期调用 :cpp:func:`esp_cli_service_bind_manager` 之后再使用。

API 参考
----------

.. include-build-file:: inc/esp_cli_service.inc
