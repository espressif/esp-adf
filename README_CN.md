# Espressif 高级开发框架

[English Version](./README.md)

## ⚠️ 重要提示

`master` 分支包含 **ADF v3.0** 的新功能，正在持续开发中，与之前的版本**不兼容**。如需使用稳定的 ADF v2.x 版本，请切换到 [`release/v2.x`](https://github.com/espressif/esp-adf/tree/release/v2.x) 分支。

## 概述

**ESP-ADF** (Espressif Advanced Development Framework) 是乐鑫官方的**高级应用层开发框架**，旨在简化音频、视频和 IoT 产品的开发。

**ADF v3.0** 是一次**重大架构升级**，经过全新设计，与 ADF v2.x **不兼容**。它专注于面向产品的功能、模块化服务和低资源消耗。

### ESP-ADF vs ESP-IDF

- **ESP-IDF** 是乐鑫芯片的基础 SDK，提供操作系统、驱动程序、网络协议栈、蓝牙等底层组件。
- **ESP-ADF** 基于 ESP-IDF 构建，提供面向产品的高级服务，如音视频播放、电池服务、OTA 等。

## ✅ ADF v3.0 特性

**ADF v3.0** 是一个**面向产品的开发框架**，具有**重大架构变更**，与之前的 ADF 版本**完全不兼容**。主要特性包括：

- **ESP-GMF 集成**：核心媒体管道使用 [**ESP-GMF**](https://github.com/espressif/esp-gmf)（通用多媒体框架）重构
- **独立组件**：功能组件（如 `playlist`）可独立运行
- **产品服务**：提供**音频播放**、**视频播放**、**电池监控**等模块化服务
- **MCP 支持**：产品服务可通过 **Model Context Protocol (MCP)** 调用
- **多产品支持**：专为**音频**、**视频**和 **IoT** 产品设计
- **资源高效**：优化**低内存和 CPU 占用**
- **多语言应用支持**：支持 **MicroPython**、**Arduino** 和 **C/C++** 开发

## ADF v3.0 架构图

<div align="center">
  <img src="docs/_static/adf_block_diagram.png" alt="ADF 架构图" width="100%" />
</div>

## 平台要求

- **ESP-IDF 版本**：v5.5.2 或更高

## 分支策略

- [`release/v2.x`](https://github.com/espressif/esp-adf/tree/release/v2.x)：旧版 ADF 实现分支 (v2)，仅接收 bug 修复和小幅增强。
  参考 [ADF v2.x 文档](https://docs.espressif.com/projects/esp-adf/zh_CN/release-v2.x/index.html)。

- `master`：**ADF v3.0** 的开发分支，与 v2.x API 和行为不兼容。

## 文档

- ADF v3.0 文档：访问 [docs.espressif.com](https://docs.espressif.com/projects/esp-adf/zh_CN/latest/)
- ADF v2.x 文档：访问 [docs.espressif.com](https://docs.espressif.com/projects/esp-adf/zh_CN/release-v2.x/)

## 路线图

- [ ] 功能模块：如 **Playlist**、**Board Manager**
- [ ] 服务层：如支持 **MCP (Model Context Protocol)** 的 **Wi-Fi 服务**
- [ ] AI 服务集成：如 **OpenAI**、**Coze** 等
- [ ] 扩展可复用的产品服务插件
