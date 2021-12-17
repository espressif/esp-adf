***********
Get Started
***********

This document is intended to help users set up the software environment for the development of audio applications using hardware based on the ESP32 family of chips by Espressif. After that, a simple example will show you how to use ESP-ADF (Espressif Audio Development Framework).

Development Board Overview
==========================

For easier start with ESP-ADF, Espressif designed ESP32, ESP32-S2, and ESP32-S3 based development boards intended for audio applications. Click the links below to learn more about the available boards.

.. toctree::
   :maxdepth: 2


   ../design-guide/dev-boards/index

If you do not have any of the above boards, you can still use ESP-ADF for the ESP32 and ESP32-S2 based audio applications. For this, your board needs to have a compatible audio codec or DSP chip; alternatively, you can develop a driver to support communication with your specific chip.

.. _get-started-about-esp-adf:

About ESP-ADF
=============

The ESP-ADF is available as a set of :adf:`components` to extend the functionality already delivered by the `ESP-IDF <https://github.com/espressif/esp-idf>`_ (Espressif IoT Development Framework).

To use ESP-ADF you need set up the ESP-IDF first, and this is described in the next section.

.. note::

    ESP-ADF provides support for specific `ESP-IDF versions (v3.3, v4.0, v4.1, v4.2, v4.3 and v4.4) <https://docs.espressif.com/projects/esp-idf/en/release-v3.3/versions.html>`_. If your have already set up another version, please switch to a supported ESP-IDF version, or you may not be able to compile ESP-ADF applications.


.. _get-started-step-by-step:

Installation Step by Step
=========================

This is a detailed roadmap to walk you through the installation process.

Setting up Development Environment
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* :ref:`get-started-setup-esp-idf` for Windows, Linux or Mac OS
* :ref:`get-started-get-esp-adf`
* :ref:`get-started-setup-path`
* :ref:`get-started-set-up-env`

Creating Your First Project
~~~~~~~~~~~~~~~~~~~~~~~~~~~

* :ref:`get-started-start-project`
* :ref:`get-started-connect`
* :ref:`get-started-configure`
* :ref:`get-started-build`
* :ref:`get-started-flash`
* :ref:`get-started-build-monitor`


.. _get-started-setup-esp-idf:

Step 1. Set up ESP-IDF
======================

Configure your PC according to **Getting Started** section of **ESP-IDF Programming Guide**. Windows, Linux and Mac OS operating systems are supported. Please select and follow the guide specific to `ESP32 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html>`_ or `ESP32-S2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s2/get-started/index.html>`_ chip. The chip name is provided in the board name.

.. note::

    This guide uses the directory ``~/esp`` on Linux and macOS or ``%userprofile%\esp`` on Windows as an installation folder for ESP-ADF. You can use any directory, but you will need to adjust paths for the commands accordingly. Keep in mind that ESP-ADF does not support spaces in paths.

To make the installation easier and less prone to errors, use the ``~/esp`` default directory for the installation. 

If this is your first exposure to the `ESP-IDF <https://github.com/espressif/esp-idf>`_, then it is recommended to get familiar with **hello_world** or **blink** example first. 

After getting familiar with ESP-IDF, decide on which ESP-IDF version to use for your application depending on the Espressif chip that you have and your project type. For this, consult `Versions <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/versions.html>`_ section of ESP-IDF Programming Guide.

Once you successfully build, upload, and run examples for your version of ESP-IDF, you can proceed to the next step.


.. _get-started-get-esp-adf:

Step 2. Get ESP-ADF
===================

.. highlight:: bash

Now you can start installing audio-specific API / libraries provided in `ESP-ADF repository <https://github.com/espressif/esp-adf>`_.

Windows
~~~~~~~

Open Command Prompt and run the following commands::

    cd %userprofile%\esp
    git clone --recursive https://github.com/espressif/esp-adf.git

Linux and macOS
~~~~~~~~~~~~~~~

Open Terminal, and run the following commands::

    cd ~/esp
    git clone --recursive https://github.com/espressif/esp-adf.git


.. _get-started-setup-path:

Step 3. Set up Path to ESP-ADF
==============================

