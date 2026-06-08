# 音频低功耗与唤醒

- [English Version](./README.md)
- 例程难度：⭐⭐

## 例程简介

本例程演示设备在 Wi-Fi 联网与 MQTT keepalive 保活下的自动 light sleep 低功耗流程，以及 UART、MQTT、GPIO、定时器等多种唤醒源的处理；进入 idle 前与唤醒后可播放提示音。

- 连接 Wi-Fi 后，MQTT 在 Auto light sleep 模式下维持保活
- 支持 UART、MQTT、GPIO、定时器等多种唤醒方式
- 例程执行两轮「休眠—唤醒」验证，每轮唤醒后恢复运行并继续下一轮

### 典型场景

本例程适用于空闲时需保持联网、并支持本地或远程唤醒的语音/音频类设备。

### 提示音资源

提示音文件存放在 `tone/` 目录，编译时通过 `littlefs_create_partition_image()` 打包进名为 `storage` 的 LittleFS 分区，挂载路径为 `/littlefs`。如需更换提示音，替换 `tone/` 目录中的文件后重新编译烧录即可。

## 环境配置

### 硬件要求

- 开启提示音播放时需 Audio DAC 和扬声器；关闭 `Enable prompt tone playback` 时可仅验证低功耗与唤醒
- 可连接到 MQTT broker 的 Wi-Fi 网络
- 可通过串口输入触发 UART 唤醒；如启用 GPIO 唤醒，需连接可控 GPIO 输入

### 默认 IDF 分支

本例程支持 IDF release/v5.4 (>= v5.4.3) 与 release/v5.5 (>= v5.5.2) 分支。

### 软件要求

- 默认 MQTT broker：`mqtt://broker.emqx.io`
- 默认 MQTT 唤醒 topic：`/gmf/audio_power_save/wakeup`

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境；若已配置可跳过本段，直接进入工程目录。若未配置，请在 ESP-IDF 根目录运行以下脚本完成环境设置，完整步骤请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/index.html)。

```
./install.sh
. ./export.sh
```

下面是简略步骤：

- 进入本例程工程目录：

```
cd adf_examples/system/audio_power_save
```

