播放列表
============

:link_to_translation:`en:[English]`

播放列表是可以按顺序或按指定顺序播放的音频文件列表。

使用 ``sdcard_scan.h`` 中的 ``sdcard_scan`` 函数扫描SD卡中音频文件，支持指定文件深度扫描并过滤文件类型。播放列表实例可以存放于多种存储介质，以下为支持的存储介质：

* SD卡_
* DRAM_
* NVS_Flash_
* Flash_Partition_


播放列表使用 ``playlist_operator_handle_t`` 句柄调用对应的操作函数，在大多数存储介质中支持创建、保存、打印playlist和获取音频序号对应的路径等功能。

有关 API 的详细信息，请参阅下文。


.. _扫描SD卡:

扫描SD卡
----------------

扫描SD卡功能提供 ``sdcard_scan`` 函数，用于扫描指定路径下的音频文件，支持文件类型过滤，利用回调函数将播放列表保存到指定的存储介质。

应用示例
^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_sdcard_mp3_control`
- :example:`cli`


.. include:: /_build/inc/sdcard_scan.inc


.. _SD卡:

播放列表（SD卡）
--------------------

播放列表（SD卡）将播放列表保存到SD卡中，并通过 ``playlist_operator_handle_t`` 句柄调用操作函数。

应用示例
^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_sdcard_mp3_control`
- :example:`cli`

.. include:: /_build/inc/sdcard_list.inc


.. _DRAM:

播放列表（DRAM）
--------------------

播放列表（DRAM）将播放列表保存到DRAM中，并通过 ``playlist_operator_handle_t`` 句柄调用操作函数。

.. include:: /_build/inc/dram_list.inc


.. _NVS_Flash:

播放列表（Flash）
----------------------

播放列表（Flash）将播放列表保存到NVS Flash中，并通过 ``playlist_operator_handle_t`` 句柄调用操作函数。

.. include:: /_build/inc/flash_list.inc


.. _Flash_Partition:

播放列表（Partition）
------------------------

播放列表（Partition）将播放列表保存到Flash Partition分区中，并通过 ``playlist_operator_handle_t`` 句柄调用操作函数。需要先将子类型为0x06和0x07的2个分区添加到分区表中。

.. include:: /_build/inc/partition_list.inc


.. _播放列表管理器（playlist）:

播放列表管理器（playlist）
------------------------------

播放列表管理器（playlist）用于管理上述播放列表，可将多个播放列表实例添加到 ``playlist_handle_t`` 句柄。

.. include:: /_build/inc/playlist.inc