The toolchain programs access ESP-ADF using ``ADF_PATH`` environment variable. This variable should be set up on your PC, otherwise the projects will not build.

Windows
~~~~~~~

Open Command Prompt and run the following command::

    set ADF_PATH=%userprofile%\esp\esp-adf

You need to enter this command each time you start your PC. To avoid retyping you can add it to "ESP-IDF Command Prompt", batch or Power Shell scripts described in Step 4 below.

To make sure that ADF_PATH has been set up properly, run::

    echo %ADF_PATH%

It should return the path to your ESP-ADF directory.

Linux and macOS
~~~~~~~~~~~~~~~

Open Terminal, and run the following commands::

    export ADF_PATH=~/esp/esp-adf

You need to enter this command each time you open a Terminal. To make this setting permanent follow similar `instructions <https://docs.espressif.com/projects/esp-idf/en/v3.3.1/get-started/add-idf_path-to-profile.html#linux-and-macos>`__ for configuration of ``IDF_PATH`` in ESP-IDF Programming Guide.

Check if ``ADF_PATH`` has been set up to point to directory with ESP-ADF::

    printenv ADF_PATH


.. _get-started-set-up-env:

Step 4. Set up the environment variables
========================================

Before being able to compile ESP-ADF projects, on each new session, ESP-IDF tools should be added to the PATH environment variable. To make the tools usable from the command line, some environment variables must be set. ESP-IDF provides a script which does that.

Windows
~~~~~~~

`ESP-IDF Tools Installer`_ for Windows creates an "ESP-IDF Command Prompt" shortcut in the Start Menu. This shortcut opens the Command Prompt and sets up all the required environment variables. You can open this shortcut and proceed to the next step.

Alternatively, if you want to use ESP-IDF in an existing Command Prompt window, you can run:

.. code-block:: batch

    %userprofile%\esp\esp-idf\export.bat

or with Windows PowerShell

.. code-block:: powershell

    .$HOME/esp/esp-idf/export.ps1

Linux and macOS
~~~~~~~~~~~~~~~

In the terminal where you have installed ESP-IDF, run:

.. code-block:: bash

    . $HOME/esp/esp-idf/export.sh

Note the space between the leading dot and the path!

You can also create an alias for the export script to your ``.profile`` or ``.bash_profile`` script. This way you can set up the environment in a new terminal window by typing ``get_idf``:

.. code-block:: bash

    alias get_idf='. $HOME/esp/esp-idf/export.sh'

Note that it is not recommended to source ``export.sh`` from the profile script directly. Doing so activates IDF virtual environment in every terminal session (even in those where IDF is not needed), defeating the purpose of the virtual environment and likely affecting other software.


.. _get-started-start-project:

Step 5. Start a Project
=======================

After initial preparation you are ready to build the first audio application. The process has already been described in ESP-IDF documentation. Now we would like to discuss remaining key steps and show how the toolchain is able to access the ESP-ADF :adf:`components` by using the ``ADF_PATH`` variable.

To demonstrate how to build an application, we will use :example:`get-started/play_mp3_control` project from :adf:`examples` directory in the ADF.

Windows
~~~~~~~

.. code-block:: batch

    cd %userprofile%\esp
    xcopy /e /i %ADF_PATH%\examples\get-started\play_mp3_control play_mp3_control

Linux and macOS
~~~~~~~~~~~~~~~

.. code-block:: bash

    cd ~/esp
    cp -r $ADF_PATH/examples/get-started/play_mp3_control .


There is a range of example projects in the :adf:`examples` directory in ESP-ADF. You can copy any project in the same way as presented above and run it.

It is also possible to build examples in-place, without copying them first.

.. important::

    The ESP-IDF build system does not support spaces in the paths to either ESP-IDF or to projects.


.. _get-started-connect:

Step 6. Connect Your Device
===========================

Connect the audio board to the PC, check under what serial port the board is visible and verify, if serial communication works as described in `ESP-IDF documentation <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/establish-serial-connection.html>`_.

.. note::

    Keep the port name handy as you will need it in the next steps.


.. _get-started-configure:

Step 7. Configure
=================

Navigate to your ``play_mp3_control`` directory from :ref:`get-started-start-project` and configure the project:

