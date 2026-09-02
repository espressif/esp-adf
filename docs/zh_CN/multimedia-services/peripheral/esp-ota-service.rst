ESP OTA Service
=======================

:link_to_translation:`en:[English]`

简介
----------

`OTA Service <https://components.espressif.com/components/espressif/esp_ota_service>`__ 是 :doc:`/multimedia-services/service-infra/esp-service` 的一个子类，实现了一套模块化、可扩展的空中固件升级流水线。它把升级过程拆成数据源、写入目标、版本检查器和完整性校验器四个独立的抽象层，因此 HTTP/HTTPS、文件系统、BLE 等传输方式可以和应用分区、数据分区、Bootloader 等写入目标任意组合，无需修改服务主体代码。

功能清单
----------

- 多数据源下载：HTTP/HTTPS、本地文件系统、遵循官方 ESP BLE OTA APP 协议的 BLE GATT 外设（可选），也可以通过 ``esp_ota_service_source_t`` 接口接入 UART、SPI 等自定义传输
- 多目标写入：应用分区、原始数据分区、Bootloader（可选），也可以通过 ``esp_ota_service_target_t`` 接入自定义写入目标
- 下载前版本检查：应用镜像头、语义版本头、JSON 清单三种内置检查器，已是最新版本的条目自动跳过
- 流式完整性校验：SHA-256 和 MD5 内置校验器，也可以通过 ``esp_ota_service_verifier_t`` 接入签名或 CRC 方案
- 基于 NVS 的断点续传：每若干 KB 保存一次下载偏移，故障或重启后从断点恢复
- 回滚支持：\ ``esp_ota_service_confirm_update()`` / ``esp_ota_service_rollback()`` 及待验证状态检测（需要 ``CONFIG_OTA_ENABLE_ROLLBACK``\ ）
- 下载中可暂停/恢复：运行中的会话可直接调用 :cpp:func:`esp_service_pause` / :cpp:func:`esp_service_resume`
- 事件总线覆盖会话开始、版本检查、条目开始/进度/结束和会话结束共 6 种类型，不强制规定重启策略

技术拆解
----------

流水线抽象
^^^^^^^^^^^^^^

每个 :cpp:type:`esp_ota_upgrade_item_t` 描述一次分区升级，由 ``source``\ 、\ ``target`` 两个必填接口和 ``checker``\ 、\ ``verifier`` 两个可选接口组成；四类接口都是函数指针结构体，实现后即可自由组合。服务通过 :cpp:func:`esp_ota_service_set_upgrade_list` 接管列表中每个组件的所有权，不应由调用方再直接调用它们的 ``destroy()``\ 。

.. only:: html

   .. mermaid::

      flowchart TD
          Start([条目开始]) --> Chk{checker?}
          Chk -- 无 --> Open[source.open]
          Chk -- 有 --> Check[checker.check]
          Check -- 非更新版本 --> Skip([跳过])
          Check -- 有新版本 --> Open
          Open --> Vb{verifier?}
          Vb -- 无 --> TOpen[target.open]
          Vb -- 有 --> VBegin[verifier.verify_begin]
          VBegin -- 拒绝 --> Skip
          VBegin -- 通过 --> TOpen
          TOpen --> Loop{source.read 循环}
          Loop --> Update[verifier.verify_update]
          Update -- 校验失败 --> Fail([中止])
          Update -- 通过 --> Write[target.write] --> Loop
          Loop -- EOF --> Vf{verifier?}
          Vf -- 无 --> Commit[target.commit]
          Vf -- 有 --> VFinish[verifier.verify_finish]
          VFinish -- 校验失败 --> Fail
          VFinish -- 通过 --> Commit

调用 :cpp:func:`esp_service_start` 后立即返回，工作任务在后台运行并通过事件上报进度，不会阻塞调用方任务。

.. code:: c

    #include "esp_ota_service_default.h"  /* 汇总内置 source/target/checker/verifier 头文件 */

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

    调用 :cpp:func:`esp_ota_service_set_upgrade_list` 之后，列表中每个 ``source``\ 、\ ``target``\ 、\ ``checker``\ 、\ ``verifier`` 的所有权都转移给服务，即使调用失败也是如此；不要再直接调用它们的 ``destroy()``\ ，\ :cpp:func:`esp_ota_service_destroy` 会负责统一释放。

