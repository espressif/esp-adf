ESP Board Manager
=================

:link_to_translation:`en:[English]`

ESP Board Manager（BMGR）是乐鑫开发板生态的基础架构组件，在乐鑫芯片平台上建立从开发板硬件到应用软件的标准化链路。BMGR 以声明式 YAML 描述硬件外设与功能设备，代码生成器据此输出标准化的初始化代码，并向应用层提供统一的运行时接口用于设备管理。基于 BMGR 完成适配的开发板，可在任意 BMGR 工程中直接使用，也可在社区中共享和复用。

板卡定义包：

.. list-table::
   :header-rows: 1
   :widths: 24 46 30

   * - 组件
     - 简介
     - 相关链接
   * - ``esp_boards``
     - 乐鑫官方开发板定义
     - `GitHub <https://github.com/espressif/esp-board-manager/tree/main/esp_boards>`__
   * - ``m5stack_boards``
     - M5Stack 开发板定义
     - `GitHub <https://github.com/espressif/esp-board-manager/tree/main/m5stack_boards>`__
   * - ``esp_friends_boards``
     - 社区开发板定义
     - `GitHub <https://github.com/espressif/esp-board-manager/tree/main/esp_friends_boards>`__

WebRTC 方案另有独立的板级外设抽象，与 Board Manager 分开维护，见 `GitHub <https://github.com/espressif/esp-webrtc-solution/tree/main/components/codec_board>`__。

相关链接：

- `组件管理器 <https://components.espressif.com/components/espressif/esp_board_manager>`__
- `GitHub 仓库 <https://github.com/espressif/esp-board-manager>`__
- `在线文档 <https://docs.espressif.com/projects/esp-board-manager/zh_CN/latest/index.html>`__
