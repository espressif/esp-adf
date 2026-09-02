蓝牙音频
================

:link_to_translation:`en:[English]`

简介
----------

蓝牙音频方案在同一套软件上覆盖经典蓝牙与 LE Audio。设备可作为音箱接收手机或 PC 的音乐并支持免提通话，也可作为音源将本地音频发送到耳机或音箱，或作为 Auracast 广播的发送端与接收端。对接现有手机蓝牙生态时，使用 A2DP 播放、AVRCP 控制与 HFP 通话；同一软件栈同时支持 LE Audio 单播、广播与多设备同步播放。

推荐使用同时支持经典蓝牙与 Bluetooth 5.4 (LE) 的 ESP32-S31。仅实现经典蓝牙音频时，可选用 ESP32。

.. only:: html

   .. mermaid::

      flowchart LR
          Phone["手机或 PC"]
          Soc["SoC"]
          Spk["扬声器"]
          Mic["麦克风"]
          Head["耳机或音箱"]
          Phone -->|"A2DP / HFP / LE Audio"| Soc
          Mic --> Soc
          Soc --> Spk
          Soc -->|"A2DP Source / Auracast"| Head

应用场景
----------

- **蓝牙音箱**：手机或 PC 通过 A2DP 向设备发送音乐，设备作为 A2DP Sink 播放，并支持上一曲、暂停、下一曲与音量联动
- **带屏中控**：车载或桌面终端显示曲目、封面与歌词，并在设备端完成播放控制与拨号
- **免提通话**：HFP 将下行语音输出到扬声器，麦克风采集上行语音，适用于带屏音箱、车载中控与桌面会议终端
- **蓝牙音源**：设备发现并连接耳机或音箱，将本地或 SD 卡中的音频发送到远端播放
- **TWS 与助听器**：LE Audio 单播（CIS）用于点对点连接或左右耳同步播放
- **共享收听**：Auracast 广播（BIS）由一个发送端向多个接收端分发音频，用于展厅讲解、会议同传、公共电视收听与多房间同步播放

功能特性
----------

- 双模蓝牙：同一产品可同时使用经典蓝牙（BR/EDR）与 BLE 5.4，兼容已有蓝牙设备与 LE Audio 终端
- 经典蓝牙协议：A2DP Sink / Source、AVRCP 播放控制与元数据、HFP 免提通话、PBAP 通讯录与通话记录
- LE Audio：CIS 单播、BIS / Auracast 广播、TMAP 电话与媒体角色组合
- ``esp_bt_audio`` 统一上报连接、音频流、播放控制、音量与通话状态
- 可选触屏界面：媒体播放、拨号盘与音量条
- 通话上行支持回声消除（AEC）
- ESP32-S31 集成硬件 ASRC，由硬件完成采样率转换

芯片对比
----------

蓝牙音频相关能力对照：

.. list-table::
   :header-rows: 1
   :widths: 22 26 26 26

   * - 项目
     - ESP32
     - ESP32-S3
     - ESP32-S31
   * - CPU
     - Xtensa LX6 双核，最高 240 MHz
     - Xtensa LX7 双核，最高 240 MHz
     - RISC-V 双核，最高 320 MHz
   * - SRAM
     - 520 KB
     - 512 KB
     - 512 KB
   * - 经典蓝牙
     - 支持（v4.2 BR/EDR）
     - 不支持
     - 支持（BR/EDR）
   * - 低功耗蓝牙
     - Bluetooth LE 4.2
     - Bluetooth 5 (LE)
     - Bluetooth 5.4 (LE)
   * - LE Audio
     - 不支持
     - 不支持
     - 支持（CIS / BIS）
   * - 硬件 ASRC
     - 不支持
     - 不支持
     - 支持
   * - I2S
     - 2 路
     - 2 路
     - 2 路，硬件蓝牙音频同步

采样率转换占用对照：

.. list-table::
   :header-rows: 1
   :widths: 28 36 36

   * - 指标
     - ESP32-S31 硬件 ASRC
     - ESP32-S3 软件 SRC
   * - 平均 CPU 占用
     - 0.79%
     - 2.35%
   * - 峰值 CPU 占用
     - 1.86%
     - 14.17%
   * - 平均内存
     - 539 B
     - 19400 B
   * - 峰值内存
     - 672 B
     - 87000 B

硬件与软件
----------------

ESP32-S31 同时支持经典蓝牙与 LE Audio，作为本方案的推荐主控。仅实现 A2DP、AVRCP 与 HFP 时，可选用 ESP32。ESP32-S3 仅支持 Bluetooth LE，不支持经典蓝牙与 LE Audio。

蓝牙角色与事件由 ``esp_bt_audio`` 管理，音频流经 ESP-GMF 处理链完成解码、音效、回声消除与播放。板级外设由 ``esp_board_manager`` 初始化。示例见 `esp_bt_audio/examples/bt_audio <https://github.com/espressif/esp-gmf/tree/main/packages/esp_bt_audio/examples/bt_audio>`__，例程索引见 :doc:`../multimedia-examples/index`。

参考资料
----------

- `ESP32-S31 应用方案系列：音频应用 <https://mp.weixin.qq.com/s/eHD7VdHQmJoQnAQPkKLGtA>`__
- `esp_bt_audio 组件 <https://components.espressif.com/components/espressif/esp_bt_audio>`__
- `ESP Bluetooth Audio 文档 <https://docs.espressif.com/projects/esp-gmf/zh_CN/latest/gmf-framework/gmf-package/esp-bt-audio.html>`__
- `bt_audio 例程 <https://github.com/espressif/esp-gmf/tree/main/packages/esp_bt_audio/examples/bt_audio>`__
- `ESP32 技术规格书 <https://documentation.espressif.com/esp32_datasheet_cn.pdf>`__
- `ESP32-S3 技术规格书 <https://documentation.espressif.com/esp32-s3_datasheet_cn.pdf>`__
- `ESP32-S31 技术规格书 <https://documentation.espressif.com/esp32-s31_datasheet_cn.pdf>`__
