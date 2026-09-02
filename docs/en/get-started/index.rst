***********
Get Started
***********

:link_to_translation:`zh_CN:[中文]`

This document helps developers set up a multimedia application development environment based on Espressif ESP32 series chips and demonstrates how to use ESP-ADF (Espressif Advanced Development Framework) through a complete example project.

After reading this document, you will be able to:

- Install and configure a supported version of ESP-IDF
- Obtain an ESP-ADF example project
- Build and flash the project, and monitor its output via serial port

About ESP-ADF
--------------------------------------

`ESP-ADF <https://github.com/espressif/esp-adf>`__ is Espressif's multimedia development framework built on ESP-IDF and `ESP-GMF <https://github.com/espressif/esp-gmf>`__. It provides product-oriented components for audio/video capture and playback, AI voice, Bluetooth audio, multimedia transport, and more. These application components are published to the `IDF Component Manager <https://components.espressif.com/>`__ and can be fetched automatically after you declare them as dependencies in your project.

Developers typically only need to declare dependencies in the project's ``idf_component.yml``. The component manager fetches them automatically at build time without any additional environment variables. This document provides two methods to obtain an example project: cloning the repository or using the component manager.

.. note::

   For currently supported ESP-IDF versions, see the `README <https://github.com/espressif/esp-adf/blob/master/README.md#idf-version>`__.

.. _get-started-step-by-step:
.. _get-started-setup-esp-idf:
.. _get-started-setup-idf:

Step 1. Install ESP-IDF
--------------------------------------

Follow the "Get Started" section in the `ESP-IDF Programming Guide <https://docs.espressif.com/projects/esp-idf/en/latest/index.html>`__ for your operating system (Windows, Linux, or macOS) to install the ESP-IDF toolchain and dependencies.

After installation and environment activation, confirm that the following command runs successfully in the terminal:

.. code-block:: bash

   idf.py --version

and outputs a supported version number (see the version note in `About ESP-ADF`_).

.. note::

   If ESP-IDF is already installed but the version is not in the supported range, see `ESP-IDF Versions <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/versions.html>`__ to switch branches.

.. _get-started-get-esp-adf:
.. _get-started-set-up-env:
.. _get-started-get-adf:

Step 2. Get an ESP-ADF Example Project
--------------------------------------

ADF example projects are stored in the ``adf_examples`` directory of the repository. This document uses ``music_player`` to demonstrate the complete build and run workflow: the example scans local music files on a microSD card, shows a player UI with song information and playback controls on the display, and plays music through the audio output device.

Either of the following two methods can be used to obtain the example project.

- Method A (Clone Repository): Download the complete ESP-ADF source code and all examples. Suitable for in-depth framework study, comparing examples, or contributing to ESP-ADF.
- Method B (Component Manager): Download only one example project; required components are fetched automatically by the component manager at build time. This approach matches real product project development and is suitable for quick evaluation or integration.

**Method A (Clone Repository):**

Run the following command in the terminal to clone the complete ESP-ADF source repository:

.. code-block:: bash

   git clone --recursive https://github.com/espressif/esp-adf.git

.. note::

   ``$ADF_PATH`` used below is a placeholder for documentation purposes only, representing the root directory of the cloned ``esp-adf`` repository. Replace it with the actual local clone path when running commands.

For users in China, downloading from `Gitee <https://gitee.com/EspressifSystems/esp-adf>`__ is usually faster:

.. code-block:: bash

   git clone --recursive https://gitee.com/EspressifSystems/esp-adf.git

**Method B (Download Example via IDF Component Manager):**

Run the following command in the target directory to fetch the example project directly via the IDF Component Manager:

.. code-block:: bash

   idf.py create-project-from-example "espressif/adf_examples:music_player"

After execution, a ``music_player`` project folder is created in the current directory without cloning the entire ESP-ADF repository.

.. _get-started-start-project:

Step 3. Open the Example Project
--------------------------------------

Navigate to the example project directory:

**Method A (Clone Repository): Linux / macOS**

.. code-block:: bash

   cd $ADF_PATH/adf_examples/player/music_player

**Method A (Clone Repository): Windows**

.. code-block:: batch

   cd %ADF_PATH%\adf_examples\player\music_player

**Method B (Component Manager):**

.. code-block:: bash

   cd music_player

.. note::

   The ESP-IDF build system does not support spaces in paths. Ensure that the full paths to both ESP-IDF and the project contain no spaces.

.. _get-started-connect:

Step 4. Connect the Development Board
--------------------------------------

``music_player`` requires a development board with microSD, LCD, and touch. Connect it to the PC via USB cable, and identify the serial port using `ESP-IDF: Establish Serial Connection <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/establish-serial-connection.html>`__:

- On Linux, typically ``/dev/ttyUSB0`` or ``/dev/ttyACM0``
- On macOS, typically ``/dev/cu.usbserial-*`` or ``/dev/cu.SLAB_USBtoUART``
- On Windows, typically ``COM3``, ``COM4``, etc.

Place ``.mp3``, ``.aac``, or ``.wav`` test files on a microSD card (FAT filesystem) under the ``/sdcard`` mount point or one level of subdirectory. For Espressif-supported audio development boards, see :doc:`../multimedia-boards/index`.

.. note::

   Note the serial port for the board; it is needed in the :ref:`get-started-flash` and :ref:`get-started-monitor` steps.

.. _get-started-board-manager:

Step 5. Configure Hardware
--------------------------------------

