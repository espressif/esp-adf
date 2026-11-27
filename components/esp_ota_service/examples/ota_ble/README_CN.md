# BLE OTA 示例

- [English Version](./README.md)
- 例程难度：⭐⭐

## 例程简介

- 本例程通过低功耗蓝牙（BLE）接收固件镜像，原生实现官方 **ESP BLE OTA** 手机应用协议（GATT 服务 `0x8018`、固件特征 `0x8020`、控制特征 `0x8022`，按 4 KB 扇区进行 CRC16-CCITT 应答校验），完成应用描述符校验、写入空闲 OTA 槽位并重启切入新固件。
- 演示了 `esp_ota_service` 搭配内置 NimBLE 数据源 `esp_ota_service_source_ble_create()`、`esp_ota_service_checker_app_create()`、`esp_ota_service_target_app_create()` 以及启动阶段的回滚确认（`esp_ota_service_confirm_update()`），无需依赖额外的外部 BLE OTA 托管组件。

### 典型场景

适用于无 Wi-Fi 环境下的智能设备，通过手机端官方 [ESP BLE OTA](https://github.com/EspressifApps/esp-ble-ota-android/releases) APP 或 PC 端自动化蓝牙桥接脚本（`../../tools/ota_ble.py`）进行近场固件升级。

### 运行机制

1. 上电启动，打印工程与运行分区信息，初始化 NVS，并调用 `esp_ota_service_ble_example_confirm_running_image()` 在 `PENDING_VERIFY` 状态下确认新固件有效。
2. 创建 `esp_ota_service` 并订阅事件，依次初始化 `esp_ota_service_source_ble_create()`、`esp_ota_service_target_app_create()` 和 `esp_ota_service_checker_app_create(NULL)`。
3. 通过 `esp_ota_service_set_upgrade_list()` 提交 `ble://` 升级项（`resumable = false`），调用 `esp_service_start()` 启动服务。
4. 通过 NimBLE 广播设备名 `OTA_BLE`，等待对端连接并发送 `CMD_START` 指令，校验远端固件头 `esp_app_desc_t`（要求更高的 `x.y.z` 版本号且 `project_name` 一致），按 4 KB 扇区流式写入目标分区。
5. 收到无失败项的 `ESP_OTA_SERVICE_EVT_SESSION_END` 后销毁服务，调用 `esp_restart()` 重启进入新固件。

### 文件结构

```text
├── main
│   ├── app_main.c
│   ├── CMakeLists.txt
│   ├── esp_ota_service_ble_example.c
│   ├── esp_ota_service_ble_example.h
│   ├── esp_ota_service_ble_example_err.h
│   └── idf_component.yml
├── CMakeLists.txt
├── partitions.csv
├── pytest_esp_ota_service_ota_ble.py
├── README.md
├── README_CN.md
└── sdkconfig.defaults
```

## 环境配置

### 硬件要求

- 一块支持 BLE 且至少具备 4 MB Flash（`CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y`）的开发板（默认适配 ESP32-S3）。
- 一台安装 **ESP BLE OTA** APP 的手机，或一块烧录了 ESP-AT 固件的 ESP32-C6 开发板（配合 `../../tools/ota_ble.py` 自动化传输）。
- USB 数据线（供电、烧录与串口监控）。

### 默认 IDF 分支

本例程支持 IDF release/v5.4 及以后的分支，例程默认使用 IDF release/v5.5 分支。

### 软件要求

- 手机端：[ESP BLE OTA Android 发布包](https://github.com/EspressifApps/esp-ble-ota-android/releases)；或 PC 端脚本 `../../tools/ota_ble.py`（需安装 `pip install pyserial esptool`）。
- 固件版本约定：`CMakeLists.txt` 中设置了初始基线 `project(ota_ble VERSION 1.0.0)`（同步维护在 `../../tools/_fw_common.py`），无 `version.txt` 时直接执行 `idf.py build` 会嵌入合法的 `1.0.0` 基线版本；执行 `../../tools/build_firmware.py ble -v 1.0.1` 构建升级包时会写入 `version.txt`（已加入根目录 `.gitignore`）并发布 `ota_ble_v1.0.1.bin`，从而满足 `esp_ota_service_checker_app_create(NULL)` 的高版本校验要求。

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境；若已配置可跳过本段，直接进入工程目录。若未配置，请在 ESP-IDF 根目录运行以下脚本完成环境设置，完整步骤请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/index.html)。

```bash
./install.sh
. ./export.sh
```

从仓库根目录进入本例程工程目录并设置芯片目标：

```bash
cd components/esp_ota_service/examples/ota_ble
idf.py set-target esp32s3
```

### 项目配置

```bash
idf.py menuconfig
```

主要配置路径（`sdkconfig.defaults` 已默认启用）：

- `ESP-Service: OTA Service` → `Download sources` → `Enable BLE OTA source (official ESP BLE OTA APP protocol)`（`CONFIG_OTA_ENABLE_BLE_SOURCE=y`）
- `ESP-Service: OTA Service` → `Verification and rollback` → `Require higher firmware version by default`（`CONFIG_OTA_FORCE_VERSION_CHECK=y`）
- `ESP-Service: OTA Service` → `Verification and rollback` → `Enable OTA rollback support`（`CONFIG_OTA_ENABLE_ROLLBACK=y`）

> 配置完成后按 `s` 保存，然后按 `Esc` 退出。

### 编译与烧录

- 编译初始 `1.0.0` 基线固件：

```bash
rm -f version.txt
idf.py build
```

- 烧录程序并运行 monitor 工具来查看串口输出（替换 `PORT` 为端口名称）：

```bash
idf.py -p PORT flash monitor
```

- 退出调试界面使用 `Ctrl-]`

## 如何使用例程

### 功能和用法

#### 方式一：使用官方 ESP BLE OTA 手机应用

1. 构建 `1.0.1` 升级固件（`cd components/esp_ota_service/tools && ./build_firmware.py ble -v 1.0.1`），将生成的 `tools/firmware_samples/ota_ble_v1.0.1.bin` 传到手机。
2. 在 `examples/ota_ble` 目录烧录 `1.0.0` 基线固件并打开串口监控（`rm -f version.txt && idf.py -p PORT build flash monitor`）。
3. 打开 **ESP BLE OTA** APP，扫描并连接 `OTA_BLE`，选择 `ota_ble_v1.0.1.bin` 开始升级；传输完成后设备自动重启运行 `1.0.1`。

#### 方式二：通过 ESP32-C6 ESP-AT 桥接器由 PC 推送

```bash
cd components/esp_ota_service/tools
./build_firmware.py ble -v 1.0.1
./ota_ble.py upload --port /dev/ttyUSB2
```

C6 固件烧录与探测（`flash-c6` / `check-c6`）详见 `../../tools/README.md`。

### 日志输出

`ota_ble` 初始化 NimBLE GATT 服务并等待 BLE Central 连接的连续启动日志：

```text
I (269) app_init: Project name:     ota_ble
I (381) esp_ota_svc_ble_ex:   BLE OTA (esp_ota_service + APP protocol)
I (387) esp_ota_svc_ble_ex:   Project:  ota_ble
I (441) OTA_BLE: Confirm_running_image: image is not pending verify, nothing to do
I (441) ADF_EVENT_HUB: Create 'esp_ota_service': domain registered
I (443) ESP_SERVICE: [esp_ota_service] Initialized
I (448) OTA_BLE: Run_ota: BLE OTA (APP protocol) ready. Use ESP BLE OTA APP to connect and upgrade.
I (457) OTA_BLE: [OTA] Session started
I (545) ESP_SERVICE: [esp_ota_service] Started
```

## 故障排除

### 等待 BLE 连接超时

```text
E (180550) OTA_BLE: [OTA] Failed: ESP_ERR_TIMEOUT (reason=4)
```

在 `wait_start_timeout_ms`（本例程设为 180 秒）内没有 BLE 对端连接并发送 `CMD_START`。请复位开发板并在 3 分钟内通过手机 APP 或 `ota_ble.py` 发起传输。

### 升级固件被版本检查跳过

```text
W (4210) OTA_BLE: [OTA] Firmware is not newer - skipping
```

`esp_ota_service_checker_app_create(NULL)` 拒绝了版本号不高于当前运行版本（或 `project_name` 不为 `ota_ble`）的镜像。请使用 `./build_firmware.py ble -v 1.0.1` 生成更高版本的升级包。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
