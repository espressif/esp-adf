ESP Playlist
============

:link_to_translation:`zh_CN:[中文]`

Introduction
------------

`ESP Playlist <https://components.espressif.com/components/espressif/esp_playlist>`__ is a media library and playlist management component for Espressif multimedia applications. It consists of two independent modules: a media library and a playlist. The media library records the name and location of each media file on the device. The playlist records playback order and the current position. The component provides directory scanning, media-library persistence, playlist JSON export and import, multi-instance management, and sequential, single-track repeat, list repeat, and shuffle modes. It is suited to local music players and spoken-content devices.

Feature List
------------

- Directory scanning: filters media files in a specified directory by recursion depth, file extension, and a custom callback, then adds them to the media library in batch
- Media library persistence: data is written to storage when using file system storage and can be reloaded after reboot; RAM storage is valid only during runtime
- Playlist construction: import from the media library with condition-based filtering, or load from a JSON file/memory buffer
- Playlist export: generates JSON that can be saved to a file, NVS, or delivered over the network
- Multi-instance management: multiple media library and playlist handles can be created simultaneously, each maintaining its own independent state
- Condition-based filtering: supports exact match, contains match, or prefix match on fields such as ``name``\ , \ ``url``\ , and \ ``id``\ , with AND/OR combination
- Playback modes: no repeat, single-track repeat, list repeat, and shuffle, switched via the current track navigation interface

Technical Deep Dive
--------------------

Media Library (esp_media_db)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The media library is a directory index of media files; it only stores ``name`` and ``url``\ , and does not parse the media content itself. When :cpp:func:`esp_media_db_init` creates a handle, the storage method is selected through the ``storage_type`` field of :cpp:type:`esp_media_db_cfg_t`: an :cpp:type:`esp_db_storage_type_t` value of ``ESP_DB_STORAGE_FS`` means the data is written to a three-file database on the file system pointed to by ``storage_path``\ , and can be reloaded after reboot with :cpp:func:`esp_media_db_load`; a value of ``ESP_DB_STORAGE_RAM`` means the data is kept only in heap memory, which suits scenarios that do not require persistence and are rebuilt frequently.

.. code:: c

    esp_media_db_cfg_t db_cfg = {
        .storage_type = ESP_DB_STORAGE_FS,
        .storage_path = "/sdcard/__playlist",
    };
    esp_media_db_handle_t media_db = NULL;
    esp_media_db_init(&db_cfg, &media_db);
    esp_media_db_load(media_db);  /* load first if a database file already exists under storage_path */

There are two ways to add files: :cpp:func:`esp_media_db_scan` recursively scans a directory according to ``path``\ , \ ``scan_depth``\ , and \ ``file_extensions`` in :cpp:type:`esp_media_db_scan_cfg_t`, with an optional ``filter_cb`` to perform a secondary check on each scanned URL; :cpp:func:`esp_media_db_add` directly writes a ``name`` / ``url`` entry already known to the caller. ``skip_duplicate`` applies only to ``scan``: entries are always added normally when the media library is empty; when the media library already contains entries, ``skip_duplicate = true`` means all entries are added without comparing URLs, while ``skip_duplicate = false`` means entries that already exist (matched by URL) are skipped. When performing an incremental scan on a media library that already has content, ``skip_duplicate = false`` is typically set to avoid duplicate entries. :cpp:func:`esp_media_db_add` always skips existing URLs and has no ``skip_duplicate`` parameter.

:cpp:func:`esp_media_db_remove` deletes a specified entry by URL; :cpp:func:`esp_media_db_clean` only clears the in-process view of the media library and does not delete the database file on the file system, so calling :cpp:func:`esp_media_db_load` again restores it when needed; :cpp:func:`esp_media_db_get_count` returns the current entry count.

Playlist (esp_playlist)
^^^^^^^^^^^^^^^^^^^^^^^

The playlist maintains an ordered sequence of entries and a current index in RAM, and :cpp:func:`esp_playlist_new` creates an empty instance. There are two independent paths for populating a playlist, with different behavior:

