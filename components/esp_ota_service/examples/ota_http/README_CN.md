# HTTP OTA 示例

- [English Version](./README.md)
- 例程难度：⭐⭐

## 例程简介

- 本例程通过 HTTP/HTTPS 服务器下载固件镜像，利用远端 JSON 清单完成语义化版本比对与流式 SHA-256 完整性校验，将新固件写入空闲的 OTA 应用分区，重启进入新槽位并在 Wi-Fi 重连成功后确认升级。
- 演示了 `esp_ota_service` 与 `esp_ota_service_checker_manifest_create()`、`esp_ota_service_verifier_sha256_create()`、`esp_ota_service_source_http_create()`、`esp_ota_service_target_app_create()`、NVS 断点续传以及 A/B 回滚确认（`esp_ota_service_confirm_update()`）的组合用法。

### 典型场景

适用于联网智能硬件通过远端清单轮询固件更新、在落盘过程中做流式哈希校验，并在新固件联网异常时自动回滚到上一版本的场景。

### 运行机制

1. 上电启动，打印当前固件版本与运行分区（`ota_0` 或 `ota_1`），初始化 NVS。
2. 连接配置的 Wi-Fi 热点；若当前镜像处于 `ESP_OTA_IMG_PENDING_VERIFY` 待验证状态且 Wi-Fi 连接失败，则调用 `esp_ota_service_rollback()` 回滚到旧版本。
3. 若处于 `PENDING_VERIFY` 状态且 Wi-Fi 连接成功，调用 `esp_ota_service_confirm_update()` 标记新固件有效并取消回滚。
4. 使用 `esp_ota_service_checker_manifest_create()` 预拉取 `manifest.json`，比对服务端 `x.y.z` 版本号并提取 SHA-256 摘要。
5. 当存在更高版本时，挂载 `esp_ota_service_verifier_sha256_create()`，通过 `esp_ota_service_set_upgrade_list()` 提交 HTTP 数据源与应用分区目标，调用 `esp_service_start()` 启动升级任务。
6. 收到无失败项的 `ESP_OTA_SERVICE_EVT_SESSION_END` 事件后销毁服务，调用 `esp_restart()` 重启切入新固件分区。

### 文件结构

```text
├── main
│   ├── app_main.c
│   ├── CMakeLists.txt
│   ├── esp_ota_service_http_example.c
│   ├── esp_ota_service_http_example.h
│   └── idf_component.yml
├── CMakeLists.txt
├── partitions.csv
├── pytest_esp_ota_service_ota_http.py
├── README.md
├── README_CN.md
├── sdkconfig.defaults
└── sdkconfig.defaults.esp32s3
```

## 环境配置

### 硬件要求

- 一块支持 Wi-Fi 且至少具备 8 MB Flash 的开发板（默认适配 ESP32-S3，`partitions.csv` 包含两个 2 MB 应用槽位）。
- 开发板与宿主机处于同一局域网的 2.4 GHz Wi-Fi 路由器。
- USB 数据线（供电、烧录与串口监控）。

### 默认 IDF 分支

本例程支持 IDF release/v5.4 及以后的分支，例程默认使用 IDF release/v5.5 分支。

### 软件要求

- 宿主机安装 Python 3，用于生成带版本号的升级包（`../../tools/build_firmware.py`）并启动本地 HTTP 固件服务（`../../tools/ota_http_serve.py`）。
- 固件版本约定：`CMakeLists.txt` 中设置了初始基线 `project(ota_http VERSION 1.0.0)`，在没有 `version.txt` 时直接执行 `idf.py build` 会生成合法的 `1.0.0` 基线固件；执行 `../../tools/build_firmware.py http -v 1.0.1` 构建升级包时会生成 `version.txt`（仓库根目录已加入 `.gitignore`），ESP-IDF 构建系统会自动以更高优先级读取 `version.txt`，默认基线表统一维护在 `../../tools/_fw_common.py`。

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境；若已配置可跳过本段，直接进入工程目录。若未配置，请在 ESP-IDF 根目录运行以下脚本完成环境设置，完整步骤请参阅 [《ESP-IDF 编程指南》](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/index.html)。

```bash
./install.sh
. ./export.sh
```

从仓库根目录进入本例程目录并设置目标芯片：

```bash
cd components/esp_ota_service/examples/ota_http
idf.py set-target esp32s3
```

### 项目配置

修改 `main/esp_ota_service_http_example.c` 中的 `WIFI_SSID`、`WIFI_PASSWORD`、`ESP_OTA_SERVICE_EXAMPLE_MANIFEST_URL` 和 `ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL`，使其对应实际路由器和宿主机局域网 IP（端口 `18070` 与 `ota_http_serve.py` 一致）。

如需调整 OTA 服务配置项：

```bash
idf.py menuconfig
```

主要配置路径（`sdkconfig.defaults` 已默认开启）：

- `ESP-Service: OTA Service` → `Verification and rollback` → `Require higher firmware version by default`（`CONFIG_OTA_FORCE_VERSION_CHECK=y`）
- `ESP-Service: OTA Service` → `Verification and rollback` → `Enable OTA rollback support`（`CONFIG_OTA_ENABLE_ROLLBACK=y`）
- `Bootloader config` → `Enable app rollback support`（`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`）
- `ESP-Service: OTA Service` → `Resume (NVS checkpoint)` → `Enable OTA resume (breakpoint continuation)`（`CONFIG_OTA_ENABLE_RESUME=y`）