选择合适的组合
^^^^^^^^^^^^^^^^^^

常见场景对应的组件选择：HTTP OTA 配合版本清单检查和 SHA-256 校验时，使用 ``esp_ota_service_source_http_create()`` + ``esp_ota_service_checker_manifest_create()`` + ``esp_ota_service_verifier_sha256_create()`` + ``esp_ota_service_target_app_create()``\ ；仅需版本检查而不做流式校验时可以省去 ``verifier``\ ；SD 卡或 U 盘部署使用 ``esp_ota_service_source_fs_create()`` 搭配同样的应用目标；数据分区升级则改用 ``esp_ota_service_checker_data_version_create()`` 和 ``esp_ota_service_target_data_create()``\ 。多分区批量升级时，一次会话可以为应用分区、数据分区分别配置独立的条目，共用同一个事件流。

事件与进度
^^^^^^^^^^^^^^

通过 :cpp:func:`esp_service_event_subscribe` 订阅后，每次回调收到的 ``adf_event_t`` 中 ``payload`` 指向 :cpp:type:`esp_ota_service_event_t`\ ，需要先读取 ``id`` 字段，再访问对应的联合体分支：\ ``ESP_OTA_SERVICE_EVT_ITEM_VER_CHECK`` 报告版本检查结果，\ ``ESP_OTA_SERVICE_EVT_ITEM_PROGRESS`` 约每秒更新一次下载字节数，\ ``ESP_OTA_SERVICE_EVT_ITEM_END`` 携带单个分区的成功/跳过/失败结果，\ ``ESP_OTA_SERVICE_EVT_SESSION_END`` 携带整个会话的汇总统计。调用方也可以直接用 :cpp:func:`esp_ota_service_get_progress` 轮询当前条目的下载进度百分比。

断点续传与回滚
^^^^^^^^^^^^^^^^^^

启用 ``CONFIG_OTA_ENABLE_RESUME``\ （默认开启）后，服务每隔固定字节数把下载偏移保存到 NVS；对没有实现 ``seek()`` 的数据源（如 BLE）或没有 ``set_write_offset()`` 的目标（数据分区、Bootloader），必须把对应条目的 ``resumable`` 设为 ``false``\ ，否则 :cpp:func:`esp_ota_service_set_upgrade_list` 会返回 ``ESP_ERR_NOT_SUPPORTED``\ 。启用 ``CONFIG_OTA_ENABLE_ROLLBACK`` 后，新固件启动后应先用 :cpp:func:`esp_ota_service_is_pending_verify` 检查是否处于待验证状态，自测通过后调用 :cpp:func:`esp_ota_service_confirm_update` 取消回滚计时器；否则每次重启都会触发自动回滚。

应用示例
----------

- ``examples/ota_http/`` 演示 HTTP + 清单版本检查 + SHA-256 校验 + 断点续传
- ``examples/ota_fs/`` 演示 SD 卡/U 盘离线部署与多分区批量升级
- ``examples/ota_ble/`` 演示无 Wi-Fi 场景下的 BLE GATT 固件推送

FAQ
------

**Q1：resumable 设为 true 但仍返回 ESP_ERR_NOT_SUPPORTED，是什么原因？**

数据源没有实现 ``seek()``\ ，或写入目标没有实现 ``set_write_offset()``\ 。数据分区和 Bootloader 目标在 ``open()`` 时会整片擦除，天然不支持续传；目前只有应用分区目标，以及支持随机访问 ``seek`` 的数据源才能续传。

**Q2：升级失败后要如何避免设备变砖？**

应用分区升级依赖 A/B 双镜像机制，失败时可以回退到旧分区；但 Bootloader OTA（\ ``CONFIG_OTA_ENABLE_BOOTLOADER_OTA``\ ）通过暂存分区写入，没有原子性保证，拷贝过程中断电存在变砖风险，量产前需要额外评估并做好回退预案。

API 参考
----------

.. include-build-file:: inc/esp_ota_service.inc

.. include-build-file:: inc/esp_ota_service_default.inc
