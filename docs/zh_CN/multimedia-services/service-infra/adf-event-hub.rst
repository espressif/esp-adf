ADF Event Hub
===============

:link_to_translation:`en:[English]`

简介
----------

`ADF Event Hub <https://components.espressif.com/components/espressif/adf_event_hub>`__ 是面向 ADF 与 GMF 组件的按域发布订阅机制。每个句柄对应一个事件源域，例如无线网络或 OTA。发布者向该域投递事件，订阅者按域和事件编号过滤后，以队列或回调方式接收。:doc:`esp-service` 中的每个服务实例都会自动创建并绑定一个事件中心，用于发布服务自身及派生服务的领域事件。

功能清单
----------

- 按域发布订阅：一个 hub 句柄代表一个事件源域，订阅者按 ``(event_domain, event_id)``\ 过滤，\ ``ADF_EVENT_ANY_ID`` 作为通配符匹配全部事件
- 两种投递模式：队列模式（非阻塞 ``xQueueSend``\ ）与回调模式（在发布者任务中同步执行），按订阅者粒度独立选择
- 启动顺序无关：订阅者可以在目标域的发布者调用 ``create()`` 之前完成订阅，目标域会被自动创建
- 引用计数的 hub 生命周期：\ ``adf_event_hub_create()`` / ``adf_event_hub_destroy()`` 成对使用，域在引用计数归零时才被真正移除
- 带引用计数的 envelope 投递：可选的 ``release_cb`` 在每次成功发布后被精确调用一次，无论投递路径是否命中队列订阅者
- 线程安全：所有公开 API 由内部互斥锁保护，可在任意任务中调用（不支持 ISR 上下文）
- 可观测：\ ``adf_event_hub_get_stats()`` 返回各域的订阅者数量与 envelope 池使用情况，\ ``adf_event_hub_dump()`` 输出完整状态日志

技术拆解
----------

发布订阅模型
^^^^^^^^^^^^^^^^

每个域用一个字符串标识（如 ``"wifi"``\ ），大小写敏感。发布者和订阅者各自持有一个 hub 句柄：发布者的句柄标识自己所属的域，订阅者的句柄只是调用方的身份标识，通过 :cpp:func:`adf_event_hub_subscribe` 的 ``event_domain`` 字段指定要监听的域，为 ``NULL`` 时默认监听自己 hub 所属的域。

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

投递模式与引用释放
^^^^^^^^^^^^^^^^^^^^^^

:cpp:type:`adf_event_subscribe_info_t` 中 ``target_queue`` 非空时使用队列模式，否则使用 ``handler`` 回调模式；两者都设置时以队列模式为准。队列模式下，发布调用只做非阻塞的 ``xQueueSend()``\ ，某个订阅者的队列满只影响它自己的这一次投递；回调模式下 ``handler`` 在发布者任务里同步执行，不能有明显阻塞。

.. only:: html

   .. mermaid::

      sequenceDiagram
          participant Pub as 发布者
          participant Hub as event hub
          participant Sub as 队列订阅者

          Pub->>Hub: adf_event_hub_publish()
          Hub-)Sub: xQueueSend(delivery)
          Sub->>Hub: adf_event_hub_delivery_done()
          Hub--)Pub: release_cb(payload)

事件载荷按浅拷贝方式投递，堆上分配的 payload 需要通过 ``release_cb`` 回收：没有队列模式订阅者命中时，\ ``release_cb`` 在 :cpp:func:`adf_event_hub_publish` 返回前就会被调用；命中队列订阅者时，则延迟到最后一次 :cpp:func:`adf_event_hub_delivery_done` 调用之后才触发，且只会被调用一次。

.. code:: c

    static void release_payload(const void *payload, void *ctx)
    {
        free((void *)payload);  /* 在最后一次 delivery_done 之后被精确调用一次 */
    }

    char *msg = strdup("hello");
    adf_event_t ev = { .domain = "wifi", .event_id = 7, .payload = msg, .payload_len = strlen(msg) + 1 };
    adf_event_hub_publish(wifi_hub, &ev, release_payload, NULL);

.. note::

    队列模式订阅者收到 :cpp:type:`adf_event_delivery_t` 后，必须对每次投递调用恰好一次 :cpp:func:`adf_event_hub_delivery_done`，否则会占用 envelope 池而不释放。

生命周期与引用计数
^^^^^^^^^^^^^^^^^^^^^^

:cpp:func:`adf_event_hub_create` 按引用计数管理域：多次对同一域调用 ``create()`` 只会增加引用计数，域仅在计数归零（即调用了同样次数的 :cpp:func:`adf_event_hub_destroy`）时才被真正移除并清理其订阅者。这一特性使得共享 hub 可以被多个持有者共同持有，任意一方销毁自己的引用都不会影响其他持有者。:doc:`esp-service` 中的 ``esp_service_init()`` 已经封装了这个流程，业务代码通常不需要直接调用 event hub 的创建/销毁接口。

应用示例
----------

- ``components/adf_event_hub/examples/`` 演示多个服务通过 ADF Event Hub 互动，同一份代码可在 PC 主机（FreeRTOS POSIX 模拟器）和 ESP-IDF 目标上编译运行。

FAQ
------

**Q1：队列模式和回调模式可以同时用在同一个订阅上吗？**

不能。\ ``target_queue`` 和 ``handler`` 同时设置时以队列模式为准，\ ``handler`` 被忽略；需要两种投递方式时应分别注册两条订阅。

API 参考
----------

.. include-build-file:: inc/adf_event_hub.inc
