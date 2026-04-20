# ESP Config Manager 简单例程

- [English Version](./README.md)
- 例程难度：⭐

## 例程简介

本例程演示如何使用 `esp_config_manager` 在持久化介质中保存多个应用配置组。例程会在首次启动时加载默认值，保存修改后的配置，再次读取并打印每个配置的来源。

本例程展示了双槽配置存储、默认值合并、带 CRC 校验的配置记录、可选加解密钩子，以及 NVS、SPIFFS、raw flash 分区等存储介质的绑定方式。

### 典型场景

本例程适用于需要可靠本地配置保存的产品，例如音频参数、启动计数、网络相关标志位，以及需要兼容结构体扩展的配置升级场景。

### 运行机制

1. 初始化 NVS；当选择 filesystem 存储方式时，同时初始化 SPIFFS。
2. 创建 audio、lifecycle、net 三个独立配置组。
3. 将每个配置组绑定到选定的存储介质。
4. 调用 `esp_config_manager_load()` 读取配置。
5. 修改每个配置组中的一个字段，调用 `esp_config_manager_save()` 保存，再次读取验证持久化结果。
6. 释放 manager handle 并打印 `Done`。

## 环境配置

### 硬件要求

- ESP-IDF 支持的 Espressif 开发板，例如 ESP32 或 ESP32-S3。

### 默认 IDF 分支

本例程支持 ESP-IDF release/v5.5 及以后的分支，ADF 仓库默认使用 ESP-IDF release/v5.5 分支。

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境。若未配置，请在 ESP-IDF 根目录运行以下脚本：

```
./install.sh
. ./export.sh
```

进入本例程工程目录：

```
cd $ADF_PATH/components/esp_config_manager/examples/simple
```

### 项目配置

默认存储方式为 NVS：

```c
#define EXAMPLE_CFG_STORAGE_BACKEND  EXAMPLE_STORAGE_NVS
```

如需测试其他存储方式，可修改 `main/config_manager_example.c` 中的 `EXAMPLE_CFG_STORAGE_BACKEND`：

- `EXAMPLE_STORAGE_NVS`：每个配置组使用独立 NVS namespace。
- `EXAMPLE_STORAGE_FS`：primary 和 backup 文件保存到 `/spiffs`，使用 `partitions.csv` 中的 `storage` SPIFFS 分区。
- `EXAMPLE_STORAGE_FLASH`：primary 和 backup 数据分别保存到 `partitions.csv` 中定义的 raw data 分区。

`sdkconfig.defaults` 已启用自定义分区表，可满足 SPIFFS 和 raw flash 存储演示所需的分区配置。

如需运行较完整的自测流程，可在 `main/config_manager_example.c` 中设置：

```c
#define CFG_EX_RUN_SELFTEST  1
```

### 编译与烧录

- 编译示例程序：

```
idf.py build
```

- 烧录程序并运行 monitor 工具来查看串口输出（替换 `PORT` 为端口名称）：

```
idf.py -p PORT flash monitor
```

- 退出调试界面使用 `Ctrl-]`。

## 如何使用例程

### 功能和用法

烧录后例程会自动运行。首次启动时，各配置组会从默认值加载并写入存储；后续启动时会优先从 primary 存储槽读取，并在上次保存值的基础上继续变化。

默认 NVS 存储方式会保留已保存数据。如需重新观察首次启动的默认值恢复流程，可在烧录前执行 `idf.py erase-flash`。

### 日志输出

以下日志片段展示默认 NVS 存储方式下的普通演示流程：

```c
I (325) CFG_MANAGER_EXAMPLE: Storage backend: NVS (3 config groups)
I (345) CFG_MANAGER_EXAMPLE: [audio] from defaults repair=0
I (345) CFG_MANAGER_EXAMPLE:   vol=42 flags=0x00000001 profile=default bri=80 lvl=10 array=esp1
I (355) CFG_MANAGER_EXAMPLE: [audio] from primary repair=0
I (355) CFG_MANAGER_EXAMPLE:   vol=43 flags=0x00000001 profile=default bri=80 lvl=0 array=esp1
I (365) CFG_MANAGER_EXAMPLE: --- group audio done ---
I (375) CFG_MANAGER_EXAMPLE: [lifecycle] from defaults — boot_count=0 session_flags=0x0100
I (375) CFG_MANAGER_EXAMPLE:   array=esp2
I (385) CFG_MANAGER_EXAMPLE: [lifecycle] from primary — boot_count=1 session_flags=0x0100
I (395) CFG_MANAGER_EXAMPLE: --- group lifecycle done ---
I (405) CFG_MANAGER_EXAMPLE: [net] from defaults — route_tag=0 if=sta0 mtu=1500
I (415) CFG_MANAGER_EXAMPLE: [net] from primary — route_tag=305419896 if=sta0 mtu=1500
I (425) CFG_MANAGER_EXAMPLE: --- group net done ---
I (425) CFG_MANAGER_EXAMPLE: Done
```

开启 self-test 模式后，关键日志如下：

```c
I (325) CFG_MANAGER_EXAMPLE: ======== selftest start (backend=NVS, 3 groups) ========
I (345) CFG_MANAGER_EXAMPLE: [TEST] g0.packed_layout PASS
I (365) CFG_MANAGER_EXAMPLE: [TEST] g0.load_defaults_after_wipe PASS — ESP_OK
I (385) CFG_MANAGER_EXAMPLE: [TEST] g0.save_load_roundtrip PASS — ESP_OK
I (405) CFG_MANAGER_EXAMPLE: [TEST] g0.fallback_to_backup_crc PASS — ESP_OK
I (425) CFG_MANAGER_EXAMPLE: [TEST] g0.primary_repaired_after_load PASS — ESP_OK
I (445) CFG_MANAGER_EXAMPLE: [TEST] g0.merge_tail_new_fields PASS
I (465) CFG_MANAGER_EXAMPLE: ======== selftest end ========
```

## 故障排除

### SPIFFS 或 raw flash 存储加载失败

请确认构建时已使用 `sdkconfig.defaults`，并启用了自定义 `partitions.csv`。这两种存储方式需要 `storage`、`cfg_pri`、`cfg_bak`、`cfg2_pri`、`cfg2_bak`、`cfg3_pri`、`cfg3_bak` 等分区。

### 重新烧录后配置值没有恢复默认值

普通应用烧录不会清除 NVS 数据。如需清除已保存的配置数据，可执行：

```
idf.py erase-flash
```

### self-test 日志出现 `FAIL`

可先执行 `idf.py erase-flash` 后重新测试。如果使用 flash 存储方式，请同时确认自定义分区表已生效，且 `partitions.csv` 中的每个 raw config 分区都存在。

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
