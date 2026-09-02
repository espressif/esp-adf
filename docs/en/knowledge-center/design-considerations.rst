Development Notes
*****************

:link_to_translation:`zh_CN:[中文]`

Sample rate, channel count, and resolution determine the data size of one media stream. Memory and CPU use rise as more features run at the same time. Chip and I/O selection are in :doc:`project-design`.

Memory
======

Internal RAM is limited. The default task stack, DMA buffers, and interrupt paths use internal RAM. Large buffers, codec working sets, and pipeline data blocks use PSRAM.

If the chip or module has PSRAM, enable ``CONFIG_SPIRAM`` in menuconfig. If ``CONFIG_SPIRAM`` is enabled but the hardware has no PSRAM, the boot log prints ``PSRAM ID read error``. If ``CONFIG_SPIRAM`` is not enabled, Wi-Fi, Bluetooth, and audio together often exhaust internal RAM.

How to Print Heap Use
---------------------

GMF prints the current free size at the call site. After ``CONFIG_SPIRAM`` is enabled it prints ``Total``, ``Inter``, and ``Dram``. ``Total`` is free memory under the default capability (including PSRAM). ``Inter`` is internal RAM. ``Dram`` is internal 8-bit-accessible RAM.

.. code:: c

    #include "esp_gmf_oal_mem.h"

    ESP_GMF_MEM_SHOW(TAG);

Print internal RAM or PSRAM by capability:

.. code:: c

    #include "esp_heap_caps.h"

    heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
    heap_caps_print_heap_info(MALLOC_CAP_SPIRAM);

After the project depends on ``gmf_app_utils``, call ``esp_gmf_app_cli_init`` to register the serial commands. Then run ``free`` in the CLI to print the current free internal RAM and PSRAM, plus the historical minimum.

.. code:: c

    #include "esp_gmf_app_cli.h"

    esp_gmf_app_cli_init("Audio >", NULL);

A falling minimum means a leak or an unreleased payload. A sharp drop when the pipeline starts, then a flat line, means the buffers in use are too large: reduce them, or allocate large blocks to PSRAM.

How to Allocate Internal RAM and PSRAM
--------------------------------------

``malloc()`` does not select a memory type. The heap policy decides the location. To allocate from PSRAM or internal RAM explicitly, call ``heap_caps_malloc`` with a capability flag. ``MALLOC_CAP_SPIRAM`` selects PSRAM. ``MALLOC_CAP_INTERNAL`` selects internal RAM.