- :cpp:func:`esp_playlist_import_media` \ **appends**\  entries from a media library; when ``filter`` is ``NULL`` all entries are imported, and when an :cpp:type:`esp_media_filter_t` is provided, entries are filtered by condition. After importing, the playlist becomes bound to that media library handle, and a subsequent import from a different handle returns ``ESP_ERR_INVALID_STATE``\ . Such entries are stored inside the playlist only as references to the media library (DB entries), so navigation needs to look back at the media library to obtain ``name`` / ``url``
- :cpp:func:`esp_playlist_load` / :cpp:func:`esp_playlist_import_ram` \ **clear and replace**\  the current list from a JSON file or memory buffer, independent of the media library; the parsed entries carry their own ``name`` / ``url`` directly (inline entries), so navigation does not need to look back at the media library

.. code:: c

    esp_playlist_handle_t playlist = NULL;
    esp_playlist_new(&(esp_playlist_cfg_t) { .playlist_name = "default" }, &playlist);
    esp_playlist_import_media(playlist, media_db, NULL);  /* import all entries from the media library */

:cpp:func:`esp_playlist_save` / :cpp:func:`esp_playlist_export_ram` only export the playlist's own JSON (name, order, entries), without involving the media library file; both share the same JSON format as :cpp:func:`esp_playlist_load` / :cpp:func:`esp_playlist_import_ram` and can be read and written interchangeably. \ :cpp:func:`esp_playlist_export_ram` returns a buffer that must be released by the caller with ``free()``\ . :cpp:func:`esp_playlist_clean` clears the current list entries without deleting the JSON file on disk or the media library file.

Current Track and Repeat Mode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The playlist maintains a zero-based current index, pointing to entry 0 by default. :cpp:func:`esp_playlist_set_curr_index` jumps to a specified index; :cpp:func:`esp_playlist_curr`\ , :cpp:func:`esp_playlist_next`\ , :cpp:func:`esp_playlist_prev`\ , and :cpp:func:`esp_playlist_get_info` are used to read entry information and populate it into an :cpp:type:`esp_playlist_info_t` provided by the caller. Among these, ``get_info`` reads by a specified ``index`` and does not change the current index, while the other three either update or depend on the current index.

The boundary behavior of ``next`` / ``prev`` is determined by the :cpp:type:`esp_playlist_repeat_mode_t` set via :cpp:func:`esp_playlist_set_repeat_mode`\ :

.. list-table::
   :header-rows: 1
   :widths: 22 78

   * - Mode
     - Behavior
   * - ``ESP_PLAYLIST_REPEAT_NONE``
     - Returns ``ESP_ERR_NOT_FOUND`` upon reaching a list boundary; does not repeat
   * - ``ESP_PLAYLIST_REPEAT_ONE``
     - Stays at the current index, repeatedly reading the same entry
   * - ``ESP_PLAYLIST_REPEAT_ALL``
     - Wraps around to the other end of the list upon reaching a boundary
   * - ``ESP_PLAYLIST_REPEAT_SHUFFLE``
     - Randomly selects an index each time ``next`` / ``prev`` is called

When an entry is a DB entry but its bound media library has become invalid, the navigation interfaces return ``ESP_ERR_INVALID_STATE``\ ; when the list is empty or a non-repeating boundary is reached, they return ``ESP_ERR_NOT_FOUND``\ .

Filter (esp_media_filter_t)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

:cpp:type:`esp_media_filter_t` consists of a number of :cpp:type:`esp_media_filter_item_t` conditions, each specifying a field name (such as ``name``\ , \ ``url``\ , or \ ``id``\ ), an expected value, and an :cpp:type:`esp_media_match_type_t` matching method (exact match ``ESP_MEDIA_MATCH_EXACT``\ , contains match \ ``ESP_MEDIA_MATCH_CONTAINS``\ , prefix match \ ``ESP_MEDIA_MATCH_PREFIX``\ ), with ``match_all`` determining whether the conditions are combined with AND or OR. This filter is also used for import filtering in :cpp:func:`esp_playlist_import_media`.

.. code:: c

    esp_media_filter_item_t items[] = {
        { .key = "url", .expected = { .type = ESP_DB_FIELD_TYPE_STRING, .value.strv = ".mp3", .size = 5 },
          .match_type = ESP_MEDIA_MATCH_CONTAINS },
    };
    esp_media_filter_t filter = {
        .items = items,
        .item_count = 1,
        .match_all = true,
    };
    esp_playlist_import_media(playlist, media_db, &filter);