本示例使用 [ESP Board Manager](https://github.com/espressif/esp-board-manager) 管理板级资源。安装辅助工具 [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) 后，可通过下文命令选择开发板。

- 在已激活的 ESP-IDF Python 环境下安装（同一环境只需安装一次）：

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist
```

- 查看支持的板子：

```bash
idf.py bmgr -l
```

  输出示例：

```text
ℹ️  Main Boards:
  [1] dual_eyes_board_v1_0
  [2] esp32_c3_lyra
  [3] esp32_c5_spot
  [4] esp32_p4_function_ev
  [5] esp32_s3_korvo2_v3
  [6] esp32_s3_korvo2l
  [7] esp_box_3
  [8] esp_box_lite
  [9] esp_hi
```

- 选择开发板：

```bash
idf.py bmgr -b <board_index|board_name>
```

  本例程使用 `esp32_s3_korvo2_v3`：

```bash
idf.py bmgr -b 5
# 或
idf.py bmgr -b esp32_s3_korvo2_v3
```

  首次执行 `idf.py bmgr` 时，组件会根据本工程 `main/idf_component.yml` 中声明的 `espressif/esp_board_manager` 依赖自动下载。

> [!NOTE]
> 如果切换为其他 `esp_board_manager` 支持的开发板，请按相同步骤执行并替换板型名称/索引。
> 自定义开发板请参考 [自定义开发板指南](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/docs/how_to_customize_board_cn.md)。
> `esp_board_manager` 更多信息请参考 [ESP_BOARD_MANAGER 入门指南](https://github.com/espressif/esp-board-manager/blob/main/esp_board_manager/README_CN.md)。

### 项目配置

```bash
idf.py menuconfig
```

在 menuconfig 中进行以下配置：

- `Wi-Fi Configuration` → `WiFi SSID`
- `Wi-Fi Configuration` → `WiFi Password`
- `MQTT Configuration` → `MQTT broker URI`
- `MQTT Configuration` → `MQTT wakeup topic`
- `MQTT Configuration` → `MQTT status topic`
- `MQTT Configuration` → `MQTT keepalive interval in seconds`
- `Power Management Configuration` → `Maximum CPU frequency in MHz`
- `Power Management Configuration` → `Minimum CPU frequency in MHz`
- `Enable prompt tone playback`
- `Wakeup Source Configuration` → `Timer wakeup delay in milliseconds`
- `Wakeup Source Configuration` → `Maximum wakeup wait time in milliseconds`
- `Wakeup Source Configuration` → `Enable GPIO wakeup source`
- `Wakeup Source Configuration` → `GPIO wakeup number`（需先启用 GPIO 唤醒）
- `Wakeup Source Configuration` → `GPIO wakeup active level`（Low level / High level）
- `Wakeup Source Configuration` → `Enable UART wakeup source`

> 配置完成后按 `s` 保存，然后按 `Esc` 退出。

### 编译与烧录

- 编译示例程序：

```
idf.py build
```

- 烧录程序并运行 monitor 工具来查看串口输出 (替换 PORT 为端口名称)：

```
idf.py -p PORT flash monitor
```

- 退出调试界面使用 `Ctrl-]`

## 如何使用例程

### 功能和用法

烧录运行后，例程按以下顺序执行：

1. 初始化提示音播放资源并配置低功耗运行参数。
2. 连接 Wi-Fi 并启动 MQTT keepalive，等待 MQTT client 连接成功。
3. 配置 UART / MQTT / GPIO / 定时器等唤醒源。
4. 进入两轮「休眠—唤醒」验证循环，每轮包括：
   - 播放 `enter_sleep.mp3`，随后释放提示音播放资源；
   - 进入空闲低功耗（自动 light sleep + Wi-Fi modem sleep），等待唤醒；
   - 唤醒后恢复提示音播放资源并播放 `exit_sleep.mp3`，再运行约 3 秒后继续下一轮。
5. 两轮验证完成后释放全部资源并结束。

每次进入空闲低功耗后，UART、MQTT、GPIO 或定时器均可触发唤醒。开启提示音播放时（`CONFIG_EXAMPLE_ENABLE_PROMPT_PLAYBACK=y`），例程会在进入 idle 前播放 `enter_sleep.mp3`，唤醒后播放 `exit_sleep.mp3`。

### 日志输出

成功运行时的关键 log 如下。自动化测试（`pytest_audio_power_save.py`）第二轮通过向默认 MQTT topic 发布消息触发唤醒；下文 log 片段为实机采集，第二轮为 GPIO（默认 GPIO0 / BOOT 键）唤醒，二者均符合例程行为：

```text
I (1700) AUDIO_POWER_SAVE: [ 1 ] Initialize audio power save
I (1740) AUDIO_POWER_SAVE: [ 2 ] Connect Wi-Fi and start MQTT keepalive
W (1799) wifi:Haven't to connect to a suitable AP now!
I (1801) NETWORK_MGR: Connect Wi-Fi, ssid:ESP-Audio, listen_interval:10
W (1805) wifi:Haven't to connect to a suitable AP now!
I (1805) NETWORK_MGR: STA_CONFIG: listen_interval=10
W (1809) wifi:Password length matches WPA2 standards, authmode threshold changes from OPEN to WPA2
I (4440) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_NONE, reason: Wi-Fi connected
I (5182) NETWORK_MGR: MQTT connected, broker=mqtt://broker.emqx.io, keepalive=30 s
I (5186) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_NONE, reason: prepare wakeup sources
I (5187) AUDIO_POWER_SAVE: [ 3 ] Configure wakeup sources
I (5192) WAKEUP_MGR: Waiting for GPIO0 to become inactive (level=1)...
I (5198) WAKEUP_MGR: GPIO wakeup enabled, gpio=0, active level=0
I (5215) WAKEUP_MGR: UART wakeup enabled, uart=0, threshold=3
I (5216) WAKEUP_MGR: Timer wakeup configured, timeout=30000 ms
I (5216) AUDIO_POWER_SAVE: [ 4 ] Enter idle and wait for wakeup
W (5229) ESP_GMF_ASMP_DEC: Not enough memory for out, need:1152, old: 1024, new: 1152
E (6929) i2s_common: i2s_channel_disable(1262): the channel has not been enabled yet
W (6930) PERIPH_I2S: Caution: Releasing TX (0x3c1c09e0).
W (6931) PERIPH_I2S: Caution: RX (0x3c1c0b9c) forced to stop.
I (6937) AUDIO_POWER_SAVE: Enter idle and wait for wakeup
I (6942) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_MAX_MODEM, reason: enter idle low power
I (6950) WAKEUP_MGR: Player idle; waiting for wakeup (UART/MQTT/GPIO/timer) in automatic light sleep
I (10831) WAKEUP_MGR: Wakeup event: uart, event=8
I (10833) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_NONE, reason: wakeup handled
I (10833) WAKEUP_MGR: Wakeup handled by UART
I (10836) AUDIO_POWER_SAVE: Exit idle after UART wakeup
W (10888) ESP_GMF_ASMP_DEC: Not enough memory for out, need:1152, old: 1024, new: 1152
E (17371) i2s_common: i2s_channel_disable(1262): the channel has not been enabled yet
W (17372) PERIPH_I2S: Caution: Releasing TX (0x3c1c322c).
W (17373) PERIPH_I2S: Caution: RX (0x3c1c33e8) forced to stop.
I (17379) AUDIO_POWER_SAVE: Enter idle and wait for wakeup
I (17384) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_MAX_MODEM, reason: enter idle low power
I (17392) WAKEUP_MGR: Player idle; waiting for wakeup (UART/MQTT/GPIO/timer) in automatic light sleep
I (20830) NETWORK_MGR: Wi-Fi power save mode: WIFI_PS_NONE, reason: wakeup handled
I (20830) WAKEUP_MGR: Wakeup handled by GPIO
I (20830) AUDIO_POWER_SAVE: Exit idle after GPIO wakeup
W (20883) ESP_GMF_ASMP_DEC: Not enough memory for out, need:1152, old: 1024, new: 1152
I (25654) AUDIO_POWER_SAVE: Wakeup validation done
I (25654) AUDIO_POWER_SAVE: [ 5 ] Destroy all the resources
E (26039) i2s_common: i2s_channel_disable(1262): the channel has not been enabled yet
W (26039) PERIPH_I2S: Caution: Releasing TX (0x3c1c322c).
W (26041) PERIPH_I2S: Caution: RX (0x3c1c33e8) forced to stop.
I (26047) AUDIO_POWER_SAVE: Func:app_main, Line:76, MEM Total:8629212 Bytes, Inter:281251 Bytes, Dram:281251 Bytes

I (26056) AUDIO_POWER_SAVE: Example finished
```

## 故障排除

### MQTT broker 连接超时

如果日志出现如下提示，说明当前网络无法连接已配置的 MQTT broker。由于本例程现在会等待 MQTT 连接成功后才进入空闲低功耗，只有配置可访问的 broker 后才会继续执行唤醒流程：

```text
E (17164) esp-tls: [sock=54] select() timeout
E (17164) transport_base: Failed to open a new connection: 32774
E (17164) mqtt_client: Error transport connect
W (17167) NETWORK_MGR: MQTT error
I (17170) NETWORK_MGR: MQTT disconnected
```

如需验证 MQTT 唤醒，请将 `CONFIG_EXAMPLE_MQTT_BROKER_URI` 配置为当前网络可访问的 broker。

## 相关参考

- [低功耗保活常见问题总结](https://github.com/espressif/esp-adf/blob/release/v2.x/examples/system/power_save/README_CN.md#%E6%95%85%E9%9A%9C%E6%8E%92%E9%99%A4)
- [Wi-Fi 低功耗模式](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/api-guides/low-power-mode/low-power-mode-wifi.html)

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
