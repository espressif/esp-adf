.. _index-sec-00:

乐鑫高级开发框架指南
===========================

:link_to_translation:`en:[English]`

.. list-table::
   :widths: 33 33 34

   * - |快速入门|_
     - |多媒体例程|_
     - |方案中心|_
   * - `快速入门`_
     - `多媒体例程`_
     - `方案中心`_
   * - |多媒体基础组件|_
     - |多媒体上层组件|_
     - |多媒体开发板|_
   * - `多媒体基础组件`_
     - `多媒体上层组件`_
     - `多媒体开发板`_

.. |快速入门| image:: ../_static/index/get-started.png
.. _快速入门: get-started/index.html

.. |多媒体开发板| image:: ../_static/index/multimedia-boards.png
.. _多媒体开发板: multimedia-boards/index.html

.. |多媒体例程| image:: ../_static/index/multimedia-examples.png
.. _多媒体例程: multimedia-examples/index.html

.. |多媒体基础组件| image:: ../_static/index/basic-components.png
.. _多媒体基础组件: basic-components/index.html

.. |多媒体上层组件| image:: ../_static/index/multimedia-services.png
.. _多媒体上层组件: multimedia-services/index.html

.. |方案中心| image:: ../_static/index/solution-center.png
.. _方案中心: solution-center/index.html

.. _index-sec-01:

.. rubric:: 概述

`ESP-ADF <https://github.com/espressif/esp-adf>`__\ （Espressif Advanced Development Framework）是乐鑫官方的高级应用层开发框架，基于 `ESP-IDF <https://github.com/espressif/esp-idf>`__ 与 `ESP-GMF <https://github.com/espressif/esp-gmf>`__ 构建，面向音频、视频与 IoT 产品的多媒体应用开发。ESP-ADF v3.0 以产品级功能、模块化服务与低资源占用为目标，提供音视频采集与播放、AI 语音、蓝牙音频、多媒体传输等方案组件；各类应用组件已发布至 `IDF 组件管理器 <https://components.espressif.com/>`__，可在工程中声明依赖后按需获取。

.. figure:: ../_static/adf_block_diagram.png
    :align: center
    :width: 100%
    :alt: 乐鑫高级开发框架
    :figclass: align-center

    乐鑫高级开发框架

.. _index-sec-02:

.. rubric:: 新特性

- **基于 ESP-GMF**：媒体处理链使用 `ESP-GMF <https://github.com/espressif/esp-gmf>`__ 重构，在同一框架内统一音频、视频、图像与通用流式数据的处理
- **独立组件**：功能组件可独立集成与运行，无需拉取整个仓库
- **产品服务**：提供音频播放、视频播放、电池监控等模块化服务
- **MCP 支持**：产品服务可通过 Model Context Protocol（MCP）调用
- **多产品覆盖**：面向音频、视频与 IoT 产品场景
- **资源占用优化**：针对低内存与低 CPU 占用进行优化
- **多语言应用**：支持 MicroPython、Arduino 与 C/C++ 开发

.. _index-sec-03:

.. rubric:: 与旧版 ESP-ADF 的关系

ESP-ADF v3.0 是一次架构升级，与 ESP-ADF v2.x 在 API 与行为上不兼容。分支定位如下：

- ``master``：ESP-ADF v3.0 开发主线，基于 ESP-GMF 提供组件与产品级方案案例
- ``release/v2.x``：旧版 ESP-ADF（v2）维护分支，仅接收缺陷修复与小幅增强

.. toctree::
    :hidden:

    get-started/index
    multimedia-examples/index
    solution-center/index
    basic-components/index
    multimedia-services/index
    knowledge-center/index
    multimedia-boards/index
    tools/index
    resources
    COPYRIGHT
    免责声明和版权公告 <disclaimer-and-copyright>
    english-chinese-glossary
    about
