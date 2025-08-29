# esp_service Test Apps

Framework-level test and simulation harness for `esp_service`. The companion usage example at [`../examples/mock_services/`](../examples/mock_services/) only demonstrates how to *use* the six mock services; everything that stresses the framework itself (service manager, MCP JSON-RPC, transports, PC simulations) lives here.

The six mock services under `../examples/mock_services/components/` are consumed as-is via `EXTRA_COMPONENT_DIRS`, so the example and the tests stay in sync automatically.

## Layout

```
test_apps/
├── CMakeLists.txt           # IDF on-device project
├── sdkconfig.defaults
├── pytest_esp_service_test_apps.py
├── main/                    # IDF on-device runner (Unity + optional transport smoke)
│   ├── app_main.c
│   ├── ut_esp_service_core.c
│   ├── ut_service_manager.c
│   ├── ut_*_transport.c     # gated by Kconfig (see CMakeLists.txt)
│   ├── ut_transport_lifecycle_test.c
│   ├── Kconfig.projbuild
│   └── CMakeLists.txt
├── scripts/                 # PC-side MCP JSON-RPC clients against a real DUT
│   ├── README.md
│   ├── test_mcp_http.py     # Plain HTTP POST /mcp (device exposes MCP HTTP)
│   ├── test_mcp_sse.py      # HTTP+SSE (use with ESP_SVC_TEST_SSE_HOLD_AFTER_SMOKE)
│   ├── test_mcp_ws.py       # WebSocket (use with ESP_SVC_TEST_WS_HOLD_AFTER_SMOKE)
│   └── test_mcp_uart.py     # UART (use with ESP_SVC_TEST_UART_HOLD_AFTER_SMOKE)
└── pc/                      # PC / host runner (FreeRTOS POSIX port, native CMake)
    ├── CMakeLists.txt
    ├── FreeRTOSConfig.h
    ├── service_sim_example.c
    ├── sim_support.{c,h}
    ├── service_tool_dispatch_test.c
    ├── service_integration_test.c
    ├── service_coverage_test.c
    ├── mcp_http_server.c
    ├── svc_helpers.h
    ├── host_http_transport.{c,h}
    ├── embedded_files.c
    └── compat/                # IDF → POSIX shim
```

## On-Device (IDF)

Exercises service_manager + MCP over the transports you enable in Kconfig.

```bash
cd test_apps
idf.py set-target esp32s3
idf.py menuconfig   # set Wi-Fi, toggle transports under "ESP Service test_apps Configuration"
idf.py build flash monitor
```

`sdkconfig.defaults` keeps **MCP network transports off** and **all `ESP_SVC_TEST_*` on-device demos off**, so the image only runs Unity `[esp_service]` tests (`ut_esp_service_core` + `ut_service_manager`, stub MCP). Turn on `ESP_MCP_TRANSPORT_*` and `ESP_SVC_TEST_ENABLE_*` in menuconfig when you need SSE / WS / UART smoke or lifecycle stress. Only one of HTTP / SSE / WS transport demos should be active at a time.

After the firmware is up (and the matching Kconfig “hold after smoke” option is on for SSE / WS / UART when you need a long-lived server for the PC scripts), drive the board from a workstation. Run these **from the `test_apps/` directory** so imports and paths stay predictable:

```bash
cd test_apps
python3 scripts/test_mcp_http.py <device-ip>
python3 scripts/test_mcp_sse.py  <device-ip> [http-port]
python3 scripts/test_mcp_ws.py   <device-ip> [port]
python3 scripts/test_mcp_uart.py  <serial-port> [baud]
```

Examples:

```bash
python3 scripts/test_mcp_http.py 192.168.1.100
python3 scripts/test_mcp_sse.py  192.168.1.100 8080
python3 scripts/test_mcp_ws.py   192.168.1.100 8081
python3 scripts/test_mcp_uart.py /dev/ttyUSB0 115200
```

Python dependencies: `pip install requests` (HTTP client), `pip install websockets` (WS client), `pip install pyserial` (UART client).

## PC (host)

```bash
cd test_apps/pc
cmake -S . -B build -G Ninja && cmake --build build
```

The CMake project produces four binaries:

| Binary | Role |
|--------|------|
| `service_sim_example` | Long-running multi-service event-hub simulation (`service_sim_example.c` + `sim_glue.c` + `sim_profile.c` + `sim_traffic.c`). Runs 10–30 s under `light`/`normal`/`heavy` profiles with burst/bulk traffic and prints a PASS/FAIL verdict. Override via `SIM_SCENARIO=heavy SIM_SEED=1 ./build/service_sim_example`. |
| `service_tool_dispatch_test` | Focused test of `esp_service_manager_register/find_by_name/find_by_category/invoke_tool/unregister` — no MCP server/transport. |
| `service_integration_test` | End-to-end: direct C API (Mode A), tool dispatch (Mode B), and MCP-over-HTTP (Mode C) driving the same service instances concurrently for ~12 s. |
| `service_coverage_test` | Full coverage / boundary / stress / multi-thread suite. 14 sections (~370 assertions) exercising NULL / invalid-state paths, full event-hub & service-core state machine, service-manager capacity limits, and concurrent publish/subscribe, queue-mode delivery and `invoke_tool` dispatch from multiple FreeRTOS tasks. |
| `mcp_http_server` | Long-running MCP HTTP server. Registers player/led/ota/wifi and serves tools over `http://127.0.0.1:${MCP_PORT:-9876}/mcp`. Pair with `tools/mcp_llm_bridge/` or `curl`. |

These are integration / scenario tests rather than Unity unit tests — each pulls up a FreeRTOS scheduler, real TCP sockets, and multiple services.

## Related

- Usage example (this harness's companion): [`../examples/mock_services/`](../examples/mock_services/)
- Component README: [`../README.md`](../README.md)
- LLM bridge for `mcp_http_server`: [`../../../tools/mcp_llm_bridge/README.md`](../../../tools/mcp_llm_bridge/README.md)
