# 文件系统 OTA 示例

- [English Version](./README.md)
- 例程难度：⭐⭐

## 例程简介

- 本例程通过 microSD 卡（默认基于 `esp_board_manager` 挂载到 `/sdcard`）或 U 盘（USB Host MSC 挂载到 `/usb`）执行离线多分区 OTA 升级，在单次会话内同时更新应用固件分区与两个原始数据分区（`userdata` 和 `flash_tone`）。
- 演示了 `esp_ota_service` 组合 `esp_ota_service_source_fs_create()` 与三种不同检查器及目标的批量流水线：`esp_ota_service_checker_app_create()` + `esp_ota_service_target_app_create()`、`esp_ota_service_checker_data_version_create()` + `esp_ota_service_target_data_create()`，以及 `esp_ota_service_checker_tone_desc_create()` + `esp_ota_service_target_data_create()`。

### 典型场景

适用于无网络现场维护或产线离线刷机场景：插入存有升级包（`firmware.bin`、`userdata.bin`、`flash_tone.bin`）的 microSD 卡或 U 盘，一次性完成主程序与配套数据/提示音资源分区的校验与升级。

### 运行机制

1. 上电启动，打印当前运行版本与分区（`ota_0` 或 `ota_1`），初始化 NVS。
2. 挂载配置的存储介质（默认调用 `esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_FS_SDCARD)` 挂载 `/sdcard`，或通过 USB Host MSC 挂载 `/usb`）。
3. 检测 `<挂载点>/firmware.bin`、`<挂载点>/userdata.bin` 和 `<挂载点>/flash_tone.bin`；按顺序组装升级列表：首项为 `firmware.bin`（`resumable = true`、`skip_on_fail = false`），随后附加存在的数据镜像（`resumable = false`、`skip_on_fail = true`）。
4. 启动 OTA 会话：各项检查器分别读取镜像版本头并与当前已安装版本比对，已是最新版本的条目自动跳过。
5. 若至少有一项完成写入（`EVT_OTA_SUCCESS`），销毁服务并调用 `esp_restart()` 重启生效；若全部跳过则卸载存储介质，并打印 `userdata` 与 `flash_tone` 分区头解析结果。

### 文件结构

```text
├── main
│   ├── app_main.c
│   ├── CMakeLists.txt
│   ├── esp_ota_service_fs_example.c
│   ├── esp_ota_service_fs_example.h
│   ├── esp_ota_service_fs_example_usb.c
│   ├── idf_component.yml
│   └── Kconfig.projbuild
├── CMakeLists.txt
├── partitions.csv
├── pytest_esp_ota_service_ota_fs.py
├── README.md
├── README_CN.md
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
└── sdkconfig.defaults.esp32s3
```

## 环境配置

### 硬件要求

- ESP32-S3-Korvo-2 V3 或 ESP32-P4 Function EV Board 开发板（至少 8 MB Flash，`partitions.csv` 包含 `ota_0`、`ota_1`、`userdata`、`flash_tone` 分区）。
- FAT32 格式的 microSD 卡（或选择 USB MSC 后端时的 USB-OTG U 盘）。
- USB 数据线（供电、烧录与串口监控）。

### 默认 IDF 分支

本例程支持 IDF release/v5.4 (>= v5.4.3) 与 release/v5.5 (>= v5.5.2) 分支。

### 软件要求

- 在 ESP-IDF Python 环境中安装 `esp-bmgr-assist` 以生成 `esp_board_manager` 板级配置代码。
- 配套脚本位于 `../../tools/`（`build_firmware.py`、`ota_fs_flash_sdcard.py`、`gen_userdata_bin.py`、`ota_fs_test_resume.py`）。
- 固件版本约定：`CMakeLists.txt` 中设置了初始基线 `project(ota_fs VERSION 0.1.0)`（同步记录于 `../../tools/_fw_common.py`），无 `version.txt` 时直接运行 `idf.py build` 会生成合法的 `0.1.0` 基线固件；执行 `../../tools/build_firmware.py fs -v 0.2.0` 时会生成 `version.txt`（已加入根目录 `.gitignore`）并输出 `ota_fs_v0.2.0.bin` 及带 4 字节语义化版本头的 `userdata_v0.2.0.bin`。

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境；若已配置可跳过本段，直接进入工程目录。若未配置，请在 ESP-IDF 根目录运行以下脚本完成环境设置，完整步骤请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/index.html)。

```bash
./install.sh
. ./export.sh
```

下面是简略步骤：

- 从仓库根目录进入本例程工程目录：

```bash
cd components/esp_ota_service/examples/ota_fs
```