ESP-ADF example projects use `ESP Board Manager <https://github.com/espressif/esp-board-manager>`__ to manage peripheral descriptions and board-level initialization code. Installing the helper tool ``esp-bmgr-assist`` as the default tool is recommended.

Install in the activated ESP-IDF Python environment (only needed once per environment):

.. code-block:: bash

   pip install esp-bmgr-assist

To upgrade to the latest version:

.. code-block:: bash

   pip install --upgrade esp-bmgr-assist

List supported development boards:

.. code-block:: bash

   idf.py bmgr -l

Select a development board:

.. code-block:: bash

   idf.py bmgr -b <board_index|board_name>

For example, to select ``esp32_s3_korvo_2_3``:

.. code-block:: bash

   idf.py bmgr -b esp32_s3_korvo_2_3

On the first run of ``idf.py bmgr``, the tool automatically downloads the ``espressif/esp_board_manager`` component based on the project dependencies.

.. note::

   - To switch to another board supported by ``esp_board_manager``, follow the same steps with a different board name or index.
   - To use a custom board not in the list, see the `Create Board Guide <https://docs.espressif.com/projects/esp-board-manager/en/latest/create-board/index.html>`__.
   - For more information on ``esp_board_manager``, see the `ESP Board Manager Getting Started Guide <https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md>`__.

.. _get-started-configure:

Step 6. Configure the Project
--------------------------------------

Open the project configuration menu:

.. code-block:: bash

   idf.py menuconfig

For the ``music_player`` example, the default configuration can usually be built and run directly. To adjust LVGL, fonts, or other display-related options, see the ``README`` in the example project directory.

After making changes, press ``S`` to save and ``Q`` to exit the menu.

.. _get-started-build:

Step 7. Build the Project
--------------------------------------

Run the following command to start building:

.. code-block:: bash

   idf.py build

This command builds all components involved in ESP-IDF and ESP-ADF in dependency order, generating the bootloader, partition table, and application binary files. The first build takes longer; subsequent incremental builds are significantly faster.

After a successful build, the terminal outputs a message similar to the following and shows the corresponding flash command:

.. code-block:: none

   Project build complete. To flash, run:
    idf.py flash
   or
    idf.py -p PORT flash

If build errors occur, check the ESP-IDF version, whether the board configuration in :ref:`get-started-board-manager` was completed, and whether dependencies were fetched correctly.

.. _get-started-flash:

Step 8. Flash the Firmware
--------------------------------------

Replace ``PORT`` with the serial port noted in :ref:`get-started-connect`, then run the following command to flash and open the serial monitor:

.. code-block:: bash

   idf.py -p PORT flash monitor

.. note::

   - ``idf.py flash`` automatically rebuilds before flashing, so running ``idf.py build`` separately is not necessary.
   - The default baud rate is ``460800``; adjust it with the ``-b BAUD`` parameter.
   - If the board has no auto-reset circuit, hold the **Boot** button, press and release the **Reset** button once, then release the **Boot** button to enter download mode before flashing.
   - If the board uses USB Serial JTAG and the serial port is not found, try entering download mode manually as described above and then check for the serial port.

.. _get-started-monitor:

Step 9. Monitor the Output
--------------------------------------

After flashing, the board resets automatically and runs the example program. The serial monitor outputs a log similar to the following (key steps shown):

.. code-block:: none

   I (1435) main_task: Calling app_main()
   I (1438) MUSIC_PLAYER: [ 1 ] Initialize board peripherals
   I (1517) BOARD_MANAGER: Device fs_sdcard initialized
   I (1578) BOARD_MANAGER: Device audio_dac initialized
   I (1621) MUSIC_PLAYER: [ 2 ] Initialize display and LVGL music UI
   I (1839) BOARD_MANAGER: Device display_lcd initialized
   I (1884) BOARD_MANAGER: Device lcd_touch initialized
   I (1999) MUSIC_PLAYER: [ 3 ] Scan SD card playlist from /sdcard
   I (2114) MUSIC_PLAYER: [ 4 ] Start playback controller
   I (2121) MUSIC_PLAYER: [ 5 ] Music player ready

If everything works, the display shows the player UI. When music files are present on the SD card, the example starts playing the first track automatically. Use the touch controls at the bottom of the screen to play, pause, skip tracks, and adjust volume.

Press ``Ctrl+]`` to exit the serial monitor.

Next Steps
--------------------------------------

Having completed this example, you now understand the basic workflow for an ESP-ADF project. Suggested next steps:

- Browse :doc:`../basic-components/index` for audio codecs, effects, media protocols, GMF, and other foundational components provided by ESP-ADF.
- Browse :doc:`../multimedia-services/index` for service infrastructure, media services, peripheral services, and AI integrations.
- Explore other projects under ``adf_examples``, including recording, AI agents, video, and more.

Related Documents
--------------------------------------

- `ESP-IDF Programming Guide <https://docs.espressif.com/projects/esp-idf/en/latest/index.html>`__
- `ESP Component Manager <https://components.espressif.com/>`__
- `ESP Component Manager Documentation <https://docs.espressif.com/projects/idf-component-manager/en/latest/>`__
- `ESP Board Manager Getting Started Guide <https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md>`__
- `ESP Board Manager Troubleshooting <https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README.md#troubleshooting>`__
- `ESP-ADF GitHub Repository <https://github.com/espressif/esp-adf>`__
- `ESP-ADF Example Collection <https://github.com/espressif/esp-adf/tree/master/adf_examples>`__
- `ESP-GMF GitHub Repository <https://github.com/espressif/esp-gmf>`__
