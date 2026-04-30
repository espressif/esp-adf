# Changelog

## v0.5.0

### Features

- Initial version of `esp_cli_service`
- `esp_service_t` subclass backed by `esp_console` UART REPL
- Extra static commands via `esp_cli_service_register_static_command()`
- Dynamic `svc` / `tool` commands when `esp_cli_service_bind_manager()` is used with `esp_service_manager`
- Auto-generated commands for every tool exposed by `esp_service_manager`
- Argument parser with support for positional and flag-style options
