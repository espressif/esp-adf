# Wi-Fi Service 例程

- [English Version](./README.md)
- 例程难度：⭐⭐

## 例程简介

本例程演示 ADF `esp_wifi_service` 组件的使用方式。例程会创建 Wi-Fi service，将 Wi-Fi profile 加密保存到 NVS，启动配网流程，监听 service 事件，并通过 CLI service 提供 profile 和配网控制命令。

本例程展示了如何在一个基于 ADF service 的应用中组合 `esp_service`、`adf_event_hub`、`esp_config_manager`、HTTP SoftAP 配网、可选 BluFi 配网、profile 存储和网络选择，以及网络质量监测。

### 典型场景

本例程适用于需要现场配网、持久化保存 Wi-Fi 凭据、自动重连，以及通过统一 service 接口获取连接状态和配网事件的 IoT 设备。

### 运行机制

1. 初始化 NVS；当启用 BluFi 时，同时初始化 Bluetooth host
2. 创建 HTTP 配网和可选 BluFi 配网
3. 配置加密 NVS 作为 Wi-Fi profile 存储
4. 配置网络选择触发条件，包括 RSSI、Internet 访问探测、延迟退化和吞吐退化
5. 创建并启动 `esp_wifi_service`
6. 订阅 service 事件，并启动 `esp_cli_service` UART REPL
7. 同时启用 `CONFIG_ESP_MCP_ENABLE` 和 `CONFIG_WIFI_SERVICE_MCP_ENABLE` 时，注册 Wi-Fi MCP 工具；如果选择 UART transport，则启动 MCP UART 服务
8. 如果没有可用 profile，则启动配网流程；如果已有 profile，则会使用这些 profile 进行连接选择

## 环境配置

### 硬件要求

- ESP-IDF 支持的 Espressif 开发板
- 用于配网和连接测试的 Wi-Fi AP
- 用于 HTTP 配网的手机；只有测试 BluFi 时才需要安装 Espressif BluFi APP 的手机

### 默认 IDF 分支

本例程支持 ESP-IDF release/v5.5 及以后的分支，ADF 仓库默认使用 ESP-IDF release/v5.5 分支。

### 软件要求

- 用于 HTTP 配网的浏览器
- 测试 BluFi 配网时使用 Espressif BluFi 手机 APP

## 编译和下载

### 编译准备

编译本例程前需先确保已配置 ESP-IDF 环境。若未配置，请在 ESP-IDF 根目录运行以下脚本：

```
./install.sh
. ./export.sh
```

进入本例程工程目录：

```
cd $ADF_PATH/components/esp_wifi_service/examples/wifi_service_example
```

### 项目配置

编译前请先设置目标芯片。例程为 ESP32 和 ESP32-S3 提供了目标相关默认配置：

```
idf.py set-target esp32
```

或：

```
idf.py set-target esp32s3
```

重要配置项：

- `Wi-Fi Service` → `Provisioning agents` → `Enable HTTP provisioning agent`
- `Wi-Fi Service` → `Provisioning agents` → `Embed default HTTP provisioning Web UI`
- `Wi-Fi Service` → `Provisioning agents` → `Enable BLUFI provisioning agent`
- `WIFI_SERVICE_SELECTOR_PROBE_ENABLE`：启用 selector HTTP probe monitor。
- `WIFI_SERVICE_SELECTOR_THROUGHPUT_ENABLE`：启用 selector throughput monitor。
- `ESP_MCP_ENABLE`：启用共享 MCP server 和 transport。
- `WIFI_SERVICE_MCP_ENABLE`：编译 Wi-Fi MCP tool 支持，依赖 `ESP_MCP_ENABLE`。
- `ESP_MCP_TRANSPORT_UART`：通过本例程的 UART transport 暴露 MCP tools。

BluFi 依赖 ESP-IDF Bluetooth 配置。例程中的 `sdkconfig.defaults.esp32` 会启用 Bluedroid BluFi，`sdkconfig.defaults.esp32s3` 会启用 NimBLE BluFi。如果当前目标未启用 Bluetooth，HTTP 配网仍可独立使用。

默认 HTTP 配网使用内置 Web UI，并使用 `ESP_SVC_XXXXXX` 形式的配网 SoftAP SSID，其中 `XXXXXX` 来自设备 SoftAP MAC 地址。如需覆盖 AP 名称或密码，可在 `main/app_main.c` 中设置 `http_cfg.softap`，例如：