Performance
-----------

Measured average per-operation times for the `playlist_benchmark <https://github.com/espressif/esp-adf/tree/master/components/esp_playlist/examples/playlist_benchmark>`__ example on an ESP32-P4 Function EV Board (SD card, 1000 entries) are as follows; actual values vary with the SoC, SD card speed, and number of media items:

.. list-table::
   :header-rows: 1
   :widths: 40 30 30

   * - Operation
     - Scenario
     - Average Time
   * - ``esp_media_db_scan``\ (cold scan, no deduplication)
     - 1000 files
     - 664.71 us/item
   * - ``esp_media_db_load``
     - 1000 entries
     - 66.34 us/item
   * - ``esp_playlist_import_media``
     - 1000 entries
     - 176.76 us/item
   * - ``esp_playlist_next`` / ``prev``\ (DB entries)
     - Single call
     - 156.71 / 205.23 us
   * - ``esp_playlist_next`` / ``prev``\ (inline entries)
     - Single call
     - 2.53 / 2.49 us
   * - ``esp_playlist_save``
     - 1000 entries, 73045 bytes
     - 366.81 us/item
   * - ``esp_playlist_load``\ (JSON)
     - 1000 entries, 73045 bytes
     - 105.63 us/item

Navigating DB entries (imported via ``import_media``\ ) is about two orders of magnitude slower than navigating inline entries (imported via ``load`` / ``import_ram``\ ), because the former must look back at the media library for ``name`` / ``url`` on every call, while the latter's entries already carry complete information. Scenarios that switch tracks frequently should prefer the inline entry path.

Application Examples
---------------------

- `playlist_benchmark <https://github.com/espressif/esp-adf/tree/master/components/esp_playlist/examples/playlist_benchmark>`__ demonstrates the complete flow of media library scanning/loading, playlist import and navigation, and JSON saving/loading, and outputs the benchmarked timing of each interface

Typical scenarios and the corresponding interfaces are as follows:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Scenario
     - Recommended Flow
   * - Local music on SD card
     - Mount the SD card, scan the directory with \ ``esp_media_db_scan``\ , import with \ ``esp_playlist_import_media``\ , and save the playlist with \ ``esp_playlist_save``
   * - Fixed URL list on flash
     - Write fixed entries with ``esp_media_db_add`` and then import with ``esp_playlist_import_media``
   * - Restore on boot
     - Restore the media library with ``esp_media_db_load``\ , restore the playlist JSON with \ ``esp_playlist_load``
   * - NVS / network-delivered list
     - Obtain JSON with ``esp_playlist_export_ram`` and store it independently; restore with ``esp_playlist_import_ram``
   * - Filter by name or URL
     - Construct an :cpp:type:`esp_media_filter_t`\ , combined with matching methods such as \ ``ESP_MEDIA_MATCH_EXACT``\ , \ ``ESP_MEDIA_MATCH_CONTAINS``\ , \ ``ESP_MEDIA_MATCH_PREFIX``

FAQ
---

**Q1: What is the difference between the media library and the playlist JSON?**

The media library (a database file under ``storage_path`` when using file system storage) stores the set of ``name`` / ``url`` entries obtained through scanning or manual addition, recording which media files exist on the device. The playlist JSON only stores a single playlist's name, order, and entries, recording the order in which items are played; the two are different persistence objects.

**Q2: If only playlist.json is loaded, is the media library still needed?**

No. The JSON entries already contain ``name`` and ``url``\ , so after loading, the URL can be retrieved directly via ``curr`` / ``next`` / ``prev`` / ``get_info`` for playback. Such entries are inline entries and do not require looking back at the media library.

**Q3: Can entries be imported by album or artist?**

The current public interfaces do not parse metadata such as ``album`` or ``artist`` from the media file itself. If the media library entries already contain such extended fields, filtering can be achieved by matching the corresponding fields via :cpp:type:`esp_media_filter_t`.

API Reference
-------------

.. include-build-file:: inc/esp_playlist.inc

.. include-build-file:: inc/esp_media_db.inc

.. include-build-file:: inc/esp_media_db_types.inc
