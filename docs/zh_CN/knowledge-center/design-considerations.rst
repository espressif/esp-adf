开发经验
****************

:link_to_translation:`en:[English]`

采样率、声道数和分辨率决定单路媒体的数据量。同时启用的功能叠加后，内存和 CPU 占用上升。芯片与接口选型见 :doc:`project-design`。

内存
==========

内部 RAM 容量有限。默认任务栈、DMA 缓冲区和中断路径使用内部 RAM。大缓冲、编解码工作区和处理链（pipeline）上的数据块使用 PSRAM。

芯片或模组带 PSRAM 时，在 menuconfig 中启用 ``CONFIG_SPIRAM`` 。软件已启用 ``CONFIG_SPIRAM`` 但硬件未焊接 PSRAM 时，启动日志会打印 ``PSRAM ID read error`` 。未启用 ``CONFIG_SPIRAM`` 时，Wi-Fi、蓝牙与音频同时运行容易耗尽内部 RAM。

如何打印堆占用
-------------------------

GMF 在调用点打印当前空闲量。启用 ``CONFIG_SPIRAM`` 后打印 ``Total`` 、 ``Inter`` 、 ``Dram`` ： ``Total`` 是默认能力下的空闲量（含 PSRAM）， ``Inter`` 是内部 RAM， ``Dram`` 是内部 8-bit 可访问 RAM。

.. code:: c

    #include "esp_gmf_oal_mem.h"

    ESP_GMF_MEM_SHOW(TAG);

按能力分别打印内部 RAM 或 PSRAM：

.. code:: c

    #include "esp_heap_caps.h"

    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
    heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);

工程依赖 ``gmf_app_utils`` 后，调用 ``esp_gmf_app_cli_init`` 注册串口命令。随后在 CLI 中执行 ``free`` ，打印内部 RAM 与 PSRAM 的当前空闲量和历史最小值。

.. code:: c

    #include "esp_gmf_app_cli.h"

    esp_gmf_app_cli_init("Audio >", NULL);

历史最小值持续下降，说明存在泄漏或未归还的数据载体（payload）。当前空闲量在处理链启动后骤降、之后保持平稳，说明同时占用的缓冲过大：减小缓冲，或将大块分配到 PSRAM。

如何分配内部 RAM 与 PSRAM
------------------------------------

``malloc()`` 不指定内存类型，实际位置由堆策略决定。需要固定分配到 PSRAM 或内部 RAM 时，使用 ``heap_caps_malloc`` ，并通过能力标志选择区域。 ``MALLOC_CAP_SPIRAM`` 表示 PSRAM， ``MALLOC_CAP_INTERNAL`` 表示内部 RAM。

