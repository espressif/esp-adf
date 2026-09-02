ESP Board Manager
=================

:link_to_translation:`zh_CN:[中文]`

ESP Board Manager (BMGR) is the infrastructure component for the Espressif board ecosystem. It establishes a standardized path from board hardware to application software on Espressif chip platforms. BMGR describes hardware peripherals and functional devices in declarative YAML, and a code generator produces standardized initialization code based on this description, providing a unified runtime interface to the application layer for device management. Boards adapted with BMGR can be used directly in any BMGR project, and can also be shared and reused within the community.

Board definition packages:

.. list-table::
   :header-rows: 1
   :widths: 24 46 30

   * - Component
     - Description
     - Related links
   * - ``esp_boards``
     - Official Espressif board definitions
     - `GitHub <https://github.com/espressif/esp-board-manager/tree/main/esp_boards>`__
   * - ``m5stack_boards``
     - M5Stack board definitions
     - `GitHub <https://github.com/espressif/esp-board-manager/tree/main/m5stack_boards>`__
   * - ``esp_friends_boards``
     - Community board definitions
     - `GitHub <https://github.com/espressif/esp-board-manager/tree/main/esp_friends_boards>`__

The WebRTC solution has a separate board-level peripheral abstraction, maintained independently of Board Manager. See `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/codec_board>`__.

Related links:

- `Component Registry <https://components.espressif.com/components/espressif/esp_board_manager>`__
- `GitHub Repository <https://github.com/espressif/esp-board-manager>`__
- `Online Documentation <https://docs.espressif.com/projects/esp-board-manager/en/latest/index.html>`__
