Playlist
===========

:link_to_translation:`zh_CN:[中文]`

Playlist is a list of audio files that can be played back either sequentially or in a specified order.

Use the ``sdcard_scan`` function in the ``sdcard_scan.h`` file to scan the audio files in the SD card, supporting file scanning with specified depth and filtering file types. Playlist instances can be stored on a variety of storage media, the following are supported storage media:

* SDcard_
* DRAM_
* NVS_Flash_
* Flash_Partition_

Playlist use ``playlist_operator_handle_t`` handle to call corresponding operation functions and support functions such as creating, saving, printing playlists and getting paths for audio sequence numbers on most storage media.

See description below for the API details.


.. _sdcard_scan:

SDcard Scan
----------------

SDcard Scan provides ``sdcard_scan`` function to scan audio files on a specified path, supports file type filtering, and uses callback function to save playlist to a specified storage media.

Application Example
^^^^^^^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_sdcard_mp3_control`
- :example:`cli`


.. include:: /_build/inc/sdcard_scan.inc


.. _SDcard:

Playlist (SDcard)
--------------------

Playlist(SDcard) save playlists to SDcard and use ``playlist_operator_handle_t`` handle to call the operation function.

Application Example
^^^^^^^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_sdcard_mp3_control`
- :example:`cli`


.. include:: /_build/inc/sdcard_list.inc


.. _DRAM:

Playlist (DRAM)
--------------------

Playlist(DRAM) save playlists to DRAM and use ``playlist_operator_handle_t`` handle to call the operation function.

.. include:: /_build/inc/dram_list.inc


.. _NVS_Flash:

Playlist (Flash)
--------------------

Playlist(Flash) save playlists to NVS Flash and use ``playlist_operator_handle_t`` handle to call the operation function.

.. include:: /_build/inc/flash_list.inc


.. _Flash_Partition:

Playlist (Partition)
--------------------

Playlist(Partition) save playlists to Flash Partition and use ``playlist_operator_handle_t`` handle to call the operation function. Please add 2 partitions to partition table whose subtype are 0x06 and 0x07 first.

.. include:: /_build/inc/partition_list.inc


.. _playlist_manager:

Playlist Manager
------------------------------

Playlist Manager manages the above playlists by adding multiple playlist instances to ``playlist_handle_t`` handle.

.. include:: /_build/inc/playlist.inc



