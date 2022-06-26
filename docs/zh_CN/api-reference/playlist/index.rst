播放列表
============

:link_to_translation:`en:[English]`

播放列表是可以按顺序或按指定顺序播放的音频文件列表。

:component_file:`playlist/include/sdcard_scan.h` 中的 :cpp:func:`sdcard_scan` 函数可扫描 microSD 卡中的音频文件，并且生成播放列表。支持指定文件深度扫描和过滤文件类型，播放列表实例可以存放于多种存储介质，以下为支持的存储介质：

* `存储至 microSD 卡`_
* `存储至 DRAM`_
* `存储至 flash 的 NVS 分区`_
* `存储至 flash 的 DATA_UNDEFINED 分区`_

扫描音频文件后，可使用 ``playlist_operator_handle_t`` 句柄调用对应的函数来实现创建、保存、打印播放列表以及获取音频序号对应的路径等功能。目前，本文中提到的大多数存储介质支持上述功能。

有关 API 的详细信息，请参阅下文。


扫描 microSD 卡
----------------

:cpp:func:`sdcard_scan` 函数可扫描指定路径下的音频文件并生成播放列表，支持指定文件深度扫描和过滤文件类型。然后，可利用回调函数将播放列表保存到指定的存储介质。

应用示例
^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_sdcard_mp3_control`
- :example:`cli`


.. include:: /_build/inc/sdcard_scan.inc


存储播放列表
--------------------------

存储至 microSD 卡
^^^^^^^^^^^^^^^^^^^

播放列表可存储至 microSD 卡中，并通过 ``playlist_operator_handle_t`` 句柄调用相应函数来实现保存、显示播放列表等功能。


应用示例
""""""""

- :example:`player/pipeline_sdcard_mp3_control`
- :example:`cli`

.. include:: /_build/inc/sdcard_list.inc


存储至 DRAM
^^^^^^^^^^^^

播放列表可存储至 DRAM 中，并通过 ``playlist_operator_handle_t`` 句柄调用相应函数来实现保存、显示播放列表等功能。

.. include:: /_build/inc/dram_list.inc


存储至 flash 的 NVS 分区
^^^^^^^^^^^^^^^^^^^^^^^^^

播放列表可存储至 flash 的 `NVS 分区 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/storage/nvs_flash.html>`_ 中，并通过 ``playlist_operator_handle_t`` 句柄调用相应函数来实现保存、显示播放列表等功能。

.. include:: /_build/inc/flash_list.inc


存储至 flash 的 ``DATA_UNDEFINED`` 分区
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

播放列表可存储至 flash 的 ``DATA_UNDEFINED`` 分区中（详情请参考 `分区表 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-guides/partition-tables.html>`_），并通过 ``playlist_operator_handle_t`` 句柄调用相应函数来实现保存、显示播放列表等功能。需要先将子类型为 0x06 和 0x07 的 2 个分区添加到 flash 的分区表中。

.. include:: /_build/inc/partition_list.inc


播放列表管理器
------------------------------

播放列表管理器用于管理上述播放列表，可将多个播放列表实例添加到 ``playlist_handle_t`` 句柄。

.. include:: /_build/inc/playlist.inc