ESP-IDF v3.3.2 and v4.0 releases
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Windows
^^^^^^^

.. code-block:: batch

    cd %userprofile%\esp\play_mp3_control
    idf.py menuconfig

Linux and macOS
^^^^^^^^^^^^^^^

.. code-block:: bash

    cd ~/esp/play_mp3_control
    idf.py menuconfig


ESP-IDF v4.1 and master releases
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Windows
^^^^^^^

.. code-block:: batch

    cd %userprofile%\esp\play_mp3_control
    idf.py set-target esp32
    idf.py menuconfig

Linux and macOS
^^^^^^^^^^^^^^^

.. code-block:: bash

    cd ~/esp/play_mp3_control
    idf.py set-target esp32
    idf.py menuconfig

.. note::

    If you are using an ESP32-S2 based board, then the second command above should be ``idf.py set-target esp32s2`` for ESP-IDF master release or ``idf.py set-target esp32s2beta`` for ESP-IDF v4.1 release.

Setting the target with ``idf.py set-target <target>`` should be done once, after opening a new project. If the project contains some existing builds and configuration, they will be cleared and initialized. The target may be saved in environment variable to skip this step at all. See `Selecting the Target <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#step-7-configure>`__ in ESP-IDF Programming Guide for additional information.

If the previous steps have been done correctly, the following menu appears:

.. figure:: ../../_static/project-configuration-home.png
    :align: center
    :alt: Project configuration - Home window
    :figclass: align-center

    Project configuration - Home window

You are using this menu to set up your board type and other project specific variables, e.g. Wi-Fi network name and password, the processor speed, etc.

.. figure:: ../../_static/project-configuration-board-selection.png
    :align: center
    :alt: Project configuration - Board selection

    Project configuration - Board selection

Select your board from the menu, press ``S`` to save configuration and then ``Q`` to exit.

.. note::

    The colors of the menu could be different in your terminal. You can change the appearance with the option
    ``--style``. Please run ``idf.py menuconfig --help`` for further information.


.. _get-started-build:

Step 8. Build the Project
=========================

Build the project by running:

.. code-block:: batch

    idf.py build

This command will compile the application and all ESP-IDF and ESP-ADF components, then it will generate the bootloader, partition table, and application binaries.

.. code-block:: none

   $ idf.py build
    Executing action: all (aliases: build)
    Running ninja in directory /path/to/esp/play_mp3_control/build
    Executing "ninja all"...
    [0/1] Re-running CMake...

   ... (more lines of build system output)

    [1064/1064] Generating binary image from built executable
    esptool.py v3.0-dev
    Generated /path/to/esp/play_mp3_control/build/play_mp3_control.bin

    Project build complete. To flash it, run this command:
    /path/to/.espressif/python_env/idf4.2_py2.7_env/bin/python ../esp-idf/components/esptool_py/esptool/esptool.py -p (PORT) -b 460800 --before default_reset --after hard_reset --chip esp32  write_flash --flash_mode dio --flash_size detect --flash_freq 40m 0x1000 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/play_mp3_control.bin
    or run 'idf.py -p (PORT) flash'

If there are no errors, the build will finish by generating the firmware binary .bin file.


.. _get-started-flash:

Step 9. Flash onto the Device
=============================

Flash the binaries that you just built onto your board by running:

.. code-block:: bash

    idf.py -p PORT [-b BAUD] flash monitor

Replace PORT with your board's serial port name from :ref:`get-started-connect`.

You can also change the flasher baud rate by replacing BAUD with the baud rate you need. The default baud rate is ``460800``.

For more information on idf.py arguments, see `Using the Build System <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#idf-py>`__ in ESP-IDF Programming Guide.

.. note::

    The option ``flash`` automatically builds and flashes the project, so running ``idf.py build`` is not necessary.

.. highlight:: none

To upload the binaries, the board should be put into upload mode. To do so, hold down **Boot** button, momentarily press **Reset** button and release the **Boot** button. The upload mode may be initiated anytime during the application build, but no later than "Connecting" message is being displayed::

    ...

    esptool.py v3.0-dev
    Serial port /dev/ttyUSB0
    Connecting........_____....

