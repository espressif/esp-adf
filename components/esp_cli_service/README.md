# CLI Service

[中文版](./README_CN.md)

**CLI Service** is an `esp_service_t` subclass that integrates the ESP-IDF `esp_console` UART REPL into the ADF service lifecycle. It provides two command layers — built-in static system commands and dynamic service/tool commands driven by `esp_service_manager` — making interactive debugging and runtime control accessible from a serial terminal with no extra tooling.

> **Version Requirements:** Requires ESP-IDF >= 5.4.

## Key Features

- **UART REPL integration** — embeds `esp_console` REPL directly into the service lifecycle; starts on `esp_service_start()` and shuts down on stop/destroy
- **Built-in system commands** — `sys_heap`, `sys_chip`, `sys_uptime`, `sys_reboot` are registered automatically
- **Static commands** — register any `esp_console_cmd_t` before or after the REPL starts via `esp_cli_service_register_static_command()`
- **Dynamic `svc` commands** — list, inspect, and control lifecycle (`start` / `stop` / `pause` / `resume`) of tracked services from the terminal
- **Dynamic `tool` commands** — enumerate and invoke JSON-schema tools exposed by `esp_service_manager`; supports both `key=value` and `--json` argument modes
- **Thread-safe API** — runtime API calls (`track`, `untrack`, `bind_manager`, `register_static_command`) are mutex-protected and safe to call from any task; `create` and `destroy` must be called from a quiesced context

## How It Works

```
UART terminal
      │
      ▼
  esp_console REPL
      │
      ├── sys_*  ──────────────────── static built-in commands
      ├── <user static cmds> ──────── esp_cli_service_register_static_command()
      │
      ├── svc list/info/start/stop ── tracked services (esp_cli_service_track_service)
      │                                or esp_service_manager lookup
      │
      └── tool list/info/call ──────── esp_service_manager tool registry
```

On `esp_service_start()` the REPL task is spawned and all pending static commands are registered with `esp_console`. `svc` commands work as long as services are tracked (or a manager is bound as fallback). `tool` commands require a manager bound via `esp_cli_service_bind_manager()` or `esp_cli_service_config_t::manager`.

## Configuration

### esp_cli_service_config_t

| Field                | Type                    | Description                                        | Default              |
|----------------------|-------------------------|----------------------------------------------------|----------------------|
| `struct_size`        | `uint32_t`              | Must equal `sizeof(esp_cli_service_config_t)`          | set by macro         |
| `base_cfg`           | `esp_service_config_t`  | Core service registration configuration            | `ESP_SERVICE_CONFIG_DEFAULT()` |
| `prompt`             | `const char *`          | REPL prompt string (copied internally)             | `"cli>"`             |
| `max_cmdline_length` | `uint16_t`              | Maximum command line length                        | `256`                |
| `task_stack`         | `uint32_t`              | REPL task stack size in bytes                      | `4096`               |
| `task_prio`          | `uint32_t`              | REPL task priority                                 | `2`                  |
| `manager`            | `esp_service_manager_t *` | Optional manager for `tool` commands (also used as fallback for `svc`) | `NULL` |

Always initialize with `ESP_CLI_SERVICE_CONFIG_DEFAULT()` and override only what you need.

## Quick Start

```c
#include "esp_service_manager.h"
#include "esp_cli_service.h"

void app_main(void)
{
    /* Create the CLI service with default config */
    esp_cli_service_config_t cfg = ESP_CLI_SERVICE_CONFIG_DEFAULT();
    cfg.prompt = "app>";

    esp_cli_service_t *cli = NULL;
    ESP_ERROR_CHECK(esp_cli_service_create(&cfg, &cli));

    /* Track another service so it appears in 'svc list'; untrack with esp_cli_service_untrack_service() */
    ESP_ERROR_CHECK(esp_cli_service_track_service(cli, (esp_service_t *)my_svc, "audio"));

    /* Register a custom static command */
    const esp_console_cmd_t my_cmd = {
        .command = "version",
        .help    = "Print firmware version",
        .func    = cmd_version,
    };
    ESP_ERROR_CHECK(esp_cli_service_register_static_command(cli, &my_cmd));

    /* Start the REPL */
    ESP_ERROR_CHECK(esp_service_start((esp_service_t *)cli));
}
```

## Built-in Commands

### System Commands (always available)

| Command       | Description                                         |
|---------------|-----------------------------------------------------|
| `sys_heap`    | Print current free heap size                        |
| `sys_chip`    | Print chip model, core count, revision, and features|
| `sys_uptime`  | Print milliseconds since boot                       |
| `sys_reboot`  | Software reset                                      |

### `svc` Commands (requires tracked services or bound manager)

```
svc list
svc info <service>
svc start <service>
svc stop <service>
svc pause <service>
svc resume <service>
```

Services visible to `svc` come first from the tracked list (added via `esp_cli_service_track_service()`), then from the bound `esp_service_manager`.

### `tool` Commands (requires bound manager)

```
tool list
tool info <tool_name>
tool call <tool_name> [key=value ...]
tool call <tool_name> --json '<json_string>'
```

`tool call` serializes `key=value` pairs into a JSON object according to the tool's `inputSchema` (supports `string`, `integer`, `number`, `boolean`). Use `--json` to pass a raw JSON argument object when the schema is complex or the field type is not natively supported.

## Typical Scenarios

- **Development / debugging shell** — inspect heap, chip info, and uptime without modifying application code
- **Runtime service control** — start, stop, pause, or resume any tracked service from the terminal during integration testing
- **LLM tool gateway** — when `esp_service_manager` exposes tools with JSON schema, `tool call` becomes a direct invocation path for AI-agent or host-side automation

## Example Project

A standalone example is available at:

```
components/esp_cli_service/examples/
```

Build and flash from that directory:

```bash
idf.py set-target <chip>
idf.py build flash monitor
```
