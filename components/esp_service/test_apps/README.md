# esp_service Test Apps

Framework-level EMU and real-device test harness for `esp_service`. The companion usage example at [`../examples/mock_services/`](../examples/mock_services/) only demonstrates how to *use* the six mock services; everything that stresses the framework itself (service manager, MCP JSON-RPC, and transports) lives here.

The six mock services under `../examples/mock_services/components/` are consumed as-is via `EXTRA_COMPONENT_DIRS`, so the example and the tests stay in sync automatically.

## Layout

```
test_apps/
├── CMakeLists.txt           # IDF on-device project
├── sdkconfig.defaults
├── sdkconfig.emu            # shared EMU overrides
├── sdkconfig.no_mcp         # MCP-off overrides, no duplicate app
├── pytest_esp_service_test_apps.py
├── main/                    # IDF on-device runner (Unity + optional transport smoke)
│   ├── app_main.c
│   ├── ut_esp_service.c    # esp_service.c: API, lifecycle, and teardown
│   ├── ut_service_manager.c
│   ├── ut_esp_service_mcp_server.c  # normal and fault profiles, selected at build time
│   ├── ut_derived_destroy.c  # real RTMP / RTSP / Extractor teardown
│   ├── ut_transport_web_smoke.c   # HTTP / SSE / WebSocket, with shared Wi-Fi setup
│   ├── ut_transport_uart_smoke.c  # real UART smoke
│   ├── ut_transport_lifecycle.c  # UART / SSE lifecycle stress
│   ├── ut_transport_*_fault.c    # web and serial fault-injection fixtures
│   ├── ut_scheduler_and_mock_tools.c
│   ├── ut_event_hub_demo.c
│   ├── ut_transport_web_fixture.inc
│   ├── stubs/               # fault-injection only
│   ├── Kconfig.projbuild
│   └── CMakeLists.txt
└── scripts/                 # MCP client for a real device
    ├── README.md
    └── test_mcp.py           # http, sse, ws, and uart smoke modes
```

## On-Device (IDF)

Exercises service_manager + MCP over the transports you enable in Kconfig.

```bash
cd test_apps
idf.py set-target esp32s3
idf.py menuconfig   # set Wi-Fi, toggle transports under "ESP Service test_apps Configuration"
idf.py build flash monitor
```

The EMU overlay `sdkconfig.emu` disables MCP network transports and all optional on-device demos, so only Unity `[esp_service]` tests run. The regular `sdkconfig.defaults` retains on-device transport and lifecycle-stress settings. Only one of HTTP / SSE / WS transport demos should be active at a time.

With the matching transport enabled, set its `ESP_SVC_TEST_*_HOLD_AFTER_SMOKE`
option to leave the server running. Use the [single MCP client](scripts/README.md)
from a workstation to check the device over HTTP, SSE, WebSocket, or UART.

## EMU Configurations