.. code:: c

    #include "esp_heap_caps.h"

    char *psram_buf = heap_caps_malloc(10 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    char *inner_buf = heap_caps_malloc(512, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

GMF 封装了两组分配接口：

* ``esp_gmf_oal_calloc`` ：启用 ``CONFIG_SPIRAM`` 后从 PSRAM 分配。
* ``esp_gmf_oal_calloc_inner`` ：优先内部 RAM，内部不足时回退到 PSRAM。

DMA 或中断路径必须使用内部 RAM 时，调用 ``heap_caps_malloc(..., MALLOC_CAP_INTERNAL)`` ，不要使用 ``esp_gmf_oal_calloc_inner`` 的回退结果。

从 PSRAM 发起 DMA 时，缓冲区地址和长度必须按 cache 对齐。调用 ``esp_gmf_oal_get_spiram_cache_align`` 取得对齐值，写入数据端口（port）的 ``buf_addr_aligned`` 和 ``buf_size_aligned`` 。对齐不正确时，画面出现花屏，音频出现毛刺。

将 Wi-Fi 与 LwIP 的缓冲分配到 PSRAM 时，启用 ``CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP`` 。

如何估算未压缩数据占用
-------------------------------------

先按格式计算一帧或一秒的原始数据量，再乘以处理链中同时缓存的块数。

PCM 每秒字节数 = 采样率 × 声道数 × 每采样字节数。16 bit 立体声 48 kHz 为 192000 字节/秒。处理链同时缓存 200 ms 时，仅 PCM 约 38 KB，还要计入声道转换、重采样等中间块。

未压缩图像一帧字节数：

* RGB565：宽 × 高 × 2
* NV12 / YUV420：宽 × 高 × 3 / 2

1280×720 的 NV12 单帧约 1.3 MB。摄像头、处理链和显示各持有若干帧时，内部 RAM 不足，将帧缓冲分配到 PSRAM。

任务栈的存储位置
----------------------------

默认任务栈位于内部 RAM。GMF 执行线程（task）默认栈 4 KB、优先级 5、绑定 CPU0，见 ``DEFAULT_ESP_GMF_TASK_CONFIG`` 。栈不足时增大 ``esp_gmf_task_cfg_t`` 的 ``thread.stack`` ，用 ``uxTaskGetStackHighWaterMark`` 查看余量。

``CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM`` 允许将栈分配到 PSRAM。GMF 对应字段是 ``thread.stack_in_ext`` 。须先阅读 IDF `片外 RAM <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/external-ram.html>`__ 中的限制。ESP32 修订版本低于 3 时，GMF 不会将栈分配到 PSRAM。

内存相关常见问题
----------------------------

* 未启用 ``CONFIG_SPIRAM`` 却同时运行 Wi-Fi、蓝牙和音频，内部堆分配失败。
* 大块 ``malloc()`` 占满内部 RAM，随后创建任务时栈空间不足。
* 处理链或 payload 未释放，空闲内存随播放时间下降。
* 软件启用了 ``CONFIG_SPIRAM`` ，模组未焊接 PSRAM，启动日志出现 ``PSRAM ID read error`` 。
* 栈溢出和堆耗尽的日志不同。先查看任务名和高水位，再查看内部 RAM 与 PSRAM 的空闲量。


CPU 占用与 Task WDT
==========================

Task WDT 默认监视各核的 IDLE 任务，超时默认 5 秒。某任务长时间占用所在核、不调用 ``vTaskDelay`` 或带超时的阻塞接口，该核 IDLE 无法喂狗，超时后复位。AFE 的 ``feed_task`` 向算法连续输入数据，是媒体工程中的典型占用任务。

先找出占用 CPU 的任务并缩短单次执行时间。不要将增大 ``CONFIG_ESP_TASK_WDT_TIMEOUT_S`` 或关闭 WDT 当作产品方案。调试阶段可以临时增大超时，测量完成后恢复默认值。

如何打印任务占用
----------------------------

``esp_gmf_oal_sys_get_real_time_stats`` 需要在 menuconfig 的 ``Component config`` → ``FreeRTOS`` → ``Kernel`` 中同时启用以下两项。缺少其中一项时，函数返回失败。

* ``CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID`` ：统计结果包含 CPU 核编号。
* ``CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS`` ：生成任务运行时间统计。

第一个参数是统计窗口，单位为毫秒。双核上每个核的占用合计约 50%。

.. code:: c

    #include "esp_gmf_oal_sys.h"

    esp_gmf_oal_sys_get_real_time_stats(1000, false);

打印示例：

.. code::

    I (6872) : ┌───────────────────┬──────────┬─────────────┬─────────┬──────────┬───────────┬────────────┬───────┐
    I (6888) : │ Task              │ Core ID  │ Run Time    │ CPU     │ Priority │ Stack HWM │ State      │ Stack │
    I (6905) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
    I (6927) : │ IDLE0             │ 0        │ 506963      │  25.26% │ 0        │ 720       │ Ready      │ Intr  │
    I (6940) : │ gmf_rec           │ 0        │ 492176      │  24.52% │ 5        │ 37072     │ Blocked    │ Extr  │
    I (6950) : │ sys_monitor       │ 0        │ 4469        │   0.22% │ 1        │ 3180      │ Running    │ Extr  │
    I (6963) : │ main              │ 0        │ 0           │   0.00% │ 1        │ 1368      │ Blocked    │ Intr  │
    I (6972) : │ ipc0              │ 0        │ 0           │   0.00% │ 24       │ 528       │ Suspended  │ Intr  │
    I (6984) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
    I (7011) : │ IDLE1             │ 1        │ 1004007     │  50.02% │ 0        │ 792       │ Ready      │ Intr  │
    I (7024) : │ ipc1              │ 1        │ 0           │   0.00% │ 24       │ 536       │ Suspended  │ Intr  │
    I (7034) : ├───────────────────┼──────────┼─────────────┼─────────┼──────────┼───────────┼────────────┼───────┤
    I (7062) : │ Tmr Svc           │ 7fffffff │ 0           │   0.00% │ 1        │ 1360      │ Blocked    │ Intr  │
    I (7072) : └───────────────────┴──────────┴─────────────┴─────────┴──────────┴───────────┴────────────┴───────┘

按列阅读：

* ``CPU`` ：该任务在统计窗口内的占用比例。数值高的任务是当前核上的主要负载。
* ``IDLE0`` / ``IDLE1`` ：对应核的空闲比例。IDLE 低，表示该核忙。
* ``Core ID`` ：任务绑定的 CPU 核。重负载集中在同一核时，将处理链绑定到空闲核。
* ``Stack HWM`` ：剩余栈空间的高水位。数值过低时增大 ``thread.stack`` 。

上表中 ``gmf_rec`` 占用 24.52%， ``IDLE0`` 为 25.26%。双核上每个核合计约 50%，录音处理链约占 CPU0 的一半，该核仍有一半空闲。 ``IDLE1`` 为 50.02%，CPU1 空闲。

``uxTaskGetSystemState`` 会挂起调度器，发布固件应关闭运行时间统计。IDF 用法见 `任务看门狗 <https://docs.espressif.com/projects/esp-techpedia/zh_CN/latest/esp-friends/advanced-development/system/watchdog.html>`__。

如何降低 CPU 占用
---------------------------

将 CPU 频率设置为该型号的最高主频。menuconfig 路径为 ``Component config`` → ``ESP System Settings`` → ``CPU frequency`` 。

.. list-table::
   :header-rows: 1
   :widths: 40 20

   * - 型号
     - 最高主频
   * - ESP32、ESP32-S2、ESP32-S3、ESP32-C5
     - 240 MHz
   * - ESP32-C3、ESP32-C6
     - 160 MHz
   * - ESP32-S31
     - 320 MHz
   * - ESP32-P4
     - 400 MHz

Flash 模式和频率在 ``Serial flasher config`` 中配置。模组支持时，优先选择 QIO、80 MHz。

ESP32-P4 使用硬件 PPA、JPEG 和 H.264。没有硬件路径时，降低采样率、声道数或算法强度。

不要在 I2S 或 GMF 回调中执行文件与网络 IO。媒体工程不要使用动态调频降低主频。若必须启用电源管理，媒体任务运行期间应持有 ``ESP_PM_CPU_FREQ_MAX`` 。发布固件将日志级别设置为 WARN，调用 ``esp_gmf_app_cli_init`` 后使用 CLI ``log`` 按 TAG 调整。

如何将处理链绑定到指定核
----------------------------------------

双核上将重负载处理单元划分到不同处理链，再绑定到不同 CPU 核，用数据端口相连。创建 GMF 执行线程时设置 ``esp_gmf_task_cfg_t`` 的 ``thread.core`` 和 ``thread.prio`` ，然后调用 ``esp_gmf_pipeline_bind_task`` 。默认绑定到 CPU0。

``gmf_ai_audio`` 的 ``aec_rec`` 例程将录音处理链绑定到 CPU1，播放处理链绑定到 CPU0。

ESP32-S2、ESP32-C3、ESP32-C5、ESP32-C6 是单核，无法将任务绑定到不同 CPU 核。单核上降低算法复杂度、提高实时任务优先级或减少并发。


回声消除与 AFE
=======================

回声消除占用的是运行它的任务时间。 ``esp_gmf_aec`` 的 ``filter_len`` 越大，滤波器越长，占用越高。 ``aec_rec`` 例程对 ESP32-S3 和 ESP32-P4 建议 ``filter_len = 4`` ，对 ESP32-C5 建议 ``filter_len = 2`` 。 ``AFE_MODE_LOW_POWER`` 占用低于 ``AFE_MODE_HIGH_PERF`` ，回声消除效果也更弱。

AFE 管理器分为 ``feed_task`` 输入数据和 ``fetch_task`` 获取结果。AFE 的输入数据为 16 bit PCM、16 kHz。默认 ``feed_task`` 在 CPU0、 ``fetch_task`` 在 CPU1，由 ``feed_task_setting.core`` 和 ``fetch_task_setting.core`` 配置。 ``feed_task`` 若长时间占用 CPU，IDLE 无法喂狗。双核上先确认这两个任务未绑定到同一核；单核上降低 ``filter_len`` ，或关闭不需要的 AFE 特性。


menuconfig 常用项
========================

.. list-table::
   :header-rows: 1
   :widths: 28 28 22 22

   * - 配置项
     - menuconfig 路径
     - 建议
     - 适用
   * - ``CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ``
     - ESP System Settings → CPU frequency
     - 该型号最高主频
     - 媒体工程
   * - ``CONFIG_ESPTOOLPY_FLASHMODE`` / ``CONFIG_ESPTOOLPY_FLASHFREQ``
     - Serial flasher config
     - QIO、80 MHz
     - 模组支持时
   * - ``CONFIG_SPIRAM``
     - SPI RAM
     - 启用
     - 带 PSRAM 的芯片或模组
   * - ``CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP``
     - SPI RAM
     - 启用
     - Wi-Fi 与 PSRAM 同时使用
   * - ``CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM``
     - FreeRTOS
     - 按需
     - 栈分配到 PSRAM 前先读 IDF 限制
   * - ``CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID``
       ``CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS``
     - FreeRTOS → Kernel
     - 同时启用
     - 查看 CPU 占用
   * - ``CONFIG_ESP_TASK_WDT_TIMEOUT_S``
     - ESP System Settings
     - 默认 5 秒
     - 不要通过增大超时掩盖占用
   * - 日志级别
     - Log output
     - 发布固件使用 WARN
     - 减少打印占用


缓冲、延迟与常见现象
==================================

处理链缓冲越大，抗抖动能力越强，延迟也越大。

* 回声消除占用过高：双核上将录音与播放划分到两条处理链，并绑定到不同 CPU 核。
* ``feed_task`` 触发 Task WDT：确认 ``feed_task`` 与 ``fetch_task`` 未绑定到同一核，或提高 ``feed_task`` 优先级。
* 算法无法及时处理新的音频帧：用 ``esp_gmf_oal_sys_get_real_time_stats`` 查看哪个任务占用 CPU。
* 堆分配失败：分别查看内部 RAM 与 PSRAM 的空闲量。内部 RAM 不足时减小内部缓冲。未启用 PSRAM 时启用 ``CONFIG_SPIRAM`` 。