```c
esp_wifi_service_prov_http_config_t http_cfg = {
    .name = "http",
    .softap = {
        .ssid = "ESP_SVC_DEMO",
        .password = "12345678",
        .channel = 3,
        .max_connection = 4,
    },
};
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

启动后，例程会启动 `esp_wifi_service`，并打开提示符为 `wifi>` 的 `esp_cli_service` UART REPL。

常用 CLI 命令：

```
help
wifi profile list
wifi profile add <ssid> <password> [priority 0..20]
wifi profile del <ssid>
wifi profile clear
wifi prov start
wifi prov stop
wifi reeval
reboot
```

测试 HTTP 配网：

1. 启动例程，等待 HTTP agent 启动。
2. 使用手机或 PC 连接设备的配网 SoftAP。
3. 打开 captive portal 页面；如果系统没有自动弹出页面，可手动访问设备网关地址。
4. 选择或输入目标 AP 的 SSID，填写密码并提交。
5. 设备会尝试连接，连接成功后保存 profile，并在串口日志中上报 service 事件。

也可以通过 CLI 执行 `wifi profile add <ssid> <password> [priority]` 添加 profile，然后根据测试需要重启设备或重新触发配网/选择流程。

启用 MCP 后，例程会注册以下 Wi-Fi tools：

```
esp_wifi_service_get_status
esp_wifi_service_list_profiles
esp_wifi_service_add_profile
esp_wifi_service_set_profile_enabled
esp_wifi_service_delete_profile
esp_wifi_service_clear_profiles
esp_wifi_service_prov_start
esp_wifi_service_prov_stop
esp_wifi_service_request_reeval
```

MCP 状态和 profile 查询工具不会返回已保存的 Wi-Fi 密码。设置 `CONFIG_ESP_MCP_TRANSPORT_UART=y` 后，本例程会启动内置 UART MCP server。

本例程使用 `UART_NUM_1` 作为 MCP 串口，波特率为 115200：

- MCP TX：GPIO25，连接到 USB-UART 转接器 RX
- MCP RX：GPIO26，连接到 USB-UART 转接器 TX
- GND：开发板 GND 与转接器 GND 共地

在本例程目录下运行 Wi-Fi MCP UART smoke test：

```
python3 scripts/test_wifi_service_mcp_uart.py /dev/ttyUSB1 115200
```

### 日志输出

以下日志片段展示 HTTP agent 启动后的 service 流程：

```
I (642) WIFI_SVC_EXAMPLE: Create Wi-Fi service skeleton with 1 agents
I (652) WIFI_SERVICE: Init wifi default: station mode started
I (672) WIFI_SERVICE: Init 'wifi_service': agent 'http' ready
I (682) WIFI_SERVICE: Init 'wifi_service': done with 1 agent(s)
I (692) WIFI_SVC_EXAMPLE: Start Wi-Fi service
I (742) WIFI_SERVICE_HTTP: HTTP server started: port=80 ctrl_port=32769
I (752) WIFI_SVC_EXAMPLE: Event received: domain=wifi_service id=4 name=PROV_STARTED
I (752) WIFI_SVC_EXAMPLE: Payload: name=http
I (762) WIFI_SERVICE_HTTP: HTTP agent started
I (772) WIFI_SERVICE: Start provisioning: agent 'http' started (index=0)
I (782) WIFI_SERVICE: Start 'wifi_service': all agents started
I (792) WIFI_SVC_CONSOLE: CLI service: type 'help', 'wifi ...' or 'reboot'
```

提交 Wi-Fi 凭据并获取 IP 后，关键日志如下：

```
I (15432) WIFI_SERVICE_HTTP: HTTP station connect begin: ssid='Office', password_len=8, timeout=15000ms
I (17342) WIFI_SERVICE_HTTP: HTTP station connect success: ssid='Office'
I (17362) WIFI_SVC_EXAMPLE: Event received: domain=wifi_service id=8 name=PROV_CREDENTIAL_RECEIVED
I (17362) WIFI_SVC_EXAMPLE: Payload: name=http ssid='Office' priority=10 password_len=8
I (17402) WIFI_SVC_EXAMPLE: Event received: domain=wifi_service id=3 name=STA_GOT_IP
I (17402) WIFI_SVC_EXAMPLE: Payload: ip=192.168.1.123 netmask=255.255.255.0 gw=192.168.1.1 ip_changed=0
```

## 故障排除

### BluFi agent 未创建

只有启用对应 ESP-IDF Bluetooth BluFi 选项时，BluFi 才会被编译进工程。请使用 `idf.py set-target esp32` 或 `idf.py set-target esp32s3` 让目标相关默认配置生效，然后检查 `Wi-Fi Service` 配网菜单和 ESP-IDF Bluetooth 菜单。

### 找不到配网 SoftAP

当前例程没有设置 `http_cfg.softap`，HTTP agent 会使用默认的 `ESP_SVC_XXXXXX` SoftAP 名称。测试时如需固定 AP 名称和密码，请在 `main/app_main.c` 中配置 `http_cfg.softap.ssid` 和 `http_cfg.softap.password`。

### HTTP 配网页面没有自动弹出

部分操作系统不会稳定弹出 captive portal 页面。连接配网 SoftAP 后，可手动打开浏览器访问设备网关地址。

### 重新烧录后 profile 仍然存在

profile 保存在 NVS 中，普通应用烧录不会清除。可使用以下任一方式清除：

```
wifi profile clear
```

或：

```
idf.py erase-flash
```

## 技术支持

请按照下面的链接获取技术支持：

- 技术支持参见 [esp32.com](https://esp32.com/viewforum.php?f=20) 论坛
- 问题反馈与功能需求，请创建 [GitHub issue](https://github.com/espressif/esp-adf/issues)

我们会尽快回复。
