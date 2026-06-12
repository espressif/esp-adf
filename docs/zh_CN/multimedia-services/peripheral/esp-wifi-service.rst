ESP Wi-Fi Service
===========================

:link_to_translation:`en:[English]`

简介
----------

`Wi-Fi Service <https://components.espressif.com/components/espressif/esp_wifi_service>`__ 把设备联网流程中的凭据存储、配网交互、自动连接、网络选择和质量探测统一到一套 :doc:`/multimedia-services/service-infra/esp-service` 服务接口中。使用它之后，应用不需要分别维护凭据存储、SoftAP/Web 配网、BluFi 配网、断线重连和多 AP 选择逻辑，可以更快构建稳定、便于现场运维的联网产品。

功能清单
----------

- Profile 管理：维护多组 Wi-Fi 凭据，支持新增、更新、启用、禁用、删除和清理，配网通道与连接选择共享同一个 profile manager
- 可插拔存储：复用 :doc:`/multimedia-services/service-infra/esp-config-manager` 的 NVS、文件系统、双分区 raw flash 或自定义存储适配层，并支持加密回调保护已保存凭据
- 多通道配网：HTTP SoftAP/Web UI、BluFi，以及应用自定义配网流程，所有通道写入同一份共享 profile
- 自动启动策略：服务启动时根据是否存在已启用配置，自动进入连接选择流程或启动已配置的配网流程
- 智能选择与切换：按用户优先级、信号质量、历史连通性和临时黑名单选择更合适的 AP，并在断线或链路退化后重新评估
- 网络质量探测：支持连通性、延迟和吞吐退化判断，处理“已连接但业务不可用”的场景
- 可选 MCP 工具支持：同时启用 ``CONFIG_ESP_MCP_ENABLE`` 和 ``CONFIG_WIFI_SERVICE_MCP_ENABLE`` 后，可通过 :doc:`/multimedia-services/service-infra/esp-service` 的 MCP 服务器远程查询状态、管理 profile 和触发配网/连接

技术拆解
----------

Profile 管理与存储
^^^^^^^^^^^^^^^^^^^^^^

每个 Wi-Fi 凭据保存为一个 :cpp:type:`esp_wifi_service_profile_t`\ （SSID、密码、优先级、启用标志），由 ``esp_wifi_service_profile_mgr_t`` 统一管理并持久化。profile manager 的存储层直接复用 :doc:`/multimedia-services/service-infra/esp-config-manager` 的存储适配接口，创建时需要先准备好一个 ``esp_config_storage_t`` 句柄：

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

同一个 ``profile_manager`` 句柄要同时传给 Wi-Fi Service 和每个配网通道的配置，这样配网写入的凭据才能被连接选择逻辑立即看到。

配网通道
^^^^^^^^^^^^

服务启动时如果存在至少一个已启用的 profile，就直接进入连接选择流程；否则启动 ``prov_list`` 中配置的所有配网通道。内置提供 HTTP SoftAP/Web UI 和 BluFi 两种通道，也可以由应用自行实现自定义配网流程写入同一个 profile manager：

- HTTP 通道启动 SoftAP、DNS captive portal、HTTP server 和默认或自定义 Web UI，凭据通过 ``POST /prov/profiles`` 提交
- BluFi 通道通过手机侧蓝牙发送网络信息，凭据同样落到共享 profile manager，依赖 ``CONFIG_WIFI_SERVICE_PROV_BLUFI_ENABLE`` 及底层 BLUFI 协议栈
- 两种通道都可以并行启用，配网结束后统一由 selector 逻辑接管连接

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

选择与切换
^^^^^^^^^^^^^^

selector 在多网络环境下自动决策连接哪个 AP：重新评估时先扫描周围 AP，只保留匹配已保存且已启用 profile 的候选项，再按用户优先级、信号质量、最近一次可正常访问的记录，以及临时黑名单排序，而不是简单选择信号最强的 AP。

.. only:: html

   .. mermaid::

      flowchart TD
          Init[服务初始化] --> Check{存在已启用配置}
          Check -- 是 --> Selector[启动 selector]
          Check -- 否 --> Prov[启动配网通道]
          Prov --> Save[接收凭据并保存]
          Save --> Selector
          Selector --> Scan[扫描候选 AP 并排序]
          Scan --> Decide{需要切换}
          Decide -- 否 --> Keep[保持当前连接]
          Decide -- 是 --> Switch[切换或故障转移]
          Switch --> Probe[连接后进行质量探测]
          Probe --> Decide2{探测失败或退化}
          Decide2 -- 否 --> Keep
          Decide2 -- 是 --> Scan

没有找到可用候选网络时，selector 按内置退避表（\ ``1000, 5000, 10000, 20000, 30000`` ms\ ）重试扫描，可通过 ``selector_policy.retry`` 覆盖；应用也可以调用 :cpp:func:`esp_wifi_service_request_connect` 跳过扫描和重新评估，直接连接一个已保存的 SSID，适合命令行工具或远程管理场景。

网络质量探测
^^^^^^^^^^^^^^^^

已连接 Wi-Fi 但业务不可用是常见的现场问题：设备已经拿到 IP，但云端接口访问失败、延迟过高或吞吐不足。质量探测覆盖连通性检查（访问指定 URL 判断外部连通性）和延迟/吞吐检查（统计请求耗时和实际吞吐），在连续失败或持续退化后才触发处理，避免因为一次偶发抖动就切换网络；确认退化后，当前 BSSID 会被临时加入黑名单，交由 selector 重新选择候选网络。

应用示例
----------

- ``examples`` 目录下的示例演示 NVS profile 存储、HTTP 配网、自定义 selector 策略的最小接入流程，完整代码可参考组件仓库。

FAQ
------

**Q1：max_connect_retry 设为 0 会一直重试吗？**

会。\ ``0`` 表示保持原有行为、持续重试；设为非 0 值后，连续连接失败达到该次数会停止自动重新评估，直到调用 :cpp:func:`esp_wifi_service_request_connect` 或 :cpp:func:`esp_wifi_service_request_reeval` 才会重新计数。

API 参考
----------

.. include-build-file:: inc/esp_wifi_service.inc

.. include-build-file:: inc/esp_wifi_service_profile_mgr.inc
