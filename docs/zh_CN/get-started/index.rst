***********
快速入门
***********

:link_to_translation:`en:[English]`

本文档帮助开发者基于乐鑫 ESP32 系列芯片搭建多媒体应用的开发环境，并通过一个完整的示例工程，演示如何使用 ESP-ADF（Espressif Advanced Development Framework）。

读完本文档后，您将能够：

- 安装并配置一个受支持的 ESP-IDF 版本
- 获取一个 ESP-ADF 示例工程
- 编译、烧录工程并通过串口监视示例工程的运行

关于 ESP-ADF
--------------------------------------

`ESP-ADF <https://github.com/espressif/esp-adf>`__ 是乐鑫基于 ESP-IDF 和 `ESP-GMF <https://github.com/espressif/esp-gmf>`__ 构建的多媒体开发框架，提供音视频采集与播放、AI 语音、蓝牙音频、多媒体传输等产品级方案组件。其各类应用组件已发布至 `IDF 组件管理器 <https://components.espressif.com/>`__，可在工程中声明依赖后由组件管理器自动获取。

开发者通常只需在工程的 ``idf_component.yml`` 中声明依赖，构建时由组件管理器自动获取，无需设置额外的环境变量。本文提供克隆仓库与组件管理器两种方式获取示例工程。

.. note::

   ESP-ADF 当前支持的 ESP-IDF 版本请参阅 `README <https://github.com/espressif/esp-adf/blob/master/README.md#idf-version>`__。

.. _get-started-step-by-step:
.. _get-started-setup-esp-idf:
.. _get-started-setup-idf:

Step 1. 安装 ESP-IDF
--------------------------------------

请按 `ESP-IDF 编程指南 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/index.html>`__ 中“快速入门”章节，根据您所在的操作系统（Windows、Linux 或 macOS）完成 ESP-IDF 工具链与依赖的安装。

安装完成并激活环境后，确认终端中可正常运行：

.. code-block:: bash

   idf.py --version

并输出一个受支持的版本号（参见 `关于 ESP-ADF`_ 中的版本说明）。

.. note::

   如已安装但版本不在支持范围内，可参考 `ESP-IDF 版本管理 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/versions.html>`__ 切换分支。

.. _get-started-get-esp-adf:
.. _get-started-set-up-env:
.. _get-started-get-adf:

Step 2. 获取 ESP-ADF 示例工程
--------------------------------------

ADF 示例工程存放在仓库的 ``adf_examples`` 目录下。本文以 ``music_player`` 为例演示完整的编译与运行流程：该示例扫描 microSD 卡中的本地音乐文件，在屏幕上显示播放器界面、歌曲信息与播放控制，并通过音频输出设备播放音乐。

可通过以下两种方式获取示例工程，二选一即可。

- 方式 A（克隆仓库）：下载 ESP-ADF 完整源码及全部示例，适合深入学习框架、对比测试各示例，或向 ESP-ADF 贡献代码。
- 方式 B（组件管理器）：仅下载一个示例工程，所需组件在编译阶段由组件管理器自动获取，此方式与实际产品项目的开发方式一致，适合快速验证或集成评估。

**方式 A（克隆仓库）：**

在终端中执行以下命令获取 ESP-ADF 完整源码仓库：

.. code-block:: bash

   git clone --recursive https://github.com/espressif/esp-adf.git

.. note::

   下文出现的 ``$ADF_PATH`` 为文档中使用的占位符，代表克隆得到的 ``esp-adf`` 仓库根目录，实际操作时请按本地克隆位置替换。

中国用户可从 `Gitee <https://gitee.com/EspressifSystems/esp-adf>`__ 下载，速度更快：

.. code-block:: bash

   git clone --recursive https://gitee.com/EspressifSystems/esp-adf.git

**方式 B（通过 IDF 组件管理器直接下载示例）：**

在目标目录下执行以下命令，由 IDF 组件管理器直接获取示例工程：

.. code-block:: bash

   idf.py create-project-from-example "espressif/adf_examples:music_player"

执行完成后，当前目录下会生成 ``music_player`` 工程文件夹，无需克隆整个 ESP-ADF 仓库。

.. _get-started-start-project:

Step 3. 启动示例工程
--------------------------------------

进入示例工程目录：

**方式 A（克隆仓库）：Linux / macOS**

.. code-block:: bash

   cd $ADF_PATH/adf_examples/player/music_player

**方式 A（克隆仓库）：Windows**

.. code-block:: batch

   cd %ADF_PATH%\adf_examples\player\music_player

**方式 B（组件管理器）：**

.. code-block:: bash

   cd music_player

.. note::

   ESP-IDF 构建系统不支持路径中包含空格，请确认 ESP-IDF 与工程的完整路径中均不含空格。

.. _get-started-connect:

Step 4. 连接开发板
--------------------------------------

``music_player`` 需要带有 microSD、LCD 与触摸的开发板。通过 USB 数据线将其连接到 PC，并按 `ESP-IDF 文档：建立串口连接 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/get-started/establish-serial-connection.html>`__ 确认开发板对应的串口号：

- Linux 下通常为 ``/dev/ttyUSB0`` 或 ``/dev/ttyACM0``
- macOS 下通常为 ``/dev/cu.usbserial-*`` 或 ``/dev/cu.SLAB_USBtoUART``
- Windows 下通常为 ``COM3``\ 、\ ``COM4`` 等

请在 microSD 卡（FAT 格式）的 ``/sdcard`` 挂载点或其一层子目录中放入 ``.mp3``\ 、\ ``.aac`` 或 ``.wav`` 测试文件。乐鑫支持的音频开发板列表请参阅 :doc:`../multimedia-boards/index`。

