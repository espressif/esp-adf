# CLI Service

[English](./README.md)

**CLI Service** 是一个 `esp_service_t` 子类，将 ESP-IDF `esp_console` UART REPL 嵌入到 ADF 服务生命周期中。它提供两层命令体系——内置静态系统命令，以及由 `esp_service_manager` 驱动的动态服务/工具命令——无需额外工具，即可通过串口终端进行交互式调试和运行时控制。

> **版本要求：** 需要 ESP-IDF >= 5.4。

## 主要特性

- **UART REPL 集成** — 将 `esp_console` REPL 直接嵌入服务生命周期，调用 `esp_service_start()` 时启动，停止/销毁时关闭
- **内置系统命令** — 自动注册 `sys_heap`、`sys_chip`、`sys_uptime`、`sys_reboot`
- **静态命令** — 通过 `esp_cli_service_register_static_command()` 在 REPL 启动前后随时注册任意 `esp_console_cmd_t`
- **动态 `svc` 命令** — 从终端列出、查看和控制（`start` / `stop` / `pause` / `resume`）已跟踪的服务
- **动态 `tool` 命令** — 枚举并调用 `esp_service_manager` 暴露的 JSON Schema 工具；支持 `key=value` 和 `--json` 两种参数传入方式
- **线程安全 API** — 所有公共函数均由内部互斥锁保护，可从任意任务安全调用

## 工作原理

```
UART 终端
      │
      ▼
  esp_console REPL
      │
      ├── sys_*  ──────────────────── 内置静态命令
      ├── <用户静态命令> ──────────── esp_cli_service_register_static_command()
      │
      ├── svc list/info/start/stop ── 已跟踪服务 (esp_cli_service_track_service)
      │                                或 esp_service_manager 查找
      │
      └── tool list/info/call ──────── esp_service_manager 工具注册表
```

调用 `esp_service_start()` 后，REPL 任务被创建，所有待注册的静态命令随即注册到 `esp_console`。绑定 `esp_service_manager`（通过 `esp_cli_service_bind_manager()` 或在配置中设置）后，动态 `svc` 和 `tool` 命令即可使用。

## 配置说明

### esp_cli_service_config_t

| 字段                   | 类型                      | 说明                                         | 默认值                        |
|------------------------|---------------------------|----------------------------------------------|-------------------------------|
| `struct_size`          | `uint32_t`                | 必须等于 `sizeof(esp_cli_service_config_t)`      | 由宏设置                      |
| `base_cfg`             | `esp_service_config_t`    | 核心服务注册配置                             | `ESP_SERVICE_CONFIG_DEFAULT()` |
| `prompt`               | `const char *`            | REPL 提示符字符串（内部拷贝）                | `"cli>"`                      |
| `max_cmdline_length`   | `uint16_t`                | 最大命令行长度                               | `256`                         |
| `task_stack`           | `uint32_t`                | REPL 任务栈大小（字节）                      | `4096`                        |
| `task_prio`            | `uint32_t`                | REPL 任务优先级                              | `2`                           |
| `manager`              | `esp_service_manager_t *` | 用于 `svc`/`tool` 命令的可选服务管理器       | `NULL`                        |

始终使用 `ESP_CLI_SERVICE_CONFIG_DEFAULT()` 初始化，再按需覆盖字段。

## 快速入门

```c
#include "esp_service_manager.h"
#include "esp_cli_service.h"

void app_main(void)
{
    /* 使用默认配置创建 CLI service */
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.prompt = "app>";

    esp_cli_service_t *cli = NULL;
    ESP_ERROR_CHECK(esp_cli_service_create(&cfg, &cli));

    /* 跟踪另一个服务，使其出现在 'svc list' 中 */
    ESP_ERROR_CHECK(esp_cli_service_track_service(cli, (esp_service_t *)my_svc, "audio"));

    /* 注册自定义静态命令 */
    const esp_console_cmd_t my_cmd = {
        .command = "version",
        .help    = "Print firmware version",
        .func    = cmd_version,
    };
    ESP_ERROR_CHECK(esp_cli_service_register_static_command(cli, &my_cmd));

    /* 启动 REPL */
    ESP_ERROR_CHECK(esp_service_start((esp_service_t *)cli));
}
```

## 内置命令

### 系统命令（始终可用）

| 命令           | 说明                                  |
|----------------|---------------------------------------|
| `sys_heap`     | 打印当前可用堆大小                    |
| `sys_chip`     | 打印芯片型号、核心数、版本号和特性    |
| `sys_uptime`   | 打印自启动以来的毫秒数                |
| `sys_reboot`   | 软件复位                              |

### `svc` 命令（需跟踪服务或绑定 manager）

```
svc list
svc info <service>
svc start <service>
svc stop <service>
svc pause <service>
svc resume <service>
```

`svc` 可见的服务优先来自跟踪列表（通过 `esp_cli_service_track_service()` 添加），其次来自绑定的 `esp_service_manager`。

### `tool` 命令（需绑定 manager）

```
tool list
tool info <tool_name>
tool call <tool_name> [key=value ...]
tool call <tool_name> --json '<json_string>'
```

`tool call` 根据工具的 `inputSchema` 将 `key=value` 键值对序列化为 JSON 对象，支持 `string`、`integer`、`number`、`boolean` 类型。对于复杂 Schema 或不受原生支持的字段类型，使用 `--json` 传入原始 JSON 参数对象。

## 典型应用场景

- **开发/调试 Shell** — 无需修改应用代码即可查看堆、芯片信息和运行时间
- **运行时服务控制** — 在集成测试期间从终端启动、停止、暂停或恢复任意已跟踪服务
- **LLM 工具网关** — 当 `esp_service_manager` 暴露带 JSON Schema 的工具时，`tool call` 成为 AI Agent 或主机侧自动化的直接调用入口

## 示例工程

独立示例位于：

```
components/esp_cli_service/examples/
```

在该目录执行以下命令构建和烧录：

```bash
idf.py set-target <chip>
idf.py build flash monitor
```

## 常见问题

### 串口终端上没有提示符出现

- 确认已调用 `esp_service_start()`，REPL 任务仅在启动时创建。
- 检查终端模拟器选择的 UART 端口和波特率是否正确（默认为 IDF menuconfig 中配置的 console UART）。
- 若注册的命令分配了大量数据，默认 4096 字节的 `task_stack` 可能不够，请适当增大。

### `svc` / `tool` 命令打印 "manager not bound"

请在配置中设置 `cfg.manager = my_mgr`，或在运行时调用 `esp_cli_service_bind_manager()` 绑定管理器后再使用这些命令。

### `tool call` 提示 "use --json"

工具的 `inputSchema` 中包含无法从 `key=value` 令牌自动反序列化的属性类型（如 `array` 或 `object`）。请使用 `--json` 以 JSON 字符串形式传入完整参数。
