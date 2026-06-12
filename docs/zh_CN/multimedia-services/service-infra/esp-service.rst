ESP Service
===========

:link_to_translation:`en:[English]`

简介
----------

`ESP Service <https://components.espressif.com/components/espressif/esp_service>`__ 是面向 ESP-IDF 的三层服务基础设施。服务基类提供统一的生命周期状态机与事件发布。服务管理器在此之上提供运行时注册、批量启停与工具调用。可选的 MCP（Model Context Protocol）服务器再把已注册服务的工具，通过多种传输方式暴露给大模型或智能体。按键、Wi-Fi、命令行、OTA 等业务服务均基于该基类实现。

功能清单
----------

- 生命周期状态机：\ ``UNINITIALIZED`` → ``INITIALIZED`` → ``RUNNING`` ⇄ ``PAUSED``\ ，所有状态切换在调用方任务上下文中同步执行
- 基于 vtable（\ ``esp_service_ops_t``\ ）的子类化机制，派生服务只需实现所需的生命周期回调
- 每个服务实例绑定一个 :doc:`adf-event-hub`，通过 ``esp_service_publish_event()`` / ``esp_service_event_subscribe()`` 发布订阅事件
- 低功耗钩子 ``on_lowpower_enter`` / ``on_lowpower_exit``\ ，不引起状态切换
- ``esp_service_manager`` 支持运行时注册/注销、按名称或类别查找、批量 ``start_all`` / ``stop_all``
- 服务注册时可附带 JSON 格式的工具描述，由管理器自动解析并支持 ``esp_service_manager_invoke_tool()`` 调用
- 可选 MCP 服务器实现 MCP 2024-11-05 协议（\ ``tools/list``\ 、\ ``tools/call``\ 、\ ``notifications/tools/list_changed``\ ），支持 HTTP、SSE、WebSocket、UART、STDIO、SDIO 六种传输方式
- 服务管理器与 MCP 服务器内部均由互斥锁保护，可在多任务环境下并发调用

技术拆解
----------

三层模型
^^^^^^^^^^^^

ESP Service 分为三层，彼此独立，可按需选用：\ ``esp_service_t`` 是最小可用单元，只用它就能获得生命周期管理和事件发布能力；\ ``esp_service_manager_t`` 在多个服务实例之上提供统一注册与查找；MCP 服务器再挂载到管理器上，把工具调用暴露给外部 Agent。

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

只使用基类即可实现一个服务；引入管理器用于多服务编排；MCP 服务器是否挂载完全可选，三者不强制绑定使用。

生命周期与子类化
^^^^^^^^^^^^^^^^^^^^

派生服务把 :cpp:type:`esp_service_t` 作为结构体的第一个成员嵌入，填充 :cpp:type:`esp_service_ops_t` 中需要的回调，再调用 :cpp:func:`esp_service_init` 完成初始化。状态机由基类维护，四个生命周期 API（:cpp:func:`esp_service_start`、:cpp:func:`esp_service_stop`、:cpp:func:`esp_service_pause`、:cpp:func:`esp_service_resume`）均在调用方任务上下文中同步调用对应的 ``ops`` 回调，服务如果需要长时间运行的后台任务，应在 ``on_start`` 内自行创建并立即返回。

.. code:: c

    typedef struct {
        esp_service_t base;  /* 必须是第一个成员 */
        /* ... 派生字段 ... */
    } my_service_t;

    static esp_err_t my_on_start(esp_service_t *base)
    {
        my_service_t *svc = (my_service_t *)base;
        /* 创建后台任务、使能硬件等 */
        return ESP_OK;
    }

    static const esp_service_ops_t s_my_ops = {
        .on_start = my_on_start,
    };

    esp_service_config_t cfg = { .name = "my_service" };
    esp_service_init(&svc->base, &cfg, &s_my_ops);

基类在每次状态切换成功后自动发布 ``ESP_SERVICE_EVENT_STATE_CHANGED`` 事件（ID 为 ``UINT16_MAX - 1``\ ），派生服务定义自己的事件枚举时不能使用该值或通配符 ``UINT16_MAX``\ 。领域事件（如 OTA 进度、按键动作）通过同一个事件总线发布，发布与订阅方式见 :doc:`adf-event-hub`。