.. code:: c

    #include "esp_heap_caps.h"

    char *psram_buf = heap_caps_malloc(10 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    char *inner_buf = heap_caps_malloc(512, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

GMF provides two allocators:

* ``esp_gmf_oal_calloc``: allocates from PSRAM after ``CONFIG_SPIRAM`` is enabled.
* ``esp_gmf_oal_calloc_inner``: prefers internal RAM and falls back to PSRAM if internal RAM is short.

If a DMA or interrupt path must stay in internal RAM, call ``heap_caps_malloc(..., MALLOC_CAP_INTERNAL)``. Do not use the fallback from ``esp_gmf_oal_calloc_inner``.

When DMA starts from PSRAM, the buffer address and length must be cache-aligned. Call ``esp_gmf_oal_get_spiram_cache_align`` to read the alignment, then write it to the port fields ``buf_addr_aligned`` and ``buf_size_aligned``. Incorrect alignment produces a torn display or audio glitches.

To allocate Wi-Fi and LwIP buffers to PSRAM, enable ``CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP``.

How to Estimate Uncompressed Data Size
--------------------------------------

Compute the raw size of one frame or one second, then multiply by the number of blocks the pipeline caches at the same time.

PCM bytes per second = sample rate × channels × bytes per sample. 16-bit stereo at 48 kHz is 192000 bytes/s. A 200 ms pipeline cache is already about 38 KB of PCM, plus channel conversion and resampler intermediates.

Uncompressed image bytes per frame:

* RGB565: width × height × 2
* NV12 / YUV420: width × height × 3 / 2

One 1280×720 NV12 frame is about 1.3 MB. When the camera, pipeline, and display each hold several frames, internal RAM is not enough. Allocate the frame buffers to PSRAM.

Task Stack Location
-------------------

The default task stack is in internal RAM. A GMF task defaults to a 4 KB stack, priority 5, and CPU0. See ``DEFAULT_ESP_GMF_TASK_CONFIG``. If the stack is too small, raise ``thread.stack`` in ``esp_gmf_task_cfg_t`` and check headroom with ``uxTaskGetStackHighWaterMark``.

``CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM`` allows a stack in PSRAM. The GMF field is ``thread.stack_in_ext``. Read the limits in the IDF `External RAM <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/external-ram.html>`__ guide first. On ESP32 revisions below 3, GMF will not place a stack in PSRAM.

Common Memory Issues
--------------------

* ``CONFIG_SPIRAM`` is not enabled, Wi-Fi, Bluetooth, and audio run together, and internal heap allocation fails.
* A large ``malloc()`` fills internal RAM, then task creation fails for lack of stack space.
* A pipeline or payload is not released, and free memory drops over time.
* ``CONFIG_SPIRAM`` is enabled on a module that has no PSRAM, and the boot log prints ``PSRAM ID read error``.
* Stack overflow and heap exhaustion produce different logs. Check the task name and high-water mark first, then free internal RAM and PSRAM.


CPU Use and the Task WDT
========================

The task watchdog watches the IDLE task of each core by default. The default timeout is 5 seconds. If a task occupies a core for a long time and does not call ``vTaskDelay`` or a blocking API with a timeout, that core's IDLE cannot feed the watchdog and the chip resets. The AFE ``feed_task`` inputs samples to the algorithm and is a typical busy task in media projects.

Find the busy task and shorten each run. Do not treat a larger ``CONFIG_ESP_TASK_WDT_TIMEOUT_S`` or a disabled WDT as a product fix. A longer timeout is acceptable only while debugging, then restore the default.

How to Print Task CPU Use
-------------------------

``esp_gmf_oal_sys_get_real_time_stats`` requires both of the following options under ``Component config`` → ``FreeRTOS`` → ``Kernel`` in menuconfig. If either is missing, the function returns failure.

* ``CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID``: include the CPU core ID in the statistics.
* ``CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS``: generate task run-time statistics.

The first argument is the measurement window in milliseconds. On dual-core chips each core accounts for about 50% of the total.

.. code:: c

    #include "esp_gmf_oal_sys.h"

    esp_gmf_oal_sys_get_real_time_stats(1000, false);

Print example:

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

Read the columns as follows:

* ``CPU``: the task share in the measurement window. A high value is the main load on that core.
* ``IDLE0`` / ``IDLE1``: idle time on that core. A low IDLE value means the core is busy.
* ``Core ID``: the core the task is pinned to. If heavy work sits on one core, pin the pipeline to the idle core.
* ``Stack HWM``: remaining stack high-water mark. Raise ``thread.stack`` if the value is too low.

In this table ``gmf_rec`` uses 24.52% and ``IDLE0`` is 25.26%. On a dual-core chip each core accounts for about 50%, so the record pipeline uses about half of CPU0 and the other half is idle. ``IDLE1`` is 50.02%, so CPU1 is idle.

``uxTaskGetSystemState`` suspends the scheduler. Disable the runtime statistics in release firmware. For the IDF usage, see `Task watchdog <https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/advanced-development/system/watchdog.html>`__.

How to Reduce CPU Load
----------------------

Set the CPU frequency to the maximum for that chip. The menuconfig path is ``Component config`` → ``ESP System Settings`` → ``CPU frequency``.

.. list-table::
   :header-rows: 1
   :widths: 40 20

   * - Chip
     - Maximum CPU frequency
   * - ESP32, ESP32-S2, ESP32-S3, ESP32-C5
     - 240 MHz
   * - ESP32-C3, ESP32-C6
     - 160 MHz
   * - ESP32-S31
     - 320 MHz
   * - ESP32-P4
     - 400 MHz

Configure the flash mode and frequency under ``Serial flasher config``. Prefer QIO at 80 MHz when the module supports it.

ESP32-P4 uses hardware PPA, JPEG, and H.264. Without a hardware path, lower the sample rate, channel count, or algorithm cost.

Do not perform file or network I/O in an I2S or GMF callback. Do not let dynamic frequency scaling lower the CPU clock in a media project. If power management must stay on, hold ``ESP_PM_CPU_FREQ_MAX`` while the media task runs. Set the log level to WARN in release firmware. After ``esp_gmf_app_cli_init``, use the CLI ``log`` command to change the level by TAG.

How to Pin a Pipeline to a Core
-------------------------------

On dual-core chips, split heavy elements into separate pipelines, pin them to different cores, and connect them with a port. Set ``thread.core`` and ``thread.prio`` in ``esp_gmf_task_cfg_t`` when the GMF task is created, then call ``esp_gmf_pipeline_bind_task``. The default core is CPU0.

The ``aec_rec`` example in ``gmf_ai_audio`` pins the record pipeline to CPU1 and the play pipeline to CPU0.

ESP32-S2, ESP32-C3, ESP32-C5, and ESP32-C6 are single-core and cannot pin tasks to different cores. Lower algorithm cost, raise the real-time task priority, or reduce concurrency.


Echo Cancellation and AFE
=========================

Echo cancellation uses the CPU time of the task that runs it. A larger ``filter_len`` in ``esp_gmf_aec`` makes a longer filter and a higher load. The ``aec_rec`` example suggests ``filter_len = 4`` on ESP32-S3 and ESP32-P4, and ``filter_len = 2`` on ESP32-C5. ``AFE_MODE_LOW_POWER`` costs less than ``AFE_MODE_HIGH_PERF`` and also weakens echo cancellation.

The AFE manager splits work into ``feed_task`` (input) and ``fetch_task`` (results). AFE input data is 16-bit PCM at 16 kHz. By default ``feed_task`` runs on CPU0 and ``fetch_task`` on CPU1, set by ``feed_task_setting.core`` and ``fetch_task_setting.core``. If ``feed_task`` occupies a core for too long, IDLE cannot feed the task WDT. On dual-core chips, first confirm the two tasks are not pinned to the same core. On a single core, lower ``filter_len`` or disable unused AFE features.


Common menuconfig Options
=========================

.. list-table::
   :header-rows: 1
   :widths: 28 28 22 22

   * - Option
     - menuconfig path
     - Suggestion
     - When
   * - ``CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ``
     - ESP System Settings → CPU frequency
     - Maximum for that chip
     - Media projects
   * - ``CONFIG_ESPTOOLPY_FLASHMODE`` / ``CONFIG_ESPTOOLPY_FLASHFREQ``
     - Serial flasher config
     - QIO, 80 MHz
     - When the module supports it
   * - ``CONFIG_SPIRAM``
     - SPI RAM
     - Enable
     - Chips or modules with PSRAM
   * - ``CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP``
     - SPI RAM
     - Enable
     - Wi-Fi and PSRAM together
   * - ``CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM``
     - FreeRTOS
     - Optional
     - Read the IDF limits before placing a stack in PSRAM
   * - ``CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID``
       ``CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS``
     - FreeRTOS → Kernel
     - Enable both
     - Inspect CPU use
   * - ``CONFIG_ESP_TASK_WDT_TIMEOUT_S``
     - ESP System Settings
     - 5 seconds by default
     - Do not hide a busy core by raising the timeout
   * - Log level
     - Log output
     - WARN for release
     - Less log overhead


Buffers, Latency, and Common Symptoms
=====================================

A larger pipeline buffer resists jitter better and adds latency.

* Echo cancellation load is too high: on dual-core chips, split record and playback into two pipelines and pin them to different cores.
* ``feed_task`` triggers the task WDT: confirm ``feed_task`` and ``fetch_task`` are not on the same core, or raise the ``feed_task`` priority.
* The algorithm cannot process new audio frames in time: use ``esp_gmf_oal_sys_get_real_time_stats`` to see which task occupies the core.
* Heap allocation fails: check free internal RAM and free PSRAM separately. Reduce internal buffers when internal RAM is short. Enable ``CONFIG_SPIRAM`` when PSRAM is not enabled.
