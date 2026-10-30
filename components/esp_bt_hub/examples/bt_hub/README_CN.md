# 共享蓝牙 Hub 控制台

- [English Version](./README.md)
- 例程难度：⭐⭐

## 例程简介

本例程演示多个应用模块如何通过 `esp_bt_hub` 共享同一个蓝牙控制器和主机协议栈。例程可运行在 ESP-IDF 支持且具有蓝牙控制器和主机后端的任意芯片上。串口控制台会根据所选芯片启用的能力注册相应命令，包括 Bluetooth Classic、BLE 或双模能力。

控制台可创建相互独立的 client、合并各 client 的 Classic 扫描模式请求，并在相应能力可用时管理 Classic inquiry、LE 扩展扫描和 LE 扩展广播等独占资源。例程还会为每个已启用的 GAP 域注册两个订阅者：一个解析发现结果，另一个观察并统计原生 GAP 事件，用于演示在不替换底层回调的情况下将事件分发给多个订阅者。

### 典型场景

- 多个蓝牙应用模块共用同一个主机协议栈。
- 将蓝牙操作集成到应用前，验证资源所有权和释放顺序。
- 在不同蓝牙芯片上验证 Classic、BLE 或双模配置。
- 通过控制台查看 Classic 发现结果和 LE 扩展广播报告。

### 运行机制

1. 应用初始化 NVS 和 `esp_bt_hub`，然后启动 `bt_hub>` 控制台。
2. 每个应用模块打开一个具名 client。
3. client 提交共享的扫描模式请求，或取得独占操作的租约。
4. GAP 事件同时分发给两个已注册的订阅者。
5. 租约所有者停止并释放操作后，才能关闭对应 client。
6. 所有 client 关闭后，hub 可以停止并重新启动。

## 环境配置

### 硬件要求

- 任意一块芯片及当前 ESP-IDF 版本支持蓝牙的 ESP 开发板
- 用于烧录和串口交互的 USB 线
- 如需查看 Classic inquiry 结果，准备一台附近的可发现 Bluetooth Classic 设备
- 如需查看 LE 扫描结果，准备一台附近发送扩展广播包的设备

### 默认 IDF 分支

`esp_bt_hub` 组件要求 ESP-IDF v5.5 或更高版本。请使用支持所选芯片及蓝牙主机后端的 ESP-IDF 版本。

本例程不使用 `esp_board_manager`。

## 编译和下载

### 编译准备

在 ESP-IDF 根目录配置开发环境：

```bash
./install.sh
. ./export.sh
```

然后从 esp-adf 仓库根目录进入例程：

```bash
cd components/esp_bt_hub/examples/bt_hub
```

### 选择目标芯片并构建

将 `TARGET` 替换为当前 ESP-IDF 支持的蓝牙芯片，例如 `esp32`、`esp32s3` 或其他受支持的 target：

```bash
idf.py set-target TARGET
idf.py build
idf.py -p PORT flash monitor
```

如果当前 ESP-IDF 仍将 ESP32-S31 的 target 选择作为预览功能，请使用：

```bash
idf.py --preview set-target esp32s31
```

各 target defaults 会选择对应芯片使用的蓝牙能力：ESP32 使用 Bluedroid 双模，ESP32-S3 使用 Bluedroid BLE，ESP32-S31 使用 Bluedroid 双模并启用 BLE 5.0 扩展扫描和扩展广播。

请将 `PORT` 替换为开发板串口。使用 `Ctrl-]` 退出 monitor。

### ESP32-S31 NimBLE 纯 LE 构建

例程为 ESP32-S31 提供了可直接使用的 NimBLE 默认配置。使用单独的构建目录和 sdkconfig 文件，避免 Bluedroid 与 NimBLE 配置相互覆盖：

```bash
idf.py --preview -B build-nimble \
  -D SDKCONFIG=sdkconfig.nimble \
  -D SDKCONFIG_DEFAULTS=sdkconfig.defaults.esp32s31.nimble \
  set-target esp32s31
idf.py -B build-nimble build
idf.py -B build-nimble -p PORT flash monitor
```

该配置启用 NimBLE central、peripheral、broadcaster 和 observer 角色，以及扩展扫描和扩展广播。Bluetooth Classic 和所有 `classic_*` 控制台命令均被关闭。其他支持 NimBLE 的 target 在配置中选择 NimBLE 及所需角色后，也可以运行本例程。

### 项目配置

本例程没有自定义 menuconfig 选项。控制台功能在编译时根据配置选择：

- Classic 命令要求 `ESP_BT_HUB_CLASSIC_SUPPORTED`。
- 本例程演示的 LE 命令使用扩展扫描和扩展广播，因此要求 `ESP_BT_HUB_LE_EXT_SUPPORTED`。
- Bluedroid 和 NimBLE 均可提供 hub 生命周期及 client 命令。

切换主机后端或启用默认配置以外的蓝牙能力时，请运行 `idf.py menuconfig`。

## 如何使用例程

### 启动控制台

应用启动后会显示 `bt_hub>` 提示符。依次执行：

```text
help
hub_status
client_open audio
client_open pan
clients
```

两个 client 分别取得 ID `0` 和 `1`。需要占用资源的命令通过该 ID 记录和检查所有权。

