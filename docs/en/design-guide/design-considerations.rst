Design Considerations
*********************

Depending on the audio data format, that may be lossless, lossy or compressed, e.g. WAV, MP3 or FLAC and the quality expressed in sampling rate and bitrate, the project will require different resources: memory, storage space, input / output throughput and the processing power. The resources will also depend on the project type and features discussed in :doc:`project-design`.

This section describes capacity and performance of ESP32 system resources that should be considered when designing an audio project to meet required data format, audio quality and functionality.

Memory
======

The spare internal Data-RAM is about 290kB with "hello_world" example. For audio system this may be insufficient, and therefore the ESP32 incorporates the ability to use up to 4MB of external SPI RAM (i.e. PSRAM) memory. The external memory is incorporated in the memory map and is, within certain restrictions, usable in the same way internal Data-RAM is.  

Refer to `External SPI-connected RAM <http://esp-idf.readthedocs.io/en/latest/api-guides/external-ram.html>`_ section in IDF documenation for details, especially pay attention to its `Restrictions <https://esp-idf.readthedocs.io/en/latest/api-guides/external-ram.html#restrictions>`_ section which is very important.

To be able to use the PSRAM, if installed on your board, it should be enabled in menucofig under *Component config > ESP32-specific > SPI RAM config*. The option *CONFIG_SPIRAM_CACHE_WORKAROUND*, set by default in the same menu, should be kept enabled.

.. note::

    Bluetooth and Wi-Fi can not coexist without PSRAM because it will not leave enough memory for an audio application.


Optimization of Internal RAM and Use of PSRAM
---------------------------------------------

Internal RAM is more valuable asset since there are some restrictions on PSRAM. Here are some tips for optimizing internal RAM.

* If PSRAM is in use, set all the static buffer to minimum value in *Component config > Wi-Fi*; if PSRAM is not used then dynamic buffer should be selected to save memory. Refer to `Wi-Fi Buffer Usage <http://esp-idf.readthedocs.io/en/latest/api-guides/wifi.html#wi-fi-buffer-usage>`_ section in IDF documentation for details.

* If PSRAM and BT are used, then *CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST* and *CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY* should be set as "yes" under *Component config > Bluetooth > Bluedroid Enable*, to allocate more of 40kB memory to PSRAM

* If PSRAM and Wi-Fi are used, then *CONFIG_WIFI_LWIP_ALLOCATION_FROM_SPIRAM_FIRST* should be set as "yes" under *Component config > ESP32-specific > SPI RAM config*, to allocate some memory to PSRAM

* Set *CONFIG_WL_SECTOR_SIZE* as 512 in *Component config > Wear Levelling*

.. note::

    The smaller the size of sector be, the slower the Write / Read speed will be, and vice versa, but only 512 and 4096 are supported.

* Call ``char *buf = heap_caps_malloc(1024 * 10, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`` instead of ``malloc(1024 * 10)`` to use PSRAM, and call ``char *buf = heap_caps_malloc(512, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`` to use internal RAM.  

* Not relying on ``malloc()`` to automatically allocate PSRAM allows to make a full control of the memory. By avoiding the use of the internal RAM by other ``malloc()`` calls, you can reserve more memory for high-efficiency usage and task stack since PSRAM cannot be used as task stack memory.

* The task stack will always be allocated at internal RAM. On the other hand you can use of the ``xTaskCreateStatic()`` function that allows to create tasks with stack on PSRAM (see options in PSRAM and FreeRTOS menuconfig), but pay attention to its help information.

.. important::

    Don't use ROM code in ``xTaskCreateStatic`` task： The ROM code itself is linked in ``components/esp32/ld/esp32.rom.ld``. However, you also need to consider other pieces of code that *call* ROM functions, as well as the code that is not recompiled against the *CONFIG_SPIRAM_CACHE_WORKAROUND* patch, like the Wi-Fi and Bluetooth libraries. In general, we advise using this only in threads that do not call any IDF libraries (including libc), doing only calculations and using FreeRTOS primitives to talk to other threads.


Memory Usage by Component Overview
----------------------------------

Below is a table that contains ESP-ADF components and their memory usage. Choose the components needed and find out how much internal RAM is left. The table is divided into two parts, when PSRAM is used or not. If PSRAM (external RAM) is in use, then some of the memory will be allocated at PSRAM automatically.

The initial spare internal RAM is 290kB.

+---------------------+---------------------------------------------------------------------------+
|                     | Internal RAM Required                                                     |
|                     +-------------------------------------+-------------------------------------+
| Component           | PSRAM not used                      | With PSRAM                          |
+=====================+=====================================+=====================================+
| Wi-Fi :sup:`1`      | 50kB+                               | 50kB+                               |
+---------------------+-------------------------------------+-------------------------------------+
| Bluetooth           | 140kB (50kB if only BLE needed)     | 95kB (50kB if only BLE needed)      |
+---------------------+-------------------------------------+-------------------------------------+
| Flash Card :sup:`2` | 12kB+                               | 12kB+                               |
+---------------------+-------------------------------------+-------------------------------------+
| I2S :sup:`3`        | Configurable, 8kB for reference     | Configurable, 8kB for reference     |
+---------------------+-------------------------------------+-------------------------------------+
| RingBuffer :sup:`4` | Configurable, 30kB for reference    | 0kB, all moved into PSRAM           |
+---------------------+-------------------------------------+-------------------------------------+

**Notes to the table above**

1. According to the Wi-Fi menuconfig each Tx and Rx buffer occupies 1.6kB internal RAM. The value of 50kB RAM is assuming use of 5 Rx static buffers and 6 Tx static buffers. If PSRAM is not in use, then the "Type of WiFi Tx Buffer" option should be set as *DYNAMIC* in order to save RAM, in this case, the RAM usage will be far less than 50kB, but programmer should keep at least 50kB available for the Wi-Fi to be able to transmit the data. **[Internal RAM only]**

2. Depending on value of *SD_CARD_OPEN_FILE_NUM_MAX* in :component_file:`audio_hal/board/board.h`, that is then used in ``sd_card_mount()`` function, the RAM needed will increase with a greater number of maximum open files. 12kB is the RAM needed with 5 max files and 512 bytes *CONFIG_WL_SECTOR_SIZE*. **[Internal RAM only]**

3. Depending on configuration settings of the I2S stream, refer to :component_file:`audio_stream/include/i2s_stream.h` and :component_file:`audio_stream/i2s_stream.c`. **[Internal RAM only]**

4. Depending on configuration setting of the Ringbuffer, refer to *DEFAULT_PIPELINE_RINGBUF_SIZE* in :component_file:`audio_pipeline/include/audio_pipeline.h` or user setting, if the buffer is created with e.g. :cpp:func:`rb_create`.


System Settings
===============

The following settings are recommended to achieve a high Wi-Fi performance in an audio project.

.. note::

    Use ESP32 modules and boards from reputable vendors that put attention to product design, component selection and product testing. This is to have confidence of receiving well designed boards with calibrated RF.

* Set these following options in menuconfig.

    * Flash SPI mode as QIO
    * Flash SPI speed as 80MHz
    * CPU frequency as 240MHz
    * Set *Default receive window size* as 5 times greater than *Maximum Segment Size* in *Component config > LWIP > TCP* 

* If external antenna is used, then set *PHY_RF_CAL_PARTIAL* as *PHY_RF_CAL_FULL* in ''esp-idf/components/esp32/phy_init.c''