There is one IDF project and one `main/app_main.c`. The CMake profile option,
`ESP_SERVICE_TEST_FAULT_INJECTION`, defaults to `OFF`. It selects normal tests
or fault-injection sources and include directories, not runtime service state.
All target apps can run via `pytest --embedded-services idf,espemu` with [`esp-emu`](https://github.com/espressif/esp-emulator).

Run from the repository root after exporting ESP-IDF:

```bash
APP=components/esp_service/test_apps
BUILD="$APP/build_esp32s3_default"
idf.py -C "$APP" -B "$BUILD" -D IDF_TARGET=esp32s3 \
  -D "SDKCONFIG=$BUILD/sdkconfig" \
  -D 'SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.emu' \
  -D ESP_SERVICE_TEST_FAULT_INJECTION=OFF build
pytest "$APP" --embedded-services idf,espemu --target esp32s3
```

Use separate build directories and sdkconfigs for these configurations:

| Configuration | ESP_SERVICE_TEST_FAULT_INJECTION | SDKCONFIG_DEFAULTS | Expected Tests |
| --- | --- | --- | --- |
| Normal, MCP on | `OFF` | `sdkconfig.defaults;sdkconfig.emu` | 37 |
| Normal, MCP off | `OFF` | `sdkconfig.defaults;sdkconfig.emu;sdkconfig.no_mcp` | 33 |
| Fault injection | `ON` | `sdkconfig.defaults;sdkconfig.emu` | 18 |

The normal build uses no fault-injection stubs or test cases. The MCP server
tests live in one file; each profile compiles only its own cases.
The fault build requires MCP enabled and real transport implementations disabled;
incompatible configurations fail during CMake configuration. Mock service
components and their embedded schemas are reused, not compiled again in `main`.
The common test defaults use a 12 KiB main stack and a large app partition;
the EMU overlay enables comprehensive heap poisoning. These settings affect only
test firmware. Production component dependencies and lifecycle code are unchanged.

### Synchronous Deinitialization

The normal configuration compiles the real RTMP, RTSP and Extractor services and
their example sources. No network is needed.
Its main manifest selects the in-tree `esp_service` with an unversioned
`override_path`, leaving the derived components' published dependency ranges unchanged.

Media MCP registration is enabled in the MCP-on configuration. MCP-off keeps the
core, manager and teardown cases; only the four MCP-dependent cases are excluded.

It covers callback-side rejection without mutation, rejection before a successful stop, retry after private cleanup failure, exactly-once resource release, callback/publish drain, caller-owned storage,
and 30 owner-side create/deinit/free cycles per derived service with heap
integrity/balance checks. With MCP enabled, the media deinit regression checks
that rejection and cleanup failure preserve registry entries and success removes them.
The runner exits nonzero on Unity failures, panic, missing summary or timeout.
Only owner-task synchronous destruction is supported. Worker/callback callers
must post requests to the application control task; no reaper task is created.

### Serial Transport Failure And Framing

The fault-injection configuration runs the real MCP server and UART/STDIO/SDIO transport sources
under ESP32-S3 FreeRTOS with heap poisoning. UART/SDIO device I/O and stdin are
scripted: these are failure-injection tests, not physical peripheral validation.
The app also compiles all five real event-hub demo services.

The cases cover blocked handlers, stop/destroy retry, callback-side rejection,
UART driver-delete failure, reader allocation failure/restart, exact release
counts, whole-message overflow rejection and recovery, SDIO fragmented packets,
embedded NUL rejection, delayed DMA completion, and 20 demo teardown cycles.
Scripted HTTP/WebSocket reads advance a test-only clock so emulator scheduling cannot
consume the receive budget.

Production state added by these fixes is limited to:

| State | Purpose |
| --- | --- |
| SDIO `driver_initialized` | Retain and deinitialize a driver exactly once, including partial start failure |
| SDIO `pending_send` | Track one queued DMA buffer until completion or driver teardown; block further queueing while pending |
| One `exited` semaphore per demo | Join the worker before releasing its context; the existing task handle records whether join completed |
| Reader-local `discard` / packet position | Discard a complete oversized message rather than treating its tail as another request |

Existing cross-task `running` flags are atomic instead of plain/volatile booleans.
SDIO adds one request assembly buffer of `recv_buffer_size + 1` bytes per active
reader; completed responses and line buffers remain reader-owned. There is no
reaper thread, asynchronous destruction ticket, or generic reference-count layer.

## Review Follow-Up Coverage

The normal configuration enables both dummy source and sink implementations
so their private timer dependency is compiled as well as the service wrappers.

The transport suite also runs actual example-tool adapters and scheduler
pairing/validation tests. Added production state is one short scheduler lock and
the bridge's default-deny `allow_execution` boolean. The hub adds one saturating
queue-drop counter and one caller-provided name-buffer capacity per statistics
entry; snapshots allocate no memory. Example adapters share two small helpers for
bounded integer validation and JSON serialization, not a general schema engine.

Hub tests now compare active domain/envelope baselines in setup/teardown. Pool
capacity and cumulative drop counts are intentionally not treated as leaked
resources. The suite covers undersized-buffer retry, names surviving domain
removal, queue drops with exact release counts, scheduler callback/context races,
invalid numeric/string arguments, escaped output and oversized Wi-Fi requests.

Bridge safety tests run on the host with mocked network calls; EMU is used for
the C/FreeRTOS code, not for Python or model-provider API verification.

## Related

- Usage example (this harness's companion): [`../examples/mock_services/`](../examples/mock_services/)
- Component README: [`../README.md`](../README.md)
- [LLM Bridge and its offline regression suite](../tools/mcp_llm_bridge/README.md)