.. note::

   请记下当前开发板对应的串口号，后续 :ref:`get-started-flash` 与 :ref:`get-started-monitor` 步骤需要使用。

.. _get-started-board-manager:

Step 5. 配置硬件
--------------------------------------

ESP-ADF 示例工程通过 `ESP Board Manager <https://github.com/espressif/esp-board-manager>`__ 统一管理开发板的外设描述与板级初始化代码。推荐安装辅助工具 ``esp-bmgr-assist`` 作为默认工具。

在已激活的 ESP-IDF Python 环境下安装（同一环境只需安装一次）：

.. code-block:: bash

   pip install esp-bmgr-assist

如需升级至最新版本：

.. code-block:: bash

   pip install --upgrade esp-bmgr-assist

查看支持的开发板：

.. code-block:: bash

   idf.py bmgr -l

选择开发板：

.. code-block:: bash

   idf.py bmgr -b <board_index|board_name>

例如选择 ``esp32_s3_korvo_2_3``\ ：

.. code-block:: bash

   idf.py bmgr -b esp32_s3_korvo_2_3

首次执行 ``idf.py bmgr`` 时，工具会根据工程依赖自动下载 ``espressif/esp_board_manager`` 组件。

.. note::

   - 如需切换为其他 ``esp_board_manager`` 支持的开发板，请按相同步骤执行并替换板型名称/索引。
   - 如需使用未列出的自定义开发板，请参考 `创建开发板指南 <https://docs.espressif.com/projects/esp-board-manager/zh_CN/latest/create-board/index.html>`__。
   - ``esp_board_manager`` 更多信息请参考 `ESP Board Manager 入门指南 <https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README_CN.md>`__。

.. _get-started-configure:

Step 6. 工程配置
--------------------------------------

打开工程配置菜单：

.. code-block:: bash

   idf.py menuconfig

对于 ``music_player`` 示例，默认配置通常即可直接编译运行。如需调整 LVGL、字体等显示相关选项，请参考示例工程目录下的 ``README``\ 。

修改完成后按 ``S`` 保存，再按 ``Q`` 退出菜单。

.. _get-started-build:

Step 7. 编译工程
--------------------------------------

执行以下命令开始编译：

.. code-block:: bash

   idf.py build

该命令会按依赖关系编译 ESP-IDF、ESP-ADF 涉及到的所有组件，并生成 bootloader、分区表与应用 bin 文件。首次编译用时较长，后续增量编译会显著加快。

编译成功后，终端会输出类似以下日志，并提示对应的烧录命令：

.. code-block:: none

   Project build complete. To flash, run:
    idf.py flash
   or
    idf.py -p PORT flash

如出现编译错误，请按错误提示检查 ESP-IDF 版本、:ref:`get-started-board-manager` 中开发板配置是否完成、依赖组件是否正确获取等。

.. _get-started-flash:

Step 8. 烧录固件
--------------------------------------

将 ``PORT`` 替换为 :ref:`get-started-connect` 中记录的串口号，运行以下命令完成烧录并打开串口监视：

.. code-block:: bash

   idf.py -p PORT flash monitor

.. note::

   - ``idf.py flash`` 会在烧录前自动重新编译，因此不必单独执行 ``idf.py build``\ 。
   - 默认烧录波特率为 ``460800``\ ，可通过 ``-b BAUD`` 参数调整。
   - 若开发板没有自动复位电路，烧录前请按住 **Boot** 键、短按一次 **Reset** 键后再松开 **Boot** 键，使芯片进入下载模式。
   - 如果开发板使用 USB Serial JTAG，且找不到串口，可以尝试按照上述方式手动进入下载模式后查看串口。

.. _get-started-monitor:

Step 9. 监视输出
--------------------------------------

烧录完成后，开发板会自动复位并运行示例程序。串口监视器中将输出类似以下日志（仅截取关键步骤）：

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

若一切正常，屏幕会显示播放器界面；若 SD 卡中存在音乐文件，例程会自动开始播放第一首。可使用触摸屏底部控制栏进行播放、暂停、切歌与音量调节。

使用快捷键 ``Ctrl+]`` 可退出串口监视器。

后续阅读
--------------------------------------

完成本示例后，您已经掌握 ESP-ADF 工程的基础工作流。建议按以下方向继续探索：

- 浏览 :doc:`../basic-components/index`，了解 ESP-ADF 提供的音频编解码、效果处理、媒体协议与 GMF 等基础组件。
- 浏览 :doc:`../multimedia-services/index`，了解服务基础设施、媒体服务、外设服务与 AI 对接等上层组件。
- 查阅 ``adf_examples`` 目录下的其他示例工程，包括录音、AI Agent、视频等更多应用场景。

相关文档
--------------------------------------

- `ESP-IDF 编程指南 <https://docs.espressif.com/projects/esp-idf/zh_CN/latest/index.html>`__
- `ESP 组件管理器 <https://components.espressif.com/>`__
- `ESP 组件管理器使用文档 <https://docs.espressif.com/projects/idf-component-manager/en/latest/>`__
- `ESP Board Manager 入门指南 <https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README_CN.md>`__
- `ESP Board Manager 常见使用问题 <https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README_CN.md#%E6%95%85%E9%9A%9C%E6%8E%92%E9%99%A4>`__
- `ESP-ADF GitHub 仓库 <https://github.com/espressif/esp-adf>`__
- `ESP-ADF 示例集合 <https://github.com/espressif/esp-adf/tree/master/adf_examples>`__
- `ESP-GMF GitHub 仓库 <https://github.com/espressif/esp-gmf>`__
