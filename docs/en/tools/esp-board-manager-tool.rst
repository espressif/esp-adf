ESP Board Manager Web Configuration Tool
==================================================

:link_to_translation:`zh_CN:[中文]`

ESP Board Manager (BMGR) is a foundational component of the Espressif development board ecosystem, used to standardize hardware adaptation across boards. Boards adapted with BMGR can be used directly in any BMGR project, and board-level configurations and project templates can be shared and reused.

To simplify the board adaptation process, BMGR provides a companion web configuration tool. The ESP Board Manager web configuration tool supports creating new board configurations, importing existing configurations, editing device and peripheral parameters, and exporting board-level files.

Links
----------------

- Web configuration tool: `ESP Board Manager <https://board-manager.espressif.com/>`__
- Online documentation: `ESP Board Manager Online Documentation <https://docs.espressif.com/projects/esp-board-manager/en/latest/create-board/web-create.html>`__

Key Features
----------------

- **Create new board configuration**: Fill in the board name, manufacturer, description, target ESP-IDF version and main chip, then select the required devices and peripherals.
- **Import existing configuration**: Supports importing local board directories, official boards, and board components as a starting point for further edits.
- **Edit configuration parameters**: Fill in mode fields, pin parameters, parameters to be confirmed, peripheral dependencies, and component dependencies on each configuration card.
- **Export board-level files**: Exports ``board_info.yaml``, ``board_devices.yaml`` and ``board_peripherals.yaml``; ``setup_device.c`` is additionally generated when board-level initialization logic is required.
- **Configuration validation**: Checks for chip capability conflicts and flags IO pin reuse issues, helping to catch obvious errors before export.

Applicable Scope
----------------

The web tool targets device and peripheral types already supported by BMGR, and is suitable as the entry point for creating new board configurations. Once configuration is complete, the following files can be exported with one click:

- ``board_info.yaml``
- ``board_devices.yaml``
- ``board_peripherals.yaml``
- ``setup_device.c``

``setup_device.c`` is generated only when the selected device requires board-level initialization logic that pure YAML cannot describe (such as a display factory function).

Usage Boundaries
----------------

- The tool only generates board-level configuration files; it does not replace the ``idf.py bmgr`` command. The exported YAML files still need to be placed into a project and processed with ``idf.py bmgr -b`` to generate board-level code.
- The tool does not generate ``sdkconfig.defaults.board``; the target chip and project configuration still need to be set up in the ESP-IDF project in the usual way.
- The exported ``setup_device.c`` is only a code template generated from the factory function interface. It needs to be completed with real hardware initialization logic and verified on the board before use; it cannot be used as-is.
- Pin numbers, addresses, clocks, timing, and power relationships in the board-level parameters still need to be confirmed against the schematic and component datasheets.