本示例使用 [ESP Board Manager](https://github.com/espressif/esp-board-manager) 管理板级资源，使用辅助工具 [`esp-bmgr-assist`](https://pypi.org/project/esp-bmgr-assist/) 生成板级配置。

- 在已激活的 ESP-IDF Python 环境下安装（同一环境只需安装一次）：

```bash
pip install esp-bmgr-assist
pip install --upgrade esp-bmgr-assist  # 当提示需要更新时执行此命令
```

- 查看支持的开发板：

```bash
idf.py bmgr -l
```

- 选择开发板（例如 `esp32_s3_korvo_2_3`）：

```bash
idf.py -D IDF_TARGET=esp32s3 gen-bmgr-config -b esp32_s3_korvo_2_3
```

首次执行时，组件会根据 `main/idf_component.yml` 中声明的 `espressif/esp_board_manager` 依赖自动下载。

> [!NOTE]
> 如需切换为其他 `esp_board_manager` 支持的开发板，请替换板型名称重新执行上述命令。
> 自定义开发板请参考 [创建开发板指南](https://docs.espressif.com/projects/esp-board-manager/zh_CN/latest/create-board/index.html)。

### 项目配置

```bash
idf.py menuconfig
```

可按需在 menuconfig 中调整存储后端和 OTA 参数：

- `OTA FS Example` → `Storage backend for OTA bin files`（选择 `SD card (via esp_board_manager FS_SDCARD)` 或 `USB flash drive (USB Host MSC)`）
- `ESP-Service: OTA Service` → `Verification and rollback` → `Require higher firmware version by default`（`CONFIG_OTA_FORCE_VERSION_CHECK=y`）
- `ESP-Service: OTA Service` → `Resume (NVS checkpoint)` → `Enable OTA resume (breakpoint continuation)`（`CONFIG_OTA_ENABLE_RESUME=y`）

> 配置完成后按 `s` 保存，然后按 `Esc` 退出。

### 编译与烧录

- 编译初始 `0.1.0` 基线固件：

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

1. 在 `components/esp_ota_service/tools` 下构建并发布 `0.2.0` 升级包（`ota_fs_v0.2.0.bin` + `userdata_v0.2.0.bin`）：

```bash
cd components/esp_ota_service/tools
./build_firmware.py fs -v 0.2.0
```

2. 将 `firmware_samples/ota_fs_v0.2.0.bin` 重命名为 `firmware.bin`、`firmware_samples/userdata_v0.2.0.bin` 重命名为 `userdata.bin`、`firmware_samples/flash_tone_v0.5.0.bin` 重命名为 `flash_tone.bin` 拷入 microSD 卡根目录（或通过 `./ota_fs_flash_sdcard.py --port PORT` 经串口自动写入 SD 卡）。
3. 将 microSD 卡插入开发板，在 `examples/ota_fs` 目录重新烧录 `0.1.0` 基线固件并监控串口（`rm -f version.txt && idf.py -p PORT build flash monitor`）。
4. 设备单次会话依次完成 `ota_1`、`userdata`、`flash_tone` 三个分区的升级并重启进入 `0.2.0`；二次启动时三项版本检查均判定已是最新版本并自动跳过。

### 日志输出

升级至 `1.0.1` + `userdata` `1.0.1` + `flash_tone` `0.5.0` 并重启切入 `ota_1` 后的连续运行日志：

```text
I (159) OTA_FS_APP: ========================================
I (159) OTA_FS_APP:   Project:  ota_fs
I (159) OTA_FS_APP:   Version:  1.0.1
I (160) OTA_FS_APP:   Partition: ota_1 (0x00210000)
I (160) OTA_FS_APP:   Storage:   SD card (mount /sdcard)
I (160) OTA_FS_APP: ========================================
I (182) DEV_FS_FAT: Filesystem mounted, base path: /sdcard
I (182) OTA_FS: Create 'fs_sdcard': mount_point=/sdcard
I (182) OTA_FS: Run_ota: found /sdcard/firmware.bin (707536 bytes)
I (182) OTA_FS: Run_ota: found /sdcard/userdata.bin (1024 bytes) → partition "userdata" (4-byte semver checker)
I (182) OTA_FS: Run_ota: found /sdcard/flash_tone.bin (83082 bytes) → partition "flash_tone" (tone-desc checker)
I (183) OTA_FS: Run_ota: installed tone version: 0.5.0
I (183) OTA_FS: Run_ota: starting OTA (3 item(s)) ...
I (184) OTA_CHK_APP: Check: incoming version='1.0.1' project='ota_fs' size=707536 upgrade_available=false
W (184) OTA_FS: [OTA] Item 0 skipped: SD image not newer
I (184) OTA_CHK_DATA: Data version check: incoming=1.0.1 current=1.0.1 upgrade_available=false
W (184) OTA_FS: [OTA] Item 1 skipped: SD image not newer
I (185) OTA_CHK_TONE: Tone-desc check: incoming version='0.5.0' project='ESP_TONE_BIN' size=83082 upgrade_available=false
W (185) OTA_FS: [OTA] Item 2 skipped: SD image not newer
I (185) OTA_FS: [OTA] Session finished: all 3 item(s) skipped, nothing to update
I (185) OTA_FS: Run_ota: all items up to date, no firmware applied
I (186) OTA_FS: Userdata header: 4-byte packed semver (LE u32=0x00010001): 1.0.1
I (186) OTA_FS: Userdata version matches running app semver '1.0.1'
I (187) OTA_FS: Flash_tone header: tone bin (magic 0xf55f9876), project='ESP_TONE_BIN' version='0.5.0'
I (188) OTA_FS_APP: Running version 1.0.1 — idle on ota_1
```

## 故障排除

### 存储介质中未找到 `firmware.bin`

```text
I (920) OTA_FS: Run_ota: no firmware.bin on /sdcard — nothing to upgrade
```

请确认升级固件已重命名为 `firmware.bin` 并存放在 FAT32 分区根目录（`/sdcard/firmware.bin` 或 `/usb/firmware.bin`）。

### microSD 卡挂载失败

```text
E (910) OTA_FS: Init_sdcard failed: esp_board_manager_init_device_by_name returned ESP_FAIL
```

请检查 microSD 卡是否已格式化为 FAT32 并插紧，且 `gen-bmgr-config` 选择的板型与实际开发板一致。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