Without the upload mode enabled, after showing several ``....._____``, the connection will eventually time out.

Once build and upload is complete, you should see the following::

    ...

    Leaving...
    Hard resetting via RTS pin...
    Executing action: monitor
    Running idf_monitor in directory /path/to/esp/play_mp3_control
    Executing "/path/to/.espressif/python_env/idf4.2_py2.7_env/bin/python /path/to/esp/esp-idf/tools/idf_monitor.py -p /dev/ttyUSB0 -b 115200 --toolchain-prefix xtensa-esp32-elf- /path/to/esp/play_mp3_control/build/play_mp3_control.elf -m '/path/to/.espressif/python_env/idf4.2_py2.7_env/bin/python' '/path/to/esp/esp-idf/tools/idf.py'"...
    --- idf_monitor on /dev/ttyUSB0 115200 ---
    --- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H ---

If there are no issues by the end of the flash process, the board will reboot and start up the “play_mp3_control” application.


.. _get-started-build-monitor:

Step 10. Monitor
================

At this point press the **Reset** button to start the application. Following several lines of start up log, the ``play_mp3_control`` application specific messages should be displayed::

    ...

    I (397) PLAY_FLASH_MP3_CONTROL: [ 1 ] Start audio codec chip
    I (427) PLAY_FLASH_MP3_CONTROL: [ 2 ] Create audio pipeline, add all elements to pipeline, and subscribe pipeline event
    I (427) PLAY_FLASH_MP3_CONTROL: [2.1] Create mp3 decoder to decode mp3 file and set custom read callback
    I (437) PLAY_FLASH_MP3_CONTROL: [2.2] Create i2s stream to write data to codec chip
    I (467) PLAY_FLASH_MP3_CONTROL: [2.3] Register all elements to audio pipeline
    I (467) PLAY_FLASH_MP3_CONTROL: [2.4] Link it together [mp3_music_read_cb]-->mp3_decoder-->i2s_stream-->[codec_chip]
    I (477) PLAY_FLASH_MP3_CONTROL: [ 3 ] Set up  event listener
    I (477) PLAY_FLASH_MP3_CONTROL: [3.1] Listening event from all elements of pipeline
    I (487) PLAY_FLASH_MP3_CONTROL: [ 4 ] Start audio_pipeline
    I (507) PLAY_FLASH_MP3_CONTROL: [ * ] Receive music info from mp3 decoder, sample_rates=44100, bits=16, ch=2
    I (7277) PLAY_FLASH_MP3_CONTROL: [ 5 ] Stop audio_pipeline

If there are no issues, besides the above log, you should hear a sound played for about 7 seconds by the speakers or headphones connected to your audio board. Reset the board to hear it again if required.

Now you are ready to try some other :adf:`examples`, or go right to developing your own applications. Check how the :adf:`examples` are made aware of location of the ESP-ADF. Open the :example_file:`get-started/play_mp3_control/Makefile` and you should see ::

    include($ENV{ADF_PATH}/CMakeLists.txt)
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)

The first line contains ``ADF_PATH`` to point the toolchain to another file in ESP-ADF directory that provides configuration variables and path to ESP-ADF :adf:`components` reacquired by the toolchain. You need similar ``Makefile`` in your own applications developed with the ESP-ADF.


Update ESP-ADF
==============

After some time of using ESP-ADF, you may want to update it to take advantage of new features or bug fixes. The simplest way to do so is by deleting existing ``esp-adf`` folder and cloning it again, which is same as when doing initial installation described in sections :ref:`get-started-get-esp-adf`.

Another solution is to update only what has changed. This method is useful if you have a slow connection to the GitHub. To do the update run the following commands::

    cd ~/esp/esp-adf
    git pull
    git submodule update --init --recursive

The ``git pull`` command is fetching and merging changes from ESP-ADF repository on GitHub. Then ``git submodule update --init --recursive`` is updating existing submodules or getting a fresh copy of new ones. On GitHub the submodules are represented as links to other repositories and require this additional command to get them onto your PC.


.. _ESP-IDF Tools Installer: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html#get-started-windows-tools-installer