### 验证 Classic 资源共享

本节适用于 `help` 中存在 `classic_*` 命令的构建。

下面的操作为两个 client 分别设置 Classic 扫描模式请求，并由 client `0` 持有 inquiry：

```text
classic_mode 0 1 general
classic_mode 1 1 limited
classic_inquiry_start 0
classic_inquiry_start 1
client_close 0
classic_inquiry_stop 0
```

第二次启动 inquiry 会失败，因为 inquiry 是独占资源。client `0` 持有 inquiry 租约时，关闭操作也会失败。执行 `classic_inquiry_stop 0` 后才能关闭该 client。

### 验证 LE 扫描和广播

本节适用于 target 和配置支持 LE 扩展扫描及扩展广播，并且 `help` 中存在 `le_*` 命令的构建。

使用 client `0` 执行扩展扫描：

```text
le_scan_start 0
hub_status
le_scan_stop 0
```

使用 client `1` 运行一路扩展广播：

```text
le_adv_start 1
hub_status
le_adv_stop 1
```

收到广播包后，解析器会同时打印后端原生事件和归一化后的 `esp_bt_hub_le_adv_report_t` 结果。inquiry 或扫描收到事件后，`hub_status` 中 parser 和 observer 的回调计数均应大于零。

### 停止并重启 hub

停止 hub 前需释放所有租约并关闭所有 client：

```text
client_close 0
client_close 1
hub_stop
hub_status
hub_start
```

### 命令参考

通用命令：

- `hub_status`：显示 hub 状态、活动资源、所有者和 GAP 回调计数。
- `hub_start` / `hub_stop`：启动 hub，或停止空闲 hub。
- `client_open <name>` / `client_close <id>`：打开或关闭 hub client。
- `clients`：列出已打开的 client。

仅在支持并启用 Classic 时注册的 Classic 命令：

- `classic_mode <id> <0|1> <none|limited|general>`
- `classic_inquiry_start <id>` / `classic_inquiry_stop <id>`

仅在支持并启用 LE 扩展功能时注册的 LE 命令：

- `le_scan_start <id>` / `le_scan_stop <id>`
- `le_adv_start <id>` / `le_adv_stop <id>`

### 日志输出

以下日志是 ESP32-S31 上一次 Bluedroid 运行的连续片段：

```text
2026-10-26 10:24:09 bt_hub> client_open audio
2026-10-26 10:24:09 Client opened: id=0 name=audio
2026-10-26 10:24:09 bt_hub> client_open pan
2026-10-26 10:24:10 Client opened: id=1 name=pan
2026-10-26 10:24:10 bt_hub> classic_mode 0 1 general
2026-10-26 10:24:10 Classic mode: success
2026-10-26 10:24:10 bt_hub> classic_mode 1 1 limited
2026-10-26 10:24:10 Classic mode: success
2026-10-26 10:24:10 bt_hub> classic_inquiry_start 0
2026-10-26 10:24:10 I (1972) BT_HUB_EXAMPLE: Classic inquiry state: 1
2026-10-26 10:24:10 Classic inquiry start: success
2026-10-26 10:24:10 bt_hub> classic_inquiry_start 1
2026-10-26 10:24:10 Classic inquiry start failed: ESP_ERR_INVALID_STATE
2026-10-26 10:24:10 Command returned non-zero error code: 0x1 (ERROR)
2026-10-26 10:24:10 bt_hub> client_close 0
2026-10-26 10:24:10 E (2072) ESP_BT_HUB: Close client failed: client still owns a lease
2026-10-26 10:24:10 Client close failed: ESP_ERR_INVALID_STATE
2026-10-26 10:24:10 Command returned non-zero error code: 0x1 (ERROR)
```

失败的命令说明 inquiry 同一时间只能由一个 client 持有，并且所有者必须先释放租约，之后才能关闭。

## 故障排除

### 命令返回 `ESP_ERR_INVALID_STATE`

执行 `hub_status` 和 `clients`，确认活动资源及其所有者。使用同一个 client ID 停止该资源，然后关闭 client。执行 `hub_stop` 前必须关闭所有 client。

### 缺少 Classic 或 LE 命令

执行 `help` 查看当前构建提供的命令。纯 BLE 芯片不会注册 `classic_*` 命令。芯片支持 BLE、但配置未启用扩展扫描和扩展广播时，也不会注册本例程的 `le_scan_*` 或 `le_adv_*` 命令。

### 没有发现结果

启动成功只表示 inquiry 或扫描正在运行。请将一台 Classic 设备或 LE 广播设备放在附近，并确认它正处于可发现或广播状态。

### 应用程序无法放入分区

保留默认配置中的 `CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE=y`。双模 Bluedroid 镜像可能超过默认 1 MB factory 分区。

### 烧录或控制台启动失败

检查串口、USB 线和端口权限，并关闭正在占用该端口的其他程序。本例程默认使用 UART 控制台。

## 技术支持

请通过以下链接获取技术支持：

- 技术支持：[esp32.com](https://esp32.com/viewforum.php?f=20)
- 问题反馈与功能需求：[esp-adf issues](https://github.com/espressif/esp-adf/issues)
