ESP Button Service
=============================

:link_to_translation:`en:[English]`

简介
----------

`Button Service <https://components.espressif.com/components/espressif/esp_button_service>`__ 是 :doc:`/multimedia-services/service-infra/esp-service` 上的按键封装。它发现 Board Manager 中登记的按键设备，为选定事件注册回调，并将按键动作作为带类型的服务事件转发。应用层不必自行维护设备名称列表。

功能清单
----------

- 零配置自动发现：遍历 ``esp_board_manager`` 中登记的所有 ``ESP_BOARD_DEVICE_TYPE_BUTTON`` 设备，同时支持单路 GPIO 按键和 ADC 多按键组
- 精细化事件注册：通过位掩码 ``event_mask`` 精确选择要安装的 ``iot_button`` 回调，减少不必要的开销
- 生命周期感知转发：只在服务处于运行状态时发布事件；服务停止、暂停或进入低功耗时自动抑制转发，恢复时自动继续
- 类型化载荷：每个事件携带包含板级设备标签的 ``esp_button_service_payload_t``\ ，便于多按键场景的分发处理
- 继承 :doc:`/multimedia-services/service-infra/esp-service` 提供的标准生命周期与事件订阅接口，无需额外学习新的调用方式

技术拆解
----------

自动发现与事件转发
^^^^^^^^^^^^^^^^^^^^^^

:cpp:func:`esp_button_service_create` 扫描 ``esp_board_manager`` 的设备注册表，初始化每个按键设备，并按 ``cfg->event_mask`` 注册所选的 ``iot_button`` 回调；调用 :cpp:func:`esp_service_start` 后，每次按键动作都会通过服务的事件总线发布，所有订阅者均可响应。

.. only:: html

   .. mermaid::

      flowchart TD
          BM[esp_board_manager 按键设备] --> BS[esp_button_service]
          BS -->|iot_button 回调| BS
          BS -->|esp_service_publish_event| App[应用层订阅者]

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

事件掩码与事件 ID
^^^^^^^^^^^^^^^^^^^^

服务定义了 10 个事件 ID（\ ``ESP_BUTTON_SERVICE_EVT_PRESS_DOWN`` 等，取值从 ``1`` 开始\ ），每个都对应一个位掩码常量 ``ESP_BUTTON_SERVICE_EVT_MASK_xxx``\ ；\ ``event_mask`` 只影响回调注册，不影响事件 ID 本身。\ ``ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT`` 默认开启 ``PRESS_DOWN``\ 、\ ``PRESS_UP``\ 、\ ``SINGLE_CLICK``\ 、\ ``DOUBLE_CLICK``\ 、\ ``LONG_PRESS_START`` 和 ``LONG_PRESS_UP`` 六个常用的按键交互。

事件载荷
^^^^^^^^^^^^

每个 ``ESP_BUTTON_SERVICE_EVT_*`` 事件携带一个堆分配的 :cpp:type:`esp_button_service_payload_t`，在所有订阅者接收完毕后自动释放，应用层不需要手动 free：

.. code:: c

    typedef struct {
        const char *label;  /* 板级设备名，进程生命周期内有效 */
    } esp_button_service_payload_t;

对于单路 GPIO 按键，\ ``label`` 就是该按键的板级设备名；对于 ADC 多按键组，\ ``label`` 取自板级设备配置里的 ``button_labels[]``\ （\ ``dev_button_config_t``\ ），而不是设备名本身，因此同一路 ADC 通道下的不同电压挡位按键也能被区分。

应用示例
----------

- ``components/esp_button_service/examples/button_svc_example`` 演示单 GPIO 按键与 ADC 多按键组的混合接入，以及完整的事件订阅流程。

FAQ
------

**Q1：事件掩码 event_mask 设为 0 会怎样？**

等同于使用 ``ESP_BUTTON_SERVICE_EVT_MASK_DEFAULT``\ ；\ :cpp:type:`esp_button_service_cfg_t` 中说明 ``event_mask`` 为 ``0`` 时会回退到默认掩码。

API 参考
----------

.. include-build-file:: inc/esp_button_service.inc
