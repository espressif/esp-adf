Playlist
===========

:link_to_translation:`zh_CN:[中文]`

A playlist is a list of audio files that can be played back either sequentially or in a specified order.

The :cpp:func:`sdcard_scan` function in the :component_file:`playlist/include/sdcard_scan.h` scans the audio files in a microSD card and generate a playlist of files. You can specify file depth and filter out file types when scanning. The playlist instances can be saved to a variety of storage media. The following are the supported storage media:

* `Saving to MicroSD Card`_
* `Saving to DRAM`_
* `Saving to NVS Partition in Flash`_
* `Saving to DATA_UNDEFINED Partition in Flash`_

After scanning the audio files, you can use the ``playlist_operator_handle_t`` handle to call the corresponding functions to create, save, print playlists, and obtain the path corresponding to the audio ID. Currently, most of the storage media mentioned in this document support the above functions. 

See the description below for the API details.


Scanning MicroSD Card
----------------------

The :cpp:func:`sdcard_scan` function can scan audio files in a specified path and generate playlists. It supports the scanning of files at a specified depth and filtering of file types. Then, the playlist can be saved to the specified storage medium using a callback function.

Application Example
^^^^^^^^^^^^^^^^^^^^^^^^^

- :example:`player/pipeline_sdcard_mp3_control`
- :example:`cli`


.. include:: /_build/inc/sdcard_scan.inc


Saving Playlist
--------------------

Saving to MicroSD Card
^^^^^^^^^^^^^^^^^^^^^^

The playlist can be stored in the microSD card. Functions, such as those to save and display the playlist, can be called through the ``playlist_operator_handle_t`` handle.


Application Example
"""""""""""""""""""

- :example:`player/pipeline_sdcard_mp3_control`
- :example:`cli`

.. include:: /_build/inc/sdcard_list.inc


Saving to DRAM
^^^^^^^^^^^^^^

The playlist can be stored in DRAM. Functions, such as those to save and display the playlist, can be called through the ``playlist_operator_handle_t`` handle.

.. include:: /_build/inc/dram_list.inc


Saving to NVS Partition in Flash
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The playlist can be stored in the `NVS partition <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html>`_ in flash. Functions, such as those to save and display the playlist, can be called through the ``playlist_operator_handle_t`` handle.

.. include:: /_build/inc/flash_list.inc


Saving to ``DATA_UNDEFINED`` Partition in Flash
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The playlist can be stored in the ``DATA_UNDEFINED`` partition (see `Partition Tables <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html>`_ for details) in flash. Functions, such as those to save and display the playlist, can be called through the ``playlist_operator_handle_t`` handle. Please add the two partitions whose subtypes are 0x06 and 0x07 to the flash partition table first.

.. include:: /_build/inc/partition_list.inc


Playlist Manager
------------------------------

Playlist Manager manages the above playlists and can add multiple playlist instances to the ``playlist_handle_t`` handle.

.. include:: /_build/inc/playlist.inc