.. note::

    低功耗钩子由 :cpp:func:`esp_service_lowpower_enter` / :cpp:func:`esp_service_lowpower_exit` 直接调用，不经过状态机，适合用来挂起无线电、LED 等外围资源。

服务管理器
^^^^^^^^^^^^

``esp_service_manager_t`` 维护一份服务注册表，每个条目通过 :cpp:type:`esp_service_registration_t` 描述：必填的 ``service`` 实例、可选的分类字符串 ``category``\ （供 ``find_by_category`` 查询）、以及一对可选的 ``tool_desc`` / ``tool_invoke``\ 。两者都设置时，管理器会解析 ``tool_desc`` 中的 JSON 工具描述数组，并把 :cpp:func:`esp_service_manager_invoke_tool` 的调用路由到 ``tool_invoke`` 回调；两者都为空则仅做生命周期管理。

.. code:: c

    esp_service_manager_t *mgr;
    esp_service_manager_create(NULL, &mgr);

    esp_service_manager_register(mgr, &(esp_service_registration_t){
        .service  = (esp_service_t *)my_service,
        .category = "audio",
    });

    esp_service_manager_start_all(mgr);

工具描述是一段 JSON 数组，每项包含 ``name``\ 、\ ``description`` 和 ``inputSchema``\ ：

.. code:: json

    [
      {
        "name": "player_service_play",
        "description": "Start audio playback",
        "inputSchema": { "type": "object", "properties": {} }
      }
    ]

CLI Service 用管理器实现它的 ``svc`` / ``tool`` 命令，具体用法见 :doc:`/multimedia-services/peripheral/esp-cli-service`。

MCP 服务器（可选）
^^^^^^^^^^^^^^^^^^^^

启用 ``CONFIG_ESP_MCP_ENABLE`` 后可以创建 MCP 服务器，把管理器上注册的工具通过 JSON-RPC 2.0 暴露给外部 Agent。服务器本身与传输方式解耦，通过 :cpp:func:`esp_service_manager_as_tool_provider` 把管理器包装为工具来源，再传入一个具体的传输实例。

.. only:: html

   .. mermaid::

      flowchart TD
          LLM["LLM / AI Agent"] --> MCP[MCP 服务器]
          MCP --> MGR[服务管理器]
          MGR --> S1[服务 A]
          MGR --> S2[服务 B]

支持的传输方式各自对应一个独立的 Kconfig 选项：HTTP（\ ``POST /mcp``\ ）、SSE 流式、WebSocket、UART、STDIO、SDIO，均以 ``esp_service_mcp_trans_t`` 为统一接口，可按目标设备的连接方式任选其一或多个。

.. code:: c

    esp_service_mcp_trans_t *transport = NULL;
    esp_service_mcp_trans_http_create(&http_cfg, &transport);

    esp_service_mcp_server_config_t cfg = ESP_SERVICE_MCP_SERVER_CONFIG_DEFAULT();
    esp_service_manager_as_tool_provider(mgr, &cfg.tool_provider);
    cfg.transport = transport;

    esp_service_mcp_server_t *server = NULL;
    esp_service_mcp_server_create(&cfg, &server);
    esp_service_mcp_server_start(server);

应用示例
----------

- ``components/esp_service/examples/mock_services/`` 演示服务管理器与全部 MCP 传输方式的组合，附带主机侧 Python 测试脚本。
- :example:`services_hub` 演示 Wi-Fi Service、OTA Service、CLI Service、Button Service 多个服务组合、并集成 ``esp_board_manager`` 的生产风格用法。

FAQ
------

**Q1：一个服务必须注册到 esp_service_manager 才能使用吗？**

不需要。\ ``esp_service_t`` 自身即可完成初始化、启停和事件发布，管理器只在需要跨服务编排或 MCP 工具调用时才有必要引入。

**Q2：MCP 服务器支持同时开启多种传输方式吗？**

每个 ``esp_service_mcp_server_t`` 实例绑定一个传输实例；需要多种传输方式同时对外提供服务时，创建多个服务器实例并共享同一个工具来源即可。

API 参考
----------

.. include-build-file:: inc/esp_service.inc

.. include-build-file:: inc/esp_service_manager.inc

.. include-build-file:: inc/esp_service_mcp_server.inc