> 配置完成后按 `s` 保存，然后按 `Esc` 退出。

### 编译与烧录

- 编译初始 `1.0.0` 基线固件（若此前运行过 `build_firmware.py`，请先删除 `version.txt`）：

```bash
rm -f version.txt
idf.py build
```

- 烧录程序并运行 monitor 工具来查看串口输出（替换 `PORT` 为实际串口名称）：

```bash
idf.py -p PORT flash monitor
```

- 退出调试界面使用 `Ctrl-]`

## 如何使用例程

### 功能和用法

1. 在 `components/esp_ota_service/tools` 目录下构建 `1.0.1` 升级固件并启动 HTTP 服务：

```bash
cd components/esp_ota_service/tools
./build_firmware.py http -v 1.0.1
./ota_http_serve.py --update-manifest-url
```

2. 确认 `examples/ota_http/main/esp_ota_service_http_example.c` 中的 URL 与 `ota_http_serve.py` 打印的地址一致后，在 `examples/ota_http` 目录重新烧录 `1.0.0` 基线固件并打开串口监控（`rm -f version.txt && idf.py -p PORT build flash monitor`）。
3. 设备启动 `1.0.0` 后连接 Wi-Fi，拉取 `/manifest.json` 识别到 `1.0.1 > 1.0.0`，流式下载 `/firmware.bin` 并完成 SHA-256 校验写入 `ota_1`，随后自动重启。
4. 重启进入 `1.0.1`（`PENDING_VERIFY`）后重新连上 Wi-Fi，调用 `esp_ota_service_confirm_update()` 取消回滚；再次拉取清单发现版本已一致，自动跳过下载。

### 日志输出

升级写入、重启切入 `ota_1`、确认取消回滚以及二次版本检查跳过的代表性日志：

```text
I (1470) OTA_HTTP_EXAMPLE: Run_ota_session: upgrade available '1.0.0' -> '1.0.1'
I (1471) OTA_HTTP_EXAMPLE: Run_ota_session: SHA-256 verifier armed from manifest
I (1472) OTA_HTTP_EXAMPLE: Run_ota_session: starting HTTP OTA service: http://10.0.2.1:18070/firmware.bin
I (1473) OTA_HTTP_EXAMPLE: [OTA] Session started
I (1510) OTA_HTTP_EXAMPLE: [OTA] Downloading from server ...
I (2054) OTA_VER_SHA256: SHA-256 verified successfully
I (2055) OTA_TGT_APP: Boot partition set to: ota_1
I (2055) esp_ota_svc_resume: Resume record cleared [idx=0]
I (2055) OTA_HTTP_EXAMPLE: [OTA] Download + write succeeded
I (2055) OTA_HTTP_EXAMPLE: Run_ota_session: OTA session finished OK; rebooting from the application
...
I (184) OTA_HTTP_APP: ========================================
I (184) OTA_HTTP_APP:   Project:  ota_http
I (184) OTA_HTTP_APP:   Version:  1.0.1
I (185) OTA_HTTP_APP:   Partition: ota_1 (0x00210000)
I (185) OTA_HTTP_APP: ========================================
I (191) OTA_HTTP_APP: Running partition OTA state: 1 (1=PENDING_VERIFY after OTA reboot)
I (191) OTA_HTTP_APP: Connecting to WiFi ...
I (1470) OTA_HTTP_EXAMPLE: WiFi connected — IP: 10.0.2.2
I (1470) OTA_HTTP_EXAMPLE: New image pending verify — WiFi OK, marking OTA valid ...
I (1470) OTA_SERVICE: OTA update confirmed - rollback cancelled
I (1470) OTA_HTTP_EXAMPLE: Confirm_if_pending_verify: upgrade OK, rollback cancelled
I (1473) OTA_CHK_MANIFEST: Manifest check: incoming=1.0.1 current=1.0.1 size=1065440 has_hash=true has_md5=false upgrade=false
W (1473) OTA_HTTP_EXAMPLE: Run_ota_session: server firmware '1.0.1' is not newer than running '1.0.1' — skipping
I (1474) OTA_HTTP_APP: Running version 1.0.1 — waiting for next upgrade ...
```

## 故障排除

### 拉取清单报 `ESP_ERR_HTTP_CONNECT`

```text
E (7495) OTA_HTTP_EXAMPLE: Run_ota_session failed: manifest check returned ESP_ERR_HTTP_CONNECT
```

请确认宿主机已启动 `ota_http_serve.py`，防火墙放行了 TCP `18070` 端口，且 `ESP_OTA_SERVICE_EXAMPLE_MANIFEST_URL` / `ESP_OTA_SERVICE_EXAMPLE_FIRMWARE_URL` 中的 IP 与开发板处于同一网段。

### 服务端固件被直接跳过

```text
W (1473) OTA_HTTP_EXAMPLE: Run_ota_session: server firmware '1.0.0' is not newer than running '1.0.0' — skipping
```

当前服务端 `manifest.json` 中的版本号不高于设备正在运行的版本。请在 `components/esp_ota_service/tools` 下执行 `./build_firmware.py http -v <更高版本号>` 并重启 `ota_http_serve.py`。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
