ESP Media Service
======================

:link_to_translation:`en:[English]`

简介
----------

`ESP Media Service <https://components.espressif.com/components/espressif/esp_media_service>`__ 为 ESP-ADF 中的音视频服务定义了一套通用媒体接口，以 :doc:`/multimedia-services/service-infra/esp-service` 为基类。应用程序创建服务、配置媒体流，再把源流链接到接收端流。链接并启动后，媒体帧通过读取接口与写入接口自动流动，应用程序不必手动转发帧数据。音视频采集与播放见 :doc:`/basic-components/esp-gmf/index`。

功能清单
----------

- 统一的音视频服务模型，源、接收端、源接收一体三种角色（\ ``ESP_MEDIA_ROLE_SRC`` / ``SINK`` / ``SRC_SINK``\ ）
- 基于 stream ID（\ ``esp_media_stream_id_t``\ ）的多媒体端点，一个服务可以同时暴露多个 stream
- 链接时的请求协商：接收端可以通过 ``esp_media_service_request_t`` 向源提出诉求（如是否需要 global cache）
- Provider 读取接口（\ ``esp_media_provider_t``\ ）与 track manager 写入接口相互解耦
- 内置默认的内存 track manager（\ ``esp_media_track_mngr_t``\ ），支持按 track 独立缓存或全局到达顺序缓存
- 统一通过 :doc:`/multimedia-services/service-infra/esp-service` 管理服务生命周期，媒体相关操作作为独立的 vtable（\ ``esp_media_service_ops_t``\ ）叠加在基类之上

技术拆解
----------

服务角色与链接
^^^^^^^^^^^^^^^^^^

ESP Media Service 通过 :cpp:type:`esp_media_service_ops_t` 中的 ``get_role`` 声明自己是源、接收端还是二者兼具；:cpp:func:`esp_media_service_link` 在链接时校验所选源和接收端的角色是否兼容，再把源的 provider 传给接收端。

.. only:: html

   .. mermaid::

      flowchart TD
          Create["创建服务"] --> Config["配置服务"]
          Config --> Link["链接源/接收端 stream"]
          Link --> Start["启动服务"]
          Start --> Flow["媒体帧流动"]
          Flow --> Stop["停止服务"]
          Stop --> Unlink["取消链接并销毁"]

.. code:: c

    esp_media_stream_id_t stream = ESP_MEDIA_DEFAULT_STREAM;

    esp_media_service_link(src_service, stream, sink_service, stream);

    esp_service_start(sink_service);
    esp_service_start(src_service);

链接建立后，媒体数据通过源服务导出的 provider 流向接收端；取消链接需要调用 :cpp:func:`esp_media_service_unlink`，会清除接收端 stream 上当前设置的 provider。

Provider 与 Track Manager
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

媒体数据的传递依赖两个互相解耦的接口：接收端在链接时从源服务获取只读的 ``esp_media_provider_t``\ ，通过 :cpp:func:`esp_media_provider_acquire_frame` / :cpp:func:`esp_media_provider_release_frame` 获取和释放帧；源服务侧则持有一个 ``esp_media_track_mngr_t``\ ，通过写入 API 产生帧，并把它导出的 provider 句柄暴露给下游。

.. code:: c

    /* 源服务侧：创建 track manager，注册 track，导出 provider */
    esp_media_track_mngr_cfg_t cfg = { .max_track_num = 2 };
    esp_media_track_mngr_create(&cfg, &svc->mngr);
    esp_media_track_mngr_add_track(svc->mngr, &audio_track);
    esp_media_track_mngr_get_provider(svc->mngr, &svc->provider);

.. code:: c

    /* 接收端侧：读取并释放帧 */
    esp_media_frame_t frame = {0};
    if (esp_media_provider_acquire_frame(&sink->provider, &frame, timeout_ms) == ESP_OK) {
        process_frame(&frame);
        esp_media_provider_release_frame(&sink->provider, &frame);
    }

.. warning::

    通过 :cpp:func:`esp_media_provider_acquire_frame` 获取的帧必须调用 :cpp:func:`esp_media_provider_release_frame` 释放，释放之后不能再访问 ``frame.data``\ 。

Track Manager 的缓存模式
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

默认的 ``esp_media_track_mngr_t`` 支持两种 payload 所有权模式：\ ``ESP_MEDIA_TRACK_CACHE_INTERNAL`` 由 manager 复制并持有帧数据；\ ``ESP_MEDIA_TRACK_CACHE_USER`` 只缓存帧元数据，payload 仍由用户持有，消费后通过 ``frame_release`` 回调归还。此外还支持 global cache，让多个 track 共用一个按到达顺序排列的队列，适合 RTMP 等音视频交错传输场景；启用 global cache 需要在添加 track 之前完成配置。

停止与中止顺序
^^^^^^^^^^^^^^^^^^

媒体接口对停止顺序做了容错设计：源服务停止时应调用 ``esp_media_track_write_abort()``\ ，通过 ``ESP_MEDIA_PROVIDER_EVENT_TRACKS_ABORT`` 事件通知下游；接收端停止时应先置本地停止标志，再调用 :cpp:func:`esp_media_provider_abort` 唤醒阻塞的读取，等待任务退出、释放已获取的帧，最后才取消链接。若 track manager 被多个服务通过链接共享，需要先取消链接，再对它执行 reset 或 destroy；只要还有任务持有已获取的帧或阻塞在队列上，就不能对 track manager 执行 reset 或 destroy。

应用示例
----------

源/接收端的完整示例见 `esp_media_service 组件仓库 <https://components.espressif.com/components/espressif/esp_media_service>`__ 的 examples 目录。服务基类用法见 :doc:`/multimedia-services/service-infra/esp-service`。

FAQ
------

**Q1：一个服务只能实现 ``get_provider`` 或 ``set_provider`` 中的一个吗？**

不是必须的。角色为 ``ESP_MEDIA_ROLE_SRC_SINK`` 的服务可以同时实现两者，既作为下游的源，也作为上游的接收端；:cpp:func:`esp_media_service_link` 只按角色位校验所选的一对源/接收端服务是否兼容。

API 参考
----------

.. include-build-file:: inc/esp_media_service.inc

.. include-build-file:: inc/esp_media_provider.inc

.. include-build-file:: inc/esp_media_track_mngr.inc
